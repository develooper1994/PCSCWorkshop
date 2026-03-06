#ifndef PAGEDEFINITION_H
#define PAGEDEFINITION_H

#include "CardDataTypes.h"
#include <cstring>

// ════════════════════════════════════════════════════════════════════════════════
// Page Definition - Zero-Copy Union for Ultralight Page (4 bytes)
// ════════════════════════════════════════════════════════════════════════════════
//
// Mifare Ultralight page = 4 byte. Farkli page tipleri ayni bellegi
// farkli yorumlar:
//   - Page 0 : SN0, SN1, SN2, BCC0  (serial part 1)
//   - Page 1 : SN3, SN4, SN5, SN6  (serial part 2)
//   - Page 2 : BCC1, Internal, Lock0, Lock1
//   - Page 3 : OTP0-OTP3 (one-time programmable)
//   - Page 4-15 : User data
//
// ════════════════════════════════════════════════════════════════════════════════

struct UltralightPage {
    union {
        BYTE raw[4];

        struct { BYTE data[4]; } data;

        // Page 0
        struct {
            BYTE sn0; BYTE sn1; BYTE sn2;
            BYTE bcc0;
        } serial0;

        // Page 1
        struct {
            BYTE sn3; BYTE sn4; BYTE sn5; BYTE sn6;
        } serial1;

        // Page 2
        struct {
            BYTE bcc1;
            BYTE internal_;
            BYTE lock0;
            BYTE lock1;
        } lock;

        // Page 3
        struct { BYTE otp[4]; } otp;
    };

    // ────────────────────────────────────────────────────────────────────────────
    // Constructors
    // ────────────────────────────────────────────────────────────────────────────

    UltralightPage() { std::memset(raw, 0, 4); }

    explicit UltralightPage(const BYTE d[4]) { std::memcpy(raw, d, 4); }

    // ────────────────────────────────────────────────────────────────────────────
    // Comparison
    // ────────────────────────────────────────────────────────────────────────────

    bool operator==(const UltralightPage& o) const { return std::memcmp(raw, o.raw, 4) == 0; }
    bool operator!=(const UltralightPage& o) const { return !(*this == o); }

    // ────────────────────────────────────────────────────────────────────────────
    // Metadata
    // ────────────────────────────────────────────────────────────────────────────

    bool isEmpty() const {
        for (int i = 0; i < 4; ++i) { if (raw[i] != 0x00) return false; }
        return true;
    }

    constexpr size_t size() const { return 4; }
    const BYTE* getRawPtr() const { return raw; }
    BYTE* getRawPtr() { return raw; }
};

#endif // PAGEDEFINITION_H
