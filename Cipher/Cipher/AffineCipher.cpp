#include "pch.h"
#include "AffineCipher.h"
#include "CipherUtil.h"
#include <numeric>
#include <algorithm>
#include <array>

struct AffineCipher::Impl { BYTE a; BYTE b; BYTE a_inv; std::array<uint8_t,256> encTable; std::array<uint8_t,256> decTable; explicit Impl(BYTE a_, BYTE b_): a(a_), b(b_), a_inv(0) {}};

BYTE AffineCipher::invMod256(BYTE a) {
    // wrapper to existing util
    return static_cast<BYTE>(cipherutil::invMod256(static_cast<unsigned>(a)));
}

AffineCipher::AffineCipher(BYTE a, BYTE b): pImpl(std::make_unique<Impl>(a,b)) {
    // ensure a is invertible mod 256: if not, force it to an odd value
    if (invMod256(pImpl->a) == 0) pImpl->a |= 1;
    pImpl->a_inv = invMod256(pImpl->a);
    // precompute tables
    unsigned ua = static_cast<unsigned>(pImpl->a);
    unsigned ub = static_cast<unsigned>(pImpl->b);
    unsigned a_inv_u = static_cast<unsigned>(pImpl->a_inv);
    for (unsigned x = 0; x < 256; ++x) {
        pImpl->encTable[x] = static_cast<uint8_t>(cipherutil::mod256(ua * x + ub));
        pImpl->decTable[x] = static_cast<uint8_t>(cipherutil::mod256(a_inv_u * cipherutil::mod256(x - ub)));
    }
}

AffineCipher::AffineCipher(const std::vector<BYTE>& key)
    : pImpl(std::make_unique<Impl>(aFromKey(key), bFromKey(key))) {
    if (invMod256(pImpl->a) == 0) pImpl->a |= 1;
    pImpl->a_inv = invMod256(pImpl->a);
    unsigned ua = static_cast<unsigned>(pImpl->a);
    unsigned ub = static_cast<unsigned>(pImpl->b);
    unsigned a_inv_u = static_cast<unsigned>(pImpl->a_inv);
    for (unsigned x = 0; x < 256; ++x) {
        pImpl->encTable[x] = static_cast<uint8_t>(cipherutil::mod256(ua * x + ub));
        pImpl->decTable[x] = static_cast<uint8_t>(cipherutil::mod256(a_inv_u * cipherutil::mod256(x - ub)));
    }
}

AffineCipher::~AffineCipher() = default;
AffineCipher::AffineCipher(AffineCipher&&) noexcept = default;
AffineCipher& AffineCipher::operator=(AffineCipher&&) noexcept = default;

BYTEV AffineCipher::encrypt(const BYTE* data, size_t len) const {
    BYTEV out(len);
    encryptInto(data, len, out.data());
    return out;
}

BYTEV AffineCipher::decrypt(const BYTE* data, size_t len) const {
    BYTEV out(len);
    decryptInto(data, len, out.data());
    return out;
}

void AffineCipher::encryptInto(const BYTE* data, size_t len, BYTE* out) const {
    if (!out || !data) return;
    const auto &tbl = pImpl->encTable;
    for (size_t i = 0; i < len; ++i) out[i] = tbl[static_cast<uint8_t>(data[i])];
}

void AffineCipher::decryptInto(const BYTE* data, size_t len, BYTE* out) const {
    if (!out || !data) return;
    const auto &tbl = pImpl->decTable;
    for (size_t i = 0; i < len; ++i) out[i] = tbl[static_cast<uint8_t>(data[i])];
}

BYTE AffineCipher::a() const { return pImpl->a; }
BYTE AffineCipher::b() const { return pImpl->b; }

BYTE AffineCipher::aFromKey(const std::vector<BYTE>& key) {
    if (key.empty()) return 1; // default multiplier 1
    unsigned sum = std::accumulate(key.begin(), key.end(), 0u);
    BYTE a = static_cast<BYTE>(sum & 0xFF);
    // Make sure a is odd (invertible mod 256)
    if ((a & 1) == 0) a |= 1;
    return a;
}

BYTE AffineCipher::bFromKey(const std::vector<BYTE>& key) {
    if (key.empty()) return 0;
    unsigned sum = std::accumulate(key.begin(), key.end(), 0u);
    return static_cast<BYTE>((sum >> 8) & 0xFF); // use high byte as offset
}

bool AffineCipher::test() const {
    BYTE sample[8] = { 'T','e','s','t','D','a','t','a' };
    auto enc = encrypt(sample, 8);
    auto dec = decrypt(enc.data(), enc.size());
    if (dec.size() != 8) return false;
    for (size_t i = 0; i < 8; ++i) if (dec[i] != sample[i]) return false;
    return true;
}
