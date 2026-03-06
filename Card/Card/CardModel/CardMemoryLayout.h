#ifndef CARDMEMORYLAYOUT_H
#define CARDMEMORYLAYOUT_H

#include "CardDataTypes.h"
#include "BlockDefinition.h"
#include <array>
#include <cstring>

// ════════════════════════════════════════════════════════════════════════════════
// CardMemoryLayout - Zero-Copy Union for Complete Card Memory
// ════════════════════════════════════════════════════════════════════════════════
//
// Single memory region with multiple interpretations:
// 1. Raw bytes (byte-level access)
// 2. Blocks (block-level access, 16-byte chunks)
// 3. Sectors (sector-level access with grouped blocks)
//
// All views share same memory - zero-copy interpretation.
//
// Example (1K):
// ┌─ Raw View ─────────────────────────┐
// │ [0...1023] BYTE raw[1024];         │
// ├─ Block View ───────────────────────┤
// │ blocks[0]  = raw[0...15]           │
// │ blocks[1]  = raw[16...31]          │
// │ ...                                 │
// │ blocks[63] = raw[1008...1023]      │
// ├─ Sector View ──────────────────────┤
// │ sectors[0] = blocks[0...3]         │
// │ sectors[1] = blocks[4...7]         │
// │ ...                                 │
// │ sectors[15] = blocks[60...63]      │
// └────────────────────────────────────┘
//
// ════════════════════════════════════════════════════════════════════════════════

// ────────────────────────────────────────────────────────────────────────────
// 1K Card Memory Layout
// ────────────────────────────────────────────────────────────────────────────

struct Card1KMemoryLayout {
    union {
        // View 1: Raw bytes (complete memory)
        BYTE raw[1024];

        // View 2: Blocks (64 blocks × 16 bytes each)
        MifareBlock blocks[64];

        // View 3: Sectors (16 sectors × 4 blocks each)
        struct {
            MifareBlock block[4];  // 4 blocks per sector
        } sectors[16];

        // View 4: Detailed sector structure
        struct {
            struct {
                MifareBlock dataBlock0;      // Block 0 (data)
                MifareBlock dataBlock1;      // Block 1 (data)
                MifareBlock dataBlock2;      // Block 2 (data)
                MifareBlock trailerBlock;    // Block 3 (trailer: keys + access bits)
            } sector[16];
        } detailed;
    };

    // Constructors
    Card1KMemoryLayout() {
        std::memset(raw, 0, 1024);
    }

    Card1KMemoryLayout(const BYTE* dataPtr) {
        std::memcpy(raw, dataPtr, 1024);
    }

    // Size info
    static constexpr size_t MEMORY_SIZE = 1024;
    static constexpr int TOTAL_BLOCKS = 64;
    static constexpr int TOTAL_SECTORS = 16;
    static constexpr int BLOCKS_PER_SECTOR = 4;
};

// ────────────────────────────────────────────────────────────────────────────
// 4K Card Memory Layout
// ────────────────────────────────────────────────────────────────────────────

struct Card4KMemoryLayout {
    union {
        // View 1: Raw bytes (complete memory)
        BYTE raw[4096];

        // View 2: Blocks (256 blocks × 16 bytes each)
        MifareBlock blocks[256];

        // View 3: Sectors (mixed: 32 normal + 8 extended)
        struct {
            struct {
                MifareBlock block[4];   // Normal: 4 blocks
            } normal[32];

            struct {
                MifareBlock block[16];  // Extended: 16 blocks
            } extended[8];
        } sectors;

        // View 4: Detailed sector structure (normal sectors)
        struct {
            struct {
                MifareBlock dataBlock0;      // Block 0 (data)
                MifareBlock dataBlock1;      // Block 1 (data)
                MifareBlock dataBlock2;      // Block 2 (data)
                MifareBlock trailerBlock;    // Block 3 (trailer)
            } sector[32];
        } detailedNormal;

        // View 5: Detailed sector structure (extended sectors)
        struct {
            struct {
                MifareBlock dataBlocks[15];  // Blocks 0-14 (data)
                MifareBlock trailerBlock;    // Block 15 (trailer)
            } sector[8];
        } detailedExtended;
    };

    // Constructors
    Card4KMemoryLayout() {
        std::memset(raw, 0, 4096);
    }

    Card4KMemoryLayout(const BYTE* dataPtr) {
        std::memcpy(raw, dataPtr, 4096);
    }

    // Size info
    static constexpr size_t MEMORY_SIZE = 4096;
    static constexpr int TOTAL_BLOCKS = 256;
    static constexpr int TOTAL_SECTORS = 40;
    static constexpr int NORMAL_SECTORS = 32;
    static constexpr int EXTENDED_SECTORS = 8;
};

// ════════════════════════════════════════════════════════════════════════════════
// CardMemoryLayout - Type-Safe Wrapper
// ════════════════════════════════════════════════════════════════════════════════
//
// Union-based memory layout for either 1K or 4K card.
// Discriminant (is4K) determines which layout is active.
//
// ════════════════════════════════════════════════════════════════════════════════

struct CardMemoryLayout {
    union {
        Card1KMemoryLayout card1K;
        Card4KMemoryLayout card4K;
    } data;

    bool is4K;

    // Constructors
    CardMemoryLayout() : data{}, is4K(false) {
        std::memset(data.card1K.raw, 0, 1024);
    }

    explicit CardMemoryLayout(bool is4KCard) : data{}, is4K(is4KCard) {
        if (is4K) {
            std::memset(data.card4K.raw, 0, 4096);
        } else {
            std::memset(data.card1K.raw, 0, 1024);
        }
    }

    // Get total memory size
    size_t memorySize() const {
        return is4K ? 4096 : 1024;
    }

    // Get pointer to raw memory
    BYTE* getRawMemory() {
        return is4K ? data.card4K.raw : data.card1K.raw;
    }

    const BYTE* getRawMemory() const {
        return is4K ? data.card4K.raw : data.card1K.raw;
    }

    // Get block by index
    MifareBlock& getBlock(int index) {
        if (is4K) {
            return data.card4K.blocks[index];
        } else {
            return data.card1K.blocks[index];
        }
    }

    const MifareBlock& getBlock(int index) const {
        if (is4K) {
            return data.card4K.blocks[index];
        } else {
            return data.card1K.blocks[index];
        }
    }
};

#endif // CARDMEMORYLAYOUT_H
