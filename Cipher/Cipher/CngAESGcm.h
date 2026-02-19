#ifndef PCSC_WORKSHOP1_CIPHER_CNGAESGCM_H
#define PCSC_WORKSHOP1_CIPHER_CNGAESGCM_H

#include "Cipher.h"
#include "ICipherAAD.h"
#include <vector>
#include <memory>

// Ensure Windows SDK exposes BCryptAuthEncrypt/Decrypt
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602 // Windows 8
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x06020000 // Windows 8
#endif

#include <windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

class CngAESGcm : public ICipherAAD {
public:
    // key: 16/24/32 bytes (AES-128/192/256)
    CngAESGcm(const std::vector<BYTE>& key);
    ~CngAESGcm() override;

    CngAESGcm(const CngAESGcm&) = delete;
    CngAESGcm& operator=(const CngAESGcm&) = delete;
    CngAESGcm(CngAESGcm&&) noexcept;
    CngAESGcm& operator=(CngAESGcm&&) noexcept;

    // Forward declaration of Impl is public so implementation file can reference it
    struct Impl;

    // ICipher interface (no AAD)
    BYTEV encrypt(const BYTE* data, size_t len) const override;
    BYTEV decrypt(const BYTE* data, size_t len) const override;

    // AAD-capable interface
    BYTEV encrypt(const BYTE* data, size_t len, const BYTE* aad, size_t aad_len) const override;
    BYTEV decrypt(const BYTE* data, size_t len, const BYTE* aad, size_t aad_len) const override;

    bool test() const override;

private:
    std::unique_ptr<Impl> pImpl;
};

#endif // PCSC_WORKSHOP1_CIPHER_CNGAESGCM_H
