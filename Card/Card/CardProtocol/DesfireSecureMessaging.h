#ifndef DESFIRE_SECURE_MESSAGING_H
#define DESFIRE_SECURE_MESSAGING_H

#include "DesfireSession.h"
#include "DesfireCommands.h"
#include <vector>
#include <functional>

// ════════════════════════════════════════════════════════════════════════════════
// DesfireSecureMessaging — EV1 CMAC-based Command/Response Integrity
// ════════════════════════════════════════════════════════════════════════════════
//
// DESFire EV1 auth sonrası tüm komutlar CMAC ile korunabilir:
//
//   Host → Card:  komut verisi üzerinden CMAC hesapla, IV güncelle
//   Card → Host:  yanıt üzerinden CMAC doğrula, IV güncelle
//
// CommMode bazlı davranış:
//   Plain   → CMAC yok (ama IV yine güncellenir)
//   MAC     → 8-byte truncated CMAC eklenir/doğrulanır
//   Full    → Veri şifrelenir + CMAC (EV2+, burada temel destek)
//
// Kullanım:
//   DesfireSecureMessaging sm;
//   BYTEV wrappedCmd = sm.wrapCommand(session, rawCmd);
//   BYTEV response = transmit(wrappedCmd);
//   BYTEV data = sm.unwrapResponse(session, response);
//
// ════════════════════════════════════════════════════════════════════════════════

class DesfireSecureMessaging {
public:
    // ── Command wrapping ────────────────────────────────────────────────────

    // CMAC hesapla ve session IV'ı güncelle.
    // CommMode::MAC ise CMAC'ın ilk 8 byte'ını komut datasına ekle.
    // CommMode::Plain ise CMAC hesapla ama ekle me (IV güncellemesi için).
    static BYTEV wrapCommand(DesfireSession& session,
                              const BYTEV& cmdHeader,
                              const BYTEV& cmdData,
                              DesfireCommMode commMode);

    // ── Response unwrapping ─────────────────────────────────────────────────

    // Response verisinden CMAC doğrula ve saf veriyi döndür.
    // CommMode::MAC → son 8 byte CMAC, doğrula ve kaldır.
    // CommMode::Plain → CMAC hesapla (IV güncelle) ama doğrulama yapma.
    // Throws on CMAC mismatch.
    static BYTEV unwrapResponse(DesfireSession& session,
                                 const BYTEV& responseData,
                                 BYTE statusCode,
                                 DesfireCommMode commMode);

    // ── IV Management ───────────────────────────────────────────────────────

    // CMAC hesapla ve session IV'ı güncelle (her iki yönde kullanılır)
    static BYTEV calculateCMAC(DesfireSession& session, const BYTEV& data);

    // 16-byte CMAC'tan 8-byte truncated CMAC üret
    // Tek byte'lar alınır: mac[1], mac[3], mac[5], mac[7], mac[9], mac[11], mac[13], mac[15]
    static BYTEV truncateCMAC(const BYTEV& fullCMAC);

private:
    DesfireSecureMessaging() = delete;
};

#endif // DESFIRE_SECURE_MESSAGING_H
