#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <utility>
#include "lib/sqlite3.h"

// One stored short-link row.
struct UrlRow {
    std::string code;
    std::string longUrl;
    long long   clicks    = 0;
    long long   createdAt = 0;
    long long   expiresAt = 0;   // 0 = never expires (unix seconds otherwise)
};

// Thin wrapper around SQLite for URL mappings + click analytics.
class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    bool codeExists(const std::string& code);

    // Auto-code path: insert a URL, return the new row id (-1 on failure).
    long long insertUrl(const std::string& longUrl, long long expiresAt);
    // Store the generated Base62 code for a row id.
    bool setCode(long long id, const std::string& code);

    // Custom-alias path: insert with a chosen code. Returns false if the code is taken.
    bool insertWithCode(const std::string& code, const std::string& longUrl, long long expiresAt);

    // Fetch a single row. Returns false if the code doesn't exist.
    bool getRecord(const std::string& code, UrlRow& out);

    // Record a click: bump the counter AND log a timestamped event (for time-series).
    void logClick(const std::string& code);

    // Batch-write many clicks in ONE transaction (used by the async logger — keeps the
    // redirect hot-path from blocking on disk).
    void logClicksBatch(const std::vector<std::string>& codes);

    // Dashboard data.
    std::vector<UrlRow> listUrls();
    std::vector<std::pair<std::string, long long>> clicksByDay();

private:
    sqlite3* db_ = nullptr;
};

#endif
