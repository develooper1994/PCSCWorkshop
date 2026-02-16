#pragma once
#include "../Cipher.h"
#include <array>

class XorCipher : public ICipher {
public:
    using Key4 = std::array<BYTE,4>;
    explicit XorCipher(const Key4& key);
    ~XorCipher() override;

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
