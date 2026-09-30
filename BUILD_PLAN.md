# Build Plan — Smart URL Shortener (C++ engine)

## Strategy
Hand-build the "brains" (algorithms + data structures) in clean C++ to show depth.
Keep the HTTP/DB plumbing simple with header-only / embedded libraries so no time is
lost to build errors.

## Tech stack
| Layer | Choice | Why |
|-------|--------|-----|
| Core engine | Pure C++17 (hand-written) | Showcase — Base62 codec, rate limiter, LRU cache |
| HTTP server | cpp-httplib | Single header, no build friction |
| JSON | nlohmann/json | Single header |
| Database | SQLite | Embedded — no DB server to install |
| Build | CMake | Standard, expected |
| Frontend | Plain HTML + Chart.js | Analytics dashboard, no framework |
| Package | Docker | One-command run |
| Benchmark | wrk or hey | Generates resume numbers |

Note: cache + rate limiter are hand-built in C++. Redis is documented as the
"how I'd scale this out" section (not implemented) — a strong interview talking point.

## Architecture
```
   Client
     |  POST /shorten --> [Rate Limiter] --> [Base62 Engine] --> [SQLite]
     |                       (your C++)         (your C++)
     |  GET /aX9k2   --> [LRU Cache] --hit--> redirect (fast)
     |                     (your C++)
     |                        |miss
     |                        v
     |                     [SQLite] --> fill cache --> redirect
     |                        |
     |                        +--> [Analytics] (async log)
                              |
                   [ HTML + Chart.js dashboard ]
```

## The engine — 3 things you hand-build in C++ (the showcase)
1. Base62 encoder/decoder — numeric counter <-> short code. 62^7 ~ 3.5 trillion combos.
2. Rate limiter — Token Bucket + Sliding Window Counter, thread-safe counters.
3. LRU cache — hashmap + doubly-linked list, O(1) get/put, built from scratch.

## 4-day plan

### Day 1 — The engine (pure C++, no web yet)
- Base62 encoder/decoder + unit tests
- LRU cache class + unit tests
- Rate limiter (token bucket) + unit tests
- Goal: all core logic works and is tested in isolation.

### Day 2 — Wire up the service
- Drop in cpp-httplib, expose POST /shorten and GET /:code
- Connect SQLite for persistence
- Plug Base62 engine + LRU cache into the request flow
- Goal: create a link, get redirected — end to end.

### Day 3 — Features + protection
- Attach rate limiter to /shorten, return 429 + Retry-After
- Async analytics logging (timestamp, IP, referrer)
- Custom aliases + link expiry
- Chart.js dashboard
- Goal: it's a real product.

### Day 4 — Resume polish
- Load test -> capture numbers (e.g. "20,000 redirects/sec, p99 3ms")
- Dockerize with one-line run command
- README: architecture diagram, Base62 math, "why I hand-built the LRU cache",
  and a "Scaling to distributed with Redis" section
- Deploy to a live URL if time allows

## What makes this version stand out
- Hand-built three classic interview components (Base62, LRU, rate limiter) in C++.
- Serious benchmark numbers from a systems language.
- A clean "how I'd scale it" story (Redis, sharding).
