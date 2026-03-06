#ifndef KEYMANAGEMENT_H
#define KEYMANAGEMENT_H

#include "../CardDataTypes.h"
#include <map>
#include <vector>
#include <stdexcept>

// Forward declares
struct CardMemoryLayout;
struct MifareBlock;

// ════════════════════════════════════════════════════════════════════════════════
// KeyManagement - Key Storage and Query System
// ════════════════════════════════════════════════════════════════════════════════
//
// Manages host-side keys (keys the reader/driver has).
// These are used for authentication to card sectors.
//
// Key Storage Structure:
// - Multiple keys per type (KeyA, KeyB)
// - Each key has a slot number (0x01, 0x02, etc.)
// - Keys can be volatile or non-volatile
//
// Usage:
// 1. Register keys from reader/driver
// 2. Query keys for authentication
// 3. Validate keys before use
//
// Note: This manages HOST keys, not card trailer keys.
// For card trailer keys, use CardMemoryLayout to read them directly.
//
// ════════════════════════════════════════════════════════════════════════════════

// Use the canonical KeyInfo from CardDataTypes.h (already included via CardModel).
// Map fields: key, ks (KeyStructure), slot, name.
// Helper constructor to keep KeyManagement call-sites unchanged:
inline KeyInfo makeKeyInfo(const KEYBYTES& k, KeyStructure s, BYTE slotNum, const std::string& n = "") {
    KeyInfo info;
    info.key  = k;
    info.ks   = s;
    info.slot = slotNum;
    info.name = n;
    return info;
}

class KeyManagement {
public:
    // ────────────────────────────────────────────────────────────────────────────
    // Constructor
    // ────────────────────────────────────────────────────────────────────────────

    // Create key manager with card memory layout reference
    // (for reading card trailer keys if needed)
    explicit KeyManagement(const CardMemoryLayout& cardMemory);

    // ────────────────────────────────────────────────────────────────────────────
    // Key Registration
    // ────────────────────────────────────────────────────────────────────────────

    // Register a key (host-side)
    void registerKey(KeyType kt, const KEYBYTES& key, KeyStructure structure,
                     BYTE slot, const std::string& name = "");

    // Register from raw pointer
    void registerKey(KeyType kt, const BYTE key[6], KeyStructure structure,
                     BYTE slot, const std::string& name = "");

    // ────────────────────────────────────────────────────────────────────────────
    // Key Queries
    // ────────────────────────────────────────────────────────────────────────────

    // Get key by type and slot
    const KEYBYTES& getKey(KeyType kt, BYTE slot) const;

    // Get full key info
    const KeyInfo& getKeyInfo(KeyType kt, BYTE slot) const;

    // Check if key is registered
    bool hasKey(KeyType kt, BYTE slot) const;
    bool hasKey(KeyType kt) const;

    // Get default key for type (first registered)
    const KEYBYTES& getDefaultKey(KeyType kt) const;

    // ────────────────────────────────────────────────────────────────────────────
    // Key Listing
    // ────────────────────────────────────────────────────────────────────────────

    // Get all slots for a key type
    std::vector<BYTE> getSlots(KeyType kt) const;

    // Get all keys (both types)
    std::vector<KeyInfo> getAllKeys() const;

    // Get all keys of one type
    std::vector<KeyInfo> getKeys(KeyType kt) const;

    // ────────────────────────────────────────────────────────────────────────────
    // Key Validation
    // ────────────────────────────────────────────────────────────────────────────

    // Validate key format (not empty, not all 0xFF, etc.)
    static bool isValidKey(const KEYBYTES& key);

    // Check if key matches card trailer key
    bool matches(int sector, KeyType kt, const KEYBYTES& key) const;

    // ────────────────────────────────────────────────────────────────────────────
    // Card Trailer Key Access (zero-copy from CardMemoryLayout)
    // ────────────────────────────────────────────────────────────────────────────

    // Get KeyA from sector trailer (direct from memory)
    KEYBYTES getCardKeyA(int sector) const;

    // Get KeyB from sector trailer (direct from memory)
    KEYBYTES getCardKeyB(int sector) const;

    // Get key from trailer by type
    KEYBYTES getCardKey(int sector, KeyType kt) const;

    // ────────────────────────────────────────────────────────────────────────────
    // Introspection
    // ────────────────────────────────────────────────────────────────────────────

    // Describe key
    std::string describeKey(KeyType kt, BYTE slot) const;

    // List all registered keys
    void printAllKeys() const;

    // ────────────────────────────────────────────────────────────────────────────
    // Clearing
    // ────────────────────────────────────────────────────────────────────────────

    // Clear all keys for a type
    void clearKeys(KeyType kt);

    // Clear all keys
    void clearAllKeys();

private:
    const CardMemoryLayout& cardMemory_;

    // Storage: KeyType -> (Slot -> KeyInfo)
    std::map<KeyType, std::map<BYTE, KeyInfo>> keys_;

    // ────────────────────────────────────────────────────────────────────────────
    // Internal Helpers
    // ────────────────────────────────────────────────────────────────────────────

    // Helper: get trailer block for sector
    const MifareBlock& getTrailer(int sector) const;

    // Helper: find first registered slot for key type
    BYTE getDefaultSlot(KeyType kt) const;
};

#endif // KEYMANAGEMENT_H
