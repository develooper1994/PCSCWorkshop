#include "AesGcmCipher.h"
#include "Random.h"
#include "Exceptions/GenericExceptions.h"
#include <openssl/evp.h>
#include <algorithm>
#include <cstring>

struct AesGcmCipher::Impl {
	BYTEV key;
	static constexpr size_t NONCE_SIZE = 12;
	static constexpr size_t TAG_SIZE = 16;
	explicit Impl(const BYTEV& k) : key(k) {}
};

AesGcmCipher::AesGcmCipher(const BYTEV& key)
	: pImpl(std::make_unique<Impl>(key)) {
	if (key.size() != 16 && key.size() != 24 && key.size() != 32)
		throw pcsc::CipherError("AES-GCM key must be 16, 24, or 32 bytes");
}

AesGcmCipher::~AesGcmCipher() = default;
AesGcmCipher::AesGcmCipher(AesGcmCipher&&) noexcept = default;
AesGcmCipher& AesGcmCipher::operator=(AesGcmCipher&&) noexcept = default;

static const EVP_CIPHER* pickAesGcm(size_t keyLen) {
	switch (keyLen) {
	case 16: return EVP_aes_128_gcm();
	case 24: return EVP_aes_192_gcm();
	case 32: return EVP_aes_256_gcm();
	default: return nullptr;
	}
}

static BYTEV gcmEncrypt(const AesGcmCipher::Impl* impl,
                        const BYTE* data, size_t len,
                        const BYTE* aad, size_t aadLen) {
	BYTEV nonce = crypto::randomBytes(impl->NONCE_SIZE);

	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	if (!ctx) throw pcsc::CipherError("EVP_CIPHER_CTX_new failed");

	EVP_EncryptInit_ex(ctx, pickAesGcm(impl->key.size()), nullptr, nullptr, nullptr);
	EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(impl->NONCE_SIZE), nullptr);
	EVP_EncryptInit_ex(ctx, nullptr, nullptr, impl->key.data(), nonce.data());

	int outLen = 0;
	if (aad && aadLen > 0)
		EVP_EncryptUpdate(ctx, nullptr, &outLen, aad, static_cast<int>(aadLen));

	BYTEV ct(len);
	EVP_EncryptUpdate(ctx, ct.data(), &outLen, data, static_cast<int>(len));
	int ctLen = outLen;

	int finalLen = 0;
	EVP_EncryptFinal_ex(ctx, ct.data() + ctLen, &finalLen);
	ctLen += finalLen;

	BYTEV tag(impl->TAG_SIZE);
	EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(impl->TAG_SIZE), tag.data());
	EVP_CIPHER_CTX_free(ctx);

	// nonce || ciphertext || tag
	BYTEV ret;
	ret.reserve(nonce.size() + static_cast<size_t>(ctLen) + tag.size());
	ret.insert(ret.end(), nonce.begin(), nonce.end());
	ret.insert(ret.end(), ct.begin(), ct.begin() + ctLen);
	ret.insert(ret.end(), tag.begin(), tag.end());
	return ret;
}

static BYTEV gcmDecrypt(const AesGcmCipher::Impl* impl,
                        const BYTE* data, size_t len,
                        const BYTE* aad, size_t aadLen) {
	if (len < impl->NONCE_SIZE + impl->TAG_SIZE)
		throw pcsc::CipherError("Input too short for GCM nonce+tag");

	const BYTE* nonce = data;
	const BYTE* ct = data + impl->NONCE_SIZE;
	size_t ctLen = len - impl->NONCE_SIZE - impl->TAG_SIZE;
	const BYTE* tagPtr = data + impl->NONCE_SIZE + ctLen;

	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	if (!ctx) throw pcsc::CipherError("EVP_CIPHER_CTX_new failed");

	EVP_DecryptInit_ex(ctx, pickAesGcm(impl->key.size()), nullptr, nullptr, nullptr);
	EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(impl->NONCE_SIZE), nullptr);
	EVP_DecryptInit_ex(ctx, nullptr, nullptr, impl->key.data(), nonce);

	int outLen = 0;
	if (aad && aadLen > 0)
		EVP_DecryptUpdate(ctx, nullptr, &outLen, aad, static_cast<int>(aadLen));

	BYTEV pt(ctLen);
	EVP_DecryptUpdate(ctx, pt.data(), &outLen, ct, static_cast<int>(ctLen));
	int ptLen = outLen;

	EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG,
	                     static_cast<int>(impl->TAG_SIZE), const_cast<BYTE*>(tagPtr));

	int rc = EVP_DecryptFinal_ex(ctx, pt.data() + ptLen, &outLen);
	EVP_CIPHER_CTX_free(ctx);

	if (rc <= 0) throw pcsc::CipherError("AES-GCM authentication failed");
	ptLen += outLen;
	pt.resize(static_cast<size_t>(ptLen));
	return pt;
}

BYTEV AesGcmCipher::encrypt(const BYTE* data, size_t len) const { return gcmEncrypt(pImpl.get(), data, len, nullptr, 0); }
BYTEV AesGcmCipher::decrypt(const BYTE* data, size_t len) const { return gcmDecrypt(pImpl.get(), data, len, nullptr, 0); }
BYTEV AesGcmCipher::encrypt(const BYTE* data, size_t len, const BYTE* aad, size_t aadLen) const { return gcmEncrypt(pImpl.get(), data, len, aad, aadLen); }
BYTEV AesGcmCipher::decrypt(const BYTE* data, size_t len, const BYTE* aad, size_t aadLen) const { return gcmDecrypt(pImpl.get(), data, len, aad, aadLen); }

bool AesGcmCipher::test() const {
	const char sample[] = "GcmTestData123";
	const char aad[] = "header-data";
	auto enc = encrypt(reinterpret_cast<const BYTE*>(sample), sizeof(sample) - 1,
	                   reinterpret_cast<const BYTE*>(aad), sizeof(aad) - 1);
	auto dec = decrypt(enc.data(), enc.size(),
	                   reinterpret_cast<const BYTE*>(aad), sizeof(aad) - 1);
	if (dec.size() != sizeof(sample) - 1) return false;
	return std::equal(dec.begin(), dec.end(), reinterpret_cast<const BYTE*>(sample));
}
