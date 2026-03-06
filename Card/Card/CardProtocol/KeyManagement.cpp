#include "KeyManagement.h"
#include "../CardModel/CardMemoryLayout.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <sstream>

// ════════════════════════════════════════════════════════════════════════════════
// Construction
// ════════════════════════════════════════════════════════════════════════════════

KeyManagement::KeyManagement(const CardMemoryLayout& cardMemory)
    : cardMemory_(cardMemory) {
}

// ════════════════════════════════════════════════════════════════════════════════
// Key Registration
// ════════════════════════════════════════════════════════════════════════════════

void KeyManagement::registerKey(KeyType kt, const KEYBYTES& key,
                                KeyStructure structure, BYTE slot,
                                const std::string& name) {
    if (!isValidKey(key)) {
        throw std::invalid_argument("Invalid key format");
    }
    
    keys_[kt][slot] = makeKeyInfo(key, structure, slot, name);
}

void KeyManagement::registerKey(KeyType kt, const BYTE key[6],
                                KeyStructure structure, BYTE slot,
                                const std::string& name) {
    KEYBYTES keyBytes;
    std::copy(key, key + 6, keyBytes.begin());
    registerKey(kt, keyBytes, structure, slot, name);
}

// ════════════════════════════════════════════════════════════════════════════════
// Key Queries
// ════════════════════════════════════════════════════════════════════════════════

const KEYBYTES& KeyManagement::getKey(KeyType kt, BYTE slot) const {
    auto ktIt = keys_.find(kt);
    if (ktIt == keys_.end()) {
        throw std::runtime_error("Key type not found");
    }
    
    auto slotIt = ktIt->second.find(slot);
    if (slotIt == ktIt->second.end()) {
        throw std::runtime_error("Slot not found for key type");
    }
    
    return slotIt->second.key;
}

const KeyInfo& KeyManagement::getKeyInfo(KeyType kt, BYTE slot) const {
    auto ktIt = keys_.find(kt);
    if (ktIt == keys_.end()) {
        throw std::runtime_error("Key type not found");
    }
    
    auto slotIt = ktIt->second.find(slot);
    if (slotIt == ktIt->second.end()) {
        throw std::runtime_error("Slot not found for key type");
    }
    
    return slotIt->second;
}

bool KeyManagement::hasKey(KeyType kt, BYTE slot) const {
    auto ktIt = keys_.find(kt);
    if (ktIt == keys_.end()) return false;
    return ktIt->second.find(slot) != ktIt->second.end();
}

bool KeyManagement::hasKey(KeyType kt) const {
    return keys_.find(kt) != keys_.end();
}

const KEYBYTES& KeyManagement::getDefaultKey(KeyType kt) const {
    auto ktIt = keys_.find(kt);
    if (ktIt == keys_.end()) {
        throw std::runtime_error("Key type not found");
    }
    
    BYTE defaultSlot = getDefaultSlot(kt);
    return ktIt->second.at(defaultSlot).key;
}

// ════════════════════════════════════════════════════════════════════════════════
// Key Listing
// ════════════════════════════════════════════════════════════════════════════════

std::vector<BYTE> KeyManagement::getSlots(KeyType kt) const {
    std::vector<BYTE> slots;
    auto ktIt = keys_.find(kt);
    if (ktIt != keys_.end()) {
        for (const auto& slotPair : ktIt->second) {
            slots.push_back(slotPair.first);
        }
    }
    return slots;
}

std::vector<KeyInfo> KeyManagement::getAllKeys() const {
    std::vector<KeyInfo> allKeys;
    for (const auto& ktPair : keys_) {
        for (const auto& slotPair : ktPair.second) {
            allKeys.push_back(slotPair.second);
        }
    }
    return allKeys;
}

std::vector<KeyInfo> KeyManagement::getKeys(KeyType kt) const {
    std::vector<KeyInfo> typeKeys;
    auto ktIt = keys_.find(kt);
    if (ktIt != keys_.end()) {
        for (const auto& slotPair : ktIt->second) {
            typeKeys.push_back(slotPair.second);
        }
    }
    return typeKeys;
}

// ════════════════════════════════════════════════════════════════════════════════
// Key Validation
// ════════════════════════════════════════════════════════════════════════════════

bool KeyManagement::isValidKey(const KEYBYTES& key) {
    // Key must not be all zeros
    bool allZero = true;
    for (BYTE b : key) {
        if (b != 0x00) {
            allZero = false;
            break;
        }
    }
    if (allZero) return false;

    // Note: all-FF (FFFFFFFFFFFF) is a valid factory default key for Mifare Classic
    return true;
}

bool KeyManagement::matches(int sector, KeyType kt, const KEYBYTES& key) const {
    KEYBYTES cardKey = getCardKey(sector, kt);
    return cardKey == key;
}

// ════════════════════════════════════════════════════════════════════════════════
// Card Trailer Key Access
// ════════════════════════════════════════════════════════════════════════════════

KEYBYTES KeyManagement::getCardKeyA(int sector) const {
    const MifareBlock& trailer = getTrailer(sector);
    KEYBYTES keyA;
    std::copy(trailer.raw, trailer.raw + 6, keyA.begin());
    return keyA;
}

KEYBYTES KeyManagement::getCardKeyB(int sector) const {
    const MifareBlock& trailer = getTrailer(sector);
    KEYBYTES keyB;
    std::copy(trailer.raw + 10, trailer.raw + 16, keyB.begin());
    return keyB;
}

KEYBYTES KeyManagement::getCardKey(int sector, KeyType kt) const {
    return (kt == KeyType::A) ? getCardKeyA(sector) : getCardKeyB(sector);
}

// ════════════════════════════════════════════════════════════════════════════════
// Introspection
// ════════════════════════════════════════════════════════════════════════════════

std::string KeyManagement::describeKey(KeyType kt, BYTE slot) const {
    const KeyInfo& info = getKeyInfo(kt, slot);
    std::ostringstream oss;
    oss << (kt == KeyType::A ? "KeyA" : "KeyB") << " Slot " << (int)slot << ": ";
    for (BYTE b : info.key) {
        oss << std::hex << std::setfill('0') << std::setw(2) << (int)b << " ";
    }
    oss << "(" << (info.ks == KeyStructure::Volatile ? "Volatile" : "NonVolatile");
    if (!info.name.empty()) {
        oss << ", " << info.name;
    }
    oss << ")";
    return oss.str();
}

void KeyManagement::printAllKeys() const {
    std::cout << "=== Registered Keys ===\n";
    for (const auto& ktPair : keys_) {
        std::cout << (ktPair.first == KeyType::A ? "Key A" : "Key B") << ":\n";
        for (const auto& slotPair : ktPair.second) {
            std::cout << "  " << describeKey(ktPair.first, slotPair.first) << "\n";
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════════
// Clearing
// ════════════════════════════════════════════════════════════════════════════════

void KeyManagement::clearKeys(KeyType kt) {
    keys_.erase(kt);
}

void KeyManagement::clearAllKeys() {
    keys_.clear();
}

// ════════════════════════════════════════════════════════════════════════════════
// Internal Helpers
// ════════════════════════════════════════════════════════════════════════════════

const MifareBlock& KeyManagement::getTrailer(int sector) const {
    if (cardMemory_.is4K()) {
        if (sector < 32) {
            return cardMemory_.data.card4K.detailed.normalSector[sector].trailerBlock;
        } else {
            return cardMemory_.data.card4K.detailed.extendedSector[sector - 32].trailerBlock;
        }
    } else {
        return cardMemory_.data.card1K.detailed.sector[sector].trailerBlock;
    }
}

BYTE KeyManagement::getDefaultSlot(KeyType kt) const {
    auto ktIt = keys_.find(kt);
    if (ktIt == keys_.end() || ktIt->second.empty()) {
        throw std::runtime_error("No keys registered for this type");
    }
    return ktIt->second.begin()->first;
}
