#include "rate_limiter.h"
#include <iostream>
#include <thread>

int main() {
    RateLimiter limiter(5, 1.0);   // burst of 5, refills 1 token/sec
    std::string user = "user1";

    int allowed = 0;
    for (int i = 0; i < 5; i++)
        if (limiter.allow(user)) allowed++;
    std::cout << "First 5 allowed: " << allowed << " (expected 5)\n";

    std::cout << (!limiter.allow(user) ? "6th blocked OK\n" : "6th blocked FAIL\n");

    std::cout << "waiting ~1.1s for a token to refill...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    std::cout << (limiter.allow(user) ? "after refill allowed OK\n" : "after refill FAIL\n");

    // a different user has an independent bucket
    std::cout << (limiter.allow("user2") ? "different user allowed OK\n" : "different user FAIL\n");
    return 0;
}
