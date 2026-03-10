#include "TripleDesCbcCipher.h"
#include "CipherUtil.h"
#include "Exceptions/GenericExceptions.h"
#include <openssl/evp.h>
#include <algorithm>
#include <cstring>

struct TripleDesCbcCipher::Impl {
	BYTEV key;
	const EVP_CIPHER* cipher;
	static const size_t DES_BLOCK_SIZE = 8;
	BYTE iv[DES_BLOCK_SIZE];

	explicit Impl(const BYTEV& k, const BYTEV& i)
		: key(k), cipher(EVP_des_ede3_cbc()) {
		std::memcpy(iv, i.data(), DES_BLOCK_SIZE);
	}

	size_t transform(const BYTE* data, size_t len, BYTE* out, size_t outCapacity, bool encrypt) const {
		EVP_CIPHER_CTX* ctx = acquireThreadLocalCtxChecked();

		BYTE ivBuf[DES_BLOCK_SIZE];
		std::memcpy(ivBuf, iv, DES_BLOCK_SIZE);

		int outLen = 0, finalLen = 0;
		if (encrypt) {
			EVP_EncryptInit_ex(ctx, cipher, nullptr, key.data(), ivBuf);
			EVP_EncryptUpdate(ctx, out, &outLen, data, static_cast<int>(len));
			int rc = EVP_EncryptFinal_ex(ctx, out + outLen, &finalLen);
			if (rc != 1) throw pcsc::CipherError("3DES encrypt failed");
		} else {
			EVP_DecryptInit_ex(ctx, cipher, nullptr, key.data(), ivBuf);
			EVP_DecryptUpdate(ctx, out, &outLen, data, static_cast<int>(len));
			int rc = EVP_DecryptFinal_ex(ctx, out + outLen, &finalLen);
			if (rc != 1) throw pcsc::CipherError("3DES decrypt failed");
		}

		return static_cast<size_t>(outLen + finalLen);
	}
};

TripleDesCbcCipher::TripleDesCbcCipher(const BYTEV& key, const BYTEV& iv)
	: pImpl(nullptr) {
	if (iv.size() != TripleDesCbcCipher::Impl::DES_BLOCK_SIZE) throw pcsc::CipherError("3DES IV must be 8 bytes");
	else if (key.size() != 24) throw pcsc::CipherError("3DES key must be 24 bytes");
	else pImpl = std::make_unique<Impl>(key, iv);
}

TripleDesCbcCipher::~TripleDesCbcCipher() = default;
TripleDesCbcCipher::TripleDesCbcCipher(TripleDesCbcCipher&&) noexcept = default;
TripleDesCbcCipher& TripleDesCbcCipher::operator=(TripleDesCbcCipher&&) noexcept = default;

BYTEV TripleDesCbcCipher::encrypt(const BYTE* data, size_t len) const {
	BYTEV out(len + TripleDesCbcCipher::Impl::DES_BLOCK_SIZE);
	size_t written = pImpl->transform(data, len, out.data(), out.size(), true);
	out.resize(written);
	return out;
}

BYTEV TripleDesCbcCipher::decrypt(const BYTE* data, size_t len) const {
	BYTEV out(len);
	size_t written = pImpl->transform(data, len, out.data(), out.size(), false);
	out.resize(written);
	return out;
}

size_t TripleDesCbcCipher::encryptIntoSized(const BYTE* data, size_t len,
                                             BYTE* out, size_t outCapacity) const {
	return pImpl->transform(data, len, out, outCapacity, true);
}

size_t TripleDesCbcCipher::decryptIntoSized(const BYTE* data, size_t len,
                                             BYTE* out, size_t outCapacity) const {
	return pImpl->transform(data, len, out, outCapacity, false);
}

bool TripleDesCbcCipher::test() const {
	const char sample[] = "TestData1234";
	auto enc = encrypt(reinterpret_cast<const BYTE*>(sample), sizeof(sample) - 1);
	auto dec = decrypt(enc.data(), enc.size());
	if (dec.size() != sizeof(sample) - 1) return false;
	return std::equal(dec.begin(), dec.end(), reinterpret_cast<const BYTE*>(sample));
}
