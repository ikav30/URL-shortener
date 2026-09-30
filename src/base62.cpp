#include "base62.h"
#include <algorithm>   // for std::reverse

// Our 62 symbols. A character's POSITION in this string is its digit value.
// '0' is value 0, '1' is value 1, ... 'a' is value 10, ... 'A' is value 36.
const std::string CHARS =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

std::string encode(long long n) {
    // Special case: 0 would skip the loop below and return "", so handle it.
    if (n == 0) return "0";

    std::string result;
    while (n > 0) {
        int remainder = n % 62;          // a value 0..61 -> an index into CHARS
        result += CHARS[remainder];      // append that character
        n = n / 62;                      // shrink n for the next digit
    }
    std::reverse(result.begin(), result.end());  // remainders came out backwards
    return result;
}

long long decode(const std::string& code) {
    long long n = 0;
    for (char c : code) {
        // find() gives the index of character c in CHARS = its digit value
        int value = CHARS.find(c);
        n = n * 62 + value;   // same idea as reading "234" = ((2*10)+3)*10+4
    }
    return n;
}
