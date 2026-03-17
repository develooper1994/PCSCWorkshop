#ifndef PCSC_WORKSHOP1_CIPHER_CAESARCIPHER_H
#define PCSC_WORKSHOP1_CIPHER_CAESARCIPHER_H

#include "Cipher.h"
#include <vector>
#include <cstddef>

class CaesarCipher : public ICipher {
public:
    // Do�rudan shift de�eri ile olu�tur (a=1)
    explicit CaesarCipher(BYTE shift);

    // �ste�e ba�l� olarak multiplier (a) ve shift (b) ile olu�tur
    explicit CaesarCipher(BYTE a, BYTE shift);

    // Anahtar verisinden shift hesapla (compatibility)
    explicit CaesarCipher(const std::vector<BYTE>& key);

    ~CaesarCipher() override;

    CaesarCipher(const CaesarCipher&) = delete;
    CaesarCipher& operator=(const CaesarCipher&) = delete;
    CaesarCipher(CaesarCipher&&) noexcept;
    CaesarCipher& operator=(CaesarCipher&&) noexcept;

    BYTEV encrypt(const BYTE* data, size_t len) const override;
    BYTEV decrypt(const BYTE* data, size_t len) const override;

    // optimized output-into overrides
    void encryptInto(const BYTE* data, size_t len, BYTE* out) const override;
    void decryptInto(const BYTE* data, size_t len, BYTE* out) const override;

    bool test() const override;

    BYTE shift() const; // b
    BYTE multiplier() const; // a

    // Anahtar -> shift d�n���m� (mevcut kodla uyumlu kalmas� i�in b�rak�ld�)
    static BYTE shiftFromKey(const std::vector<BYTE>& key);

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

#endif // PCSC_WORKSHOP1_CIPHER_CAESARCIPHER_H
