#pragma once
#include "PcscUtils.h"
#include <memory>
#include <array>

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

// ============================================================
// XorCipher — 4-byte XOR þifreleme (NFC page boyutuna uygun)
// ============================================================
class XorCipher : public ICipher {
public:
    using Key4 = std::array<BYTE, 4>;

    explicit XorCipher(const Key4& key);
    ~XorCipher() override;

    // Non-copyable, movable
    XorCipher(const XorCipher&) = delete;
    XorCipher& operator=(const XorCipher&) = delete;
    XorCipher(XorCipher&&) noexcept;
    XorCipher& operator=(XorCipher&&) noexcept;

    BYTEV encrypt(const BYTE* data, size_t len) const override;
    BYTEV decrypt(const BYTE* data, size_t len) const override;

    bool test() const override;

    const Key4& key() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};
