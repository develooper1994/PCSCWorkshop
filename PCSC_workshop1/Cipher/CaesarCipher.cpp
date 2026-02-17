#include "CaesarCipher.h"
#include <numeric>

struct CaesarCipher::Impl { BYTE shift; explicit Impl(BYTE s): shift(s) {} };

// Doðrudan shift ile oluþtur
CaesarCipher::CaesarCipher(BYTE shift): pImpl(std::make_unique<Impl>(shift)) {}

// Anahtar verisinden shift hesapla
CaesarCipher::CaesarCipher(const std::vector<BYTE>& key)
    : pImpl(std::make_unique<Impl>(shiftFromKey(key))) {}

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
BYTE CaesarCipher::shiftFromKey(const std::vector<BYTE>& key) {
    if (key.empty()) return 1;
    // Tüm byte'larýn toplamý mod 256; sýfýrsa 1 yap (shift=0 þifreleme yapmaz)
    BYTE s = static_cast<BYTE>(std::accumulate(key.begin(), key.end(), 0u) & 0xFF);
    return (s == 0) ? 1 : s;
}

bool CaesarCipher::test() const {
    BYTE sample[8] = { 'T','e','s','t','D','a','t','a' };
    auto enc = encrypt(sample, 8);
    auto dec = decrypt(enc.data(), enc.size());
    if (dec.size() != 8) return false;
    for (size_t i = 0; i < 8; ++i) if (dec[i] != sample[i]) return false;
    return true;
}
