#ifndef PCSC_CRYPTO_H
#define PCSC_CRYPTO_H

#include "CryptoTypes.h"
#include "Cipher.h"
#include "ICipherAAD.h"
#include "Random.h"
#include "Hash.h"
#include "Hmac.h"
#include "Kdf.h"
#include "BlockCipher.h"
#include "AesCbcCipher.h"
#include "AesCtrCipher.h"
#include "TripleDesCbcCipher.h"
#include "AesGcmCipher.h"
#include "XorCipher.h"
#include "CaesarCipher.h"
#include "AffineCipher.h"
#include <memory>
#include <stdexcept>

// ════════════════════════════════════════════════════════════════════════════════
// crypto — Tek giriş noktası
// ════════════════════════════════════════════════════════════════════════════════
//
//   Derleme zamanı:
//     auto enc = crypto::encrypt<CipherType::AES_128_CBC>(key, iv, data);
//
//   Çalışma zamanı:
//     auto enc = crypto::encrypt(CipherType::AES_128_CBC, key, iv, data);
//
//   Factory:
//     auto cipher = crypto::create(CipherType::AES_128_CBC, key, iv);
//     auto enc = cipher->encrypt(data);
//
// ════════════════════════════════════════════════════════════════════════════════

namespace crypto {

// ── Factory ─────────────────────────────────────────────────────────────────

inline std::unique_ptr<ICipher> create(CipherType type, const BYTEV& key, const BYTEV& iv = {}) {
	switch (type) {
	case CipherType::AES_128_CBC:
	case CipherType::AES_192_CBC:
	case CipherType::AES_256_CBC:
		return std::make_unique<AesCbcCipher>(key, iv);
	case CipherType::AES_128_CTR:
		return std::make_unique<AesCtrCipher>(key, iv);
	case CipherType::AES_128_GCM:
	case CipherType::AES_256_GCM:
		return std::make_unique<AesGcmCipher>(key);
	case CipherType::TripleDES_CBC:
		return std::make_unique<TripleDesCbcCipher>(key, iv);
	default:
		throw std::invalid_argument("Unsupported CipherType for factory");
	}
}

// ── Runtime encrypt/decrypt ─────────────────────────────────────────────────

inline BYTEV encrypt(CipherType type, const BYTEV& key, const BYTEV& iv,
                     const BYTE* data, size_t len) {
	switch (type) {
	case CipherType::AES_128_CBC_RAW:
		return block::encryptAesCbc(key, iv, data, len);
	case CipherType::DES2K_CBC_RAW:
		return block::encrypt2K3DesCbc(key, iv, data, len);
	case CipherType::DES3K_CBC_RAW:
		return block::encrypt3K3DesCbc(key, iv, data, len);
	default: {
		auto c = create(type, key, iv);
		return c->encrypt(data, len);
	}
	}
}

inline BYTEV decrypt(CipherType type, const BYTEV& key, const BYTEV& iv,
                     const BYTE* data, size_t len) {
	switch (type) {
	case CipherType::AES_128_CBC_RAW:
		return block::decryptAesCbc(key, iv, data, len);
	case CipherType::DES2K_CBC_RAW:
		return block::decrypt2K3DesCbc(key, iv, data, len);
	case CipherType::DES3K_CBC_RAW:
		return block::decrypt3K3DesCbc(key, iv, data, len);
	default: {
		auto c = create(type, key, iv);
		return c->decrypt(data, len);
	}
	}
}

inline BYTEV encrypt(CipherType t, const BYTEV& k, const BYTEV& iv, const BYTEV& d) {
	return encrypt(t, k, iv, d.data(), d.size());
}
inline BYTEV decrypt(CipherType t, const BYTEV& k, const BYTEV& iv, const BYTEV& d) {
	return decrypt(t, k, iv, d.data(), d.size());
}

// ── Compile-time encrypt/decrypt ────────────────────────────────────────────

template<CipherType CT>
BYTEV encrypt(const BYTEV& key, const BYTEV& iv, const BYTE* data, size_t len) {
	if constexpr (CT == CipherType::AES_128_CBC || CT == CipherType::AES_192_CBC || CT == CipherType::AES_256_CBC)
		return AesCbcCipher(key, iv).encrypt(data, len);
	else if constexpr (CT == CipherType::AES_128_CTR)
		return AesCtrCipher(key, iv).encrypt(data, len);
	else if constexpr (CT == CipherType::AES_128_GCM || CT == CipherType::AES_256_GCM)
		return AesGcmCipher(key).encrypt(data, len);
	else if constexpr (CT == CipherType::TripleDES_CBC)
		return TripleDesCbcCipher(key, iv).encrypt(data, len);
	else if constexpr (CT == CipherType::AES_128_CBC_RAW)
		return block::encryptAesCbc(key, iv, data, len);
	else if constexpr (CT == CipherType::DES2K_CBC_RAW)
		return block::encrypt2K3DesCbc(key, iv, data, len);
	else if constexpr (CT == CipherType::DES3K_CBC_RAW)
		return block::encrypt3K3DesCbc(key, iv, data, len);
	else
		static_assert(CT != CT, "Unsupported CipherType");
}

template<CipherType CT>
BYTEV decrypt(const BYTEV& key, const BYTEV& iv, const BYTE* data, size_t len) {
	if constexpr (CT == CipherType::AES_128_CBC || CT == CipherType::AES_192_CBC || CT == CipherType::AES_256_CBC)
		return AesCbcCipher(key, iv).decrypt(data, len);
	else if constexpr (CT == CipherType::AES_128_CTR)
		return AesCtrCipher(key, iv).decrypt(data, len);
	else if constexpr (CT == CipherType::AES_128_GCM || CT == CipherType::AES_256_GCM)
		return AesGcmCipher(key).decrypt(data, len);
	else if constexpr (CT == CipherType::TripleDES_CBC)
		return TripleDesCbcCipher(key, iv).decrypt(data, len);
	else if constexpr (CT == CipherType::AES_128_CBC_RAW)
		return block::decryptAesCbc(key, iv, data, len);
	else if constexpr (CT == CipherType::DES2K_CBC_RAW)
		return block::decrypt2K3DesCbc(key, iv, data, len);
	else if constexpr (CT == CipherType::DES3K_CBC_RAW)
		return block::decrypt3K3DesCbc(key, iv, data, len);
	else
		static_assert(CT != CT, "Unsupported CipherType");
}

template<CipherType CT>
BYTEV encrypt(const BYTEV& key, const BYTEV& iv, const BYTEV& d) {
	return encrypt<CT>(key, iv, d.data(), d.size());
}
template<CipherType CT>
BYTEV decrypt(const BYTEV& key, const BYTEV& iv, const BYTEV& d) {
	return decrypt<CT>(key, iv, d.data(), d.size());
}

} // namespace crypto

#endif // PCSC_CRYPTO_H
