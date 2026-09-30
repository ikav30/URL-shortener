#include "database.h"

Database::Database(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        db_ = nullptr;
        return;
    }
    const char* schema =
        "CREATE TABLE IF NOT EXISTS urls ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  code TEXT UNIQUE,"
        "  long_url TEXT NOT NULL,"
        "  clicks INTEGER NOT NULL DEFAULT 0,"
        "  created_at INTEGER NOT NULL DEFAULT (strftime('%s','now')),"
        "  expires_at INTEGER NOT NULL DEFAULT 0"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_code ON urls(code);"
        "CREATE TABLE IF NOT EXISTS click_events ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  code TEXT NOT NULL,"
        "  ts INTEGER NOT NULL"
        ");";
    sqlite3_exec(db_, schema, nullptr, nullptr, nullptr);
    // Migration for DBs created before expires_at existed (error ignored if it's there).
    sqlite3_exec(db_, "ALTER TABLE urls ADD COLUMN expires_at INTEGER NOT NULL DEFAULT 0;",
                 nullptr, nullptr, nullptr);
    // WAL lets readers and the writer proceed concurrently; NORMAL sync is the standard
    // speed/durability trade-off for WAL.
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;",   nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

bool Database::codeExists(const std::string& code) {
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, "SELECT 1 FROM urls WHERE code = ?;", -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}

long long Database::insertUrl(const std::string& longUrl, long long expiresAt) {
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, "INSERT INTO urls (long_url, expires_at) VALUES (?, ?);",
                           -1, &stmt, nullptr) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, longUrl.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, expiresAt);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return -1;
    return sqlite3_last_insert_rowid(db_);
}

bool Database::setCode(long long id, const std::string& code) {
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, "UPDATE urls SET code = ? WHERE id = ?;",
                           -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool Database::insertWithCode(const std::string& code, const std::string& longUrl, long long expiresAt) {
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, "INSERT INTO urls (code, long_url, expires_at) VALUES (?, ?, ?);",
                           -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, code.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, longUrl.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, expiresAt);
    int rc = sqlite3_step(stmt);          // fails the UNIQUE constraint if code is taken
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool Database::getRecord(const std::string& code, UrlRow& out) {
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_,
            "SELECT code, long_url, clicks, created_at, expires_at FROM urls WHERE code = ?;",
            -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out.code      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        out.longUrl   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        out.clicks    = sqlite3_column_int64(stmt, 2);
        out.createdAt = sqlite3_column_int64(stmt, 3);
        out.expiresAt = sqlite3_column_int64(stmt, 4);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

void Database::logClick(const std::string& code) {
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, "UPDATE urls SET clicks = clicks + 1 WHERE code = ?;",
                           -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    if (sqlite3_prepare_v2(db_,
            "INSERT INTO click_events (code, ts) VALUES (?, strftime('%s','now'));",
            -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void Database::logClicksBatch(const std::vector<std::string>& codes) {
    if (codes.empty()) return;
    sqlite3_exec(db_, "BEGIN;", nullptr, nullptr, nullptr);
    sqlite3_stmt* up = nullptr;
    sqlite3_stmt* ev = nullptr;
    sqlite3_prepare_v2(db_, "UPDATE urls SET clicks = clicks + 1 WHERE code = ?;", -1, &up, nullptr);
    sqlite3_prepare_v2(db_, "INSERT INTO click_events (code, ts) VALUES (?, strftime('%s','now'));",
                       -1, &ev, nullptr);
    for (const auto& c : codes) {
        sqlite3_bind_text(up, 1, c.c_str(), -1, SQLITE_TRANSIENT); sqlite3_step(up); sqlite3_reset(up);
        sqlite3_bind_text(ev, 1, c.c_str(), -1, SQLITE_TRANSIENT); sqlite3_step(ev); sqlite3_reset(ev);
    }
    sqlite3_finalize(up);
    sqlite3_finalize(ev);
    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
}

std::vector<UrlRow> Database::listUrls() {
    std::vector<UrlRow> rows;
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_,
            "SELECT code, long_url, clicks, created_at, expires_at "
            "FROM urls WHERE code IS NOT NULL ORDER BY id DESC;",
            -1, &stmt, nullptr) != SQLITE_OK) return rows;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        UrlRow r;
        r.code      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        r.longUrl   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        r.clicks    = sqlite3_column_int64(stmt, 2);
        r.createdAt = sqlite3_column_int64(stmt, 3);
        r.expiresAt = sqlite3_column_int64(stmt, 4);
        rows.push_back(r);
    }
    sqlite3_finalize(stmt);
    return rows;
}

std::vector<std::pair<std::string, long long>> Database::clicksByDay() {
    std::vector<std::pair<std::string, long long>> out;
    sqlite3_stmt* stmt;
    const char* sql =
        "SELECT strftime('%Y-%m-%d', ts, 'unixepoch') AS day, COUNT(*) "
        "FROM click_events GROUP BY day ORDER BY day;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string day = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        out.emplace_back(day, sqlite3_column_int64(stmt, 1));
    }
    sqlite3_finalize(stmt);
    return out;
}
