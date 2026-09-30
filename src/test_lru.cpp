#include "lru_cache.h"
#include <iostream>

int main() {
    LRUCache cache(2);          // capacity: only 2 items fit
    std::string v;

    cache.put("a", "apple");
    cache.put("b", "banana");
    std::cout << (cache.get("a", v) && v == "apple" ? "get a OK\n" : "get a FAIL\n");

    // "a" was just used (MRU). Adding "c" must evict the STALEST item -> "b".
    cache.put("c", "cherry");
    std::cout << (!cache.get("b", v)                 ? "b evicted OK\n"   : "b evicted FAIL\n");
    std::cout << (cache.get("a", v) && v == "apple"  ? "a still here OK\n": "a FAIL\n");
    std::cout << (cache.get("c", v) && v == "cherry" ? "c present OK\n"   : "c FAIL\n");
    std::cout << "size = " << cache.size() << " (expected 2)\n";
    return 0;
}
