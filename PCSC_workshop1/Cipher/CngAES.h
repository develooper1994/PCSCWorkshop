#ifndef PCSC_WORKSHOP1_CIPHER_CNGAES_H
#define PCSC_WORKSHOP1_CIPHER_CNGAES_H

#include "Cipher.h"
#include <vector>
#include <memory>
#include <windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

class CngAES : public ICipher {
public:
    // key: 16/24/32 bytes (AES-128/192/256), iv: 16 bytes (CBC)
    CngAES(const std::vector<BYTE>& key, const std::vector<BYTE>& iv);
    ~CngAES() override;

    CngAES(const CngAES&) = delete;
    CngAES& operator=(const CngAES&) = delete;
    CngAES(CngAES&&) noexcept;
    CngAES& operator=(CngAES&&) noexcept;

    BYTEV encrypt(const BYTE* data, size_t len) const override;
    BYTEV decrypt(const BYTE* data, size_t len) const override;

    bool test() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

#endif // PCSC_WORKSHOP1_CIPHER_CNGAES_H
