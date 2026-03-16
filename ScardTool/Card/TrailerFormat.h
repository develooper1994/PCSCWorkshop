/** @file TrailerFormat.h
 * @brief Mifare Classic trailer encode/decode and access bits. */
#pragma once
#include "Types.h"
#include "CardDataTypes.h"
#include <array>
#include <string>
#include <optional>
#include <stdexcept>
#include <sstream>
#include <iomanip>

// ════════════════════════════════════════════════════════════════════════════════
// TrailerFormat — Mifare Classic trailer bloğu encode/decode
//
// Trailer layout (16 byte):
//   [0..5]   KeyA      (6 byte) — okuma: 000000000000
//   [6..8]   Access    (3 byte) — C1, C2, C3 bitleri
//   [9]      GPB       (1 byte) — General Purpose Byte
//   [10..15] KeyB      (6 byte)
//
// Access byte encoding (C1,C2,C3 her block için 1 bit):
//   byte6 = (~C2 & 0xF)<<4 | (~C1 & 0xF)
//   byte7 = ( C1 & 0xF)<<4 | (~C3 & 0xF)
//   byte8 = ( C3 & 0xF)<<4 | ( C2 & 0xF)
//
// Nibble bit mapping (n = block index 0..3):
//   C1[n], C2[n], C3[n] kombinasyonu → erişim koşulu
//
// UYARI: Yanlış access byte yazılırsa kart kalıcı kilitlener.
//        write-trailer öncesi mutlaka --dry-run ile doğrulayın.
// ════════════════════════════════════════════════════════════════════════════════

// Her block için erişim koşulu (C1C2C3 bit triple)
struct BlockAccess {
    uint8_t c1 : 1;
    uint8_t c2 : 1;
    uint8_t c3 : 1;

    // Standart kombinasyonlar (data block)
    static BlockAccess transportDefault()  { return {0,0,0}; } // R/W: KeyA/B
    static BlockAccess readOnly()          { return {0,1,0}; } // R: KeyA/B  W: —
    static BlockAccess keyBWrite()         { return {1,0,0}; } // R: KeyA/B  W: KeyB
    static BlockAccess keyBOnly()          { return {0,1,1}; } // R/W: KeyB
    static BlockAccess noAccess()          { return {1,1,1}; } // R/W: —

    uint8_t bits() const { return (c3 << 2) | (c2 << 1) | c1; }

    std::string describe() const {
        switch (bits()) {
            case 0b000: return "R/W: KeyA/B (transport default)";
            case 0b010: return "R: KeyA/B  W: — (read-only)";
            case 0b100: return "R: KeyA/B  W: KeyB";
            case 0b110: return "R: KeyA/B  W: — (locked read-only)";
            case 0b001: return "R: KeyA/B  W: KeyB (decrementable)";
            case 0b011: return "R: KeyB    W: KeyB";
            case 0b101: return "R: KeyB    W: — (KeyB read-only)";
            case 0b111: return "R: —       W: — (no access)";
            default:    return "unknown";
        }
    }
};

// Sektör için 4 block (data0, data1, data2, trailer) erişim koşulları
struct SectorAccess {
    BlockAccess data[3];    // block 0,1,2 (data blocks)
    BlockAccess trailer;    // block 3 (trailer)

    // Fabrika varsayılanı
    static SectorAccess transportDefault() {
        return {
            {BlockAccess::transportDefault(),
             BlockAccess::transportDefault(),
             BlockAccess::transportDefault()},
            {0, 0, 1}  // trailer default: KeyA RW, Access R, KeyB RW
        };
    }
};

// Parsed trailer
struct TrailerData {
    KEYBYTES     keyA{};
    KEYBYTES     keyB{};
    ACCESSBYTES  accessBytes{};  // [C1byte, C2byte, C3byte, GPB]
    SectorAccess access{};
    bool         valid = false;
    std::string  parseError;
};

class TrailerFormat {
public:
    // ── Access bytes → 4 byte (C1byte, C2byte, C3byte, GPB) ─────────────────
    static ACCESSBYTES encodeAccess(const SectorAccess& sa, uint8_t gpb = 0x00) {
        // C1[n], C2[n], C3[n] nibble'larını topla
        uint8_t C1 = 0, C2 = 0, C3 = 0;
        for (int n = 0; n < 3; ++n) {
            C1 |= (sa.data[n].c1 & 1) << n;
            C2 |= (sa.data[n].c2 & 1) << n;
            C3 |= (sa.data[n].c3 & 1) << n;
        }
        // trailer block = bit 3
        C1 |= (sa.trailer.c1 & 1) << 3;
        C2 |= (sa.trailer.c2 & 1) << 3;
        C3 |= (sa.trailer.c3 & 1) << 3;

        ACCESSBYTES ab;
        ab[0] = static_cast<uint8_t>((((~C2) & 0xF) << 4) | ((~C1) & 0xF));
        ab[1] = static_cast<uint8_t>(((  C1  & 0xF) << 4) | ((~C3) & 0xF));
        ab[2] = static_cast<uint8_t>(((  C3  & 0xF) << 4) | (  C2  & 0xF));
        ab[3] = gpb;
        return ab;
    }

    // ── 4 access byte → SectorAccess ─────────────────────────────────────────
    static std::optional<SectorAccess> decodeAccess(const ACCESSBYTES& ab) {
        uint8_t b6 = ab[0], b7 = ab[1], b8 = ab[2];

        // NibbleExtract
        uint8_t nC2inv = (b6 >> 4) & 0xF;
        uint8_t nC1inv = (b6 >> 0) & 0xF;
        uint8_t nC1    = (b7 >> 4) & 0xF;
        uint8_t nC3inv = (b7 >> 0) & 0xF;
        uint8_t nC3    = (b8 >> 4) & 0xF;
        uint8_t nC2    = (b8 >> 0) & 0xF;

        // Bütünlük kontrolü
        if ((nC1 ^ nC1inv) != 0xF) return std::nullopt;
        if ((nC2 ^ nC2inv) != 0xF) return std::nullopt;
        if ((nC3 ^ nC3inv) != 0xF) return std::nullopt;

        SectorAccess sa;
        for (int n = 0; n < 3; ++n) {
            sa.data[n].c1 = (nC1 >> n) & 1;
            sa.data[n].c2 = (nC2 >> n) & 1;
            sa.data[n].c3 = (nC3 >> n) & 1;
        }
        sa.trailer.c1 = (nC1 >> 3) & 1;
        sa.trailer.c2 = (nC2 >> 3) & 1;
        sa.trailer.c3 = (nC3 >> 3) & 1;
        return sa;
    }

    // ── 16 byte trailer bloğu → TrailerData ──────────────────────────────────
    static TrailerData parse(const BYTEV& raw) {
        TrailerData td;
        if (raw.size() < 16) {
            td.parseError = "Trailer block must be 16 bytes";
            return td;
        }

        std::copy(raw.begin(),     raw.begin() + 6,  td.keyA.begin());
        std::copy(raw.begin() + 6, raw.begin() + 10, td.accessBytes.begin());
        std::copy(raw.begin() + 10, raw.end(),       td.keyB.begin());

        auto sa = decodeAccess(td.accessBytes);
        if (!sa) {
            td.parseError = "Access bytes integrity check failed (bit complement mismatch)";
            return td;
        }
        td.access = *sa;
        td.valid  = true;
        return td;
    }

    // ── TrailerData → 16 byte ────────────────────────────────────────────────
    static BYTEV build(const KEYBYTES& keyA,
                       const KEYBYTES& keyB,
                       const SectorAccess& access,
                       uint8_t gpb = 0x00) {
        auto ab = encodeAccess(access, gpb);
        BYTEV result(16, 0);
        std::copy(keyA.begin(), keyA.end(), result.begin());
        result[6]  = ab[0];
        result[7]  = ab[1];
        result[8]  = ab[2];
        result[9]  = gpb;
        std::copy(keyB.begin(), keyB.end(), result.begin() + 10);
        return result;
    }

    // ── Hex string → KEYBYTES ────────────────────────────────────────────────
    static std::optional<KEYBYTES> parseKey(const std::string& hex) {
        if (hex.size() != 12) return std::nullopt;
        KEYBYTES key;
        for (int i = 0; i < 6; ++i) {
            try {
                key[i] = static_cast<uint8_t>(
                    std::stoi(hex.substr(i*2, 2), nullptr, 16));
            } catch (...) { return std::nullopt; }
        }
        return key;
    }

    // ── KEYBYTES → hex string ────────────────────────────────────────────────
    static std::string keyToHex(const KEYBYTES& k) {
        std::ostringstream oss;
        oss << std::uppercase << std::hex << std::setfill('0');
        for (auto b : k) oss << std::setw(2) << (int)b;
        return oss.str();
    }

    // ── ACCESSBYTES → hex string ─────────────────────────────────────────────
    static std::string accessToHex(const ACCESSBYTES& ab) {
        std::ostringstream oss;
        oss << std::uppercase << std::hex << std::setfill('0');
        for (auto b : ab) oss << std::setw(2) << (int)b;
        return oss.str();
    }

    // ── Hex string (8 chars) → ACCESSBYTES ───────────────────────────────────
    static std::optional<ACCESSBYTES> parseAccessBytes(const std::string& hex) {
        if (hex.size() != 8) return std::nullopt;
        ACCESSBYTES ab;
        for (int i = 0; i < 4; ++i) {
            try {
                ab[i] = static_cast<uint8_t>(
                    std::stoi(hex.substr(i*2, 2), nullptr, 16));
            } catch (...) { return std::nullopt; }
        }
        return ab;
    }
};
