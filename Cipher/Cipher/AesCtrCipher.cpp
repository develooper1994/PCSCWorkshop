#include "AesCtrCipher.h"
#include "Exceptions/GenericExceptions.h"
#include <openssl/evp.h>
#include <algorithm>
#include <cstring>

static const size_t AES_IV_SIZE = 16;

static const EVP_CIPHER* pickAesCtr(size_t keyLen) {
	switch (keyLen) {
	case 16: return EVP_aes_128_ctr();
	case 24: return EVP_aes_192_ctr();
	case 32: return EVP_aes_256_ctr();
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

struct AesCtrCipher::Impl {
	BYTEV key;
	BYTE nonce[AES_IV_SIZE];
	const EVP_CIPHER* cipher;

	explicit Impl(const BYTEV& k, const BYTEV& n)
		: key(k), cipher(pickAesCtr(k.size())) {
		std::memcpy(nonce, n.data(), AES_IV_SIZE);
	}
};

AesCtrCipher::AesCtrCipher(const BYTEV& key, const BYTEV& nonce)
	: pImpl(nullptr) {
	if (nonce.size() != AES_IV_SIZE)
		throw pcsc::CipherError("AES-CTR IV/nonce must be 16 bytes");
	if (key.size() != 16 && key.size() != 24 && key.size() != 32)
		throw pcsc::CipherError("AES key must be 16, 24, or 32 bytes");
	pImpl = std::make_unique<Impl>(key, nonce);
}

AesCtrCipher::~AesCtrCipher() = default;
AesCtrCipher::AesCtrCipher(AesCtrCipher&&) noexcept = default;
AesCtrCipher& AesCtrCipher::operator=(AesCtrCipher&&) noexcept = default;

static size_t ctrTransformInto(const AesCtrCipher::Impl* impl,
                               const BYTE* data, size_t len,
                               BYTE* out, size_t outCapacity) {
	EVP_CIPHER_CTX* ctx = acquireThreadLocalCtx();
	if (!ctx) throw pcsc::CipherError("EVP_CIPHER_CTX thread-local allocation failed");

	BYTE ivBuf[AES_IV_SIZE];
	std::memcpy(ivBuf, impl->nonce, AES_IV_SIZE);
	EVP_EncryptInit_ex(ctx, impl->cipher, nullptr, impl->key.data(), ivBuf);

	int outLen = 0, finalLen = 0;
	EVP_EncryptUpdate(ctx, out, &outLen, data, static_cast<int>(len));
	EVP_EncryptFinal_ex(ctx, out + outLen, &finalLen);

	return static_cast<size_t>(outLen + finalLen);
}

BYTEV AesCtrCipher::encrypt(const BYTE* data, size_t len) const {
	BYTEV out(len);
	size_t written = ctrTransformInto(pImpl.get(), data, len, out.data(), out.size());
	out.resize(written);
	return out;
}

BYTEV AesCtrCipher::decrypt(const BYTE* data, size_t len) const {
	BYTEV out(len);
	size_t written = ctrTransformInto(pImpl.get(), data, len, out.data(), out.size());
	out.resize(written);
	return out;
}

size_t AesCtrCipher::encryptIntoSized(const BYTE* data, size_t len,
                                       BYTE* out, size_t outCapacity) const {
	return ctrTransformInto(pImpl.get(), data, len, out, outCapacity);
}

size_t AesCtrCipher::decryptIntoSized(const BYTE* data, size_t len,
                                       BYTE* out, size_t outCapacity) const {
	return ctrTransformInto(pImpl.get(), data, len, out, outCapacity);
}

bool AesCtrCipher::test() const {
	const char sample[] = "AesCtrTest12345";
	auto enc = encrypt(reinterpret_cast<const BYTE*>(sample), sizeof(sample) - 1);
	auto dec = decrypt(enc.data(), enc.size());
	if (dec.size() != sizeof(sample) - 1) return false;
	return std::equal(dec.begin(), dec.end(), reinterpret_cast<const BYTE*>(sample));
}
