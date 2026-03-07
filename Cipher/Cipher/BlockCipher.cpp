#include "BlockCipher.h"
#include "Exceptions/GenericExceptions.h"
#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <stdexcept>
#include <cstring>

namespace crypto { namespace block {

// ════════════════════════════════════════════════════════════════════════════════
// Raw CBC (no padding) — generic helper
// ════════════════════════════════════════════════════════════════════════════════

static BYTEV rawCbc(const EVP_CIPHER* cipher, const BYTEV& key, const BYTEV& iv,
                    const BYTE* data, size_t len, size_t blockSize, bool encrypt) {
	if (len == 0 || (len % blockSize) != 0)
		throw std::invalid_argument("Data length must be a non-zero multiple of block size");

	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	if (!ctx) throw pcsc::CipherError("EVP_CIPHER_CTX_new failed");

	BYTEV ivCopy = iv;
	int rc;
	if (encrypt)
		rc = EVP_EncryptInit_ex(ctx, cipher, nullptr, key.data(), ivCopy.data());
	else
		rc = EVP_DecryptInit_ex(ctx, cipher, nullptr, key.data(), ivCopy.data());

	if (rc != 1) { EVP_CIPHER_CTX_free(ctx); throw pcsc::CipherError("EVP_CipherInit failed"); }

	EVP_CIPHER_CTX_set_padding(ctx, 0);

	BYTEV out(len + blockSize);
	int outLen = 0;
	int finalLen = 0;

	if (encrypt) {
		rc = EVP_EncryptUpdate(ctx, out.data(), &outLen, data, static_cast<int>(len));
		if (rc != 1) { EVP_CIPHER_CTX_free(ctx); throw pcsc::CipherError("EVP_EncryptUpdate failed"); }
		rc = EVP_EncryptFinal_ex(ctx, out.data() + outLen, &finalLen);
		if (rc != 1) { EVP_CIPHER_CTX_free(ctx); throw pcsc::CipherError("EVP_EncryptFinal failed"); }
	} else {
		rc = EVP_DecryptUpdate(ctx, out.data(), &outLen, data, static_cast<int>(len));
		if (rc != 1) { EVP_CIPHER_CTX_free(ctx); throw pcsc::CipherError("EVP_DecryptUpdate failed"); }
		rc = EVP_DecryptFinal_ex(ctx, out.data() + outLen, &finalLen);
		if (rc != 1) { EVP_CIPHER_CTX_free(ctx); throw pcsc::CipherError("EVP_DecryptFinal failed"); }
	}

	EVP_CIPHER_CTX_free(ctx);
	out.resize(static_cast<size_t>(outLen + finalLen));
	return out;
}

// ════════════════════════════════════════════════════════════════════════════════
// AES-CBC raw
// ════════════════════════════════════════════════════════════════════════════════

static const EVP_CIPHER* aesCipherByKeyLen(size_t keyLen) {
	switch (keyLen) {
	case 16: return EVP_aes_128_cbc();
	case 24: return EVP_aes_192_cbc();
	case 32: return EVP_aes_256_cbc();
	default: throw std::invalid_argument("AES key must be 16, 24, or 32 bytes");
	}
}

BYTEV encryptAesCbc(const BYTEV& key, const BYTEV& iv, const BYTE* data, size_t len) {
	return rawCbc(aesCipherByKeyLen(key.size()), key, iv, data, len, AES_BLOCK, true);
}

BYTEV decryptAesCbc(const BYTEV& key, const BYTEV& iv, const BYTE* data, size_t len) {
	return rawCbc(aesCipherByKeyLen(key.size()), key, iv, data, len, AES_BLOCK, false);
}

// ════════════════════════════════════════════════════════════════════════════════
// 2K3DES-CBC raw (16-byte key → 24 expansion)
// ════════════════════════════════════════════════════════════════════════════════

BYTEV encrypt2K3DesCbc(const BYTEV& key, const BYTEV& iv, const BYTE* data, size_t len) {
	BYTEV key24 = key;
	if (key24.size() == 16)
		key24.insert(key24.end(), key.begin(), key.begin() + 8);
	return rawCbc(EVP_des_ede3_cbc(), key24, iv, data, len, DES_BLOCK, true);
}

BYTEV decrypt2K3DesCbc(const BYTEV& key, const BYTEV& iv, const BYTE* data, size_t len) {
	BYTEV key24 = key;
	if (key24.size() == 16)
		key24.insert(key24.end(), key.begin(), key.begin() + 8);
	return rawCbc(EVP_des_ede3_cbc(), key24, iv, data, len, DES_BLOCK, false);
}

// ════════════════════════════════════════════════════════════════════════════════
// 3K3DES-CBC raw (24-byte key, native)
// ════════════════════════════════════════════════════════════════════════════════

BYTEV encrypt3K3DesCbc(const BYTEV& key, const BYTEV& iv, const BYTE* data, size_t len) {
	if (key.size() != 24)
		throw std::invalid_argument("3K3DES key must be 24 bytes");
	return rawCbc(EVP_des_ede3_cbc(), key, iv, data, len, DES_BLOCK, true);
}

BYTEV decrypt3K3DesCbc(const BYTEV& key, const BYTEV& iv, const BYTE* data, size_t len) {
	if (key.size() != 24)
		throw std::invalid_argument("3K3DES key must be 24 bytes");
	return rawCbc(EVP_des_ede3_cbc(), key, iv, data, len, DES_BLOCK, false);
}

// ════════════════════════════════════════════════════════════════════════════════
// AES-128 CMAC (OMAC1)
// ════════════════════════════════════════════════════════════════════════════════

BYTEV cmacAes128(const BYTEV& key, const BYTE* data, size_t len) {
	EVP_MAC* mac = EVP_MAC_fetch(nullptr, "CMAC", nullptr);
	if (!mac) throw pcsc::CipherError("EVP_MAC_fetch(CMAC) failed");

	EVP_MAC_CTX* ctx = EVP_MAC_CTX_new(mac);
	if (!ctx) { EVP_MAC_free(mac); throw pcsc::CipherError("EVP_MAC_CTX_new failed"); }

	OSSL_PARAM params[2];
	params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_CIPHER,
	                                              const_cast<char*>("AES-128-CBC"), 0);
	params[1] = OSSL_PARAM_construct_end();

	if (EVP_MAC_init(ctx, key.data(), key.size(), params) != 1) {
		EVP_MAC_CTX_free(ctx); EVP_MAC_free(mac);
		throw pcsc::CipherError("EVP_MAC_init(CMAC) failed");
	}

	EVP_MAC_update(ctx, data, len);

	BYTEV out(AES_BLOCK);
	size_t outLen = 0;
	EVP_MAC_final(ctx, out.data(), &outLen, out.size());
	out.resize(outLen);

	EVP_MAC_CTX_free(ctx);
	EVP_MAC_free(mac);
	return out;
}

}} // namespace crypto::block
