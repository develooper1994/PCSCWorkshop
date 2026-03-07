#ifndef DESFIRE_CRYPTO_H
#define DESFIRE_CRYPTO_H

#include "CardDataTypes.h"
#include "../CardModel/DesfireMemoryLayout.h"
#include <vector>
#include <cstddef>

// ════════════════════════════════════════════════════════════════════════════════
// DesfireCrypto — DESFire'a özgü kriptografik yardımcılar
// ════════════════════════════════════════════════════════════════════════════════
//
// Sorumluluklar:
//   - Random nonce üretimi (RndA)
//   - RndB rotate (left shift 1 byte)
//   - Session key türetme (RndA + RndB parçalarından)
//   - Auth challenge/response hazırlama
//
// Bu sınıf CngBlockCipher'ı kullanır (Cipher projesinde).
// Stateless — tüm methodlar static.
//
// ════════════════════════════════════════════════════════════════════════════════

class DesfireCrypto {
public:
    // ── Nonce ───────────────────────────────────────────────────────────────

    // Kriptografik rastgele nonce üret (16 byte AES, 8 byte DES)
    static BYTEV generateRndA(DesfireKeyType kt);

    // ── Rotate ──────────────────────────────────────────────────────────────

    // 1-byte left rotate: {a,b,c,d} → {b,c,d,a}
    static BYTEV rotateLeft(const BYTEV& data);

    // 1-byte right rotate (rotate undo): {b,c,d,a} → {a,b,c,d}
    static BYTEV rotateRight(const BYTEV& data);

    // ── Auth Challenge-Response ─────────────────────────────────────────────

    // Host→Card auth payload: encrypt(RndA || RndB')
    //   RndB' = rotateLeft(RndB)
    //   Encryption: AES-CBC or 2K3DES-CBC with zero IV (first frame)
    //              or with last ciphertext block as IV (chained)
    static BYTEV buildAuthPayload(const BYTEV& rndA, const BYTEV& rndBdecrypted,
                                   const BYTEV& key, DesfireKeyType kt,
                                   const BYTEV& iv);

    // Card→Host verify: decrypt response and check rotated RndA
    //   Decrypted should equal rotateLeft(RndA)
    static bool verifyAuthResponse(const BYTEV& encryptedResponse,
                                    const BYTEV& rndA,
                                    const BYTEV& key, DesfireKeyType kt,
                                    const BYTEV& iv);

    // ── Session Key Derivation ──────────────────────────────────────────────

    // AES-128:  SK = RndA[0..3] || RndB[0..3] || RndA[12..15] || RndB[12..15]
    // 2K3DES:   SK = RndA[0..3] || RndB[0..3] || RndA[4..7] || RndB[4..7]
    static BYTEV deriveSessionKey(const BYTEV& rndA, const BYTEV& rndB,
                                   DesfireKeyType kt);

    // ── Block/Nonce sizes ───────────────────────────────────────────────────

    static size_t nonceSize(DesfireKeyType kt);
    static size_t blockSize(DesfireKeyType kt);
    static size_t keySize(DesfireKeyType kt);

private:
    DesfireCrypto() = delete;
};

#endif // DESFIRE_CRYPTO_H
