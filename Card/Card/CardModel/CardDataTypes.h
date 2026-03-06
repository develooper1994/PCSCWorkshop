#ifndef CARDDATATYPES_H
#define CARDDATATYPES_H

#include <cstdint>
#include <array>
#include <vector>

// ════════════════════════════════════════════════════════════════════════════════
// Minimal Type Definitions - Pure Data, Zero Logic
// ════════════════════════════════════════════════════════════════════════════════
// 
// Current: Mifare Classic 1K/4K only
// Future: DESFire, Ultralight (separate CardDataTypes if needed)
//

// Fundamental types
using BYTE = std::uint8_t;
using WORD = std::uint16_t;
using DWORD = std::uint32_t;

// Collections
using BYTEV = std::vector<BYTE>;
using KEYBYTES = std::array<BYTE, 6>;      // Mifare key = 6 bytes
using BLOCK = std::array<BYTE, 16>;        // Block = 16 bytes
using ACCESSBYTES = std::array<BYTE, 4>;   // Access bits = 4 bytes (C1 C2 C3 GPB)

// ════════════════════════════════════════════════════════════════════════════════
// Enumerations
// ════════════════════════════════════════════════════════════════════════════════

enum class CardType {
    MifareClassic1K,
    MifareClassic4K,
    DESFire,           // Future
    Ultralight         // Future
};

enum class KeyType {
    A,
    B
};

enum class KeyStructure {
    Volatile,
    NonVolatile
};

enum class CardBlockKind {
    Manufacturer,   // Block 0, read-only
    Data,           // Regular data blocks
    Trailer         // Sector trailer (keys + access bits)
};

enum class Permission {
    None       = 0b00,
    Read       = 0b01,
    Write      = 0b10,
    ReadWrite  = 0b11
};

#endif // CARDDATATYPES_H
