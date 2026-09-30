#ifndef BASE62_H
#define BASE62_H

#include <string>

// Convert a non-negative number into a short Base62 string.
std::string encode(long long n);

// Convert a Base62 string back into the original number.
long long decode(const std::string& code);

#endif
