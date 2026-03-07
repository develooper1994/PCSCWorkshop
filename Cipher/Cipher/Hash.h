#ifndef PCSC_CRYPTO_HASH_H
#define PCSC_CRYPTO_HASH_H

#include "Types.h"

namespace crypto {

BYTEV sha256(const BYTE* data, size_t len);
BYTEV sha384(const BYTE* data, size_t len);
BYTEV sha512(const BYTE* data, size_t len);

inline BYTEV sha256(const BYTEV& d) { return sha256(d.data(), d.size()); }
inline BYTEV sha384(const BYTEV& d) { return sha384(d.data(), d.size()); }
inline BYTEV sha512(const BYTEV& d) { return sha512(d.data(), d.size()); }

} // namespace crypto

#endif // PCSC_CRYPTO_HASH_H
