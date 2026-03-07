#ifndef PCSC_CRYPTO_AESGCMCIPHER_H
#define PCSC_CRYPTO_AESGCMCIPHER_H

#include "Cipher.h"
#include "ICipherAAD.h"
#include <memory>

class AesGcmCipher : public ICipherAAD {
public:
	AesGcmCipher(const BYTEV& key);
	~AesGcmCipher() override;

	AesGcmCipher(const AesGcmCipher&) = delete;
	AesGcmCipher& operator=(const AesGcmCipher&) = delete;
	AesGcmCipher(AesGcmCipher&&) noexcept;
	AesGcmCipher& operator=(AesGcmCipher&&) noexcept;

	struct Impl;

	BYTEV encrypt(const BYTE* data, size_t len) const override;
	BYTEV decrypt(const BYTE* data, size_t len) const override;
	BYTEV encrypt(const BYTE* data, size_t len, const BYTE* aad, size_t aadLen) const override;
	BYTEV decrypt(const BYTE* data, size_t len, const BYTE* aad, size_t aadLen) const override;
	bool test() const override;

private:
	std::unique_ptr<Impl> pImpl;
};

#endif // PCSC_CRYPTO_AESGCMCIPHER_H
