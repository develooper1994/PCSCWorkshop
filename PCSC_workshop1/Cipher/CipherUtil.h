#ifndef PCSC_WORKSHOP1_CIPHER_CIPHERUTIL_H
#define PCSC_WORKSHOP1_CIPHER_CIPHERUTIL_H

#include <cstdint>

namespace cipherutil {

inline int egcd(int a, int b, int& x, int& y) {
    if (b == 0) { x = 1; y = 0; return a; }
    int x1, y1;
    int g = egcd(b, a % b, x1, y1);
    x = y1;
    y = x1 - (a / b) * y1;
    return g;
}

inline unsigned invMod256(unsigned a) {
    int x, y;
    int g = egcd(static_cast<int>(a & 0xFFu), 256, x, y);
    if (g != 1) return 0;
    int res = x % 256;
    if (res < 0) res += 256;
    return static_cast<unsigned>(res);
}

inline unsigned mod256(unsigned x) { return x & 0xFFu; }

} // namespace cipherutil

#endif // PCSC_WORKSHOP1_CIPHER_CIPHERUTIL_H
