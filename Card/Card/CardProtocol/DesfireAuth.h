#ifndef DESFIRE_AUTH_H
#define DESFIRE_AUTH_H

#include "DesfireSession.h"
#include "DesfireCrypto.h"
#include <functional>

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
    // Transmit callback type: send APDU, receive response (with SW)
    using TransmitFn = std::function<BYTEV(const BYTEV&)>;

    // ── Full 3-pass auth ────────────────────────────────────────────────────

    // Performs complete 3-pass mutual authentication.
    // On success: session is populated with session key and IV.
    // On failure: session is reset, throws std::runtime_error.
    //
    // key: 16 bytes (AES-128) or 16 bytes (2K3DES)
    // keyNo: 0–13 (application key number)
    // transmit: callback that sends APDU and returns full response incl. SW
    void authenticate(DesfireSession& session,
                      const BYTEV& key, BYTE keyNo,
                      DesfireKeyType keyType,
                      const TransmitFn& transmit);

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
};

#endif // DESFIRE_AUTH_H
