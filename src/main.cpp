#include "lib/httplib.h"
#include "lib/json.hpp"
#include "base62.h"
#include "lru_cache.h"
#include "rate_limiter.h"
#include "database.h"
#include "click_logger.h"

#include <iostream>
#include <ctime>
#include <set>

using json = nlohmann::json;

// Aliases can't collide with our real routes, and must be URL-safe.
static bool validAlias(const std::string& a) {
    static const std::set<std::string> reserved = {"api", "stats", "shorten", "index"};
    if (a.empty() || a.size() > 32) return false;
    if (reserved.count(a)) return false;
    for (char c : a)
        if (!std::isalnum(static_cast<unsigned char>(c))) return false;
    return true;
}

int main() {
    Database    db("urls.db");
    LRUCache    cache(1000);          // 1000 hottest codes in memory
    RateLimiter limiter(10, 5.0);     // per-IP: burst 10, refill 5/sec
    ClickLogger clicks(db);           // async, batched click persistence

    httplib::Server svr;

    // ---- POST /shorten  {"url","alias?","expires_in?"} ---------------------
    svr.Post("/shorten", [&](const httplib::Request& req, httplib::Response& res) {
        if (!limiter.allow(req.remote_addr)) {
            res.status = 429;
            res.set_header("Retry-After", "1");
            res.set_content(R"({"error":"rate limit exceeded"})", "application/json");
            return;
        }
        std::string longUrl, alias;
        long long expiresIn = 0;
        try {
            auto body = json::parse(req.body);
            longUrl = body.at("url").get<std::string>();
            if (body.contains("alias") && !body["alias"].is_null())
                alias = body["alias"].get<std::string>();
            if (body.contains("expires_in") && !body["expires_in"].is_null())
                expiresIn = body["expires_in"].get<long long>();   // seconds from now
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"expected JSON {\"url\":\"...\"}"})", "application/json");
            return;
        }
        if (longUrl.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"url is required"})", "application/json");
            return;
        }
        long long expiresAt = (expiresIn > 0) ? (static_cast<long long>(time(nullptr)) + expiresIn) : 0;

        std::string code;
        if (!alias.empty()) {                          // ---- custom alias path ----
            if (!validAlias(alias)) {
                res.status = 400;
                res.set_content(R"({"error":"alias must be 1-32 alphanumeric chars"})", "application/json");
                return;
            }
            if (db.codeExists(alias) || !db.insertWithCode(alias, longUrl, expiresAt)) {
                res.status = 409;                      // Conflict
                res.set_content(R"({"error":"alias already taken"})", "application/json");
                return;
            }
            code = alias;
        } else {                                       // ---- auto Base62 path ----
            long long id = db.insertUrl(longUrl, expiresAt);
            if (id < 0) {
                res.status = 500;
                res.set_content(R"({"error":"database error"})", "application/json");
                return;
            }
            code = encode(id);
            db.setCode(id, code);
        }

        if (expiresAt == 0) cache.put(code, longUrl);  // only cache non-expiring links

        json out;
        out["code"]       = code;
        out["short_url"]  = "http://localhost:8080/" + code;
        out["long_url"]   = longUrl;
        out["expires_at"] = expiresAt;
        res.set_content(out.dump(), "application/json");
    });

    // ---- GET /api/urls  -> all links (dashboard table) ---------------------
    svr.Get("/api/urls", [&](const httplib::Request&, httplib::Response& res) {
        json arr = json::array();
        for (const auto& r : db.listUrls()) {
            arr.push_back({
                {"code", r.code}, {"long_url", r.longUrl}, {"clicks", r.clicks},
                {"created_at", r.createdAt}, {"expires_at", r.expiresAt}
            });
        }
        res.set_content(arr.dump(), "application/json");
    });

    // ---- GET /api/timeseries  -> clicks grouped by day (dashboard chart) ----
    svr.Get("/api/timeseries", [&](const httplib::Request&, httplib::Response& res) {
        json arr = json::array();
        for (const auto& p : db.clicksByDay())
            arr.push_back({{"day", p.first}, {"clicks", p.second}});
        res.set_content(arr.dump(), "application/json");
    });

    // ---- GET /stats/{code}  -> analytics for one code ----------------------
    svr.Get(R"(/stats/([0-9A-Za-z]+))", [&](const httplib::Request& req, httplib::Response& res) {
        UrlRow r;
        if (!db.getRecord(req.matches[1], r)) {
            res.status = 404;
            res.set_content(R"({"error":"code not found"})", "application/json");
            return;
        }
        json out;
        out["code"] = r.code; out["long_url"] = r.longUrl; out["clicks"] = r.clicks;
        out["created_at"] = r.createdAt; out["expires_at"] = r.expiresAt;
        res.set_content(out.dump(), "application/json");
    });

    // ---- GET /{code}  -> 302 redirect --------------------------------------
    svr.Get(R"(/([0-9A-Za-z]+))", [&](const httplib::Request& req, httplib::Response& res) {
        std::string code = req.matches[1];
        std::string longUrl;

        if (!cache.get(code, longUrl)) {               // cache miss -> DB
            UrlRow r;
            if (!db.getRecord(code, r)) {
                res.status = 404;
                res.set_content("Not found", "text/plain");
                return;
            }
            if (r.expiresAt != 0 && static_cast<long long>(time(nullptr)) >= r.expiresAt) {
                res.status = 410;                      // Gone (expired)
                res.set_content("This link has expired", "text/plain");
                return;
            }
            longUrl = r.longUrl;
            if (r.expiresAt == 0) cache.put(code, longUrl);
        }
        clicks.enqueue(code);        // async — never blocks the redirect on disk
        res.set_redirect(longUrl, 302);
    });

    // ---- static dashboard at / ---------------------------------------------
    svr.set_mount_point("/", "./public");

    svr.set_tcp_nodelay(true);       // disable Nagle: avoids ~40ms delay on small responses

    std::cout << "Server running on http://localhost:8080  (dashboard at /)\n";
    svr.listen("0.0.0.0", 8080);
    return 0;
}
