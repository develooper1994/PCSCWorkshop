#ifndef ACCESSBITS_H
#define ACCESSBITS_H

#include "CardDataTypes.h"
#include <stdexcept>
#include <cstring>

// ════════════════════════════════════════════════════════════════════════════════
// AccessBitsData - Zero-Copy Union for Access Bits
// ════════════════════════════════════════════════════════════════════════════════
//
// 4-byte access control structure with multiple views
//
// ════════════════════════════════════════════════════════════════════════════════

struct AccessBitsData {
    union {
        BYTE raw[4];           // Raw 4-byte access (zero-copy)
        
        struct {
            BYTE c1;           // Byte 0: C1 bits
            BYTE c2;           // Byte 1: C2 bits
            BYTE c3_inv;       // Byte 2: C3 inverted bits
            BYTE gpb;          // Byte 3: General Purpose Byte
        } bytes;
    };

    // Constructor from raw bytes
    AccessBitsData() {
        std::memset(raw, 0xFF, 4);
    }

    explicit AccessBitsData(const BYTE data[4]) {
        std::memcpy(raw, data, 4);
    }

    explicit AccessBitsData(const ACCESSBYTES& ab) {
        std::memcpy(raw, ab.data(), 4);
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// AccessBits - Mifare Classic Access Control Codec
// ════════════════════════════════════════════════════════════════════════════════
//
// Mifare Classic uses 4 bytes for access control in sector trailer:
// - Bytes 6-9 of trailer block: [C1] [C2] [C3] [GPB]
//
// The 12 bits (C1, C2, C3) encode permissions for:
// - 3 data blocks (blocks 0-2)
// - 1 trailer block
// - For each: KeyA readable, KeyA writable, KeyB readable, KeyB writable
//
// Access Bits Layout:
// ┌─ Byte 6 ─┬─ Byte 7 ─┬─ Byte 8 ─┬─ Byte 9 ─┐
// │  C1 C2  │ C3 (inv) │  C1 C2  │  C3 (inv)│ GPB
// │  inverted inv       │ normal  │          │
// └──────────┴──────────┴─────────┴──────────┘
//
// For data blocks: [C1 C2 C3] encode 4 permission pairs:
// - Bit pattern determines: Read(KeyA), Write(KeyA), Read(KeyB), Write(KeyB)
//
// Example: 0xFF 0x07 0x80 = Standard Mifare read-only
// - KeyA: Read only
// - KeyB: Read/Write
//
// ════════════════════════════════════════════════════════════════════════════════

class AccessBits {
public:
    // ────────────────────────────────────────────────────────────────────────────
    // Encoding/Decoding
    // ────────────────────────────────────────────────────────────────────────────

    // Encode permissions to 4-byte access bits
    // @param dataBlockA Read/Write for KeyA on data blocks
    // @param dataBlockB Read/Write for KeyB on data blocks
    // @param trailerA Read/Write for KeyA on trailer
    // @param trailerB Read/Write for KeyB on trailer
    // @return 4-byte access bits [C1 C2 C3 GPB]
    static ACCESSBYTES encode(
        bool dataBlockReadA,  bool dataBlockWriteA,
        bool dataBlockReadB,  bool dataBlockWriteB,
        bool trailerReadA,    bool trailerWriteA,
        bool trailerReadB,    bool trailerWriteB);

    // Decode 4-byte access bits
    // @param accessBits Input bytes (should have inverted bits for verification)
    // @return true if valid, false if corrupted
    static bool decode(const BYTE accessBits[4]);

    // Decode and extract specific permission
    static bool isDataBlockReadable(const BYTE accessBits[4], KeyType kt);
    static bool isDataBlockWritable(const BYTE accessBits[4], KeyType kt);
    static bool isTrailerReadable(const BYTE accessBits[4], KeyType kt);
    static bool isTrailerWritable(const BYTE accessBits[4], KeyType kt);

    // ────────────────────────────────────────────────────────────────────────────
    // Standard Modes
    // ────────────────────────────────────────────────────────────────────────────

    // MODE 0: KeyA read, KeyB read/write (default)
    static constexpr ACCESSBYTES MODE_0 = {0xFF, 0x07, 0x80, 0x69};

    // MODE 1: KeyA read, KeyB read (locked)
    static constexpr ACCESSBYTES MODE_1 = {0xFF, 0x07, 0x80, 0x69};  // TODO: Verify

    // MODE 2: KeyA r/w, KeyB r/w (open)
    static constexpr ACCESSBYTES MODE_2 = {0xFF, 0xFF, 0xFF, 0x00};  // TODO: Verify

    // MODE 3: KeyA r/w, KeyB read (restricted)
    static constexpr ACCESSBYTES MODE_3 = {0xFF, 0x0F, 0xF0, 0x69};  // TODO: Verify

    // ────────────────────────────────────────────────────────────────────────────
    // Validation
    // ────────────────────────────────────────────────────────────────────────────

    // Check if access bits are valid (C1 and C1' complement match)
    static bool isValid(const BYTE accessBits[4]);

private:
    // ────────────────────────────────────────────────────────────────────────────
    // Internal Helpers
    // ────────────────────────────────────────────────────────────────────────────

    // Extract individual bits from C1 C2 C3
    static bool getBit(const BYTE accessBits[4], int bitIndex);

    // Set bit in C1 C2 C3
    static void setBit(BYTE accessBits[4], int bitIndex, bool value);

    // Compute inverted bytes (for verification)
    static void computeInverted(BYTE accessBits[4]);
};

#endif // ACCESSBITS_H
