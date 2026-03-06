#ifndef SECTORCONFIGBUILDER_H
#define SECTORCONFIGBUILDER_H

#include "SectorConfig.h"
#include <array>
#include <stdexcept>

// ════════════════════════════════════════════════════════════════════════════════
// SectorConfigBuilder — Fluent API for Building SectorConfig
// ════════════════════════════════════════════════════════════════════════════════
//
// Provides a convenient fluent interface for constructing SectorConfig objects.
// This avoids repetitive setup and makes configuration more readable.
//
// Example:
//   auto config = SectorConfigBuilder()
//       .keyA({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
//       .keyB({0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
//       .dataBlockAccess(true, true, true, true)
//       .build();
//
// ════════════════════════════════════════════════════════════════════════════════
class SectorConfigBuilder {
public:
    // ────────────────────────────────────────────────────────────────────────────
    // Constructor
    // ────────────────────────────────────────────────────────────────────────────
    SectorConfigBuilder() = default;

    // ────────────────────────────────────────────────────────────────────────────
    // Builder Methods (Return *this for chaining)
    // ────────────────────────────────────────────────────────────────────────────

    // Set Key A (6 bytes)
    // @return *this for method chaining
    SectorConfigBuilder& keyA(const KEYBYTES& key) noexcept;

    // Set Key B (6 bytes)
    // @return *this for method chaining
    SectorConfigBuilder& keyB(const KEYBYTES& key) noexcept;

    // Set data block access permissions (applies to blocks 0-2)
    // @param aRead  KeyA can read data blocks
    // @param aWrite KeyA can write data blocks
    // @param bRead  KeyB can read data blocks
    // @param bWrite KeyB can write data blocks
    // @return *this for method chaining
    SectorConfigBuilder& dataBlockAccess(bool aRead, bool aWrite,
                                          bool bRead, bool bWrite) noexcept;

    // Set access for Key A only
    // @param readable KeyA can read
    // @param writable KeyA can write
    // @return *this for method chaining
    SectorConfigBuilder& keyAAccess(bool readable, bool writable) noexcept;

    // Set access for Key B only
    // @param readable KeyB can read
    // @param writable KeyB can write
    // @return *this for method chaining
    SectorConfigBuilder& keyBAccess(bool readable, bool writable) noexcept;

    // Set predefined access mode (Mifare Classic standard modes)
    enum class AccessMode {
        MODE_0,  // KeyA: R, KeyB: RW  (0b000)
        MODE_1,  // KeyA: R, KeyB: R   (0b010)
        MODE_2,  // KeyA: RW, KeyB: RW (0b100)
        MODE_3,  // KeyA: RW, KeyB: R  (0b110)
    };

    // Set standard access mode
    // @return *this for method chaining
    SectorConfigBuilder& accessMode(AccessMode mode) noexcept;

    // Set Key A properties (storage, slot, name)
    SectorConfigBuilder& keyAProperties(KeyStructure ks, BYTE slot,
                                         const std::string& name = "") noexcept;

    // Set Key B properties (storage, slot, name)
    SectorConfigBuilder& keyBProperties(KeyStructure ks, BYTE slot,
                                         const std::string& name = "") noexcept;

    // ────────────────────────────────────────────────────────────────────────────
    // Build & Reset
    // ────────────────────────────────────────────────────────────────────────────

    // Build the SectorConfig and return it
    // Throws if configuration is invalid
    SectorConfig build() const;

    // Reset builder to initial state
    void reset() noexcept;

private:
    // ────────────────────────────────────────────────────────────────────────────
    // Internal State
    // ────────────────────────────────────────────────────────────────────────────
    KeyInfo keyA_{ .kt = KeyType::A, .slot = 0x01 };
    KeyInfo keyB_{ .kt = KeyType::B, .slot = 0x02 };

    // ────────────────────────────────────────────────────────────────────────────
    // Validation
    // ────────────────────────────────────────────────────────────────────────────
    void validateConfiguration() const;
};

#endif // SECTORCONFIGBUILDER_H
