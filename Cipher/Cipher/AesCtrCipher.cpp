#include "AesCtrCipher.h"
#include "Exceptions/GenericExceptions.h"
#include "CipherUtil.h"
#include <openssl/evp.h>
#include <algorithm>
#include <cstring>

static inline const EVP_CIPHER* pickAesCtr(size_t keyLen) {
	switch (keyLen) {
		case 16: return EVP_aes_128_ctr();
		case 24: return EVP_aes_192_ctr();
		case 32: return EVP_aes_256_ctr();
		default: return nullptr;
	}
}

struct AesCtrCipher::Impl {
	BYTEV key;
	const EVP_CIPHER* cipher;
	static const size_t AES_IV_SIZE = 16;
	BYTE nonce[AES_IV_SIZE];

	explicit Impl(const BYTEV& k, const BYTEV& n) : key(k), cipher(pickAesCtr(k.size())) {
		std::memcpy(nonce, n.data(), AES_IV_SIZE);
	}

	size_t transform(const BYTE* data, size_t len, BYTE* out, size_t outCapacity, bool encrypt) const {
		EVP_CIPHER_CTX* ctx = acquireThreadLocalCtxChecked();

		BYTE ivBuf[AES_IV_SIZE];
		std::memcpy(ivBuf, nonce, AES_IV_SIZE);

		int outLen = 0, finalLen = 0;
		if (encrypt) {
			EVP_EncryptInit_ex(ctx, cipher, nullptr, key.data(), ivBuf);
			EVP_EncryptUpdate(ctx, out, &outLen, data, static_cast<int>(len));
			int rc = EVP_EncryptFinal_ex(ctx, out + outLen, &finalLen);
			if (rc != 1) throw pcsc::CipherError("AES-CTR encrypt failed (bad padding or key)");
		} else {
			EVP_DecryptInit_ex(ctx, cipher, nullptr, key.data(), ivBuf);
			EVP_DecryptUpdate(ctx, out, &outLen, data, static_cast<int>(len));
			int rc = EVP_DecryptFinal_ex(ctx, out + outLen, &finalLen);
			if (rc != 1) throw pcsc::CipherError("AES-CTR decrypt failed (bad padding or key)");
		}

		return static_cast<size_t>(outLen + finalLen);
	}
};

AesCtrCipher::AesCtrCipher(const BYTEV& key, const BYTEV& nonce)
	: pImpl(nullptr) {
	if (nonce.size() != AesCtrCipher::Impl::AES_IV_SIZE) throw pcsc::CipherError("AES-CTR IV/nonce must be 16 bytes");
	else if (key.size() != 16 && key.size() != 24 && key.size() != 32) throw pcsc::CipherError("AES key must be 16, 24, or 32 bytes");
	else pImpl = std::make_unique<Impl>(key, nonce);
}

AesCtrCipher::~AesCtrCipher() = default;
AesCtrCipher::AesCtrCipher(AesCtrCipher&&) noexcept = default;
AesCtrCipher& AesCtrCipher::operator=(AesCtrCipher&&) noexcept = default;

BYTEV AesCtrCipher::encrypt(const BYTE* data, size_t len) const {
	BYTEV out(len);
	size_t written = pImpl->transform(data, len, out.data(), out.size(), true);
	out.resize(written);
	return out;
}

BYTEV AesCtrCipher::decrypt(const BYTE* data, size_t len) const {
	BYTEV out(len);
	size_t written = pImpl->transform(data, len, out.data(), out.size(), false);
	out.resize(written);
	return out;
}

size_t AesCtrCipher::encryptIntoSized(const BYTE* data, size_t len,
                                       BYTE* out, size_t outCapacity) const {
	return pImpl->transform(data, len, out, outCapacity, true);
}

size_t AesCtrCipher::decryptIntoSized(const BYTE* data, size_t len,
                                       BYTE* out, size_t outCapacity) const {
	return pImpl->transform(data, len, out, outCapacity, false);
}

bool AesCtrCipher::test() const {
	const char sample[] = "AesCtrTest12345";
	auto enc = encrypt(reinterpret_cast<const BYTE*>(sample), sizeof(sample) - 1);
	auto dec = decrypt(enc.data(), enc.size());
	if (dec.size() != sizeof(sample) - 1) return false;
	return std::equal(dec.begin(), dec.end(), reinterpret_cast<const BYTE*>(sample));
}
