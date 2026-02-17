#ifndef PCSC_WORKSHOP1_CIPHER_CNG3DES_H
#define PCSC_WORKSHOP1_CIPHER_CNG3DES_H

#include "Cipher.h"
#include <vector>
#include <memory>
#include <windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

class Cng3DES : public ICipher {
public:
    // key: 24 bytes (3DES), iv: 8 bytes (CBC)
    Cng3DES(const std::vector<BYTE>& key, const std::vector<BYTE>& iv);
    ~Cng3DES() override;

    Cng3DES(const Cng3DES&) = delete;
    Cng3DES& operator=(const Cng3DES&) = delete;
    Cng3DES(Cng3DES&&) noexcept;
    Cng3DES& operator=(Cng3DES&&) noexcept;

    BYTEV encrypt(const BYTE* data, size_t len) const override;
    BYTEV decrypt(const BYTE* data, size_t len) const override;

    bool test() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

#endif // PCSC_WORKSHOP1_CIPHER_CNG3DES_H
