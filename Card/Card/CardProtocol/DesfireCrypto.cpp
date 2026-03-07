#include "DesfireCrypto.h"
#include "CngBlockCipher.h"
#include <stdexcept>
#include <cstring>
#include <windows.h>
#include <bcrypt.h>

// ════════════════════════════════════════════════════════════════════════════════
// Sizes
// ════════════════════════════════════════════════════════════════════════════════

size_t DesfireCrypto::nonceSize(DesfireKeyType kt) {
    switch (kt) {
        case DesfireKeyType::AES128:  return 16;
        case DesfireKeyType::TwoDES:  return 8;
        case DesfireKeyType::DES:     return 8;
        case DesfireKeyType::ThreeDES:return 16;
        default: return 16;
    }
}

size_t DesfireCrypto::blockSize(DesfireKeyType kt) {
    return (kt == DesfireKeyType::AES128) ? 16 : 8;
}

size_t DesfireCrypto::keySize(DesfireKeyType kt) {
    switch (kt) {
        case DesfireKeyType::AES128:  return 16;
        case DesfireKeyType::TwoDES:  return 16;
        case DesfireKeyType::DES:     return 8;
        case DesfireKeyType::ThreeDES:return 24;
        default: return 16;
    }
}

// ════════════════════════════════════════════════════════════════════════════════
// Nonce
// ════════════════════════════════════════════════════════════════════════════════

BYTEV DesfireCrypto::generateRndA(DesfireKeyType kt) {
    size_t n = nonceSize(kt);
    BYTEV rndA(n);

    NTSTATUS st = BCryptGenRandom(nullptr, rndA.data(),
        static_cast<ULONG>(n), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!BCRYPT_SUCCESS(st))
        throw std::runtime_error("BCryptGenRandom failed");

    return rndA;
}

// ════════════════════════════════════════════════════════════════════════════════
// Rotate
// ════════════════════════════════════════════════════════════════════════════════

BYTEV DesfireCrypto::rotateLeft(const BYTEV& data) {
    if (data.empty()) return data;
    BYTEV r(data.begin() + 1, data.end());
    r.push_back(data[0]);
    return r;
}

BYTEV DesfireCrypto::rotateRight(const BYTEV& data) {
    if (data.empty()) return data;
    BYTEV r;
    r.push_back(data.back());
    r.insert(r.end(), data.begin(), data.end() - 1);
    return r;
}

// ════════════════════════════════════════════════════════════════════════════════
// Auth Payload
// ════════════════════════════════════════════════════════════════════════════════

BYTEV DesfireCrypto::buildAuthPayload(const BYTEV& rndA, const BYTEV& rndBdecrypted,
                                       const BYTEV& key, DesfireKeyType kt,
                                       const BYTEV& iv) {
    // Concat: RndA || rotateLeft(RndB)
    BYTEV rndBrot = rotateLeft(rndBdecrypted);
    BYTEV plain;
    plain.insert(plain.end(), rndA.begin(), rndA.end());
    plain.insert(plain.end(), rndBrot.begin(), rndBrot.end());

    // Encrypt
    if (kt == DesfireKeyType::AES128) {
        return CngBlockCipher::encryptAES(key, iv, plain);
    } else if (kt == DesfireKeyType::ThreeDES) {
        return CngBlockCipher::encrypt3K3DES(key, iv, plain);
    } else {
        return CngBlockCipher::encrypt2K3DES(key, iv, plain);
    }
}

bool DesfireCrypto::verifyAuthResponse(const BYTEV& encryptedResponse,
                                        const BYTEV& rndA,
                                        const BYTEV& key, DesfireKeyType kt,
                                        const BYTEV& iv) {
    BYTEV decrypted;
    if (kt == DesfireKeyType::AES128) {
        decrypted = CngBlockCipher::decryptAES(key, iv, encryptedResponse);
    } else if (kt == DesfireKeyType::ThreeDES) {
        decrypted = CngBlockCipher::decrypt3K3DES(key, iv, encryptedResponse);
    } else {
        decrypted = CngBlockCipher::decrypt2K3DES(key, iv, encryptedResponse);
    }

    BYTEV expected = rotateLeft(rndA);
    return decrypted == expected;
}

// ════════════════════════════════════════════════════════════════════════════════
// Session Key Derivation
// ════════════════════════════════════════════════════════════════════════════════

BYTEV DesfireCrypto::deriveSessionKey(const BYTEV& rndA, const BYTEV& rndB,
                                       DesfireKeyType kt) {
    BYTEV sk;

    if (kt == DesfireKeyType::AES128) {
        // AES-128 session key:
        //   SK = RndA[0..3] || RndB[0..3] || RndA[12..15] || RndB[12..15]
        if (rndA.size() < 16 || rndB.size() < 16)
            throw std::invalid_argument("AES nonces must be 16 bytes");
        sk.insert(sk.end(), rndA.begin(),      rndA.begin() + 4);
        sk.insert(sk.end(), rndB.begin(),      rndB.begin() + 4);
        sk.insert(sk.end(), rndA.begin() + 12, rndA.begin() + 16);
        sk.insert(sk.end(), rndB.begin() + 12, rndB.begin() + 16);
    }
    else if (kt == DesfireKeyType::TwoDES) {
        // 2K3DES session key:
        //   SK = RndA[0..3] || RndB[0..3] || RndA[4..7] || RndB[4..7]
        if (rndA.size() < 8 || rndB.size() < 8)
            throw std::invalid_argument("2K3DES nonces must be 8 bytes");
        sk.insert(sk.end(), rndA.begin(),     rndA.begin() + 4);
        sk.insert(sk.end(), rndB.begin(),     rndB.begin() + 4);
        sk.insert(sk.end(), rndA.begin() + 4, rndA.begin() + 8);
        sk.insert(sk.end(), rndB.begin() + 4, rndB.begin() + 8);
    }
    else if (kt == DesfireKeyType::ThreeDES) {
        // 3K3DES session key (24 bytes):
        //   SK = RndA[0..3]  || RndB[0..3]  ||
        //        RndA[6..9]  || RndB[6..9]  ||
        //        RndA[12..15]|| RndB[12..15]
        if (rndA.size() < 16 || rndB.size() < 16)
            throw std::invalid_argument("3K3DES nonces must be 16 bytes");
        sk.insert(sk.end(), rndA.begin(),      rndA.begin() + 4);
        sk.insert(sk.end(), rndB.begin(),      rndB.begin() + 4);
        sk.insert(sk.end(), rndA.begin() + 6,  rndA.begin() + 10);
        sk.insert(sk.end(), rndB.begin() + 6,  rndB.begin() + 10);
        sk.insert(sk.end(), rndA.begin() + 12, rndA.begin() + 16);
        sk.insert(sk.end(), rndB.begin() + 12, rndB.begin() + 16);
    }
    else {
        throw std::invalid_argument("Unsupported key type for session key derivation");
    }

    return sk;
}
