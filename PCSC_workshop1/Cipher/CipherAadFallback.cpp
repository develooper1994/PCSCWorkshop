#include "Cipher.h"
#include "ICipherAAD.h"

BYTEV ICipher::encrypt(const BYTE* data, size_t len, const BYTE* aad, size_t aad_len) const {
    const ICipherAAD* aadImpl = dynamic_cast<const ICipherAAD*>(this);
    if (aadImpl) return aadImpl->encrypt(data, len, aad, aad_len);
    (void)aad; (void)aad_len;
    return encrypt(data, len);
}

BYTEV ICipher::decrypt(const BYTE* data, size_t len, const BYTE* aad, size_t aad_len) const {
    const ICipherAAD* aadImpl = dynamic_cast<const ICipherAAD*>(this);
    if (aadImpl) return aadImpl->decrypt(data, len, aad, aad_len);
    (void)aad; (void)aad_len;
    return decrypt(data, len);
}
