#include "SectorConfigBuilder.h"

// ════════════════════════════════════════════════════════════════════════════════
// Builder Methods Implementation
// ════════════════════════════════════════════════════════════════════════════════

SectorConfigBuilder& SectorConfigBuilder::keyA(const KEYBYTES& key) noexcept {
    keyA_.key = key;
    return *this;
}

SectorConfigBuilder& SectorConfigBuilder::keyB(const KEYBYTES& key) noexcept {
    keyB_.key = key;
    return *this;
}

SectorConfigBuilder& SectorConfigBuilder::dataBlockAccess(bool aRead, bool aWrite,
                                                           bool bRead, bool bWrite) noexcept {
    keyA_.readable = aRead;
    keyA_.writable = aWrite;
    keyB_.readable = bRead;
    keyB_.writable = bWrite;
    return *this;
}

SectorConfigBuilder& SectorConfigBuilder::keyAAccess(bool readable, bool writable) noexcept {
    keyA_.readable = readable;
    keyA_.writable = writable;
    return *this;
}

SectorConfigBuilder& SectorConfigBuilder::keyBAccess(bool readable, bool writable) noexcept {
    keyB_.readable = readable;
    keyB_.writable = writable;
    return *this;
}

SectorConfigBuilder& SectorConfigBuilder::accessMode(AccessMode mode) noexcept {
    // Standard Mifare Classic permission modes
    switch (mode) {
    case AccessMode::MODE_0:  // KeyA: R, KeyB: RW
        keyA_.readable = true;  keyA_.writable = false;
        keyB_.readable = true;  keyB_.writable = true;
        break;
    case AccessMode::MODE_1:  // KeyA: R, KeyB: R
        keyA_.readable = true;  keyA_.writable = false;
        keyB_.readable = true;  keyB_.writable = false;
        break;
    case AccessMode::MODE_2:  // KeyA: RW, KeyB: RW
        keyA_.readable = true;  keyA_.writable = true;
        keyB_.readable = true;  keyB_.writable = true;
        break;
    case AccessMode::MODE_3:  // KeyA: RW, KeyB: R
        keyA_.readable = true;  keyA_.writable = true;
        keyB_.readable = true;  keyB_.writable = false;
        break;
    }
    return *this;
}

SectorConfigBuilder& SectorConfigBuilder::keyAProperties(KeyStructure ks, BYTE slot,
                                                          const std::string& name) noexcept {
    keyA_.ks = ks;
    keyA_.slot = slot;
    if (!name.empty()) {
        keyA_.name = name;
    }
    return *this;
}

SectorConfigBuilder& SectorConfigBuilder::keyBProperties(KeyStructure ks, BYTE slot,
                                                          const std::string& name) noexcept {
    keyB_.ks = ks;
    keyB_.slot = slot;
    if (!name.empty()) {
        keyB_.name = name;
    }
    return *this;
}

// ════════════════════════════════════════════════════════════════════════════════
// Build & Reset
// ════════════════════════════════════════════════════════════════════════════════

SectorConfig SectorConfigBuilder::build() const {
    validateConfiguration();
    SectorConfig cfg(keyA_, keyB_);
    return cfg;
}

void SectorConfigBuilder::reset() noexcept {
    keyA_ = KeyInfo{ .kt = KeyType::A, .slot = 0x01 };
    keyB_ = KeyInfo{ .kt = KeyType::B, .slot = 0x02 };
}

// ════════════════════════════════════════════════════════════════════════════════
// Validation
// ════════════════════════════════════════════════════════════════════════════════

void SectorConfigBuilder::validateConfiguration() const {
    // Keys are validated by SectorConfig constructor
    // This is a placeholder for future validation if needed
    // For now, SectorConfig's constructor will validate
}
