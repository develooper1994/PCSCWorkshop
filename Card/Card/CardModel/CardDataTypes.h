#ifndef CARDDATATYPES_MODEL_H
#define CARDDATATYPES_MODEL_H

// ════════════════════════════════════════════════════════════════════════════════
// CardModel Data Types
// ════════════════════════════════════════════════════════════════════════════════
// Re-exports the canonical Card/Card/CardDataTypes.h so that every Card
// subsystem uses the SAME type definitions (no redefinition conflicts).

#include "../CardDataTypes.h"     // canonical: BYTE, BYTEV, KEYBYTES, KeyType, ...

// Additional permission enum (not in the base header)
enum class Permission {
    None       = 0b00,
    Read       = 0b01,
    Write      = 0b10,
    ReadWrite  = 0b11
};

#endif // CARDDATATYPES_MODEL_H
