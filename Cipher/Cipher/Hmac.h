#ifndef PCSC_CRYPTO_HMAC_H
#define PCSC_CRYPTO_HMAC_H

#include "Types.h"

namespace crypto {

BYTEV hmacSha256(const BYTEV& key, const BYTE* data, size_t len);
BYTEV hmacSha384(const BYTEV& key, const BYTE* data, size_t len);
BYTEV hmacSha512(const BYTEV& key, const BYTE* data, size_t len);

inline BYTEV hmacSha256(const BYTEV& k, const BYTEV& d) { return hmacSha256(k, d.data(), d.size()); }
inline BYTEV hmacSha384(const BYTEV& k, const BYTEV& d) { return hmacSha384(k, d.data(), d.size()); }
inline BYTEV hmacSha512(const BYTEV& k, const BYTEV& d) { return hmacSha512(k, d.data(), d.size()); }

} // namespace crypto

#endif // PCSC_CRYPTO_HMAC_H
