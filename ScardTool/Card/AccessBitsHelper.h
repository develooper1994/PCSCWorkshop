/** @file AccessBitsHelper.h
 * @brief Access condition table and interactive access byte calculator. */
#pragma once
#include "TrailerFormat.h"
#include <iostream>
#include <string>
#include <vector>
#include <functional>

// ════════════════════════════════════════════════════════════════════════════════
// AccessBitsHelper — make-access komutu için Mifare Classic access condition
//
// Data block erişim koşulları (C1C2C3):
//
//  bits  C1C2C3  Read      Write     Increment  Dec/Transfer/Restore
//  000   000     KeyA/B    KeyA/B    KeyA/B     KeyA/B   ← transport default
//  001   001     KeyA/B    KeyB      —          KeyA/B
//  010   010     KeyA/B    —         —          —        ← read-only
//  011   011     KeyB      KeyB      —          —
//  100   100     KeyA/B    KeyB      —          —
//  101   101     KeyB      —         —          —
//  110   110     KeyA/B    —         —          —        ← locked read-only
//  111   111     —         —         —          —        ← no access
//
// Trailer block erişim koşulları (C1C2C3 bit 3):
//
//  bits  KeyA R  KeyA W  Access R  Access W  KeyB R  KeyB W
//  000   —       KeyA    KeyA      —         KeyA    KeyA   ← transport default
//  001   —       —       KeyA      KeyA      KeyA    KeyA
//  010   —       KeyB    KeyA/B    —         —       KeyB
//  011   —       —       KeyA/B    KeyB      —       —
//  100   —       KeyB    KeyA/B    —         KeyB    KeyB
//  101   —       —       KeyA/B    KeyB      KeyB    —
//  110   —       —       KeyA/B    —         —       —      ← read-only, locked
//  111   —       —       KeyA/B    —         —       —      ← read-only, locked
//
// ════════════════════════════════════════════════════════════════════════════════

struct DataBlockCondition {
    uint8_t     bits;       // C1C2C3 (0-7)
    std::string read;
    std::string write;
    std::string incr;
    std::string decr;
    std::string label;      // kısa açıklama
};

struct TrailerCondition {
    uint8_t     bits;
    std::string keyARd;
    std::string keyAWr;
    std::string accessRd;
    std::string accessWr;
    std::string keyBRd;
    std::string keyBWr;
    std::string label;
};

class AccessBitsHelper {
public:

    // ── Data block koşul tablosu ─────────────────────────────────────────────
    static const std::vector<DataBlockCondition>& dataConditions() {
        static const std::vector<DataBlockCondition> table = {
            {0b000, "A/B",  "A/B",  "A/B", "A/B",  "transport default (R/W all)"},
            {0b001, "A/B",  "B",    "—",   "A/B",  "KeyB write (decrementable)"},
            {0b010, "A/B",  "—",    "—",   "—",    "read-only"},
            {0b011, "B",    "B",    "—",   "—",    "KeyB only"},
            {0b100, "A/B",  "B",    "—",   "—",    "KeyB write"},
            {0b101, "B",    "—",    "—",   "—",    "KeyB read-only"},
            {0b110, "A/B",  "—",    "—",   "—",    "locked read-only"},
            {0b111, "—",    "—",    "—",   "—",    "no access"},
        };
        return table;
    }

    // ── Trailer koşul tablosu ────────────────────────────────────────────────
    static const std::vector<TrailerCondition>& trailerConditions() {
        static const std::vector<TrailerCondition> table = {
            {0b000, "—","A", "A","—",  "A","A",  "transport (KeyA r/w, KeyB r/w)"},
            {0b001, "—","—", "A","A",  "A","A",  "access writable"},
            {0b010, "—","B", "A/B","—","—","B",  "KeyB write only"},
            {0b011, "—","—", "A/B","B","—","—",  "access+KeyB writable by B"},
            {0b100, "—","B", "A/B","—","B","B",  "KeyB R/W"},
            {0b101, "—","—", "A/B","B","B","—",  "KeyB read, access writable"},
            {0b110, "—","—", "A/B","—","—","—",  "locked (read-only access)"},
            {0b111, "—","—", "A/B","—","—","—",  "locked (read-only access)"},
        };
        return table;
    }

    // ── C1C2C3 bits → BlockAccess ────────────────────────────────────────────
    static BlockAccess fromBits(uint8_t bits) {
        return {
            static_cast<uint8_t>((bits >> 0) & 1),
            static_cast<uint8_t>((bits >> 1) & 1),
            static_cast<uint8_t>((bits >> 2) & 1)
        };
    }

    // ── Tablo yazdır ─────────────────────────────────────────────────────────
    static void printDataTable(std::ostream& out = std::cout) {
        out << "\nData Block Access Conditions (C1C2C3):\n"
            << "  idx  C1C2C3  Read   Write  Incr   Decr   Description\n"
            << "  ---  ------  -----  -----  -----  -----  -----------\n";
        for (auto& c : dataConditions()) {
            out << "  [" << (int)c.bits << "]  "
                << (int)((c.bits>>2)&1) << (int)((c.bits>>1)&1) << (int)(c.bits&1)
                << "     "
                << pad(c.read,5) << "  "
                << pad(c.write,5) << "  "
                << pad(c.incr,5) << "  "
                << pad(c.decr,5) << "  "
                << c.label << "\n";
        }
        out << "\n";
    }

    static void printTrailerTable(std::ostream& out = std::cout) {
        out << "\nTrailer Block Access Conditions (C1C2C3 bit 3):\n"
            << "  idx  KeyA-R  KeyA-W  Access-R  Access-W  KeyB-R  KeyB-W  Description\n"
            << "  ---  ------  ------  --------  --------  ------  ------  -----------\n";
        for (auto& c : trailerConditions()) {
            out << "  [" << (int)c.bits << "]  "
                << pad(c.keyARd,6) << "  "
                << pad(c.keyAWr,6) << "  "
                << pad(c.accessRd,8) << "  "
                << pad(c.accessWr,8) << "  "
                << pad(c.keyBRd,6) << "  "
                << pad(c.keyBWr,6) << "  "
                << c.label << "\n";
        }
        out << "\n";
    }

    // ── SectorAccess + GPB → hex string (8 chars) ────────────────────────────
    static std::string toHex(const SectorAccess& sa, uint8_t gpb = 0x00) {
        auto ab = TrailerFormat::encodeAccess(sa, gpb);
        return TrailerFormat::accessToHex(ab);
    }

    // ── Interactive mode: kullanıcıdan block koşullarını seç ─────────────────
    // promptFn: (prompt) → string
    static SectorAccess interactiveSelect(
        std::function<std::string(const std::string&)> promptFn)
    {
        SectorAccess sa;

        printDataTable();
        for (int i = 0; i < 3; ++i) {
            while (true) {
                std::string input = promptFn(
                    "Data block " + std::to_string(i) + " condition [0-7]: ");
                try {
                    int v = std::stoi(input);
                    if (v >= 0 && v <= 7) {
                        sa.data[i] = fromBits(static_cast<uint8_t>(v));
                        break;
                    }
                } catch (...) {}
                std::cout << "  Invalid — enter 0-7\n";
            }
        }

        printTrailerTable();
        while (true) {
            std::string input = promptFn("Trailer block condition [0-7]: ");
            try {
                int v = std::stoi(input);
                if (v >= 0 && v <= 7) {
                    sa.trailer = fromBits(static_cast<uint8_t>(v));
                    break;
                }
            } catch (...) {}
            std::cout << "  Invalid — enter 0-7\n";
        }

        return sa;
    }

    // ── SectorAccess → human-readable summary ────────────────────────────────
    static void printSummary(const SectorAccess& sa, std::ostream& out = std::cout) {
        out << "Access condition summary:\n";
        for (int i = 0; i < 3; ++i) {
            uint8_t bits = sa.data[i].bits();
            out << "  Data block " << i << " [" << (int)bits << "]: ";
            if (bits < dataConditions().size())
                out << dataConditions()[bits].label;
            out << "\n";
        }
        {
            uint8_t bits = sa.trailer.bits();
            out << "  Trailer    [" << (int)bits << "]: ";
            if (bits < trailerConditions().size())
                out << trailerConditions()[bits].label;
            out << "\n";
        }
    }

private:
    static std::string pad(const std::string& s, size_t w) {
        if (s.size() >= w) return s;
        return s + std::string(w - s.size(), ' ');
    }
};
