#ifndef PCSC_CRYPTO_AESCBCCIPHER_H
#define PCSC_CRYPTO_AESCBCCIPHER_H

#include "Cipher.h"
#include <memory>

class AesCbcCipher : public ICipher {
public:
	AesCbcCipher(const BYTEV& key, const BYTEV& iv);
	~AesCbcCipher() override;

	AesCbcCipher(const AesCbcCipher&) = delete;
	AesCbcCipher& operator=(const AesCbcCipher&) = delete;
	AesCbcCipher(AesCbcCipher&&) noexcept;
	AesCbcCipher& operator=(AesCbcCipher&&) noexcept;

	BYTEV encrypt(const BYTE* data, size_t len) const override;
	BYTEV decrypt(const BYTE* data, size_t len) const override;
	size_t encryptIntoSized(const BYTE* data, size_t len, BYTE* out, size_t outCapacity) const override;
	size_t decryptIntoSized(const BYTE* data, size_t len, BYTE* out, size_t outCapacity) const override;
	bool test() const override;

private:
	struct Impl;
	std::unique_ptr<Impl> pImpl;
};

#endif // PCSC_CRYPTO_AESCBCCIPHER_H
