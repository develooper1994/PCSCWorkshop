#ifndef SECTORDEFINITION_H
#define SECTORDEFINITION_H

#include "CardDataTypes.h"
#include <array>

// ════════════════════════════════════════════════════════════════════════════════
// SectorDefinition - What is a Sector
// ════════════════════════════════════════════════════════════════════════════════
//
// A sector is a grouping of sequential blocks with shared authentication.
// Mifare Classic structure:
// - 1K card: 16 sectors × 4 blocks = 64 blocks
// - 4K card: 32 sectors × 4 blocks + 8 sectors × 16 blocks = 256 blocks
//
// Each sector has:
// - Data blocks (first 3 or 15)
// - Trailer block (last 1, contains keys + access bits)
//
// Authentication is per-sector: once authenticated to a sector with a key,
// that key can be used for all operations in that sector.
//
// ════════════════════════════════════════════════════════════════════════════════

// Information about a single sector
struct SectorInfo {
    int index;              // Sector number (0-15 for 1K, 0-39 for 4K)
    int blockCount;         // 4 for normal, 16 for extended (4K only)
    int firstBlock;         // Block number of first block
    int lastBlock;          // Block number of last (trailer) block
    bool isExtended;        // true for 4K extended sectors (32-39)

    // Constructor
    SectorInfo() = default;
    SectorInfo(int idx, int cnt, int first, int last, bool extended)
        : index(idx), blockCount(cnt), firstBlock(first), lastBlock(last), 
          isExtended(extended) {
    }
};

#endif // SECTORDEFINITION_H
