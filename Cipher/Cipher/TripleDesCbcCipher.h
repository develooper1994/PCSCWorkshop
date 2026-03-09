#ifndef PCSC_CRYPTO_TRIPLEDESCBCCIPHER_H
#define PCSC_CRYPTO_TRIPLEDESCBCCIPHER_H

#include "Cipher.h"
#include <memory>

class TripleDesCbcCipher : public ICipher {
public:
	TripleDesCbcCipher(const BYTEV& key, const BYTEV& iv);
	~TripleDesCbcCipher() override;

	TripleDesCbcCipher(const TripleDesCbcCipher&) = delete;
	TripleDesCbcCipher& operator=(const TripleDesCbcCipher&) = delete;
	TripleDesCbcCipher(TripleDesCbcCipher&&) noexcept;
	TripleDesCbcCipher& operator=(TripleDesCbcCipher&&) noexcept;

	BYTEV encrypt(const BYTE* data, size_t len) const override;
	BYTEV decrypt(const BYTE* data, size_t len) const override;
	size_t encryptIntoSized(const BYTE* data, size_t len, BYTE* out, size_t outCapacity) const override;
	size_t decryptIntoSized(const BYTE* data, size_t len, BYTE* out, size_t outCapacity) const override;
	bool test() const override;

private:
	struct Impl;
	std::unique_ptr<Impl> pImpl;
};

#endif // PCSC_CRYPTO_TRIPLEDESCBCCIPHER_H
