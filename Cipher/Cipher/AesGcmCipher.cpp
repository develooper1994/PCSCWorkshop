#include "AesGcmCipher.h"
#include "CipherUtil.h"
#include "Random.h"
#include "Exceptions/GenericExceptions.h"
#include <openssl/evp.h>
#include <algorithm>
#include <cstring>

static const EVP_CIPHER* pickAesGcm(size_t keyLen) {
	switch (keyLen) {
		case 16: return EVP_aes_128_gcm();
		case 24: return EVP_aes_192_gcm();
		case 32: return EVP_aes_256_gcm();
		default: return nullptr;
	}
}

struct AesGcmCipher::Impl {
	BYTEV key;
	const EVP_CIPHER* cipher;
	static constexpr size_t NONCE_SIZE = 12;
	static constexpr size_t TAG_SIZE = 16;

	explicit Impl(const BYTEV& k) : key(k), cipher(pickAesGcm(k.size())) {}
};

AesGcmCipher::AesGcmCipher(const BYTEV& key)
	: pImpl(nullptr) {
	if (key.size() != 16 && key.size() != 24 && key.size() != 32) throw pcsc::CipherError("AES-GCM key must be 16, 24, or 32 bytes");
	else pImpl = std::make_unique<Impl>(key);
}

AesGcmCipher::~AesGcmCipher() = default;
AesGcmCipher::AesGcmCipher(AesGcmCipher&&) noexcept = default;
AesGcmCipher& AesGcmCipher::operator=(AesGcmCipher&&) noexcept = default;

static inline BYTEV gcmEncrypt(const AesGcmCipher::Impl* impl,
                        const BYTE* data, size_t len,
                        const BYTE* aad, size_t aadLen) {
	BYTEV nonce = crypto::randomBytes(impl->NONCE_SIZE);

	EVP_CIPHER_CTX* ctx = acquireThreadLocalCtxChecked();

	EVP_EncryptInit_ex(ctx, impl->cipher, nullptr, nullptr, nullptr);
	EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(impl->NONCE_SIZE), nullptr);
	EVP_EncryptInit_ex(ctx, nullptr, nullptr, impl->key.data(), nonce.data());

	int outLen = 0;
	if (aad && aadLen > 0)
		EVP_EncryptUpdate(ctx, nullptr, &outLen, aad, static_cast<int>(aadLen));

	// nonce || ciphertext || tag — tek allocation
	BYTEV ret(impl->NONCE_SIZE + len + impl->TAG_SIZE);
	std::memcpy(ret.data(), nonce.data(), impl->NONCE_SIZE);

	BYTE* ctOut = ret.data() + impl->NONCE_SIZE;
	EVP_EncryptUpdate(ctx, ctOut, &outLen, data, static_cast<int>(len));
	int ctLen = outLen;

	int finalLen = 0;
	EVP_EncryptFinal_ex(ctx, ctOut + ctLen, &finalLen);
	ctLen += finalLen;

	EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG,
	                     static_cast<int>(impl->TAG_SIZE), ctOut + ctLen);

	ret.resize(impl->NONCE_SIZE + static_cast<size_t>(ctLen) + impl->TAG_SIZE);
	return ret;
}

static inline BYTEV gcmDecrypt(const AesGcmCipher::Impl* impl,
                        const BYTE* data, size_t len,
                        const BYTE* aad, size_t aadLen) {
	if (len < impl->NONCE_SIZE + impl->TAG_SIZE)
		throw pcsc::CipherError("Input too short for GCM nonce+tag");

	const BYTE* nonce = data;
	const BYTE* ct = data + impl->NONCE_SIZE;
	size_t ctLen = len - impl->NONCE_SIZE - impl->TAG_SIZE;
	const BYTE* tagPtr = data + impl->NONCE_SIZE + ctLen;

	EVP_CIPHER_CTX* ctx = acquireThreadLocalCtxChecked();

	EVP_DecryptInit_ex(ctx, impl->cipher, nullptr, nullptr, nullptr);
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
