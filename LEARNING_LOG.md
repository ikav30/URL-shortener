# Learning Log — Smart URL Shortener (C++)

> Re-read this before interviews. Each entry = what I built, why, and the one-line
> insight to say out loud.

## Setup (Day 0)
- Installed a modern C++ toolchain on Windows via **MSYS2** (GCC 16.2.0) after finding
  the pre-installed compiler was an ancient GCC 6.3.0 (2016).
- Learned the **PATH shadowing** lesson: when two programs share a name, the folder
  listed *earlier* in PATH wins. The old `C:\MinGW\bin` was shadowing the new
  `C:\msys64\ucrt64\bin`; fixed by removing the old entry from the **System** PATH.
  Also learned: System PATH is searched before User PATH, and a running terminal caches
  its PATH — you need a *fresh* terminal after editing.
- Hit `error: 'mutex' is not a member of 'std'` — NOT a code bug. The old MinGW used the
  `win32` thread model which omits C++'s threading library; modern MinGW-w64 uses
  winpthreads and includes `std::mutex`/`std::thread`. Lesson: some errors point at the
  *toolchain*, not the code.
- Tools now in use: g++ (GCC 16), CMake, Git, plus header-only libs later
  (cpp-httplib, nlohmann/json) and SQLite.

## Day 1 — The engine (pure C++)
### 1. Base62 encoder/decoder  ✅
- **What:** Convert an auto-increment ID (1,2,3,...) into a short code using 62 symbols
  (0-9, a-z, A-Z). `encode` = repeatedly take `n%62` (a digit) and `n/=62`, then reverse.
  `decode` = the inverse (`n = n*62 + value` for each char).
- **Why:** turns ugly `/1000000` into short `/4c92`. 7 chars = 62^7 ≈ 3.5 trillion URLs.
- **Key insight to say:** a character's *position* in the alphabet string IS its digit value.
  Verified the round-trip property `decode(encode(x)) == x` on edge cases (0, 61, 62, max).
- **Build lesson:** hit an "undefined reference" linker error — learned the difference
  between *compiling* (per-file, needs only the `.h` declaration) and *linking* (stitches
  all `.cpp` definitions together). Fix: compile ALL `.cpp` files together, not just one.
- **Files:** `src/base62.h`, `src/base62.cpp`, `src/test_base62.cpp`
- **Build/run:** `g++ -std=c++17 src/base62.cpp src/test_base62.cpp -o test_base62`

### 2. LRU cache  ✅
- **What:** Fixed-capacity cache that evicts the Least Recently Used item when full.
  Both `get` and `put` are O(1).
- **Why:** popular short codes get requested thousands of times; serve them from memory
  instead of hitting the DB every time.
- **How (the key design):** hash map (`unordered_map`) for O(1) *lookup* + doubly-linked
  `std::list` for O(1) *reordering/eviction*. front = most recently used, back = LRU.
  `list::splice` moves an accessed node to the front in O(1) WITHOUT invalidating the
  iterator stored in the map — that's why a linked list, not a vector.
- **Key insight to say:** "map for O(1) find, linked list for O(1) reorder+evict; splice
  promotes a node without breaking the map's iterators."
- **Proof:** capacity 2; touched `a`, inserted `c`, and `b` (the stalest) was evicted, not `a`.
- **Files:** `src/lru_cache.h`, `src/test_lru.cpp`

### 3. Rate limiter (Token Bucket)  ✅
- **What:** Each user/IP gets a "bucket" of tokens (max = capacity). Each request spends 1
  token; tokens refill at a fixed rate. Token available → allow; empty → reject (429).
- **Why:** stops abuse — one user can't hammer the create endpoint and take the service down.
- **Key trick — lazy refill:** instead of a background timer thread adding tokens, we compute
  `tokens = min(capacity, tokens + elapsedSeconds * refillRate)` on each request using a
  stored timestamp. No threads, scales to millions of keys.
- **Concurrency:** a `std::mutex` guards the per-key buckets because a real web server handles
  requests on many threads at once. This is the systems/concurrency showpiece.
- **Behavior:** allows bursts up to capacity, caps sustained rate at refill rate, per-user isolation.
- **Proof:** capacity 5 → first 5 allowed, 6th blocked, allowed again after ~1.1s refill,
  a different user unaffected.
- **Files:** `src/rate_limiter.h`, `src/test_rate_limiter.cpp`

---

## ✅ Day 1 COMPLETE — the engine (pure C++) is built and tested
Three hand-written, unit-tested components: **Base62 codec**, **LRU cache**, **Token-bucket
rate limiter**. These are the "brains" I can point to in interviews. Next (Day 2): wrap them
in an HTTP service with cpp-httplib + SQLite so it becomes a real, callable API.

## Day 2 — the HTTP service (real, callable API)  ✅
### Libraries added (all in `src/lib/`)
- **cpp-httplib** (`httplib.h`) — single-header HTTP server. No install.
- **nlohmann/json** (`json.hpp`) — single-header JSON parse/build.
- **SQLite** (`sqlite3.c` + `sqlite3.h`) — the "amalgamation": the whole DB engine as one
  C file compiled straight into our app. No DB server to install/run.

### Database layer (`src/database.h/.cpp`)
- Wraps SQLite with **prepared statements** (`sqlite3_prepare_v2` + bind + step) — the safe
  way to run SQL; the `?` placeholders prevent SQL injection.
- Table `urls(id PK AUTOINCREMENT, code UNIQUE, long_url, clicks, created_at)` with an
  **index on `code`** so lookups are fast (B-tree, not a full scan).
- Methods: insertUrl -> id, setCode, getUrl, incrementClicks, getClicks.

### Server (`src/main.cpp`)
- `POST /shorten {"url":"..."}` -> rate-limit by IP -> insert -> id -> **Base62** code ->
  store -> warm the **LRU cache** -> return JSON.
- `GET /{code}` -> **cache-first** lookup (LRU, then DB on miss) -> increment clicks ->
  **302 redirect** (302 not 301 so clicks keep being tracked).
- `GET /stats/{code}` -> analytics JSON (long_url + click count).
- All three Day-1 components are now wired into a live service.

### Build (the tricky Windows bits I learned)
- SQLite compiled once to `build/sqlite3.o` (it's huge — don't recompile every build).
- Link httplib with **`-lws2_32`** (Windows sockets library).
- First run crashed with **exit 127** = missing MSYS runtime DLLs. Fixed with
  **static linking** (`-static -static-libgcc -static-libstdc++`) so the `.exe` is
  self-contained (only depends on Windows' own `ucrtbase.dll`). Bonus: portable for Docker.
- One-command build: **`./build.sh`**

### Proven working (end-to-end over HTTP)
- Created a short code, got a **302 redirect** to the original URL.
- Analytics counted **3 clicks** correctly.
- Fired **40 concurrent** POSTs -> 19 allowed, **21 returned HTTP 429** — rate limiter
  blocks bursts AND is thread-safe under real concurrent load.

**Key interview line for Day 2:** "It's a single self-contained C++ binary — an httplib
server with a cache-first read path (LRU -> SQLite), Base62 IDs, and a token-bucket rate
limiter, all thread-safe. Reads hit memory first; the DB has an index on the code column."

## Day 3 — features + analytics dashboard  ✅
### Custom aliases
- `POST /shorten` accepts optional `"alias"`. Validated (1-32 alphanumeric, not a reserved
  route like `api`/`stats`/`shorten`). Uniqueness enforced by the DB `UNIQUE` constraint on
  `code` — a duplicate insert fails and we return **409 Conflict**.
### Link expiry
- Optional `"expires_in"` (seconds). Stored as absolute `expires_at` unix time (0 = never).
- On redirect, expired links return **410 Gone**. Expiring links are deliberately NOT cached
  (the LRU only holds permanent links) so the cache can never serve a stale/expired link.
### Analytics (time-series)
- New `click_events(code, ts)` table logs every click with a timestamp (in addition to the
  fast `clicks` counter). Enables a real "clicks over time" chart.
- `GET /api/urls` — all links (for the table + tiles). `GET /api/timeseries` — clicks grouped
  by day via SQL `strftime('%Y-%m-%d', ts, 'unixepoch')` + `GROUP BY`.
### Dashboard (`public/index.html`, served via httplib `set_mount_point`)
- Live stat tiles (total links / clicks / active), create form (URL + alias + expiry),
  instant **QR code** (qrcodejs), two **Chart.js** graphs (clicks over time + top links),
  and a links table. Auto-refreshes every 5s.
### HTTP status codes used (good to name in interviews)
- 200 OK, 302 redirect (not 301 — keeps click tracking), 400 bad request, 409 conflict
  (alias taken), 410 gone (expired), 429 too many requests (rate limited), 404 not found.
- **Route ordering matters:** specific routes (`/api/...`, `/stats/...`) are registered
  before the catch-all `/{code}` so they aren't swallowed by it.

**Key interview line for Day 3:** "Aliases use the DB's UNIQUE constraint for collision
safety; expiry is enforced on read and expiring links bypass the cache so it can't serve
stale data; analytics uses a separate events table so I can show clicks over time, not just
a total."

## Day 4 — performance, benchmarks, Docker, README  ✅
### Async click logging (`src/click_logger.h`)
- Moved click persistence OFF the redirect hot-path: a background thread drains a queue and
  writes clicks in **batched transactions**. Redirect just does an O(1) enqueue.
- Also enabled SQLite **WAL mode** + `synchronous=NORMAL` for concurrent read/write.

### Benchmark tool (`src/bench.cpp`) + results
- Wrote a concurrent load tester in C++ (threads + latency percentiles).
- **Result: 33,528 req/sec, p50 0.18ms, p90 0.25ms, p99 0.37ms, 0 failures** (40k requests,
  cache-hit redirect path).

### Debugging the benchmark (great war stories for interviews)
1. **First run: ~40ms/request.** Suspected Nagle's algorithm + delayed-ACK; added TCP_NODELAY
   (right instinct, but not the main cause here).
2. **Real culprit #1: `localhost` = 208ms/connect vs `127.0.0.1` = 2ms.** On Windows,
   `localhost` resolves to IPv6 `::1` first; the server listens on IPv4, so every connect
   wasted ~200ms failing over. Fix: connect to `127.0.0.1`.
3. **Real culprit #2: keep-alive cap.** httplib closes a connection after
   `CPPHTTPLIB_KEEPALIVE_MAX_COUNT` (default low) requests, forcing constant reconnects →
   30% failures + low throughput. Raised the cap → 762 req/s jumped to **33,528 req/s**.
4. Git Bash mangled the `/1` CLI arg into a Windows path; fixed with `MSYS_NO_PATHCONV=1`.

### Deployment
- **Dockerfile** (multi-stage: `gcc:14` build → `debian:stable-slim` runtime, static binary).
- **docker-compose.yml** with a volume so `urls.db` persists.
- **README.md** — architecture diagram, benchmarks, API, design decisions, and a
  "scaling to distributed with Redis" section.

**Key interview line for Day 4:** "It sustains 33k req/s at 0.18ms p50 on a single core
because reads hit an in-memory LRU and click writes are async/batched off the hot path. The
hardest bug was a benchmarking artifact — Windows resolving localhost to IPv6 added 200ms per
connect, and httplib's keep-alive cap was forcing reconnects."

---

## 🎉 PROJECT COMPLETE (Days 1-4)
A single self-contained C++ URL shortener: Base62 IDs, O(1) LRU cache, token-bucket rate
limiter, async analytics, SQLite persistence, live dashboard, Dockerized, benchmarked at
33k req/s. Every component hand-written and tested. See README.md for the full writeup.
