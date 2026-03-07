#ifndef PCSC_CRYPTO_KDF_H
#define PCSC_CRYPTO_KDF_H

#include "Types.h"
#include <string>

namespace crypto {

BYTEV pbkdf2Sha256(const std::string& password, const BYTEV& salt,
                   int iterations, size_t keyLen);

BYTEV hkdf(const BYTEV& ikm, const BYTEV& salt, const BYTEV& info,
           size_t outLen);

} // namespace crypto

#endif // PCSC_CRYPTO_KDF_H
