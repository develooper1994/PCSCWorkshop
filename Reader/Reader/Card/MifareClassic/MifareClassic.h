#ifndef MIFARECLASSIC_H
#define MIFARECLASSIC_H

#include "Utils.h"
#include "../Card.h"
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
// MifareCardCore — Mifare Classic 1K / 4K card abstraction
// ============================================================
// Reader must provide:
//   void loadKey(const BYTE*, KeyStructure, BYTE slot);
//   void auth(BYTE block, KeyType, BYTE slot);
//   void setKeyType(KeyType);  void setKeyNumber(BYTE);
//   void setAuthRequested(bool);
//   void performAuth(BYTE block);
//   BYTEV readPage(BYTE block);
//   void  writePage(BYTE block, const BYTE*);
// ============================================================
class MifareCardCore : public Card {
public:
    // ── type aliases ──
    using KeyType      = ::KeyType;
    using KeyStructure = ::KeyStructure;
    using BLOCK        = std::array<BYTE, 16>;
    using ACCESSBYTES  = std::array<BYTE, 4>;

    // ── sector permission descriptor (host-side only) ──
    struct SectorKeyConfig {
        bool keyA_canRead  = true;
        bool keyA_canWrite = false;
        bool keyB_canRead  = true;
        bool keyB_canWrite = true;
        SectorKeyConfig(bool aR = true, bool aW = false,
                        bool bR = true, bool bW = true) noexcept;
    };

    // ════════════════════════════════════════════════════════
    //  Construction
    // ════════════════════════════════════════════════════════
    explicit MifareCardCore(Reader& r, bool is4K = false);

    // ════════════════════════════════════════════════════════
    //  Card interface implementation (override from Card base class)
    // ════════════════════════════════════════════════════════
    std::string getCardType() const override;
    size_t getMemorySize() const noexcept override;
    BYTEV read(size_t address, size_t length) override;
    void write(size_t address, const BYTEV& data) override;
    void reset() override;
    BYTEV getUID() const override;

    // ════════════════════════════════════════════════════════
    //  Mifare-specific operations
    // ════════════════════════════════════════════════════════
    int getSectorCount() const noexcept;

    // ════════════════════════════════════════════════════════
    //  Key management
    // ════════════════════════════════════════════════════════
    void setKey(KeyType kt, const std::array<BYTE, 6>& key,
                KeyStructure ks, BYTE slot);
    
    // Force load all registered keys to reader immediately (optional, for testing)
    // Normally keys are loaded lazily when first needed
    void loadAllKeysToReader();

    // ════════════════════════════════════════════════════════
    //  Sector config (host-side permission model)
    // ════════════════════════════════════════════════════════
    void setSectorConfig(int sector, const SectorKeyConfig& cfg);
    void setAllSectorsConfig(const SectorKeyConfig& cfg);
    void setAllSectorsConfig(const std::map<int, SectorKeyConfig>& configs);
    void setAllSectorsConfig(const std::vector<SectorKeyConfig>& configs);

    // ════════════════════════════════════════════════════════
    //  Layout helpers (public)
    // ════════════════════════════════════════════════════════
    int blockToSector(BYTE block) const;
    
    // Get the index of a block within its sector (0-3 for small sectors, 0-15 for large sectors)
    int blockIndexInSector(BYTE block) const;
    
    // Get the first block number of a given sector
    int firstBlockInSector(int sector) const;
    
    // Get the number of blocks in a sector (4 for sectors 0-31, 16 for sectors 32-39 on 4K cards)
    int blocksInSector(int sector) const;

    // ════════════════════════════════════════════════════════
    //  Authentication
    // ════════════════════════════════════════════════════════
    void ensureAuthorized(int sector, KeyType kt, BYTE keySlot);
    KeyType tryAnyAuthForSector(int sector, BYTE& outSlot);

    // ════════════════════════════════════════════════════════
    //  Single-block read / write  (cache-aware)
    // ════════════════════════════════════════════════════════
    // Auto key selection versions (recommended)
    BYTEV read(BYTE block);
    void write(BYTE block, const BYTEV& data);
    void write(BYTE block, const std::string& text);
    
    // Explicit key specification versions (for advanced use)
    BYTEV readWithKey(BYTE block, KeyType keyType, BYTE keySlot);
    void writeWithKey(BYTE block, const BYTEV& data, KeyType keyType, BYTE keySlot);
    void writeWithKey(BYTE block, const std::string& text, KeyType keyType, BYTE keySlot);

    // ════════════════════════════════════════════════════════
    //  Convenience single-block (auto key selection)
    // ════════════════════════════════════════════════════════
    BLOCK readBlock(BYTE block);
    void writeBlock(BYTE block, const BYTE data[16]);

    // ════════════════════════════════════════════════════════
    //  Linear (multi-block) read / write
    // ════════════════════════════════════════════════════════
    BYTEV readLinear(BYTE startBlock, size_t length);
    void writeLinear(BYTE startBlock, const BYTEV& data);

    // ════════════════════════════════════════════════════════
    //  Trailer operations
    // ════════════════════════════════════════════════════════
    BLOCK readTrailer(int sector);
    std::vector<BLOCK> readAllTrailers();
    void printAllTrailers();
    void changeTrailer(int sector, const BYTE trailer16[16]);
    void applySectorConfigToCard(int sector);
    void applySectorConfigStrict(int sector,
                                 KeyType authKeyType = KeyType::B,
                                 bool enableRollback = true);

    // ════════════════════════════════════════════════════════
    //  Batch apply / load config helpers
    // ════════════════════════════════════════════════════════
    void applyAllSectorsConfig();
    void applyAllSectorsConfig(const std::map<int, SectorKeyConfig>& configs);
    void applyAllSectorsConfig(const std::vector<SectorKeyConfig>& configs);
    void applyAllSectorsConfigStrict(KeyType authKeyType = KeyType::B,
                                     bool enableRollback = true);
    void applyAllSectorsConfigStrict(
            const std::map<int, SectorKeyConfig>& configs,
            KeyType authKeyType = KeyType::B,
            bool enableRollback = true);
    void applyAllSectorsConfigStrict(
            const std::vector<SectorKeyConfig>& configs,
            KeyType authKeyType = KeyType::B,
            bool enableRollback = true);
    void loadSectorConfigFromCard(int sector);
    void loadAllSectorConfigsFromCard();
    void loadSectorConfigsFromCard(const std::vector<int>& sectors);

    // ════════════════════════════════════════════════════════
    //  Access-bits codec (static helpers)
    // ════════════════════════════════════════════════════════
    static SectorKeyConfig parseAccessBitsToConfig(const BYTE ab[4]) noexcept;
    static bool validateAccessBits(const BYTE a[4]) noexcept;
    static ACCESSBYTES makeSectorAccessBits(const SectorKeyConfig& dataCfg) noexcept;
    static BLOCK buildTrailer(const BYTE keyA[6],
                              const BYTE access[4],
                              const BYTE keyB[6]) noexcept;

private:
    const bool is4KCard_;
    const int  numberOfSectors_;

    std::vector<KeyInfo>          keys_;
    std::vector<SectorKeyConfig>  sectorConfigs_;
    std::vector<KeyInfo> authCache_;

    // ── auth cache helpers ──
    bool isSectorAuthorized(int sector, KeyType kt, BYTE slot) const noexcept;
    void setAuthCache(int sector, KeyType kt, BYTE slot);
    void invalidateAuthForSector(int sector) noexcept;
    void ensureAuthCacheSize(int sector);

    // ── layout (1K / 4K aware) ──
    int sectorFromBlock(BYTE block) const noexcept;
    int blocksPerSector(int sector) const noexcept;
    int firstBlockOfSector(int sector) const noexcept;
    BYTE trailerBlockOf(int sector) const noexcept;
    bool isTrailerBlock(BYTE block) const noexcept;
    bool isManufacturerBlock(BYTE block) const noexcept;
    void checkTrailerBlock(BYTE block) const;
    void checkManufacturerBlock(BYTE block) const;
    void validateBlockNumber(int block) const;
    void validateSectorNumber(int sector) const;

    // ── key / config helpers ──
    KeyType chooseKeyForOperation(int sector, bool isWrite) const;
    const KeyInfo& findKeyOrThrow(KeyType kt) const;
    bool hasKey(KeyType kt) const noexcept;
    void requireBothKeys() const;

    // ── batch helper ──
    template<typename Fn>
    void batchApply(Fn fn);

    static void throwIfErrors(const std::vector<std::string>& errors,
                               const char* header);
    static void extractAccessBits(const BYTE ab[4],
                                  BYTE c1[4], BYTE c2[4], BYTE c3[4]) noexcept;
    static SectorKeyConfig configFromC1C2C3(BYTE c1, BYTE c2, BYTE c3) noexcept;
    static void mapDataBlock(const SectorKeyConfig& cfg,
                             BYTE& c1, BYTE& c2, BYTE& c3);
};

// ── Template implementation for batchApply ──
template<typename Fn>
void MifareCardCore::batchApply(Fn fn) {
    std::vector<std::string> errors;
    for (int i = 0; i < numberOfSectors_; ++i) {
        try { fn(i); }
        catch (const std::exception& e) {
            errors.push_back("Sector " + std::to_string(i) + ": " + e.what());
        }
    }
    throwIfErrors(errors, "Batch operation failed on some sectors:\n");
}

#endif // MIFARECLASSIC_H#endif // MIFARECLASSIC_H