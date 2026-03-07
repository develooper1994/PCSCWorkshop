#include "TripleDesCbcCipher.h"
#include "Exceptions/GenericExceptions.h"
#include <openssl/evp.h>
#include <algorithm>

struct TripleDesCbcCipher::Impl {
	BYTEV key;
	BYTEV iv;
	explicit Impl(const BYTEV& k, const BYTEV& i) : key(k), iv(i) {}
};

TripleDesCbcCipher::TripleDesCbcCipher(const BYTEV& key, const BYTEV& iv)
	: pImpl(std::make_unique<Impl>(key, iv)) {
	if (iv.size() != 8) throw pcsc::CipherError("3DES IV must be 8 bytes");
	if (key.size() != 24) throw pcsc::CipherError("3DES key must be 24 bytes");
}

TripleDesCbcCipher::~TripleDesCbcCipher() = default;
TripleDesCbcCipher::TripleDesCbcCipher(TripleDesCbcCipher&&) noexcept = default;
TripleDesCbcCipher& TripleDesCbcCipher::operator=(TripleDesCbcCipher&&) noexcept = default;

BYTEV TripleDesCbcCipher::encrypt(const BYTE* data, size_t len) const {
	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	if (!ctx) throw pcsc::CipherError("EVP_CIPHER_CTX_new failed");

	BYTEV ivCopy = pImpl->iv;
	EVP_EncryptInit_ex(ctx, EVP_des_ede3_cbc(), nullptr,
	                   pImpl->key.data(), ivCopy.data());

	BYTEV out(len + 16);
	int outLen = 0, finalLen = 0;
	EVP_EncryptUpdate(ctx, out.data(), &outLen, data, static_cast<int>(len));
	EVP_EncryptFinal_ex(ctx, out.data() + outLen, &finalLen);
	EVP_CIPHER_CTX_free(ctx);

	out.resize(static_cast<size_t>(outLen + finalLen));
	return out;
}

BYTEV TripleDesCbcCipher::decrypt(const BYTE* data, size_t len) const {
	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	if (!ctx) throw pcsc::CipherError("EVP_CIPHER_CTX_new failed");

	BYTEV ivCopy = pImpl->iv;
	EVP_DecryptInit_ex(ctx, EVP_des_ede3_cbc(), nullptr,
	                   pImpl->key.data(), ivCopy.data());

	BYTEV out(len + 8);
	int outLen = 0, finalLen = 0;
	EVP_DecryptUpdate(ctx, out.data(), &outLen, data, static_cast<int>(len));
	int rc = EVP_DecryptFinal_ex(ctx, out.data() + outLen, &finalLen);
	EVP_CIPHER_CTX_free(ctx);
	if (rc != 1) throw pcsc::CipherError("3DES decrypt failed");

	out.resize(static_cast<size_t>(outLen + finalLen));
	return out;
}

bool TripleDesCbcCipher::test() const {
	const char sample[] = "TestData1234";
	auto enc = encrypt(reinterpret_cast<const BYTE*>(sample), sizeof(sample) - 1);
	auto dec = decrypt(enc.data(), enc.size());
	if (dec.size() != sizeof(sample) - 1) return false;
	return std::equal(dec.begin(), dec.end(), reinterpret_cast<const BYTE*>(sample));
}
