#include "CaesarCipher.h"

struct CaesarCipher::Impl { BYTE shift; explicit Impl(BYTE s): shift(s) {} };

CaesarCipher::CaesarCipher(BYTE shift): pImpl(std::make_unique<Impl>(shift)) {}
CaesarCipher::~CaesarCipher() = default;
CaesarCipher::CaesarCipher(CaesarCipher&&) noexcept = default;
CaesarCipher& CaesarCipher::operator=(CaesarCipher&&) noexcept = default;

BYTEV CaesarCipher::encrypt(const BYTE* data, size_t len) const {
    BYTEV out(len);
    for (size_t i = 0; i < len; ++i) out[i] = static_cast<BYTE>(data[i] + pImpl->shift);
    return out;
}

BYTEV CaesarCipher::decrypt(const BYTE* data, size_t len) const {
    BYTEV out(len);
    for (size_t i = 0; i < len; ++i) out[i] = static_cast<BYTE>(data[i] - pImpl->shift);
    return out;
}

BYTE CaesarCipher::shift() const { return pImpl->shift; }

bool CaesarCipher::test() const {
    BYTE sample[8] = { 'T','e','s','t','D','a','t','a' };
    auto enc = encrypt(sample, 8);
    auto dec = decrypt(enc.data(), enc.size());
    if (dec.size() != 8) return false;
    for (size_t i = 0; i < 8; ++i) if (dec[i] != sample[i]) return false;
    return true;
}
