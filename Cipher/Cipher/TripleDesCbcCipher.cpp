#include "TripleDesCbcCipher.h"
#include "Exceptions/GenericExceptions.h"
#include <openssl/evp.h>
#include <algorithm>
#include <cstring>

static const size_t DES_BLOCK_SIZE = 8;

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

struct TripleDesCbcCipher::Impl {
	BYTEV key;
	BYTE iv[DES_BLOCK_SIZE];
	const EVP_CIPHER* cipher;

	explicit Impl(const BYTEV& k, const BYTEV& i)
		: key(k), cipher(EVP_des_ede3_cbc()) {
		std::memcpy(iv, i.data(), DES_BLOCK_SIZE);
	}
};

TripleDesCbcCipher::TripleDesCbcCipher(const BYTEV& key, const BYTEV& iv)
	: pImpl(nullptr) {
	if (iv.size() != DES_BLOCK_SIZE)
		throw pcsc::CipherError("3DES IV must be 8 bytes");
	if (key.size() != 24)
		throw pcsc::CipherError("3DES key must be 24 bytes");
	pImpl = std::make_unique<Impl>(key, iv);
}

TripleDesCbcCipher::~TripleDesCbcCipher() = default;
TripleDesCbcCipher::TripleDesCbcCipher(TripleDesCbcCipher&&) noexcept = default;
TripleDesCbcCipher& TripleDesCbcCipher::operator=(TripleDesCbcCipher&&) noexcept = default;

BYTEV TripleDesCbcCipher::encrypt(const BYTE* data, size_t len) const {
	BYTEV out(len + DES_BLOCK_SIZE);
	size_t written = encryptIntoSized(data, len, out.data(), out.size());
	out.resize(written);
	return out;
}

BYTEV TripleDesCbcCipher::decrypt(const BYTE* data, size_t len) const {
	BYTEV out(len);
	size_t written = decryptIntoSized(data, len, out.data(), out.size());
	out.resize(written);
	return out;
}

size_t TripleDesCbcCipher::encryptIntoSized(const BYTE* data, size_t len,
                                             BYTE* out, size_t outCapacity) const {
	EVP_CIPHER_CTX* ctx = acquireThreadLocalCtx();
	if (!ctx) throw pcsc::CipherError("EVP_CIPHER_CTX thread-local allocation failed");

	BYTE ivBuf[DES_BLOCK_SIZE];
	std::memcpy(ivBuf, pImpl->iv, DES_BLOCK_SIZE);
	EVP_EncryptInit_ex(ctx, pImpl->cipher, nullptr,
	                   pImpl->key.data(), ivBuf);

	int outLen = 0, finalLen = 0;
	EVP_EncryptUpdate(ctx, out, &outLen, data, static_cast<int>(len));
	EVP_EncryptFinal_ex(ctx, out + outLen, &finalLen);

	return static_cast<size_t>(outLen + finalLen);
}

size_t TripleDesCbcCipher::decryptIntoSized(const BYTE* data, size_t len,
                                             BYTE* out, size_t outCapacity) const {
	EVP_CIPHER_CTX* ctx = acquireThreadLocalCtx();
	if (!ctx) throw pcsc::CipherError("EVP_CIPHER_CTX thread-local allocation failed");

	BYTE ivBuf[DES_BLOCK_SIZE];
	std::memcpy(ivBuf, pImpl->iv, DES_BLOCK_SIZE);
	EVP_DecryptInit_ex(ctx, pImpl->cipher, nullptr,
	                   pImpl->key.data(), ivBuf);

	int outLen = 0, finalLen = 0;
	EVP_DecryptUpdate(ctx, out, &outLen, data, static_cast<int>(len));
	int rc = EVP_DecryptFinal_ex(ctx, out + outLen, &finalLen);
	if (rc != 1) throw pcsc::CipherError("3DES decrypt failed");

	return static_cast<size_t>(outLen + finalLen);
}

bool TripleDesCbcCipher::test() const {
	const char sample[] = "TestData1234";
	auto enc = encrypt(reinterpret_cast<const BYTE*>(sample), sizeof(sample) - 1);
	auto dec = decrypt(enc.data(), enc.size());
	if (dec.size() != sizeof(sample) - 1) return false;
	return std::equal(dec.begin(), dec.end(), reinterpret_cast<const BYTE*>(sample));
}
