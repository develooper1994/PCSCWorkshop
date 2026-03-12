#include "DesfireCrypto.h"
#include "BlockCipher.h"
#include "Random.h"
#include "Result.h"
#include <cstring>

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
    return crypto::randomBytes(nonceSize(kt));
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
        return crypto::block::encryptAesCbc(key, iv, plain);
    } else if (kt == DesfireKeyType::ThreeDES) {
        return crypto::block::encrypt3K3DesCbc(key, iv, plain);
    } else {
        return crypto::block::encrypt2K3DesCbc(key, iv, plain);
    }
}

bool DesfireCrypto::verifyAuthResponse(const BYTEV& encryptedResponse,
                                        const BYTEV& rndA,
                                        const BYTEV& key, DesfireKeyType kt,
                                        const BYTEV& iv) {
    BYTEV decrypted;
    if (kt == DesfireKeyType::AES128) {
        decrypted = crypto::block::decryptAesCbc(key, iv, encryptedResponse);
    } else if (kt == DesfireKeyType::ThreeDES) {
        decrypted = crypto::block::decrypt3K3DesCbc(key, iv, encryptedResponse);
    } else {
        decrypted = crypto::block::decrypt2K3DesCbc(key, iv, encryptedResponse);
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
        if (rndA.size() < 16 || rndB.size() < 16) {
			PcscError::make(CardError::InvalidData, "AES nonces must be 16 bytes").throwIfError();
            return {};
        }
        sk.insert(sk.end(), rndA.begin(),      rndA.begin() + 4);
        sk.insert(sk.end(), rndB.begin(),      rndB.begin() + 4);
        sk.insert(sk.end(), rndA.begin() + 12, rndA.begin() + 16);
        sk.insert(sk.end(), rndB.begin() + 12, rndB.begin() + 16);
    }
    else if (kt == DesfireKeyType::TwoDES) {
        // 2K3DES session key:
        //   SK = RndA[0..3] || RndB[0..3] || RndA[4..7] || RndB[4..7]
        if (rndA.size() < 8 || rndB.size() < 8) {
			PcscError::make(CardError::InvalidData, "2K3DES nonces must be 8 bytes").throwIfError();
            return {};
        }
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
        if (rndA.size() < 16 || rndB.size() < 16) {
			PcscError::make(CardError::InvalidData, "3K3DES nonces must be 16 bytes").throwIfError();
            return {};
        }
        sk.insert(sk.end(), rndA.begin(),      rndA.begin() + 4);
        sk.insert(sk.end(), rndB.begin(),      rndB.begin() + 4);
        sk.insert(sk.end(), rndA.begin() + 6,  rndA.begin() + 10);
        sk.insert(sk.end(), rndB.begin() + 6,  rndB.begin() + 10);
        sk.insert(sk.end(), rndA.begin() + 12, rndA.begin() + 16);
        sk.insert(sk.end(), rndB.begin() + 12, rndB.begin() + 16);
    }
    else {
		PcscError::make(CardError::InvalidData, "Unsupported key type for session key derivation").throwIfError();
        return {};
    }

    return sk;
}
