#include "pch.h"
#include "Cng3DES.h"
#include <stdexcept>
#include <vector>
#include <windows.h>
#include <bcrypt.h>
#include <sstream>
#include <algorithm>
#include <cstring>

struct Cng3DES::Impl {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    std::vector<BYTE> key;
    std::vector<BYTE> iv;
    ULONG blockSize = 8;

    Impl(const std::vector<BYTE>& k, const std::vector<BYTE>& i) : key(k), iv(i) {
        NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_3DES_ALGORITHM, nullptr, 0);
        if (!BCRYPT_SUCCESS(st)) throw std::runtime_error("BCryptOpenAlgorithmProvider failed (3DES)");
        // set CBC mode — wide string required
        st = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
            (PUCHAR)BCRYPT_CHAIN_MODE_CBC,
            static_cast<ULONG>(sizeof(BCRYPT_CHAIN_MODE_CBC)), 0);
        if (!BCRYPT_SUCCESS(st)) throw std::runtime_error("BCryptSetProperty(CHAINING_MODE) failed (3DES)");
        st = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, key.data(), static_cast<ULONG>(key.size()), 0);
        if (!BCRYPT_SUCCESS(st)) throw std::runtime_error("BCryptGenerateSymmetricKey failed (3DES)");
    }

    ~Impl() {
        if (hKey) BCryptDestroyKey(hKey);
        if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    }
};

Cng3DES::Cng3DES(const std::vector<BYTE>& key, const std::vector<BYTE>& iv)
    : pImpl(std::make_unique<Impl>(key, iv)) {
    if (pImpl->iv.size() != pImpl->blockSize) throw std::runtime_error("3DES IV must be 8 bytes");
    if (pImpl->key.size() != 24) throw std::runtime_error("3DES key must be 24 bytes");
}

Cng3DES::~Cng3DES() = default;
Cng3DES::Cng3DES(Cng3DES&&) noexcept = default;
Cng3DES& Cng3DES::operator=(Cng3DES&&) noexcept = default;

static void throwStatus3(NTSTATUS st) {
    std::ostringstream ss;
    ss << "BCrypt error: 0x" << std::hex << st;
    throw std::runtime_error(ss.str());
}

BYTEV Cng3DES::encrypt(const BYTE* data, size_t len) const {
    // Copy IV — BCrypt modifies the IV buffer in-place
    std::vector<BYTE> ivCopy = pImpl->iv;
    ULONG outSize = static_cast<ULONG>(len) + pImpl->blockSize + 8;
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
    if (!BCRYPT_SUCCESS(st)) throwStatus3(st);
    out.resize(result);
    return out;
}

BYTEV Cng3DES::decrypt(const BYTE* data, size_t len) const {
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
    if (!BCRYPT_SUCCESS(st)) throwStatus3(st);
    out.resize(result);
    return out;
}

bool Cng3DES::test() const {
    const char sample[] = "TestData1234";
    auto enc = encrypt(reinterpret_cast<const BYTE*>(sample), sizeof(sample)-1);
    auto dec = decrypt(enc.data(), enc.size());
    if (dec.size() != sizeof(sample)-1) return false;
    return std::equal(dec.begin(), dec.end(), reinterpret_cast<const BYTE*>(sample));
}
