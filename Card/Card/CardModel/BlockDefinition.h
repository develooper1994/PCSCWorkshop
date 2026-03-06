#ifndef BLOCKDEFINITION_H
#define BLOCKDEFINITION_H

#include "CardDataTypes.h"
#include <cstring>

// ════════════════════════════════════════════════════════════════════════════════
// Block Definition - Zero-Copy Union for Different Block Types
// ════════════════════════════════════════════════════════════════════════════════
//
// A Mifare Classic block is always 16 bytes. Different block types interpret
// this memory differently:
// - Manufacturer block (0): UID + BCC + data
// - Data block: Raw 16 bytes
// - Trailer block: KeyA (6) + Access (4) + KeyB (6)
//
// Using union provides zero-copy access - same memory, different views.
//
// ════════════════════════════════════════════════════════════════════════════════

struct MifareBlock {
    // Anonymous union for different block interpretations
    union {
        BYTE raw[16];                           // Raw 16-byte access

        struct {
            BYTE data[16];                      // Generic data block
        } data;

        struct {
            BYTE uid[4];                        // Unique ID
            BYTE bcc;                           // Block Check Character
            BYTE manufacturerData[11];          // Vendor-specific
        } manufacturer;

        struct {
            BYTE keyA[6];                       // Authentication Key A
            BYTE accessBits[4];                 // C1 C2 C3 + GPB
            BYTE keyB[6];                       // Authentication Key B
        } trailer;
    };

    // ────────────────────────────────────────────────────────────────────────────
    // Constructors
    // ────────────────────────────────────────────────────────────────────────────

    // Default: all zeros
    MifareBlock() {
        std::memset(raw, 0, 16);
    }

    // From raw array
    explicit MifareBlock(const BYTE data[16]) {
        std::memcpy(raw, data, 16);
    }

    // From BLOCK array
    explicit MifareBlock(const BLOCK& block) {
        std::memcpy(raw, block.data(), 16);
    }

    // From BYTEV (if size == 16)
    explicit MifareBlock(const BYTEV& vec) {
        if (vec.size() != 16) {
            throw std::invalid_argument("Block must be 16 bytes");
        }
        std::memcpy(raw, vec.data(), 16);
    }

    // ────────────────────────────────────────────────────────────────────────────
    // Conversion Operators
    // ────────────────────────────────────────────────────────────────────────────

    // Convert to BLOCK
    BLOCK toBlock() const {
        BLOCK b;
        std::memcpy(b.data(), raw, 16);
        return b;
    }

    // Convert to BYTEV
    BYTEV toVector() const {
        return BYTEV(raw, raw + 16);
    }

    // ────────────────────────────────────────────────────────────────────────────
    // Comparison
    // ────────────────────────────────────────────────────────────────────────────

    bool operator==(const MifareBlock& other) const {
        return std::memcmp(raw, other.raw, 16) == 0;
    }

    bool operator!=(const MifareBlock& other) const {
        return !(*this == other);
    }

    // ────────────────────────────────────────────────────────────────────────────
    // Metadata & Validation
    // ────────────────────────────────────────────────────────────────────────────

    // Check if block is all zeros
    bool isEmpty() const {
        for (int i = 0; i < 16; ++i) {
            if (raw[i] != 0x00) return false;
        }
        return true;
    }

    // Check if block is all 0xFF
    bool isAllFF() const {
        for (int i = 0; i < 16; ++i) {
            if (raw[i] != 0xFF) return false;
        }
        return true;
    }

    // Get size (always 16)
    constexpr size_t size() const { return 16; }

    // Pointer to raw data
    const BYTE* data() const { return raw; }
    BYTE* data() { return raw; }
};

#endif // BLOCKDEFINITION_H
