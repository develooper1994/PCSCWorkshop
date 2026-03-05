#include "CaesarCipher.h"
#include "CipherUtil.h"
#include <numeric>
#include <stdexcept>
#include <array>
#include <algorithm>

struct CaesarCipher::Impl {
    unsigned a;      // multiplier  (stored as unsigned to avoid implicit BYTE arithmetic)
    unsigned b;      // shift/offset
    unsigned a_inv;  // precomputed inverse of a mod 256 (0 if not invertible)
    std::array<uint8_t, 256> encTable;
    std::array<uint8_t, 256> decTable;
    explicit Impl(unsigned a_, unsigned b_, unsigned a_inv_)
        : a(a_ & 0xFFu), b(b_ & 0xFFu), a_inv(a_inv_ & 0xFFu) {
        // precompute tables
        for (unsigned x = 0; x < 256; ++x) {
            encTable[x] = static_cast<uint8_t>(cipherutil::mod256(a * x + b));
            decTable[x] = static_cast<uint8_t>(cipherutil::mod256(a_inv * cipherutil::mod256(x - b)));
        }
    }
};

// ?? constructors ??????????????????????????????????????
CaesarCipher::CaesarCipher(BYTE shift)
    : pImpl(std::make_unique<Impl>(1u, static_cast<unsigned>(shift), 1u)) {}

CaesarCipher::CaesarCipher(BYTE a, BYTE shift)
    : pImpl(nullptr)
{
    unsigned ua = static_cast<unsigned>(a) & 0xFFu;
    unsigned ub = static_cast<unsigned>(shift) & 0xFFu;
    unsigned inv = cipherutil::invMod256(ua);
    if (inv == 0 && ua != 0) {
        // a is not invertible mod 256 ? force it odd so gcd(a,256)==1
        ua |= 1u;
        inv = cipherutil::invMod256(ua);
    }
    if (ua == 0) { ua = 1u; inv = 1u; }   // a=0 is degenerate
    pImpl = std::make_unique<Impl>(ua, ub, inv);
}

CaesarCipher::CaesarCipher(const std::vector<BYTE>& key)
    : pImpl(std::make_unique<Impl>(1u, static_cast<unsigned>(shiftFromKey(key)), 1u)) {}

CaesarCipher::~CaesarCipher() = default;
CaesarCipher::CaesarCipher(CaesarCipher&&) noexcept = default;
CaesarCipher& CaesarCipher::operator=(CaesarCipher&&) noexcept = default;

// ?? encrypt: E(x) = (a * x + b) mod 256 ??????????????
BYTEV CaesarCipher::encrypt(const BYTE* data, size_t len) const {
    BYTEV out(len);
    encryptInto(data, len, out.data());
    return out;
}

// ?? decrypt: D(y) = a_inv * (y - b) mod 256 ??????????
BYTEV CaesarCipher::decrypt(const BYTE* data, size_t len) const {
    BYTEV out(len);
    decryptInto(data, len, out.data());
    return out;
}

// optimized output-into overrides
void CaesarCipher::encryptInto(const BYTE* data, size_t len, BYTE* out) const {
    if (!out || !data) return;
    const auto &tbl = pImpl->encTable;
    for (size_t i = 0; i < len; ++i) out[i] = tbl[static_cast<uint8_t>(data[i])];
}

void CaesarCipher::decryptInto(const BYTE* data, size_t len, BYTE* out) const {
    if (!out || !data) return;
    const auto &tbl = pImpl->decTable;
    for (size_t i = 0; i < len; ++i) out[i] = tbl[static_cast<uint8_t>(data[i])];
}

BYTE CaesarCipher::shift() const { return static_cast<BYTE>(pImpl->b); }
BYTE CaesarCipher::multiplier() const { return static_cast<BYTE>(pImpl->a); }

BYTE CaesarCipher::shiftFromKey(const std::vector<BYTE>& key) {
    if (key.empty()) return 1;
    // Tüm byte'larýn toplamý mod 256; sýfýrsa 1 yap (shift=0 þifreleme yapmaz)
    unsigned s = std::accumulate(key.begin(), key.end(), 0u) & 0xFFu;
    return static_cast<BYTE>(s == 0 ? 1u : s);
}

bool CaesarCipher::test() const {
    BYTE sample[8] = { 'T','e','s','t','D','a','t','a' };
    auto enc = encrypt(sample, 8);
    auto dec = decrypt(enc.data(), enc.size());
    if (dec.size() != 8) return false;
    for (size_t i = 0; i < 8; ++i) if (dec[i] != sample[i]) return false;
    return true;
}
