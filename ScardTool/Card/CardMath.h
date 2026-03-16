/** @file CardMath.h
 * @brief Card-agnostic sector/block calculations (Classic1K/4K/Ultralight). */
#pragma once
#include "CardDataTypes.h"
#include <vector>
#include <stdexcept>
#include <string>

// ════════════════════════════════════════════════════════════════════════════════
// CardMath — kart tipi bağımsız sector/block hesapları
//
// Desteklenen tipler:
//   MifareClassic1K  — 16 sector × 4 block  = 64 block  (0..63)
//   MifareClassic4K  — 32 sector × 4 block
//                    +  8 sector × 16 block = 256 block (0..255)
//   MifareUltralight — 16 page × 4 byte     (sector kavramı yok, page=block)
//   MifareDesfire    — uygulama tabanlı      (sector kavramı yok)
//
// Terim sözlüğü:
//   block  : reader'ın tek seferde okuduğu atom (16 byte Classic, 4 byte UL)
//   page   : Ultralight'ta block ile eşdeğer
//   sector : birden fazla block'un grubu (Classic'e özgü)
//   trailer: sektörün son bloğu (key + access bits)
// ════════════════════════════════════════════════════════════════════════════════

class CardMath {
public:

    // ── Sector count ─────────────────────────────────────────────────────────
    static int sectorCount(CardType t) {
        switch (t) {
            case CardType::MifareClassic1K:   return 16;
            case CardType::MifareClassic4K:   return 40;
            case CardType::MifareUltralight:  return 1;  // tek "sector"
            default: return 0;
        }
    }

    // ── Blocks per sector ────────────────────────────────────────────────────
    // Classic4K: sector 0-31 → 4 block, sector 32-39 → 16 block
    static int blocksInSector(int sector, CardType t) {
        switch (t) {
            case CardType::MifareClassic1K:
                return 4;
            case CardType::MifareClassic4K:
                return sector < 32 ? 4 : 16;
            case CardType::MifareUltralight:
                return 16; // 16 page
            default:
                return 0;
        }
    }

    // ── First block address of sector ────────────────────────────────────────
    static int sectorFirstBlock(int sector, CardType t) {
        switch (t) {
            case CardType::MifareClassic1K:
                return sector * 4;
            case CardType::MifareClassic4K:
                if (sector < 32) return sector * 4;
                return 128 + (sector - 32) * 16;
            case CardType::MifareUltralight:
                return 0;
            default:
                return -1;
        }
    }

    // ── Trailer block address ────────────────────────────────────────────────
    static int trailerBlock(int sector, CardType t) {
        int first = sectorFirstBlock(sector, t);
        if (first < 0) return -1;
        return first + blocksInSector(sector, t) - 1;
    }

    // ── All block addresses in sector ────────────────────────────────────────
    static std::vector<int> sectorBlocks(int sector, CardType t) {
        int first = sectorFirstBlock(sector, t);
        int count = blocksInSector(sector, t);
        if (first < 0 || count == 0) return {};
        std::vector<int> result;
        result.reserve(count);
        for (int i = 0; i < count; ++i) result.push_back(first + i);
        return result;
    }

    // ── Block → sector ───────────────────────────────────────────────────────
    static int blockToSector(int block, CardType t) {
        switch (t) {
            case CardType::MifareClassic1K:
                return block / 4;
            case CardType::MifareClassic4K:
                if (block < 128) return block / 4;
                return 32 + (block - 128) / 16;
            case CardType::MifareUltralight:
                return 0;
            default:
                return -1;
        }
    }

    // ── Block kind ───────────────────────────────────────────────────────────
    static CardBlockKind blockKind(int block, CardType t) {
        if (block == 0 && t != CardType::MifareUltralight)
            return CardBlockKind::Manufacturer;
        int sector = blockToSector(block, t);
        if (sector < 0) return CardBlockKind::Unknown;
        if (block == trailerBlock(sector, t))
            return CardBlockKind::Trailer;
        return CardBlockKind::Data;
    }

    // ── Is trailer ───────────────────────────────────────────────────────────
    static bool isTrailer(int block, CardType t) {
        return blockKind(block, t) == CardBlockKind::Trailer;
    }

    // ── Validate ─────────────────────────────────────────────────────────────
    static bool validBlock(int block, CardType t) {
        switch (t) {
            case CardType::MifareClassic1K:   return block >= 0 && block < 64;
            case CardType::MifareClassic4K:   return block >= 0 && block < 256;
            case CardType::MifareUltralight:  return block >= 0 && block < 16;
            default: return false;
        }
    }

    static bool validSector(int sector, CardType t) {
        return sector >= 0 && sector < sectorCount(t);
    }

    // ── Block size (bytes) ────────────────────────────────────────────────────
    static int blockSize(CardType t) {
        switch (t) {
            case CardType::MifareUltralight: return 4;
            default: return 16;  // Classic / DESFire blok = 16
        }
    }

    // ── CardType from string ──────────────────────────────────────────────────
    static CardType fromString(const std::string& s) {
        if (s == "classic1k" || s == "1k") return CardType::MifareClassic1K;
        if (s == "classic4k" || s == "4k") return CardType::MifareClassic4K;
        if (s == "ultralight" || s == "ul") return CardType::MifareUltralight;
        if (s == "desfire")                 return CardType::MifareDesfire;
        return CardType::Unknown;
    }

    static std::string toString(CardType t) {
        switch (t) {
            case CardType::MifareClassic1K:  return "classic1k";
            case CardType::MifareClassic4K:  return "classic4k";
            case CardType::MifareUltralight: return "ultralight";
            case CardType::MifareDesfire:    return "desfire";
            default: return "unknown";
        }
    }
};
