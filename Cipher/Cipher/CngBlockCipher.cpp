#include "CngBlockCipher.h"
#include <stdexcept>
#include <cstring>
#include <windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

// ════════════════════════════════════════════════════════════════════════════════
// Helpers
// ════════════════════════════════════════════════════════════════════════════════

namespace {

void checkStatus(NTSTATUS st, const char* msg) {
    if (!BCRYPT_SUCCESS(st))
        throw std::runtime_error(msg);
}

// Generic raw CBC encrypt/decrypt (no padding)
BYTEV rawCBC(LPCWSTR algorithm, const BYTEV& key, const BYTEV& iv,
             const BYTE* data, size_t len, size_t blockSize, bool encrypt)
{
    if (len == 0 || (len % blockSize) != 0)
        throw std::invalid_argument("Data length must be a non-zero multiple of block size");

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;

    checkStatus(
        BCryptOpenAlgorithmProvider(&hAlg, algorithm, nullptr, 0),
        "BCryptOpenAlgorithmProvider failed");

    // Set CBC mode
    NTSTATUS st = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
        (PUCHAR)BCRYPT_CHAIN_MODE_CBC,
        static_cast<ULONG>(sizeof(BCRYPT_CHAIN_MODE_CBC)), 0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw std::runtime_error("BCryptSetProperty(CHAINING_MODE) failed");
    }

    st = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
        const_cast<PUCHAR>(key.data()), static_cast<ULONG>(key.size()), 0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw std::runtime_error("BCryptGenerateSymmetricKey failed");
    }

    // IV is modified in-place by BCrypt — use a copy
    BYTEV ivCopy = iv;

    BYTEV out(len);
    ULONG result = 0;

    if (encrypt) {
        st = BCryptEncrypt(hKey,
            const_cast<PUCHAR>(data), static_cast<ULONG>(len),
            nullptr,
            ivCopy.data(), static_cast<ULONG>(ivCopy.size()),
            out.data(), static_cast<ULONG>(out.size()),
            &result, 0);  // 0 = no padding
    } else {
        st = BCryptDecrypt(hKey,
            const_cast<PUCHAR>(data), static_cast<ULONG>(len),
            nullptr,
            ivCopy.data(), static_cast<ULONG>(ivCopy.size()),
            out.data(), static_cast<ULONG>(out.size()),
            &result, 0);  // 0 = no padding
    }

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (!BCRYPT_SUCCESS(st))
        throw std::runtime_error(encrypt ? "BCryptEncrypt failed" : "BCryptDecrypt failed");

    out.resize(result);
    return out;
}

// ── CMAC sub-key generation ─────────────────────────────────────────────────

static void shiftLeft(BYTE* block, size_t len) {
    BYTE overflow = 0;
    for (int i = static_cast<int>(len) - 1; i >= 0; --i) {
        BYTE next = (block[i] >> 7) & 1;
        block[i] = (block[i] << 1) | overflow;
        overflow = next;
    }
}

static void xorBlock(BYTE* dst, const BYTE* src, size_t len) {
    for (size_t i = 0; i < len; ++i)
        dst[i] ^= src[i];
}

} // namespace

// ════════════════════════════════════════════════════════════════════════════════
// AES
// ════════════════════════════════════════════════════════════════════════════════

BYTEV CngBlockCipher::encryptAES(const BYTEV& key, const BYTEV& iv,
                                  const BYTE* data, size_t len) {
    return rawCBC(BCRYPT_AES_ALGORITHM, key, iv, data, len, AES_BLOCK, true);
}

BYTEV CngBlockCipher::decryptAES(const BYTEV& key, const BYTEV& iv,
                                  const BYTE* data, size_t len) {
    return rawCBC(BCRYPT_AES_ALGORITHM, key, iv, data, len, AES_BLOCK, false);
}

BYTEV CngBlockCipher::encryptAES(const BYTEV& key, const BYTEV& iv, const BYTEV& data) {
    return encryptAES(key, iv, data.data(), data.size());
}

BYTEV CngBlockCipher::decryptAES(const BYTEV& key, const BYTEV& iv, const BYTEV& data) {
    return decryptAES(key, iv, data.data(), data.size());
}

// ════════════════════════════════════════════════════════════════════════════════
// 2K3DES
// ════════════════════════════════════════════════════════════════════════════════

BYTEV CngBlockCipher::encrypt2K3DES(const BYTEV& key, const BYTEV& iv,
                                     const BYTE* data, size_t len) {
    // Windows CNG: BCRYPT_3DES_ALGORITHM expects 24-byte key.
    // 2K3DES (16 byte) → expand to 24 by repeating first 8 bytes.
    BYTEV key24 = key;
    if (key24.size() == 16) {
        key24.insert(key24.end(), key.begin(), key.begin() + 8);
    }
    return rawCBC(BCRYPT_3DES_ALGORITHM, key24, iv, data, len, DES_BLOCK, true);
}

BYTEV CngBlockCipher::decrypt2K3DES(const BYTEV& key, const BYTEV& iv,
                                     const BYTE* data, size_t len) {
    BYTEV key24 = key;
    if (key24.size() == 16) {
        key24.insert(key24.end(), key.begin(), key.begin() + 8);
    }
    return rawCBC(BCRYPT_3DES_ALGORITHM, key24, iv, data, len, DES_BLOCK, false);
}

BYTEV CngBlockCipher::encrypt2K3DES(const BYTEV& key, const BYTEV& iv, const BYTEV& data) {
    return encrypt2K3DES(key, iv, data.data(), data.size());
}

BYTEV CngBlockCipher::decrypt2K3DES(const BYTEV& key, const BYTEV& iv, const BYTEV& data) {
    return decrypt2K3DES(key, iv, data.data(), data.size());
}

// ════════════════════════════════════════════════════════════════════════════════
// 3K3DES (24-byte key, native)
// ════════════════════════════════════════════════════════════════════════════════

BYTEV CngBlockCipher::encrypt3K3DES(const BYTEV& key, const BYTEV& iv,
                                     const BYTE* data, size_t len) {
    if (key.size() != 24)
        throw std::invalid_argument("3K3DES key must be 24 bytes");
    return rawCBC(BCRYPT_3DES_ALGORITHM, key, iv, data, len, DES_BLOCK, true);
}

BYTEV CngBlockCipher::decrypt3K3DES(const BYTEV& key, const BYTEV& iv,
                                     const BYTE* data, size_t len) {
    if (key.size() != 24)
        throw std::invalid_argument("3K3DES key must be 24 bytes");
    return rawCBC(BCRYPT_3DES_ALGORITHM, key, iv, data, len, DES_BLOCK, false);
}

BYTEV CngBlockCipher::encrypt3K3DES(const BYTEV& key, const BYTEV& iv, const BYTEV& data) {
    return encrypt3K3DES(key, iv, data.data(), data.size());
}

BYTEV CngBlockCipher::decrypt3K3DES(const BYTEV& key, const BYTEV& iv, const BYTEV& data) {
    return decrypt3K3DES(key, iv, data.data(), data.size());
}

// ════════════════════════════════════════════════════════════════════════════════
// CMAC (OMAC1) — AES-128
// ════════════════════════════════════════════════════════════════════════════════
//
// RFC 4493 implementation:
//   1. Generate sub-keys K1, K2
//   2. XOR last block with K1 (complete) or K2 (incomplete + padding)
//   3. CBC-MAC over all blocks
//
// ════════════════════════════════════════════════════════════════════════════════

BYTEV CngBlockCipher::cmacAES(const BYTEV& key, const BYTE* data, size_t len) {
    constexpr size_t B = AES_BLOCK;
    constexpr BYTE Rb = 0x87;  // AES-128 polynomial

    // Step 1: Generate sub-keys K1, K2
    BYTEV zeroIV(B, 0);
    BYTEV zeroBlock(B, 0);
    BYTEV L = encryptAES(key, zeroIV, zeroBlock);

    BYTE K1[B], K2[B];
    std::memcpy(K1, L.data(), B);
    shiftLeft(K1, B);
    if (L[0] & 0x80) K1[B - 1] ^= Rb;

    std::memcpy(K2, K1, B);
    shiftLeft(K2, B);
    if (K1[0] & 0x80) K2[B - 1] ^= Rb;

    // Step 2: Determine number of blocks
    size_t n = (len == 0) ? 1 : (len + B - 1) / B;
    bool completeLastBlock = (len > 0) && (len % B == 0);

    // Step 3: Prepare last block (XOR with K1 or K2)
    BYTE lastBlock[B];
    size_t lastStart = (n - 1) * B;

    if (completeLastBlock) {
        std::memcpy(lastBlock, data + lastStart, B);
        xorBlock(lastBlock, K1, B);
    } else {
        std::memset(lastBlock, 0, B);
        size_t remain = len - lastStart;
        if (remain > 0) std::memcpy(lastBlock, data + lastStart, remain);
        lastBlock[remain] = 0x80;  // padding
        xorBlock(lastBlock, K2, B);
    }

    // Step 4: CBC-MAC
    BYTE X[B] = {};  // zero
    for (size_t i = 0; i < n - 1; ++i) {
        xorBlock(X, data + i * B, B);
        BYTEV enc = encryptAES(key, zeroIV, X, B);
        std::memcpy(X, enc.data(), B);
    }
    xorBlock(X, lastBlock, B);
    BYTEV mac = encryptAES(key, zeroIV, X, B);
    return mac;
}

BYTEV CngBlockCipher::cmacAES(const BYTEV& key, const BYTEV& data) {
    return cmacAES(key, data.data(), data.size());
}
