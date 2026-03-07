#ifndef PCSC_CRYPTO_TYPES_H
#define PCSC_CRYPTO_TYPES_H

#include "Types.h"
#include <cstddef>

enum class CipherType {
	Xor,
	Caesar,
	Affine,

	AES_128_CBC,
	AES_192_CBC,
	AES_256_CBC,
	AES_128_CTR,
	AES_128_GCM,
	AES_256_GCM,
	TripleDES_CBC,

	AES_128_CBC_RAW,
	DES2K_CBC_RAW,
	DES3K_CBC_RAW,
};

#endif // PCSC_CRYPTO_TYPES_H
