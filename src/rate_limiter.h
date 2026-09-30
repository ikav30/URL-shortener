#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <algorithm>

class RateLimiter {
public:
    // capacity   = max burst (bucket size)
    // refillRate = tokens added per second
    RateLimiter(double capacity, double refillRate)
        : capacity_(capacity), refillRate_(refillRate) {}

    // Returns true if the request is ALLOWED, false if it must be rejected (429).
    bool allow(const std::string& key) {
        std::lock_guard<std::mutex> lock(mtx_);          // thread-safe (server is multi-threaded)
        auto now = std::chrono::steady_clock::now();

        auto it = buckets_.find(key);
        if (it == buckets_.end()) {                      // first request from this key
            buckets_[key] = { capacity_ - 1.0, now };    // full bucket, spend one token
            return true;
        }

        Bucket& b = it->second;
        double elapsed = std::chrono::duration<double>(now - b.lastRefill).count();
        b.tokens = std::min(capacity_, b.tokens + elapsed * refillRate_);  // lazy refill
        b.lastRefill = now;

        if (b.tokens >= 1.0) {
            b.tokens -= 1.0;
            return true;                                 // allowed
        }
        return false;                                    // rejected — bucket empty
    }

private:
    struct Bucket {
        double tokens;
        std::chrono::steady_clock::time_point lastRefill;
    };
    double capacity_;
    double refillRate_;
    std::mutex mtx_;                                     // guards the map + buckets
    std::unordered_map<std::string, Bucket> buckets_;   // one bucket per user/IP
};

#endif
