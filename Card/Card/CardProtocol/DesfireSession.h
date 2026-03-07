#ifndef DESFIRE_SESSION_H
#define DESFIRE_SESSION_H

#include "CardDataTypes.h"
#include "../CardModel/DesfireMemoryLayout.h"
#include <vector>
#include <chrono>

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
//   - Timeout süresi dolduğunda → session düşer
//
// Timeout:
//   - Varsayılan: 0 (sınırsız — devre dışı)
//   - setTimeoutMs(5000) → 5 saniye sonra session düşer
//   - isExpired() ile kontrol edilir
//   - Her auth başarılı olduğunda authTime_ güncellenir
//
// ════════════════════════════════════════════════════════════════════════════════

struct DesfireSession {
    bool   authenticated = false;

    // Auth parametreleri
    BYTE   authKeyNo = 0;
    DesfireKeyType keyType = DesfireKeyType::AES128;

    // Session key (auth sonrası türetilir)
    BYTEV  sessionKey;

    // IV tracking
    BYTEV  iv;

    // Seçili application
    DesfireAID currentAID = DesfireAID::picc();

    // Command counter (EV2+ secure messaging için)
    uint16_t cmdCounter = 0;

    // ── Timeout ─────────────────────────────────────────────────────────────

    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    // Auth zamanı (authenticate() başarılı olduğunda set edilir)
    TimePoint authTime_{};

    // Timeout süresi (ms). 0 = sınırsız (timeout devre dışı).
    uint32_t timeoutMs_ = 0;

    // Timeout süresini ayarla (ms). 0 = devre dışı.
    void setTimeoutMs(uint32_t ms) { timeoutMs_ = ms; }

    // Auth zamanını "şimdi" olarak güncelle
    void touchAuthTime() { authTime_ = Clock::now(); }

    // Session süresi dolmuş mu?
    bool isExpired() const {
        if (!authenticated) return true;
        if (timeoutMs_ == 0) return false;  // timeout devre dışı
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - authTime_).count();
        return elapsed >= timeoutMs_;
    }

    // authenticated VE süresi dolmamışsa true
    bool isValid() const {
        return authenticated && !isExpired();
    }

    // ── Helpers ─────────────────────────────────────────────────────────────

    void reset() {
        authenticated = false;
        authKeyNo = 0;
        sessionKey.clear();
        iv.clear();
        cmdCounter = 0;
        authTime_ = {};
    }

    void resetKeepApp() {
        auto aid = currentAID;
        reset();
        currentAID = aid;
    }

    // Zero IV oluştur
    BYTEV zeroIV() const {
        size_t bs = (keyType == DesfireKeyType::AES128) ? 16 : 8;
        return BYTEV(bs, 0);
    }
};

#endif // DESFIRE_SESSION_H
