#include "AesCtrCipher.h"
#include "Exceptions/GenericExceptions.h"
#include <openssl/evp.h>
#include <algorithm>

struct AesCtrCipher::Impl {
	BYTEV key;
	BYTEV nonce;
	explicit Impl(const BYTEV& k, const BYTEV& n) : key(k), nonce(n) {}
};

AesCtrCipher::AesCtrCipher(const BYTEV& key, const BYTEV& nonce)
	: pImpl(std::make_unique<Impl>(key, nonce)) {
	if (nonce.size() != 16) throw pcsc::CipherError("AES-CTR IV/nonce must be 16 bytes");
	if (key.size() != 16 && key.size() != 24 && key.size() != 32)
		throw pcsc::CipherError("AES key must be 16, 24, or 32 bytes");
}

AesCtrCipher::~AesCtrCipher() = default;
AesCtrCipher::AesCtrCipher(AesCtrCipher&&) noexcept = default;
AesCtrCipher& AesCtrCipher::operator=(AesCtrCipher&&) noexcept = default;

static const EVP_CIPHER* pickAesCtr(size_t keyLen) {
	switch (keyLen) {
	case 16: return EVP_aes_128_ctr();
	case 24: return EVP_aes_192_ctr();
	case 32: return EVP_aes_256_ctr();
	default: return nullptr;
	}
}

static BYTEV ctrTransform(const BYTEV& key, const BYTEV& nonce,
						  const BYTE* data, size_t len) {
	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	if (!ctx) throw pcsc::CipherError("EVP_CIPHER_CTX_new failed");

	BYTEV ivCopy = nonce;
	EVP_EncryptInit_ex(ctx, pickAesCtr(key.size()), nullptr, key.data(), ivCopy.data());

	BYTEV out(len + 16);
	int outLen = 0, finalLen = 0;
	EVP_EncryptUpdate(ctx, out.data(), &outLen, data, static_cast<int>(len));
	EVP_EncryptFinal_ex(ctx, out.data() + outLen, &finalLen);
	EVP_CIPHER_CTX_free(ctx);

	out.resize(static_cast<size_t>(outLen + finalLen));
	return out;
}

BYTEV AesCtrCipher::encrypt(const BYTE* data, size_t len) const {
	return ctrTransform(pImpl->key, pImpl->nonce, data, len);
}

BYTEV AesCtrCipher::decrypt(const BYTE* data, size_t len) const {
	return ctrTransform(pImpl->key, pImpl->nonce, data, len);
}

bool AesCtrCipher::test() const {
	const char sample[] = "AesCtrTest12345";
	auto enc = encrypt(reinterpret_cast<const BYTE*>(sample), sizeof(sample) - 1);
	auto dec = decrypt(enc.data(), enc.size());
	if (dec.size() != sizeof(sample) - 1) return false;
	return std::equal(dec.begin(), dec.end(), reinterpret_cast<const BYTE*>(sample));
}
