#ifndef PCSC_WORKSHOP1_CIPHER_AFFINECIPHER_H
#define PCSC_WORKSHOP1_CIPHER_AFFINECIPHER_H

#include "Cipher.h"
#include <vector>

class AffineCipher : public ICipher {
public:
    // E(x) = (a*x + b) mod 256
    explicit AffineCipher(BYTE a, BYTE b);

    // Anahtar verisinden a ve b hesapla
    explicit AffineCipher(const std::vector<BYTE>& key);

    ~AffineCipher() override;

    AffineCipher(const AffineCipher&) = delete;
    AffineCipher& operator=(const AffineCipher&) = delete;
    AffineCipher(AffineCipher&&) noexcept;
    AffineCipher& operator=(AffineCipher&&) noexcept;

    BYTEV encrypt(const BYTE* data, size_t len) const override;
    BYTEV decrypt(const BYTE* data, size_t len) const override;

    // optimized output-into overrides (void to match ICipher)
    void encryptInto(const BYTE* data, size_t len, BYTE* out) const override;
    void decryptInto(const BYTE* data, size_t len, BYTE* out) const override;

    bool test() const override;

    BYTE a() const;
    BYTE b() const;

    // Key -> a,b dönüþmleri
    static BYTE aFromKey(const std::vector<BYTE>& key);
    static BYTE bFromKey(const std::vector<BYTE>& key);

    // modular inverse of a modulo 256 (returns 0 if not invertible)
    static BYTE invMod256(BYTE a);

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

#endif // PCSC_WORKSHOP1_CIPHER_AFFINECIPHER_H
