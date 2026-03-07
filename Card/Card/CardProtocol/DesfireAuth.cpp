#include "DesfireAuth.h"
#include "DesfireCrypto.h"
#include "CngBlockCipher.h"
#include "Result.h"
#include <stdexcept>
#include <sstream>

// ════════════════════════════════════════════════════════════════════════════════
// APDU Construction
// ════════════════════════════════════════════════════════════════════════════════

BYTEV DesfireAuth::buildAuthCmd(BYTE keyNo, DesfireKeyType keyType) {
    // ISO 7816-4 wrapped: CLA=0x90, INS=cmd, P1=0, P2=0, Lc=1, data=keyNo, Le=0
    BYTE ins = (keyType == DesfireKeyType::AES128) ? CMD_AUTH_AES : CMD_AUTH_ISO;
    return { 0x90, ins, 0x00, 0x00, 0x01, keyNo, 0x00 };
}

BYTEV DesfireAuth::buildAdditionalFrame(const BYTEV& payload) {
    // ISO 7816-4 wrapped: CLA=0x90, INS=0xAF, P1=0, P2=0, Lc, data..., Le=0
    BYTEV apdu;
    apdu.push_back(0x90);
    apdu.push_back(CMD_MORE_DATA);
    apdu.push_back(0x00);
    apdu.push_back(0x00);
    apdu.push_back(static_cast<BYTE>(payload.size()));
    apdu.insert(apdu.end(), payload.begin(), payload.end());
    apdu.push_back(0x00);
    return apdu;
}

// ════════════════════════════════════════════════════════════════════════════════
// Response Parsing
// ════════════════════════════════════════════════════════════════════════════════

static PcscError evaluateSW(const BYTEV& resp, BYTE expectedSW2) {
    if (resp.size() < 2)
        return {ErrorCode::ResponseTooShort};
    BYTE sw1 = resp[resp.size() - 2];
    BYTE sw2 = resp[resp.size() - 1];
    if (sw1 == DesfireAuth::SW1_OK && sw2 == expectedSW2)
        return {};
    StatusWord sw(sw1, sw2);
    if (sw2 == DesfireAuth::SW2_AE) return {ErrorCode::DesfireAuthMismatch, sw};
    if (sw2 == DesfireAuth::SW2_PERM) return {ErrorCode::DesfirePermissionDenied, sw};
    return {ErrorCode::DesfireError, sw};
}

static void checkSW(const BYTEV& resp, BYTE expectedSW2, const char* context) {
    evaluateSW(resp, expectedSW2).throwIfError();
}

BYTEV DesfireAuth::extractEncRndB(const BYTEV& response, DesfireKeyType keyType) {
    checkSW(response, SW2_AF, "Auth step 1");
    size_t dataLen = response.size() - 2;
    size_t expected = DesfireCrypto::nonceSize(keyType);
    if (dataLen != expected) {
        std::ostringstream ss;
        ss << "Auth step 1: expected " << expected << " bytes, got " << dataLen;
        throw std::runtime_error(ss.str());
    }
    return BYTEV(response.begin(), response.begin() + dataLen);
}

BYTEV DesfireAuth::extractEncRndA(const BYTEV& response, DesfireKeyType keyType) {
    checkSW(response, SW2_OK, "Auth step 3");
    size_t dataLen = response.size() - 2;
    size_t expected = DesfireCrypto::nonceSize(keyType);
    if (dataLen != expected) {
        std::ostringstream ss;
        ss << "Auth step 3: expected " << expected << " bytes, got " << dataLen;
        throw std::runtime_error(ss.str());
    }
    return BYTEV(response.begin(), response.begin() + dataLen);
}

// ════════════════════════════════════════════════════════════════════════════════
// 3-Pass Mutual Authentication
// ════════════════════════════════════════════════════════════════════════════════

void DesfireAuth::authenticate(DesfireSession& session,
                                const BYTEV& key, BYTE keyNo,
                                DesfireKeyType keyType,
                                const TransmitFn& transmit) {
    session.reset();
    session.keyType = keyType;

    // ── Step 1: Send auth command, receive ek(RndB) ─────────────────────────

    BYTEV authCmd = buildAuthCmd(keyNo, keyType);
    BYTEV resp1 = transmit(authCmd);
    BYTEV encRndB = extractEncRndB(resp1, keyType);

    // ── Step 2: Decrypt RndB, generate RndA, build payload ──────────────────

    BYTEV zeroIV(DesfireCrypto::blockSize(keyType), 0);

    BYTEV rndB;
    if (keyType == DesfireKeyType::AES128)
        rndB = CngBlockCipher::decryptAES(key, zeroIV, encRndB);
    else if (keyType == DesfireKeyType::ThreeDES)
        rndB = CngBlockCipher::decrypt3K3DES(key, zeroIV, encRndB);
    else
        rndB = CngBlockCipher::decrypt2K3DES(key, zeroIV, encRndB);

    BYTEV rndA = DesfireCrypto::generateRndA(keyType);

    // IV for step 2 encryption: last block of received ciphertext (encRndB)
    // For AES: last 16 bytes of encRndB
    // For DES: last 8 bytes of encRndB
    size_t bs = DesfireCrypto::blockSize(keyType);
    BYTEV iv2(encRndB.end() - bs, encRndB.end());

    BYTEV payload = DesfireCrypto::buildAuthPayload(rndA, rndB, key, keyType, iv2);

    // ── Step 3: Send payload, receive ek(rotL(RndA)) ────────────────────────

    BYTEV frame2 = buildAdditionalFrame(payload);
    BYTEV resp2 = transmit(frame2);
    BYTEV encRndArot = extractEncRndA(resp2, keyType);

    // IV for step 3 decryption: last block of sent ciphertext (payload)
    BYTEV iv3(payload.end() - bs, payload.end());

    bool ok = DesfireCrypto::verifyAuthResponse(encRndArot, rndA, key, keyType, iv3);
    if (!ok) {
        session.reset();
        throw std::runtime_error("DESFire mutual auth failed: RndA verification mismatch");
    }

    // ── Auth başarılı — session oluştur ─────────────────────────────────────

    session.authenticated = true;
    session.authKeyNo = keyNo;
    session.keyType = keyType;
    session.sessionKey = DesfireCrypto::deriveSessionKey(rndA, rndB, keyType);
    session.iv = BYTEV(bs, 0);  // IV reset after auth
    session.cmdCounter = 0;
    session.touchAuthTime();     // timeout timer başlat
}

Result<void> DesfireAuth::tryAuthenticate(DesfireSession& session,
                                          const BYTEV& key, BYTE keyNo,
                                          DesfireKeyType keyType,
                                          const TryTransmitFn& transmit) {
    session.reset();
    session.keyType = keyType;

    // Step 1
    auto r1 = transmit(buildAuthCmd(keyNo, keyType));
    if (!r1) { session.reset(); return {r1.error}; }
    auto e1 = evaluateSW(r1.value, SW2_AF);
    if (!e1.ok()) { session.reset(); return {e1}; }

    size_t dataLen1 = r1.value.size() - 2;
    size_t expected = DesfireCrypto::nonceSize(keyType);
    if (dataLen1 != expected) { session.reset(); return {{ErrorCode::InvalidData}}; }
    BYTEV encRndB(r1.value.begin(), r1.value.begin() + dataLen1);

    // Step 2
    size_t bs = DesfireCrypto::blockSize(keyType);
    BYTEV zeroIV(bs, 0);
    BYTEV rndB;
    if (keyType == DesfireKeyType::AES128)
        rndB = CngBlockCipher::decryptAES(key, zeroIV, encRndB);
    else if (keyType == DesfireKeyType::ThreeDES)
        rndB = CngBlockCipher::decrypt3K3DES(key, zeroIV, encRndB);
    else
        rndB = CngBlockCipher::decrypt2K3DES(key, zeroIV, encRndB);

    BYTEV rndA = DesfireCrypto::generateRndA(keyType);
    BYTEV iv2(encRndB.end() - bs, encRndB.end());
    BYTEV payload = DesfireCrypto::buildAuthPayload(rndA, rndB, key, keyType, iv2);

    // Step 3
    auto r2 = transmit(buildAdditionalFrame(payload));
    if (!r2) { session.reset(); return {r2.error}; }
    auto e2 = evaluateSW(r2.value, SW2_OK);
    if (!e2.ok()) { session.reset(); return {e2}; }

    size_t dataLen2 = r2.value.size() - 2;
    if (dataLen2 != expected) { session.reset(); return {{ErrorCode::InvalidData}}; }
    BYTEV encRndArot(r2.value.begin(), r2.value.begin() + dataLen2);

    BYTEV iv3(payload.end() - bs, payload.end());
    bool ok = DesfireCrypto::verifyAuthResponse(encRndArot, rndA, key, keyType, iv3);
    if (!ok) { session.reset(); return {{ErrorCode::DesfireAuthMismatch}}; }

    session.authenticated = true;
    session.authKeyNo = keyNo;
    session.keyType = keyType;
    session.sessionKey = DesfireCrypto::deriveSessionKey(rndA, rndB, keyType);
    session.iv = BYTEV(bs, 0);
    session.cmdCounter = 0;
    session.touchAuthTime();
    return {PcscError{}};
}
