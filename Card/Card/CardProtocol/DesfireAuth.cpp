#include "DesfireAuth.h"
#include "DesfireCrypto.h"
#include "BlockCipher.h"

// ════════════════════════════════════════════════════════════════════════════════
// APDU Construction
// ════════════════════════════════════════════════════════════════════════════════

BYTEV DesfireAuth::buildAuthCmd(BYTE keyNo, DesfireKeyType keyType) {
    BYTE ins = (keyType == DesfireKeyType::AES128) ? CMD_AUTH_AES : CMD_AUTH_ISO;
    return { 0x90, ins, 0x00, 0x00, 0x01, keyNo, 0x00 };
}

BYTEV DesfireAuth::buildAdditionalFrame(const BYTEV& payload) {
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
// Response Parsing (legacy extractors — kept for test/debug)
// ════════════════════════════════════════════════════════════════════════════════

BYTEV DesfireAuth::extractEncRndB(const BYTEV& response, DesfireKeyType keyType) {
    evaluateAuthSW(response, SW2_AF).throwIfError();
    size_t dataLen = response.size() - 2;
    size_t expected = DesfireCrypto::nonceSize(keyType);
    if (dataLen != expected)
        throw std::runtime_error("Auth step 1: size mismatch");
    return BYTEV(response.begin(), response.begin() + dataLen);
}

BYTEV DesfireAuth::extractEncRndA(const BYTEV& response, DesfireKeyType keyType) {
    evaluateAuthSW(response, SW2_OK).throwIfError();
    size_t dataLen = response.size() - 2;
    size_t expected = DesfireCrypto::nonceSize(keyType);
    if (dataLen != expected)
        throw std::runtime_error("Auth step 3: size mismatch");
    return BYTEV(response.begin(), response.begin() + dataLen);
}

// ════════════════════════════════════════════════════════════════════════════════
// Non-template crypto helpers (called from template in header)
// ════════════════════════════════════════════════════════════════════════════════

PcscError DesfireAuth::evaluateAuthSW(const BYTEV& resp, BYTE expectedSW2) {
    if (resp.size() < 2) return {ErrorCode::ResponseTooShort};
    BYTE sw1 = resp[resp.size() - 2];
    BYTE sw2 = resp[resp.size() - 1];
    if (sw1 == SW1_OK && sw2 == expectedSW2) return {};
    StatusWord sw(sw1, sw2);
    if (sw2 == SW2_AE) return {ErrorCode::DesfireAuthMismatch, sw};
    if (sw2 == SW2_PERM) return {ErrorCode::DesfirePermissionDenied, sw};
    return {ErrorCode::DesfireError, sw};
}

BYTEV DesfireAuth::decryptNonce(const BYTEV& encRndB, const BYTEV& key, DesfireKeyType keyType) {
    BYTEV zeroIV(DesfireCrypto::blockSize(keyType), 0);
    if (keyType == DesfireKeyType::AES128)
        return crypto::block::decryptAesCbc(key, zeroIV, encRndB);
    else if (keyType == DesfireKeyType::ThreeDES)
        return crypto::block::decrypt3K3DesCbc(key, zeroIV, encRndB);
    else
        return crypto::block::decrypt2K3DesCbc(key, zeroIV, encRndB);
}

BYTEV DesfireAuth::buildAuthPayloadInternal(const BYTEV& rndA, const BYTEV& rndB,
                                             const BYTEV& key, DesfireKeyType keyType,
                                             const BYTEV& iv) {
    return DesfireCrypto::buildAuthPayload(rndA, rndB, key, keyType, iv);
}
