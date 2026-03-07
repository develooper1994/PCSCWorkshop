#include "AesCbcCipher.h"
#include "Exceptions/GenericExceptions.h"
#include <openssl/evp.h>
#include <algorithm>

struct AesCbcCipher::Impl {
	BYTEV key;
	BYTEV iv;
	explicit Impl(const BYTEV& k, const BYTEV& i) : key(k), iv(i) {}
};

AesCbcCipher::AesCbcCipher(const BYTEV& key, const BYTEV& iv)
	: pImpl(std::make_unique<Impl>(key, iv)) {
	if (iv.size() != 16) throw pcsc::CipherError("AES IV must be 16 bytes");
	if (key.size() != 16 && key.size() != 24 && key.size() != 32)
		throw pcsc::CipherError("AES key must be 16, 24, or 32 bytes");
}

AesCbcCipher::~AesCbcCipher() = default;
AesCbcCipher::AesCbcCipher(AesCbcCipher&&) noexcept = default;
AesCbcCipher& AesCbcCipher::operator=(AesCbcCipher&&) noexcept = default;

static const EVP_CIPHER* pickAes(size_t keyLen) {
	switch (keyLen) {
	case 16: return EVP_aes_128_cbc();
	case 24: return EVP_aes_192_cbc();
	case 32: return EVP_aes_256_cbc();
	default: return nullptr;
	}
}

BYTEV AesCbcCipher::encrypt(const BYTE* data, size_t len) const {
	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	if (!ctx) throw pcsc::CipherError("EVP_CIPHER_CTX_new failed");

	BYTEV ivCopy = pImpl->iv;
	EVP_EncryptInit_ex(ctx, pickAes(pImpl->key.size()), nullptr,
	                   pImpl->key.data(), ivCopy.data());

	BYTEV out(len + 32);
	int outLen = 0, finalLen = 0;
	EVP_EncryptUpdate(ctx, out.data(), &outLen, data, static_cast<int>(len));
	EVP_EncryptFinal_ex(ctx, out.data() + outLen, &finalLen);
	EVP_CIPHER_CTX_free(ctx);

	out.resize(static_cast<size_t>(outLen + finalLen));
	return out;
}

BYTEV AesCbcCipher::decrypt(const BYTE* data, size_t len) const {
	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	if (!ctx) throw pcsc::CipherError("EVP_CIPHER_CTX_new failed");

	BYTEV ivCopy = pImpl->iv;
	EVP_DecryptInit_ex(ctx, pickAes(pImpl->key.size()), nullptr,
	                   pImpl->key.data(), ivCopy.data());

	BYTEV out(len + 16);
	int outLen = 0, finalLen = 0;
	EVP_DecryptUpdate(ctx, out.data(), &outLen, data, static_cast<int>(len));
	int rc = EVP_DecryptFinal_ex(ctx, out.data() + outLen, &finalLen);
	EVP_CIPHER_CTX_free(ctx);
	if (rc != 1) throw pcsc::CipherError("AES-CBC decrypt failed (bad padding or key)");

	out.resize(static_cast<size_t>(outLen + finalLen));
	return out;
}

bool AesCbcCipher::test() const {
	const char sample[] = "TestData1234567";
	auto enc = encrypt(reinterpret_cast<const BYTE*>(sample), sizeof(sample) - 1);
	auto dec = decrypt(enc.data(), enc.size());
	if (dec.size() != sizeof(sample) - 1) return false;
	return std::equal(dec.begin(), dec.end(), reinterpret_cast<const BYTE*>(sample));
}
