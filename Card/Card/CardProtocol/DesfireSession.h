#ifndef DESFIRE_SESSION_H
#define DESFIRE_SESSION_H

#include "CardDataTypes.h"
#include "../CardModel/DesfireMemoryLayout.h"
#include <vector>

// ════════════════════════════════════════════════════════════════════════════════
// DesfireSession — Aktif DESFire oturum bilgisi
// ════════════════════════════════════════════════════════════════════════════════
//
// 3-pass mutual auth başarılı olduktan sonra oluşturulur.
// Session key, IV, seçili application ve auth durumunu tutar.
//
// Ömür:
//   - Auth başarılı → session oluşur
//   - SelectApplication → session düşer (yeniden auth gerekir)
//   - Kart çıkartılırsa → session düşer
//   - Timeout → session düşer (opsiyonel)
//
// ════════════════════════════════════════════════════════════════════════════════

struct DesfireSession {
    bool   authenticated = false;

    // Auth parametreleri
    BYTE   authKeyNo = 0;                    // hangi key ile auth yapıldı
    DesfireKeyType keyType = DesfireKeyType::AES128;

    // Session key (auth sonrası türetilir)
    BYTEV  sessionKey;

    // IV tracking — her komut sonrası güncellenir
    //   AES: son encrypt/decrypt bloğunun çıktısı
    //   DES: benzer
    BYTEV  iv;

    // Seçili application
    DesfireAID currentAID = DesfireAID::picc();

    // Command counter (EV2+ secure messaging için)
    uint16_t cmdCounter = 0;

    // ── Helpers ─────────────────────────────────────────────────────────────

    void reset() {
        authenticated = false;
        authKeyNo = 0;
        sessionKey.clear();
        iv.clear();
        cmdCounter = 0;
    }

    void resetKeepApp() {
        authenticated = false;
        authKeyNo = 0;
        sessionKey.clear();
        iv.clear();
        cmdCounter = 0;
    }

    // Zero IV oluştur (auth başlangıcı ve bazı komutlar için)
    BYTEV zeroIV() const {
        size_t bs = (keyType == DesfireKeyType::AES128) ? 16 : 8;
        return BYTEV(bs, 0);
    }
};

#endif // DESFIRE_SESSION_H
