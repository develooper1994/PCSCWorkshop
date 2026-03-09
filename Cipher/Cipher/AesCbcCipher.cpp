#include "AesCbcCipher.h"
#include "Exceptions/GenericExceptions.h"
#include <openssl/evp.h>
#include <algorithm>
#include <cstring>

static const size_t AES_BLOCK_SIZE = 16;

static const EVP_CIPHER* pickAes(size_t keyLen) {
	switch (keyLen) {
	case 16: return EVP_aes_128_cbc();
	case 24: return EVP_aes_192_cbc();
	case 32: return EVP_aes_256_cbc();
	default: return nullptr;
	}
}

static EVP_CIPHER_CTX* acquireThreadLocalCtx() {
	thread_local struct CtxHolder {
		EVP_CIPHER_CTX* ctx;
		CtxHolder() : ctx(EVP_CIPHER_CTX_new()) {}
		~CtxHolder() { if (ctx) EVP_CIPHER_CTX_free(ctx); }
		CtxHolder(const CtxHolder&) = delete;
		CtxHolder& operator=(const CtxHolder&) = delete;
	} holder;
	return holder.ctx;
}

struct AesCbcCipher::Impl {
	BYTEV key;
	BYTE iv[AES_BLOCK_SIZE];
	const EVP_CIPHER* cipher;

	explicit Impl(const BYTEV& k, const BYTEV& i)
		: key(k), cipher(pickAes(k.size())) {
		std::memcpy(iv, i.data(), AES_BLOCK_SIZE);
	}
};

AesCbcCipher::AesCbcCipher(const BYTEV& key, const BYTEV& iv)
	: pImpl(nullptr) {
	if (iv.size() != AES_BLOCK_SIZE)
		throw pcsc::CipherError("AES IV must be 16 bytes");
	else if (key.size() != 16 && key.size() != 24 && key.size() != 32)
		throw pcsc::CipherError("AES key must be 16, 24, or 32 bytes");
	else
		pImpl = std::make_unique<Impl>(key, iv);
}

AesCbcCipher::~AesCbcCipher() = default;
AesCbcCipher::AesCbcCipher(AesCbcCipher&&) noexcept = default;
AesCbcCipher& AesCbcCipher::operator=(AesCbcCipher&&) noexcept = default;

BYTEV AesCbcCipher::encrypt(const BYTE* data, size_t len) const {
	BYTEV out(len + AES_BLOCK_SIZE);
	size_t written = encryptIntoSized(data, len, out.data(), out.size());
	out.resize(written);
	return out;
}

BYTEV AesCbcCipher::decrypt(const BYTE* data, size_t len) const {
	BYTEV out(len);
	size_t written = decryptIntoSized(data, len, out.data(), out.size());
	out.resize(written);
	return out;
}

size_t AesCbcCipher::encryptIntoSized(const BYTE* data, size_t len,
                                       BYTE* out, size_t outCapacity) const {
	EVP_CIPHER_CTX* ctx = acquireThreadLocalCtx();
	if (!ctx) throw pcsc::CipherError("EVP_CIPHER_CTX thread-local allocation failed");

	BYTE ivBuf[AES_BLOCK_SIZE];
	std::memcpy(ivBuf, pImpl->iv, AES_BLOCK_SIZE);
	EVP_EncryptInit_ex(ctx, pImpl->cipher, nullptr,
	                   pImpl->key.data(), ivBuf);

	int outLen = 0, finalLen = 0;
	EVP_EncryptUpdate(ctx, out, &outLen, data, static_cast<int>(len));
	EVP_EncryptFinal_ex(ctx, out + outLen, &finalLen);

	return static_cast<size_t>(outLen + finalLen);
}

size_t AesCbcCipher::decryptIntoSized(const BYTE* data, size_t len,
                                       BYTE* out, size_t outCapacity) const {
	EVP_CIPHER_CTX* ctx = acquireThreadLocalCtx();
	if (!ctx) throw pcsc::CipherError("EVP_CIPHER_CTX thread-local allocation failed");

	BYTE ivBuf[AES_BLOCK_SIZE];
	std::memcpy(ivBuf, pImpl->iv, AES_BLOCK_SIZE);
	EVP_DecryptInit_ex(ctx, pImpl->cipher, nullptr,
	                   pImpl->key.data(), ivBuf);

	int outLen = 0, finalLen = 0;
	EVP_DecryptUpdate(ctx, out, &outLen, data, static_cast<int>(len));
	int rc = EVP_DecryptFinal_ex(ctx, out + outLen, &finalLen);
	if (rc != 1) throw pcsc::CipherError("AES-CBC decrypt failed (bad padding or key)");

	return static_cast<size_t>(outLen + finalLen);
}

bool AesCbcCipher::test() const {
	const char sample[] = "TestData1234567";
	auto enc = encrypt(reinterpret_cast<const BYTE*>(sample), sizeof(sample) - 1);
	auto dec = decrypt(enc.data(), enc.size());
	if (dec.size() != sizeof(sample) - 1) return false;
	return std::equal(dec.begin(), dec.end(), reinterpret_cast<const BYTE*>(sample));
}
