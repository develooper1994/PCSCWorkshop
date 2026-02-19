#ifndef PCSC_WORKSHOP1_CIPHER_ICAIPHERAAD_H
#define PCSC_WORKSHOP1_CIPHER_ICAIPHERAAD_H

#include "Cipher.h"

// Backwards-compatible extension: ICipherAAD inherits ICipher and adds AAD-capable methods.
// Existing code using ICipher remains valid. New implementations that support AAD
// can implement this interface to offer AAD-aware encrypt/decrypt.
class ICipherAAD : public ICipher {
public:
    // Encrypt with associated authenticated data (AAD).
    virtual BYTEV encrypt(const BYTE* data, size_t len, const BYTE* aad, size_t aad_len) const = 0;

    // Decrypt with associated authenticated data (AAD).
    virtual BYTEV decrypt(const BYTE* data, size_t len, const BYTE* aad, size_t aad_len) const = 0;

    // Convenience overloads
    BYTEV encrypt(const BYTEV& data, const BYTEV& aad) const { return encrypt(data.data(), data.size(), aad.data(), aad.size()); }
    BYTEV decrypt(const BYTEV& data, const BYTEV& aad) const { return decrypt(data.data(), data.size(), aad.data(), aad.size()); }
};

#endif // PCSC_WORKSHOP1_CIPHER_ICAIPHERAAD_H
