#include "base62.h"
#include <iostream>

int main() {
    long long tests[] = {0, 1, 61, 62, 125, 1000000, 3521614606207LL};
    for (long long t : tests) {
        std::string code = encode(t);
        long long back = decode(code);
        std::cout << t << "  ->  \"" << code << "\"  ->  " << back
                  << (back == t ? "   OK" : "   FAIL") << "\n";
    }
    return 0;
}
