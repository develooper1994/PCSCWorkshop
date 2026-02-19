#include "pch.h"
#include "Cipher.h"

// Default fallback implementations for AAD-aware overloads: these call the non-AAD
// virtuals so existing ciphers without AAD support continue to work.

BYTEV ICipher::encrypt(const BYTE* data, size_t len, const BYTE* /*aad*/, size_t /*aad_len*/) const {
    return encrypt(data, len);
}

BYTEV ICipher::decrypt(const BYTE* data, size_t len, const BYTE* /*aad*/, size_t /*aad_len*/) const {
    return decrypt(data, len);
}
