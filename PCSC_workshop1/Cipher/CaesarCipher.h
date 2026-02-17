#ifndef PCSC_WORKSHOP1_CIPHER_CAESARCIPHER_H
#define PCSC_WORKSHOP1_CIPHER_CAESARCIPHER_H

#include "Cipher.h"

class CaesarCipher : public ICipher {
public:
    explicit CaesarCipher(BYTE shift);
    ~CaesarCipher() override;

    CaesarCipher(const CaesarCipher&) = delete;
    CaesarCipher& operator=(const CaesarCipher&) = delete;
    CaesarCipher(CaesarCipher&&) noexcept;
    CaesarCipher& operator=(CaesarCipher&&) noexcept;

    BYTEV encrypt(const BYTE* data, size_t len) const override;
    BYTEV decrypt(const BYTE* data, size_t len) const override;

    bool test() const override;

    BYTE shift() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

#endif // PCSC_WORKSHOP1_CIPHER_CAESARCIPHER_H
