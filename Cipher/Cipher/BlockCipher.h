#ifndef PCSC_CRYPTO_BLOCKCIPHER_H
#define PCSC_CRYPTO_BLOCKCIPHER_H

#include "Types.h"
#include <cstddef>

namespace crypto { namespace block {

constexpr size_t AES_BLOCK = 16;
constexpr size_t DES_BLOCK = 8;

BYTEV encryptAesCbc(const BYTEV& key, const BYTEV& iv, const BYTE* data, size_t len);
BYTEV decryptAesCbc(const BYTEV& key, const BYTEV& iv, const BYTE* data, size_t len);
inline BYTEV encryptAesCbc(const BYTEV& key, const BYTEV& iv, const BYTEV& d) { return encryptAesCbc(key, iv, d.data(), d.size()); }
inline BYTEV decryptAesCbc(const BYTEV& key, const BYTEV& iv, const BYTEV& d) { return decryptAesCbc(key, iv, d.data(), d.size()); }

BYTEV encrypt2K3DesCbc(const BYTEV& key, const BYTEV& iv, const BYTE* data, size_t len);
BYTEV decrypt2K3DesCbc(const BYTEV& key, const BYTEV& iv, const BYTE* data, size_t len);
inline BYTEV encrypt2K3DesCbc(const BYTEV& key, const BYTEV& iv, const BYTEV& d) { return encrypt2K3DesCbc(key, iv, d.data(), d.size()); }
inline BYTEV decrypt2K3DesCbc(const BYTEV& key, const BYTEV& iv, const BYTEV& d) { return decrypt2K3DesCbc(key, iv, d.data(), d.size()); }

BYTEV encrypt3K3DesCbc(const BYTEV& key, const BYTEV& iv, const BYTE* data, size_t len);
BYTEV decrypt3K3DesCbc(const BYTEV& key, const BYTEV& iv, const BYTE* data, size_t len);
inline BYTEV encrypt3K3DesCbc(const BYTEV& key, const BYTEV& iv, const BYTEV& d) { return encrypt3K3DesCbc(key, iv, d.data(), d.size()); }
inline BYTEV decrypt3K3DesCbc(const BYTEV& key, const BYTEV& iv, const BYTEV& d) { return decrypt3K3DesCbc(key, iv, d.data(), d.size()); }

BYTEV cmacAes128(const BYTEV& key, const BYTE* data, size_t len);
inline BYTEV cmacAes128(const BYTEV& key, const BYTEV& d) { return cmacAes128(key, d.data(), d.size()); }

}} // namespace crypto::block

#endif // PCSC_CRYPTO_BLOCKCIPHER_H
