#include "pch.h"
#include "CngAES.h"
#include "Exceptions/GenericExceptions.h"
#include <stdexcept>
#include <vector>
#include <windows.h>
#include <bcrypt.h>
#include <sstream>
#include <algorithm>
#include <cstring>

struct CngAES::Impl {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    std::vector<BYTE> key;
    std::vector<BYTE> iv;
    ULONG blockSize = 16;

    Impl(const std::vector<BYTE>& k, const std::vector<BYTE>& i) : key(k), iv(i) {
        NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
        if (!BCRYPT_SUCCESS(st)) throw pcsc::CipherError("BCryptOpenAlgorithmProvider failed");
        // set CBC mode — BCRYPT_CHAIN_MODE_CBC is a wide string (L"ChainingModeCBC")
        st = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
            (PUCHAR)BCRYPT_CHAIN_MODE_CBC,
            static_cast<ULONG>(sizeof(BCRYPT_CHAIN_MODE_CBC)), 0);
        if (!BCRYPT_SUCCESS(st)) throw pcsc::CipherError("BCryptSetProperty(CHAINING_MODE) failed");
        // generate symmetric key object
        st = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, key.data(), static_cast<ULONG>(key.size()), 0);
        if (!BCRYPT_SUCCESS(st)) throw pcsc::CipherError("BCryptGenerateSymmetricKey failed");
    }

    ~Impl() {
        if (hKey) BCryptDestroyKey(hKey);
        if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    }
};

CngAES::CngAES(const std::vector<BYTE>& key, const std::vector<BYTE>& iv)
    : pImpl(std::make_unique<Impl>(key, iv)) {
    if (pImpl->iv.size() != pImpl->blockSize) throw pcsc::CipherError("AES IV must be 16 bytes");
}

CngAES::~CngAES() = default;
CngAES::CngAES(CngAES&&) noexcept = default;
CngAES& CngAES::operator=(CngAES&&) noexcept = default;

static void throwStatus(NTSTATUS st) {
    std::ostringstream ss;
    ss << "BCrypt error: 0x" << std::hex << st;
    throw pcsc::CipherError(ss.str());
}

BYTEV CngAES::encrypt(const BYTE* data, size_t len) const {
    // Copy IV — BCrypt modifies the IV buffer in-place
    std::vector<BYTE> ivCopy = pImpl->iv;
    ULONG outSize = static_cast<ULONG>(len) + pImpl->blockSize + 16;
    BYTEV out(outSize);
    ULONG result = 0;
    NTSTATUS st = BCryptEncrypt(
        pImpl->hKey,
        const_cast<PUCHAR>(data),
        static_cast<ULONG>(len),
        nullptr,
        ivCopy.data(),
        static_cast<ULONG>(ivCopy.size()),
        out.data(),
        outSize,
        &result,
        BCRYPT_BLOCK_PADDING
    );
    if (!BCRYPT_SUCCESS(st)) throwStatus(st);
    out.resize(result);
    return out;
}

BYTEV CngAES::decrypt(const BYTE* data, size_t len) const {
    // Copy IV — BCrypt modifies the IV buffer in-place
    std::vector<BYTE> ivCopy = pImpl->iv;
    ULONG outSize = static_cast<ULONG>(len) + pImpl->blockSize;
    BYTEV out(outSize);
    ULONG result = 0;
    NTSTATUS st = BCryptDecrypt(
        pImpl->hKey,
        const_cast<PUCHAR>(data),
        static_cast<ULONG>(len),
        nullptr,
        ivCopy.data(),
        static_cast<ULONG>(ivCopy.size()),
        out.data(),
        outSize,
        &result,
        BCRYPT_BLOCK_PADDING
    );
    if (!BCRYPT_SUCCESS(st)) throwStatus(st);
    out.resize(result);
    return out;
}

bool CngAES::test() const {
    const char sample[] = "TestData1234567"; // 15 bytes
    auto enc = encrypt(reinterpret_cast<const BYTE*>(sample), sizeof(sample)-1);
    auto dec = decrypt(enc.data(), enc.size());
    if (dec.size() != sizeof(sample)-1) return false;
    return std::equal(dec.begin(), dec.end(), reinterpret_cast<const BYTE*>(sample));
}
