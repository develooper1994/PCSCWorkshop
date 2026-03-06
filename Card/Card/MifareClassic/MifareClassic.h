#ifndef MIFARECLASSIC_H
#define MIFARECLASSIC_H

#include "Utils.h"
#include "../Card.h"
#include "../Topology/SectorConfig.h"
#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <map>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <iostream>

constexpr int SMALL_SECTOR_COUNT  = 32;
constexpr int SMALL_SECTOR_PAGES  = 4;
constexpr int SMALL_SECTORS_TOTAL_PAGES = SMALL_SECTOR_COUNT * SMALL_SECTOR_PAGES; // 128
constexpr int LARGE_SECTOR_PAGES  = 16;

// ============================================================
// MifareCardCore — Mifare Classic 1K / 4K Card Implementation
// ============================================================
// Provides reading, writing, and security configuration
// for Mifare Classic 1K (16 sectors) and 4K (40 sectors) cards.
//
// Features:
// - Block/sector read/write with automatic key selection
// - Key management and caching
// - Sector security configuration (access bits)
// - Sector trailer operations
// - Batch configuration apply
// ============================================================
class MifareCardCore : public Card {
public:
    // ────────────────────────────────────────────────────────────────────────────
    // Type Aliases
    // ────────────────────────────────────────────────────────────────────────────
    using KeyType      = ::KeyType;
    using KeyStructure = ::KeyStructure;
    using BLOCK        = ::BLOCK;
    using ACCESSBYTES  = ::ACCESSBYTES;

    // ────────────────────────────────────────────────────────────────────────────
    // Construction
    // ────────────────────────────────────────────────────────────────────────────
    // Create card instance (1K by default, set is4K=true for 4K)
    explicit MifareCardCore(Reader& r, bool is4K = false);

    // ────────────────────────────────────────────────────────────────────────────
    // Card Base Class Implementation
    // ────────────────────────────────────────────────────────────────────────────
    CardType getCardType() const noexcept override;
    size_t getMemorySize() const noexcept override;
    BYTEV read(size_t address, size_t length) override;
    void write(size_t address, const BYTEV& data) override;
    void reset() override;
    CardTopology getTopology() const override;

    // ────────────────────────────────────────────────────────────────────────────
    // Card Properties
    // ────────────────────────────────────────────────────────────────────────────
    // Get number of sectors (16 for 1K, 40 for 4K)
    size_t getSectorCount() const noexcept;

    // ────────────────────────────────────────────────────────────────────────────
    // Key Management
    // ────────────────────────────────────────────────────────────────────────────
    // Register a key for authentication
    // @param kt Key type (A or B)
    // @param key 6-byte key
    // @param ks Storage mode (Volatile/NonVolatile)
    // @param slot Reader slot number (typically 0x01 for A, 0x02 for B)
    void setKey(KeyType kt, const std::array<BYTE, 6>& key,
                KeyStructure ks, BYTE slot);
    
    // Load all registered keys to reader (for testing)
    void loadAllKeysToReader();

    // ────────────────────────────────────────────────────────────────────────────
    // Sector Configuration (Host-Side Permissions)
    // ────────────────────────────────────────────────────────────────────────────
    // Set security config for one sector
    void setSectorConfig(int sector, const SectorConfig& cfg);
    
    // Set same config for all sectors
    void setAllSectorsConfig(const SectorConfig& cfg);
    
    // Set individual config for each sector
    void setAllSectorsConfig(const std::vector<SectorConfig>& configs);

    // ────────────────────────────────────────────────────────────────────────────
    // Layout Information (Block ↔ Sector Mapping)
    // ────────────────────────────────────────────────────────────────────────────
    // Convert block number to sector number
    int blockToSector(BYTE block) const;
    
    // Get index of block within its sector (0-3 for small, 0-15 for large)
    int blockIndexInSector(BYTE block) const;
    
    // Get first block number of sector
    int firstBlockInSector(int sector) const;
    
    // Get number of blocks in sector (4 for small, 16 for large)
    int blocksInSector(int sector) const;

    // ────────────────────────────────────────────────────────────────────────────
    // Authentication
    // ────────────────────────────────────────────────────────────────────────────
    // Ensure sector is authenticated with given key
    // (cached if already authenticated with same key)
    void ensureAuthorized(int sector, KeyType kt, BYTE keySlot);
    
    // Try to authenticate with any registered key
    // @param outSlot Returns the slot number used
    // @return KeyType used (A or B)
    KeyType tryAnyAuthForSector(int sector, BYTE& outSlot);

    // ────────────────────────────────────────────────────────────────────────────
    // Single-Block Operations (Recommended: Auto Key Selection)
    // ────────────────────────────────────────────────────────────────────────────
    // Read block (auto selects key)
    BYTEV read(BYTE block);
    
    // Write block (auto selects key)
    void write(BYTE block, const BYTEV& data);
    void write(BYTE block, const std::string& text);
    
    // Read/write with explicit key specification
    BYTEV readWithKey(BYTE block, KeyType keyType, BYTE keySlot);
    void writeWithKey(BYTE block, const BYTEV& data, KeyType keyType, BYTE keySlot);
    void writeWithKey(BYTE block, const std::string& text, KeyType keyType, BYTE keySlot);

    // ────────────────────────────────────────────────────────────────────────────
    // Block Access Convenience
    // ────────────────────────────────────────────────────────────────────────────
    // Read entire block as BLOCK type
    BLOCK readBlock(BYTE block);
    
    // Write entire block from BLOCK type
    void writeBlock(BYTE block, const BYTE data[16]);

    // ────────────────────────────────────────────────────────────────────────────
    // Multi-Block Linear Operations
    // ────────────────────────────────────────────────────────────────────────────
    // Read multiple blocks (auto-advancing)
    BYTEV readLinear(BYTE startBlock, size_t length);
    
    // Write multiple blocks (auto-advancing)
    void writeLinear(BYTE startBlock, const BYTEV& data);

    // ────────────────────────────────────────────────────────────────────────────
    // Trailer Block Operations
    // ────────────────────────────────────────────────────────────────────────────
    // Read sector trailer (16 bytes: keyA + access + keyB)
    BLOCK readTrailer(int sector);
    
    // Read all sector trailers
    std::vector<BLOCK> readAllTrailers();
    
    // Print all trailers (debug)
    void printAllTrailers();
    
    // Write trailer directly (dangerous, use applySectorConfig instead)
    void changeTrailer(int sector, const BYTE trailer16[16]);
    
    // Apply host-side config to card (simple)
    void applySectorConfigToCard(int sector);
    
    // Apply config with strict validation and rollback
    void applySectorConfigStrict(int sector,
                                 KeyType authKeyType = KeyType::B,
                                 bool enableRollback = true);

    // ────────────────────────────────────────────────────────────────────────────
    // Batch Operations (All Sectors)
    // ────────────────────────────────────────────────────────────────────────────
    // Apply config to all sectors
    void applyAllSectorsConfig();
    void applyAllSectorsConfig(const std::vector<SectorConfig>& configs);
    
    // Apply config with strict validation
    void applyAllSectorsConfigStrict(KeyType authKeyType = KeyType::B,
                                     bool enableRollback = true);
    void applyAllSectorsConfigStrict(const std::vector<SectorConfig>& configs,
                                     KeyType authKeyType = KeyType::B,
                                     bool enableRollback = true);
    
    // Load config from card (read trailers and parse permissions)
    void loadSectorConfigFromCard(int sector);
    void loadAllSectorConfigsFromCard();
    void loadSectorConfigsFromCard(const std::vector<int>& sectors);

private:
    // ────────────────────────────────────────────────────────────────────────────
    // Card Properties
    // ────────────────────────────────────────────────────────────────────────────
    const bool is4KCard_;             // True for 4K, false for 1K
    const size_t numberOfSectors_;    // 16 for 1K, 40 for 4K

    // ────────────────────────────────────────────────────────────────────────────
    // Key & Config Storage
    // ────────────────────────────────────────────────────────────────────────────
    std::vector<KeyInfo>      keys_;            // Registered keys (KeyA, KeyB, etc.)
    std::vector<SectorConfig> sectorConfigs_;   // Per-sector security config
    std::vector<KeyInfo>      authCache_;       // Cached auth state (optimization)

    // ────────────────────────────────────────────────────────────────────────────
    // Auth Caching (Optimization: avoid redundant auth on same key)
    // ────────────────────────────────────────────────────────────────────────────
    bool isSectorAuthorized(int sector, KeyType kt, BYTE slot) const noexcept;
    void setAuthCache(int sector, KeyType kt, BYTE slot);
    void invalidateAuthForSector(int sector) noexcept;
    void ensureAuthCacheSize(int sector);

    // ────────────────────────────────────────────────────────────────────────────
    // Layout Helpers (1K/4K aware)
    // ────────────────────────────────────────────────────────────────────────────
    // Convert block number → sector number (1K: block/4, 4K: mixed)
    int sectorFromBlock(BYTE block) const noexcept;
    
    // Get block count per sector (4 for small, 16 for large)
    int blocksPerSector(int sector) const noexcept;
    
    // Get first block number of sector
    int firstBlockOfSector(int sector) const noexcept;
    
    // Get trailer block number for sector
    BYTE trailerBlockOf(int sector) const noexcept;
    
    // Check if block is a trailer block
    bool isTrailerBlock(BYTE block) const noexcept;
    
    // Check if block is manufacturer block (block 0)
    bool isManufacturerBlock(BYTE block) const noexcept;
    
    // Validation: throw if trailer block (shouldn't be called directly)
    void checkTrailerBlock(BYTE block) const;
    
    // Validation: throw if manufacturer block (read-only)
    void checkManufacturerBlock(BYTE block) const;
    
    // Validation: throw if block out of range (inline)
    void validateBlockNumber(int block) const;
    
    // Validation: throw if sector out of range (inline)
    void validateSectorNumber(int sector) const;

    // ────────────────────────────────────────────────────────────────────────────
    // Key & Config Selection Helpers
    // ────────────────────────────────────────────────────────────────────────────
    
    // Choose best key (A or B) for an operation (read/write)
    KeyType chooseKeyForOperation(int sector, bool isWrite) const;
    
    // Find registered key by type, throw if not found
    const KeyInfo& findKeyOrThrow(KeyType kt) const;
    
    // Check if key of given type is registered
    bool hasKey(KeyType kt) const noexcept;
    
    // Throw if both KeyA and KeyB not registered
    void requireBothKeys() const;

    // ────────────────────────────────────────────────────────────────────────────
    // Batch Operations
    // ────────────────────────────────────────────────────────────────────────────
    
    // Apply function to all sectors, collect errors
    template<typename Fn>
    void batchApply(Fn fn);

    // Collect and throw all batch errors
    static void throwIfErrors(const std::vector<std::string>& errors,
                               const char* header);
};

// ════════════════════════════════════════════════════════════════════════════════
// Inline Private Helper Functions
// ════════════════════════════════════════════════════════════════════════════════

// Validate block number is in range
inline void MifareCardCore::validateBlockNumber(int block) const {
    int maxBlock = is4KCard_ ? 255 : 63;
    if (block < 0 || block > maxBlock) {
        throw std::out_of_range("Block number out of range");
    }
}

// Validate sector number is in range
inline void MifareCardCore::validateSectorNumber(int sector) const {
    if (sector < 0 || sector >= static_cast<int>(numberOfSectors_)) {
        throw std::out_of_range("Sector number out of range");
    }
}

// ════════════════════════════════════════════════════════════════════════════════
// Template Implementation: batchApply
// ════════════════════════════════════════════════════════════════════════════════
template<typename Fn>
void MifareCardCore::batchApply(Fn fn) {
    std::vector<std::string> errors;
    for (int i = 0; i < static_cast<int>(numberOfSectors_); ++i) {
        try {
            fn(i);
        } catch (const std::exception& e) {
            errors.push_back("Sector " + std::to_string(i) + ": " + e.what());
        }
    }
    throwIfErrors(errors, "Batch operation failed on some sectors:\n");
}

// ════════════════════════════════════════════════════════════════════════════════
// Inline Helper Functions (Layout Calculation)
// ════════════════════════════════════════════════════════════════════════════════

// Get memory size: sector 0 has manufacturer block, others don't; no trailers counted
inline size_t MifareCardCore::getMemorySize() const noexcept {
    // Sector 0: blocks 1-2 are data (block 0 is manufacturer, block 3 is trailer)
    // Other sectors: blocks 0-2 are data (block 3 is trailer)
    constexpr size_t firstSectorDataBytes = 2 * 16;
    const size_t otherSectorsDataBytes = 3 * 16 * (numberOfSectors_ - 1);
    return firstSectorDataBytes + otherSectorsDataBytes;
}

// Map block number to sector (handles both 1K and 4K layouts)
// 1K: sectors 0-15, blocks 0-63 (4 blocks each)
// 4K: sectors 0-31 (small: 4 blocks each), sectors 32-39 (large: 16 blocks each)
inline int MifareCardCore::sectorFromBlock(BYTE block) const noexcept {
    int b = static_cast<int>(block);
    return (!is4KCard_ || b < 128) ? b / 4 : 32 + (b - 128) / 16;
}

// Get blocks-per-sector (small sectors: 4, large sectors: 16 on 4K only)
inline int MifareCardCore::blocksPerSector(int sector) const noexcept {
    return (!is4KCard_ || sector < 32) ? 4 : 16;
}

// Get first block of sector
inline int MifareCardCore::firstBlockOfSector(int sector) const noexcept {
    return (!is4KCard_ || sector < 32) ? sector * 4 : 128 + (sector - 32) * 16;
}

// Get trailer block (last block of sector)
inline BYTE MifareCardCore::trailerBlockOf(int sector) const noexcept {
    return static_cast<BYTE>(firstBlockOfSector(sector) + blocksPerSector(sector) - 1);
}

// Check if block is a sector trailer
inline bool MifareCardCore::isTrailerBlock(BYTE block) const noexcept {
    return block == trailerBlockOf(sectorFromBlock(block));
}

// Check if block is manufacturer block (always block 0)
inline bool MifareCardCore::isManufacturerBlock(BYTE block) const noexcept {
    return block == 0;
}

// Validation error: trailer block (unsupported operation)
inline void MifareCardCore::checkTrailerBlock(BYTE block) const {
    if (isTrailerBlock(block)) {
        throw std::runtime_error(std::string(__func__) + ": operation not supported on trailer block");
    }
}

// Validation error: manufacturer block (read-only)
inline void MifareCardCore::checkManufacturerBlock(BYTE block) const {
    if (isManufacturerBlock(block)) {
        throw std::runtime_error(std::string(__func__) + ": manufacturer block is read-only");
    }
}

#endif // !MIFARECLASSIC_H
