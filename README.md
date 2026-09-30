# 🔗 Shortly — A High-Performance URL Shortener in C++

A fast, self-contained URL shortening service built from scratch in modern C++ (C++17).
It turns long URLs into short codes, redirects in **~0.18 ms**, and sustains
**33,000+ requests/second** on a single core — with a cache-first read path, Base62 ID
generation, per-IP rate limiting, link expiry, custom aliases, and a live analytics dashboard.

> Built as a systems-design exercise: the classic "Design TinyURL" interview question, actually implemented.

---

## ✨ Features
- **Short links** via Base62-encoded auto-increment IDs (7 chars ≈ 3.5 trillion URLs).
- **Cache-first reads** — an O(1) hand-built LRU cache serves hot links from memory.
- **Per-IP rate limiting** — a token-bucket limiter (allows bursts, caps sustained rate).
- **Custom aliases** — `zmt.co/my-brand`, with uniqueness enforced by the DB.
- **Link expiry** — optional TTL; expired links return `410 Gone` and bypass the cache.
- **Analytics** — every click is logged **asynchronously** (off the hot path) into a
  time-series table; a dashboard shows clicks over time, top links, and QR codes.
- **Single static binary** — no runtime dependencies; SQLite is compiled in.

## 🏗️ Architecture

```
                          ┌──────────────────────────────────────────────┐
   Client                 │                 C++ Server (httplib)          │
     │                    │                                               │
     │  POST /shorten ────┼──► [Rate Limiter] ─► [Base62] ─► [SQLite]     │
     │                    │      token bucket      encode      (indexed)  │
     │                    │                                               │
     │  GET /{code} ──────┼──► [LRU Cache] ──hit──► 302 redirect (0.18ms) │
     │                    │        │miss                                  │
     │                    │        ▼                                      │
     │                    │     [SQLite] ─► fill cache ─► 302 redirect    │
     │                    │        │                                      │
     │                    │        └─► [Async Click Logger] ─► SQLite     │
     │                    │              (background thread, batched)      │
     │                    │                                               │
     │  GET /  ───────────┼──► static dashboard (Chart.js + QR)           │
     └────────────────────┴──────────────────────────────────────────────┘
```

Each component is hand-written and unit-tested:
| Component | File | Idea |
|-----------|------|------|
| Base62 codec | `src/base62.*` | number ↔ short code |
| LRU cache | `src/lru_cache.h` | hash map + doubly-linked list, O(1) get/put |
| Rate limiter | `src/rate_limiter.h` | token bucket, lazy refill, thread-safe |
| Async click logger | `src/click_logger.h` | queue + worker thread, batched writes |
| Database | `src/database.*` | SQLite, prepared statements, WAL mode |
| Server | `src/main.cpp` | httplib routing, wires it all together |

## 📊 Benchmarks
Load test on the cache-hit redirect path (`build/bench.exe`, 8 threads × 5,000 requests,
single machine, loopback):

| Metric | Result |
|--------|--------|
| Throughput | **33,528 req/sec** |
| Latency p50 | **0.18 ms** |
| Latency p90 | 0.25 ms |
| Latency p99 | 0.37 ms |
| Failures | 0 / 40,000 |

## 🚀 Running it

### Option A — local (Windows, MinGW-w64 / MSYS2 GCC 13+)
```bash
./build.sh                # compiles server + benchmark
./build/server.exe        # starts on http://localhost:8080
```
Open **http://localhost:8080** for the dashboard.

### Option B — Docker (any platform)
```bash
docker compose up --build
```
Then open **http://localhost:8080**.

## 📡 API

| Method | Route | Description |
|--------|-------|-------------|
| `POST` | `/shorten` | Create a short link. Body: `{"url","alias?","expires_in?"}` |
| `GET`  | `/{code}` | 302-redirect to the original URL |
| `GET`  | `/stats/{code}` | Analytics for one link (JSON) |
| `GET`  | `/api/urls` | All links (JSON) — powers the dashboard table |
| `GET`  | `/api/timeseries` | Clicks grouped by day (JSON) — powers the chart |
| `GET`  | `/` | The analytics dashboard |

**Create example**
```bash
curl -X POST http://localhost:8080/shorten \
  -H "Content-Type: application/json" \
  -d '{"url":"https://example.com/very/long/link","alias":"demo","expires_in":3600}'
# -> {"code":"demo","short_url":"http://localhost:8080/demo","expires_at":...}
```

**Status codes:** `200` ok · `302` redirect · `400` bad request · `409` alias taken ·
`410` expired · `429` rate limited · `404` not found.

## 🧠 Key design decisions
- **302, not 301, redirects** — a permanent (301) redirect gets cached by browsers, so
  repeat clicks never reach the server and analytics undercount. 302 keeps every click visible.
- **Cache-first reads** — the read:write ratio is ~100:1, so an in-memory LRU in front of
  SQLite is the dominant performance lever. Expiring links skip the cache so it can never
  serve stale data.
- **Async click logging** — persisting a click is a *side effect*, not part of serving the
  redirect. A background thread batches writes in transactions, keeping the hot path at
  sub-millisecond latency.
- **Base62 from a counter** — collision-free by construction (unlike random codes), and 7
  characters already covers 62⁷ ≈ 3.5 trillion links.
- **Prepared statements + an index on `code`** — safe against SQL injection and fast lookups
  (B-tree, not a full scan). WAL mode lets reads and the writer proceed concurrently.

## 📈 Scaling to distributed (design, not yet implemented)
This runs as a single node. To scale horizontally:
- **Move the cache and rate-limiter state to Redis** so many stateless app instances share
  one source of truth (atomic `INCR` for the counter, Lua scripts for atomic rate-limit checks).
- **Shard the database** by code (or move to a distributed KV store like DynamoDB/Cassandra)
  once a single SQLite/Postgres node is the bottleneck.
- **Put the instances behind a load balancer**; the ID counter becomes a central Redis `INCR`
  (or a per-node ID range) to keep codes globally unique.

## 🗂️ Project structure
```
src/
  base62.{h,cpp}      Base62 encoder/decoder
  lru_cache.h         O(1) LRU cache
  rate_limiter.h      token-bucket rate limiter
  click_logger.h      async batched click persistence
  database.{h,cpp}    SQLite wrapper
  main.cpp            HTTP server
  bench.cpp           concurrent load tester
  lib/                httplib, nlohmann/json, sqlite3 (vendored)
public/index.html     analytics dashboard
build.sh  Dockerfile  docker-compose.yml
```

## 🛠️ Built with
Modern C++17 · [cpp-httplib](https://github.com/yhirose/cpp-httplib) ·
[nlohmann/json](https://github.com/nlohmann/json) · [SQLite](https://sqlite.org) ·
Chart.js · qrcode.js
