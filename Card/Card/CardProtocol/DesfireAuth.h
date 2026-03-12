#ifndef DESFIRE_AUTH_H
#define DESFIRE_AUTH_H

#include "DesfireSession.h"
#include "DesfireCrypto.h"
#include "Result.h"

// ════════════════════════════════════════════════════════════════════════════════
// DesfireAuth — 3-pass Mutual Authentication State Machine
// ════════════════════════════════════════════════════════════════════════════════
//
// DESFire kartla 3 adımlı karşılıklı doğrulama:
//
//   Step 1: Host → Card  :  AuthenticateAES/ISO cmd + keyNo
//           Card → Host  :  ek(RndB)   (encrypted RndB)
//
//   Step 2: Host          :  RndB = dk(ek(RndB))
//                            RndA = random 16 byte
//                            payload = ek(RndA || rotL(RndB))
//           Host → Card  :  AdditionalFrame + payload
//           Card → Host  :  ek(rotL(RndA))
//
//   Step 3: Host          :  verify dk(response) == rotL(RndA)
//                            SessionKey = derive(RndA, RndB)
//
// Bu sınıf I/O yapmaz — transmit callback kullanır.
// Böylece Reader/CardConnection'a bağımlı olmaz.
//
// ─── Kullanım ──────────────────────────────────────────────────────────────
//
//   DesfireAuth auth;
//   DesfireSession session;
//
//   // Transmit callback: APDU gönder, response al
//   auto transmit = [&](const BYTEV& cmd) -> BYTEV {
//       return cardConnection.transmit(cmd);
//   };
//
//   bool ok = auth.authenticate(session, key, keyNo, keyType, transmit);
//
// ════════════════════════════════════════════════════════════════════════════════

class DesfireAuth {
public:

    // ── Full 3-pass auth ────────────────────────────────────────────────────

    // Sadece bir template bildirimi olmalı
    template<typename TransmitFn>
    void authenticate(DesfireSession& session,
                      const BYTEV& key, BYTE keyNo,
                      DesfireKeyType keyType,
                      TransmitFn&& transmit)
    {
        auto tryTx = [&](const BYTEV& apdu) -> PcscResult<BYTEV>
        {
			return PcscResult<BYTEV>::Ok(transmit(apdu));
        };
        tryAuthenticate(session, key, keyNo, keyType, tryTx).unwrap();
    }

    // Exception-free variant
    template<typename TryTransmitFn>
	PcscResultVoid tryAuthenticate(DesfireSession& session,
                                     const BYTEV& key, BYTE keyNo,
                                     DesfireKeyType keyType,
                                     TryTransmitFn&& transmit);

    // ── Individual steps (test/debug) ───────────────────────────────────────

    // Step 1: Build auth command APDU
    //   AES: {0x90, 0xAA, 0x00, 0x00, 0x01, keyNo, 0x00}
    //   2K3DES: {0x90, 0x1A, 0x00, 0x00, 0x01, keyNo, 0x00}
    static BYTEV buildAuthCmd(BYTE keyNo, DesfireKeyType keyType);

    // Step 2: Parse card's first response → extract encrypted RndB
    //   Response format: [ek(RndB) ... ] [SW1] [SW2]
    //   SW = 0x91AF (additional frame expected)
    static BYTEV extractEncRndB(const BYTEV& response, DesfireKeyType keyType);

    // Step 3: Build additional frame APDU
    //   {0x90, 0xAF, 0x00, 0x00, Lc, payload..., 0x00}
    static BYTEV buildAdditionalFrame(const BYTEV& payload);

    // Step 4: Parse card's second response → extract encrypted rotated RndA
    //   Response format: [ek(rotL(RndA))] [SW1] [SW2]
    //   SW = 0x9100 (success)
    static BYTEV extractEncRndA(const BYTEV& response, DesfireKeyType keyType);

    // ── DESFire status codes ────────────────────────────────────────────────

    static constexpr BYTE SW1_OK     = 0x91;
    static constexpr BYTE SW2_OK     = 0x00;
    static constexpr BYTE SW2_AF     = 0xAF;  // Additional Frame
    static constexpr BYTE SW2_AE     = 0xAE;  // Authentication Error
    static constexpr BYTE SW2_NO_CHG = 0xCA;  // No changes
    static constexpr BYTE SW2_PERM   = 0x9D;  // Permission denied

    static constexpr BYTE CMD_AUTH_ISO   = 0x1A;  // 2K3DES
    static constexpr BYTE CMD_AUTH_AES   = 0xAA;  // AES-128
    static constexpr BYTE CMD_MORE_DATA  = 0xAF;  // Additional frame

    // Non-template crypto helpers (defined in .cpp)
    static BYTEV decryptNonce(const BYTEV& encRndB, const BYTEV& key, DesfireKeyType keyType);
    static BYTEV buildAuthPayloadInternal(const BYTEV& rndA, const BYTEV& rndB,
                                           const BYTEV& key, DesfireKeyType keyType,
                                           const BYTEV& iv);
	static Result<StatusWord, PcscError> evaluateAuthSW(const BYTEV& resp, BYTE expectedSW2);
};

// ════════════════════════════════════════════════════════════════════════════════
// Template implementation
// ════════════════════════════════════════════════════════════════════════════════

template<typename TryTransmitFn>
PcscResultVoid DesfireAuth::tryAuthenticate(DesfireSession& session,
                                        const BYTEV& key, BYTE keyNo,
                                        DesfireKeyType keyType,
                                        TryTransmitFn&& transmit) {
    session.reset();
    session.keyType = keyType;

    // Step 1: Send auth cmd, receive ek(RndB)
    auto r1 = transmit(DesfireAuth::buildAuthCmd(keyNo, keyType));
    if (!r1) { session.reset(); return Result<void, PcscError>::Err(std::move(r1.error())); }
    auto e1 = evaluateAuthSW(r1.unwrap(), DesfireAuth::SW2_AF);
    if (!e1.is_ok()) { session.reset(); return Result<void, PcscError>::Err(std::move(e1.error())); }

    size_t expected = DesfireCrypto::nonceSize(keyType);
    if (r1.unwrap().size() - 2 != expected) {
		session.reset();
		return PcscResultVoid::Err({CardError::InvalidData});
	}
    BYTEV encRndB(r1.unwrap().begin(), r1.unwrap().begin() + expected);

    // Step 2: Decrypt, build payload
    size_t bs = DesfireCrypto::blockSize(keyType);
    BYTEV rndB = decryptNonce(encRndB, key, keyType);
    BYTEV rndA = DesfireCrypto::generateRndA(keyType);
    BYTEV iv2(encRndB.end() - bs, encRndB.end());
    BYTEV payload = buildAuthPayloadInternal(rndA, rndB, key, keyType, iv2);

    // Step 3: Send payload, receive ek(rotL(RndA))
    auto r2 = transmit(buildAdditionalFrame(payload));
    if (!r2) { session.reset(); return Result<void, PcscError>::Err(std::move(r2.error())); }
    auto e2 = evaluateAuthSW(r2.unwrap(), DesfireAuth::SW2_OK);
    if (!e2.is_ok()) {
        session.reset();
		return Result<void, PcscError>::Err(std::move(e2.error()));
	}

    if (r2.unwrap().size() - 2 != expected) {
		session.reset();
		return Result<void, PcscError>::Err(PcscError::from(CardError::InvalidData));
	}
    BYTEV encRndArot(r2.unwrap().begin(), r2.unwrap().begin() + expected);
    BYTEV iv3(payload.end() - bs, payload.end());
    bool ok = DesfireCrypto::verifyAuthResponse(encRndArot, rndA, key, keyType, iv3);
	if (!ok) {
		session.reset();
		return Result<void, PcscError>::Err(PcscError::from(DesfireError::AuthMismatch));
	}
    session.authenticated = true;
    session.authKeyNo = keyNo;
    session.keyType = keyType;
    session.sessionKey = DesfireCrypto::deriveSessionKey(rndA, rndB, keyType);
    session.iv = BYTEV(bs, 0);
    session.cmdCounter = 0;
    session.touchAuthTime();
    return Result<void, PcscError>::Ok();
}

#endif // DESFIRE_AUTH_H
