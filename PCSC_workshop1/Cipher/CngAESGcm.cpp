#include "CngAESGcm.h"
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <random>
#include <cstring>

#ifndef BCRYPT_AUTH_TAG_LENGTH
#define BCRYPT_AUTH_TAG_LENGTH L"AuthTagLength"
#endif

struct CngAESGcm::Impl {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    std::vector<BYTE> key;
    ULONG tagSize = 16; // 128-bit tag
    ULONG nonceSize = 12; // recommended 96-bit nonce

    Impl(const std::vector<BYTE>& k) : key(k) {
        NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
        if (!BCRYPT_SUCCESS(st)) throw std::runtime_error("BCryptOpenAlgorithmProvider failed");
        // For AES-GCM we'll use BCryptEncrypt/Decrypt with BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO
    }

    ~Impl() {
        if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    }
};

CngAESGcm::CngAESGcm(const std::vector<BYTE>& key) : pImpl(std::make_unique<Impl>(key)) {}
CngAESGcm::~CngAESGcm() = default;
CngAESGcm::CngAESGcm(CngAESGcm&&) noexcept = default;
CngAESGcm& CngAESGcm::operator=(CngAESGcm&&) noexcept = default;

static void throwStatusG(NTSTATUS st) {
    std::ostringstream ss;
    ss << "BCrypt error: 0x" << std::hex << st;
    throw std::runtime_error(ss.str());
}

static std::vector<BYTE> randomBytes(size_t n) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);
    std::vector<BYTE> v(n);
    for (size_t i = 0; i < n; ++i) v[i] = static_cast<BYTE>(dist(gen));
    return v;
}

// Helper that performs AAD-aware encrypt
BYTEV encrypt_with_aad(const CngAESGcm::Impl* impl, const BYTE* data, size_t len, const BYTE* aad, size_t aad_len) {
    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS st = BCryptGenerateSymmetricKey(impl->hAlg, &hKey, nullptr, 0, const_cast<PUCHAR>(impl->key.data()), static_cast<ULONG>(impl->key.size()), 0);
    if (!BCRYPT_SUCCESS(st)) throwStatusG(st);

    auto nonce = randomBytes(impl->nonceSize);
    ULONG cipherTextSize = static_cast<ULONG>(len);
    ULONG resultSize = 0;
    ULONG outBufferSize = cipherTextSize + impl->tagSize;
    std::vector<BYTE> out(outBufferSize);

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = nonce.data();
    authInfo.cbNonce = static_cast<ULONG>(nonce.size());
    authInfo.pbTag = out.data() + cipherTextSize; // tag placed after ciphertext
    authInfo.cbTag = impl->tagSize;

    if (aad && aad_len > 0) {
        authInfo.pbAuthData = const_cast<PUCHAR>(aad);
        authInfo.cbAuthData = static_cast<ULONG>(aad_len);
    }

    st = BCryptEncrypt(
        hKey,
        const_cast<PUCHAR>(data), static_cast<ULONG>(len),
        &authInfo,
        nonce.data(), static_cast<ULONG>(nonce.size()),
        out.data(), outBufferSize,
        &resultSize,
        0
    );

    BCryptDestroyKey(hKey);
    if (!BCRYPT_SUCCESS(st)) throwStatusG(st);

    BYTEV ret;
    ret.reserve(nonce.size() + resultSize);
    ret.insert(ret.end(), nonce.begin(), nonce.end());
    ret.insert(ret.end(), out.begin(), out.begin() + resultSize);
    return ret;
}

// Helper that performs AAD-aware decrypt
BYTEV decrypt_with_aad(const CngAESGcm::Impl* impl, const BYTE* data, size_t len, const BYTE* aad, size_t aad_len) {
    if (len < impl->nonceSize + impl->tagSize) throw std::runtime_error("Input too short for nonce+tag");
    const BYTE* nonce = data;
    const BYTE* ctAndTag = data + impl->nonceSize;
    ULONG ctAndTagLen = static_cast<ULONG>(len - impl->nonceSize);
    if (ctAndTagLen < impl->tagSize) throw std::runtime_error("Ciphertext too short");
    ULONG cipherTextLen = ctAndTagLen - impl->tagSize;

    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS st = BCryptGenerateSymmetricKey(impl->hAlg, &hKey, nullptr, 0, const_cast<PUCHAR>(impl->key.data()), static_cast<ULONG>(impl->key.size()), 0);
    if (!BCRYPT_SUCCESS(st)) throwStatusG(st);

    std::vector<BYTE> out(cipherTextLen);
    ULONG resultSize = 0;

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = const_cast<PUCHAR>(nonce);
    authInfo.cbNonce = static_cast<ULONG>(impl->nonceSize);
    authInfo.pbTag = const_cast<PUCHAR>(ctAndTag + cipherTextLen);
    authInfo.cbTag = impl->tagSize;

    if (aad && aad_len > 0) {
        authInfo.pbAuthData = const_cast<PUCHAR>(aad);
        authInfo.cbAuthData = static_cast<ULONG>(aad_len);
    }

    st = BCryptDecrypt(
        hKey,
        const_cast<PUCHAR>(ctAndTag), ctAndTagLen,
        &authInfo,
        const_cast<PUCHAR>(nonce), static_cast<ULONG>(impl->nonceSize),
        out.data(), cipherTextLen,
        &resultSize,
        0
    );

    BCryptDestroyKey(hKey);
    if (!BCRYPT_SUCCESS(st)) throwStatusG(st);

    out.resize(resultSize);
    return out;
}

BYTEV CngAESGcm::encrypt(const BYTE* data, size_t len) const {
    return encrypt_with_aad(pImpl.get(), data, len, nullptr, 0);
}

BYTEV CngAESGcm::decrypt(const BYTE* data, size_t len) const {
    return decrypt_with_aad(pImpl.get(), data, len, nullptr, 0);
}

BYTEV CngAESGcm::encrypt(const BYTE* data, size_t len, const BYTE* aad, size_t aad_len) const {
    return encrypt_with_aad(pImpl.get(), data, len, aad, aad_len);
}

BYTEV CngAESGcm::decrypt(const BYTE* data, size_t len, const BYTE* aad, size_t aad_len) const {
    return decrypt_with_aad(pImpl.get(), data, len, aad, aad_len);
}

bool CngAESGcm::test() const {
    const char sample[] = "GcmTestData123";
    const char aad[] = "header-data";
    auto enc = encrypt(reinterpret_cast<const BYTE*>(sample), sizeof(sample)-1, reinterpret_cast<const BYTE*>(aad), sizeof(aad)-1);
    auto dec = decrypt(enc.data(), enc.size(), reinterpret_cast<const BYTE*>(aad), sizeof(aad)-1);
    if (dec.size() != sizeof(sample)-1) return false;
    return std::equal(dec.begin(), dec.end(), reinterpret_cast<const BYTE*>(sample));
}
