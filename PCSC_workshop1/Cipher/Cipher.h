#ifndef PCSC_WORKSHOP1_CIPHER_H
#define PCSC_WORKSHOP1_CIPHER_H

#include "../utils/PcscUtils.h"
#include <memory>

// Forward declare ICipherAAD so we can reference it in implementation file
class ICipherAAD;

// ============================================================
// ICipher — þifreleme stratejisi arayüzü
// ============================================================
class ICipher {
public:
    virtual ~ICipher() = default;

    // Veriyi þifrele — out buffer'ý en az len byte olmalý
    virtual BYTEV encrypt(const BYTE* data, size_t len) const = 0;

    // Þifreli veriyi çöz — out buffer'ý en az len byte olmalý
    virtual BYTEV decrypt(const BYTE* data, size_t len) const = 0;

    // Convenience overloads
    BYTEV encrypt(const BYTEV& data) const { return encrypt(data.data(), data.size()); }
    BYTEV decrypt(const BYTEV& data) const { return decrypt(data.data(), data.size()); }

    // AAD-aware overloads: declared here, default implementation in CipherAadFallback.cpp
    virtual BYTEV encrypt(const BYTE* data, size_t len, const BYTE* aad, size_t aad_len) const;
    virtual BYTEV decrypt(const BYTE* data, size_t len, const BYTE* aad, size_t aad_len) const;

    // Convenience AAD overloads for vector
    BYTEV encrypt(const BYTEV& data, const BYTEV& aad) const { return encrypt(data.data(), data.size(), aad.data(), aad.size()); }
    BYTEV decrypt(const BYTEV& data, const BYTEV& aad) const { return decrypt(data.data(), data.size(), aad.data(), aad.size()); }

    // Her cipher kendi testini implemente eder
    virtual bool test() const = 0;
};

#endif // PCSC_WORKSHOP1_CIPHER_H
