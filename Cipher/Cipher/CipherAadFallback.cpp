#include "pch.h"
#include "Cipher.h"
#include "ICipherAAD.h"
#include "Exceptions/GenericExceptions.h"

// Default AAD-aware fallbacks: if the implementation supports ICipherAAD, forward to it;
// otherwise, ignore AAD and call the non-AAD overloads.

BYTEV ICipher::encrypt(const BYTE* data, size_t len, const BYTE* aad, size_t aad_len) const {
    if (!aad || aad_len == 0) return encrypt(data, len);
    const ICipherAAD* aadImpl = dynamic_cast<const ICipherAAD*>(this);
    if (aadImpl) return aadImpl->encrypt(data, len, aad, aad_len);
    // Fallback: AAD not supported, ignore AAD and encrypt normally
    return encrypt(data, len);
}

BYTEV ICipher::decrypt(const BYTE* data, size_t len, const BYTE* aad, size_t aad_len) const {
    if (!aad || aad_len == 0) return decrypt(data, len);
    const ICipherAAD* aadImpl = dynamic_cast<const ICipherAAD*>(this);
    if (aadImpl) return aadImpl->decrypt(data, len, aad, aad_len);
    // Fallback: AAD not supported, ignore AAD and decrypt normally
    return decrypt(data, len);
}
