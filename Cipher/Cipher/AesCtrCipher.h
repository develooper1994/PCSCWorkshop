#ifndef PCSC_CRYPTO_AESCTRCIPHER_H
#define PCSC_CRYPTO_AESCTRCIPHER_H

#include "Cipher.h"
#include <memory>

class AesCtrCipher : public ICipher {
public:
	AesCtrCipher(const BYTEV& key, const BYTEV& nonce);
	~AesCtrCipher() override;

	AesCtrCipher(const AesCtrCipher&) = delete;
	AesCtrCipher& operator=(const AesCtrCipher&) = delete;
	AesCtrCipher(AesCtrCipher&&) noexcept;
	AesCtrCipher& operator=(AesCtrCipher&&) noexcept;

	BYTEV encrypt(const BYTE* data, size_t len) const override;
	BYTEV decrypt(const BYTE* data, size_t len) const override;
	size_t encryptIntoSized(const BYTE* data, size_t len, BYTE* out, size_t outCapacity) const override;
	size_t decryptIntoSized(const BYTE* data, size_t len, BYTE* out, size_t outCapacity) const override;
	bool test() const override;

private:
	struct Impl;
	std::unique_ptr<Impl> pImpl;
};

#endif // PCSC_CRYPTO_AESCTRCIPHER_H
