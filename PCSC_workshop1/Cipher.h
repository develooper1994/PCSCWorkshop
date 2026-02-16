#pragma once
#include "PcscUtils.h"
#include <memory>

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

    // Her cipher kendi testini implemente eder
    virtual bool test() const = 0;
};