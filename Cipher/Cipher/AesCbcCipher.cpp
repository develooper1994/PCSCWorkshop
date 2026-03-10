#include "AesCbcCipher.h"
#include "CipherUtil.h"
#include "Exceptions/GenericExceptions.h"
#include <openssl/evp.h>
#include <algorithm>
#include <cstring>

static inline const EVP_CIPHER* pickAes(size_t keyLen) {
	switch (keyLen) {
		case 16: return EVP_aes_128_cbc();
		case 24: return EVP_aes_192_cbc();
		case 32: return EVP_aes_256_cbc();
	default: return nullptr;
	}
}

struct AesCbcCipher::Impl {
	BYTEV key;
	const EVP_CIPHER* cipher;
	static const size_t AES_BLOCK_SIZE = 16;
	BYTE iv[AES_BLOCK_SIZE];

	explicit Impl(const BYTEV& k, const BYTEV& i) : key(k), cipher(pickAes(k.size())) {
		std::memcpy(iv, i.data(), AES_BLOCK_SIZE);
	}

	size_t transform(const BYTE* data, size_t len, BYTE* out, size_t outCapacity, bool encrypt) const {
		EVP_CIPHER_CTX* ctx = acquireThreadLocalCtxChecked();

		BYTE ivBuf[AES_BLOCK_SIZE];
		std::memcpy(ivBuf, iv, AES_BLOCK_SIZE);

		int outLen = 0, finalLen = 0;
		if (encrypt) {
			EVP_EncryptInit_ex(ctx, cipher, nullptr, key.data(), ivBuf);
			EVP_EncryptUpdate(ctx, out, &outLen, data, static_cast<int>(len));
			int rc = EVP_EncryptFinal_ex(ctx, out + outLen, &finalLen);
			if (rc != 1) throw pcsc::CipherError("AES-CBC encrypt failed (bad padding or key)");
		} else {
			EVP_DecryptInit_ex(ctx, cipher, nullptr, key.data(), ivBuf);
			EVP_DecryptUpdate(ctx, out, &outLen, data, static_cast<int>(len));
			int rc = EVP_DecryptFinal_ex(ctx, out + outLen, &finalLen);
			if (rc != 1) throw pcsc::CipherError("AES-CBC decrypt failed (bad padding or key)");
		}

		return static_cast<size_t>(outLen + finalLen);
	}
};

AesCbcCipher::AesCbcCipher(const BYTEV& key, const BYTEV& iv)
	: pImpl(nullptr) {
	if (iv.size() != AesCbcCipher::Impl::AES_BLOCK_SIZE) throw pcsc::CipherError("AES IV must be 16 bytes");
	else if (key.size() != 16 && key.size() != 24 && key.size() != 32) throw pcsc::CipherError("AES key must be 16, 24, or 32 bytes");
	else pImpl = std::make_unique<Impl>(key, iv);
}

AesCbcCipher::~AesCbcCipher() = default;
AesCbcCipher::AesCbcCipher(AesCbcCipher&&) noexcept = default;
AesCbcCipher& AesCbcCipher::operator=(AesCbcCipher&&) noexcept = default;

BYTEV AesCbcCipher::encrypt(const BYTE* data, size_t len) const {
	BYTEV out(len + AesCbcCipher::Impl::AES_BLOCK_SIZE);
	size_t written = pImpl->transform(data, len, out.data(), out.size(), true);
	out.resize(written);
	return out;
}

BYTEV AesCbcCipher::decrypt(const BYTE* data, size_t len) const {
	BYTEV out(len);
	size_t written = pImpl->transform(data, len, out.data(), out.size(), false);
	out.resize(written);
	return out;
}

size_t AesCbcCipher::encryptIntoSized(const BYTE* data, size_t len,
                                       BYTE* out, size_t outCapacity) const {
	return pImpl->transform(data, len, out, outCapacity, true);
}

size_t AesCbcCipher::decryptIntoSized(const BYTE* data, size_t len,
                                       BYTE* out, size_t outCapacity) const {
	return pImpl->transform(data, len, out, outCapacity, false);
}

bool AesCbcCipher::test() const {
	const char sample[] = "TestData1234567";
	auto enc = encrypt(reinterpret_cast<const BYTE*>(sample), sizeof(sample) - 1);
	auto dec = decrypt(enc.data(), enc.size());
	if (dec.size() != sizeof(sample) - 1) return false;
	return std::equal(dec.begin(), dec.end(), reinterpret_cast<const BYTE*>(sample));
}
