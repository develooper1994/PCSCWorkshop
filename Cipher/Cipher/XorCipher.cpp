#include "pch.h"
#include "XorCipher.h"
#include <algorithm>

struct XorCipher::Impl {
    Key4 key;
    explicit Impl(const Key4& k) : key(k) {}
    BYTEV xorTransform(const BYTE* data, size_t len) const {
        BYTEV out(len);
        for (size_t i = 0; i < len; ++i) out[i] = data[i] ^ key[i % 4];
        return out;
    }
};

XorCipher::XorCipher(const Key4& key) : pImpl(std::make_unique<Impl>(key)) {}
XorCipher::~XorCipher() = default;
XorCipher::XorCipher(XorCipher&&) noexcept = default;
XorCipher& XorCipher::operator=(XorCipher&&) noexcept = default;

BYTEV XorCipher::encrypt(const BYTE* data, size_t len) const { return pImpl->xorTransform(data, len); }
BYTEV XorCipher::decrypt(const BYTE* data, size_t len) const { return pImpl->xorTransform(data, len); }
const XorCipher::Key4& XorCipher::key() const { return pImpl->key; }

bool XorCipher::test() const {
    bool ok = true;
    BYTE sample[8] = { 'T','e','s','t','D','a','t','a' };
    auto enc = encrypt(sample, 8);
    auto dec = decrypt(enc.data(), enc.size());
    if (dec.size() != 8) return false;
    for (size_t i = 0; i < 8; ++i) if (dec[i] != sample[i]) ok = false;
    return ok;
}
