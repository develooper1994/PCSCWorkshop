#ifndef CARDTOPOLOGY_H
#define CARDTOPOLOGY_H

#include "CardDataTypes.h"
#include <stdexcept>

// ════════════════════════════════════════════════════════════════════════════════
// CardTopologyData - Zero-Copy Union for Card Layout
// ════════════════════════════════════════════════════════════════════════════════
//
// Memory-efficient union representation of card layout.
// Single discriminant (is4K) determines which layout is active.
//
// 1K Layout:
// - Blocks: 64 (0-63)
// - Sectors: 16 (0-15)
// - Each sector: 4 blocks
//
// 4K Layout:
// - Blocks: 256 (0-255)
// - Sectors: 40 (0-39)
// - Sectors 0-31: 4 blocks each
// - Sectors 32-39: 16 blocks each
//
// ════════════════════════════════════════════════════════════════════════════════

struct CardTopologyData {
    union {
        struct {
            // 1K Card: 16 sectors × 4 blocks = 64 blocks = 1024 bytes
            static constexpr int TOTAL_BLOCKS = 64;
            static constexpr int TOTAL_SECTORS = 16;
            static constexpr int BLOCKS_PER_SECTOR = 4;
            static constexpr size_t TOTAL_MEMORY = 1024;
        } card1K;
        
        struct {
            // 4K Card: 32×4 + 8×16 = 128 + 128 = 256 blocks = 4096 bytes
            // Sectors 0-31: 4 blocks each (128 blocks)
            // Sectors 32-39: 16 blocks each (128 blocks)
            static constexpr int TOTAL_BLOCKS = 256;
            static constexpr int TOTAL_SECTORS = 40;
            static constexpr int NORMAL_SECTORS = 32;      // Sectors 0-31
            static constexpr int EXTENDED_SECTORS = 8;     // Sectors 32-39
            static constexpr int NORMAL_BLOCKS = 128;      // 32 × 4
            static constexpr int EXTENDED_BLOCKS = 128;    // 8 × 16
            static constexpr size_t TOTAL_MEMORY = 4096;
        } card4K;
    } layout;
};

// ════════════════════════════════════════════════════════════════════════════════
// CardLayoutTopology - Pure Card Layout Information (No Logic)
// ════════════════════════════════════════════════════════════════════════════════
//
// Responsible for:
// - Card type (1K vs 4K)
// - Memory size calculations
// - Block/sector mappings
// - Layout queries (which block is in which sector, etc.)
//
// NOT responsible for:
// - Authentication
// - Access control
// - Key management
// - Reading/writing data
//
// Uses CardTopologyData union for efficient layout representation.
// 
// ════════════════════════════════════════════════════════════════════════════════

class CardLayoutTopology {
public:
    // ────────────────────────────────────────────────────────────────────────────
    // Constructor
    // ────────────────────────────────────────────────────────────────────────────

    // Create topology for 1K (is4K=false) or 4K (is4K=true) card
    explicit CardLayoutTopology(bool is4K = false);

    // ────────────────────────────────────────────────────────────────────────────
    // Card Type Queries
    // ────────────────────────────────────────────────────────────────────────────

    bool is4K() const noexcept { return is4K_; }
    bool is1K() const noexcept { return !is4K_; }

    // ────────────────────────────────────────────────────────────────────────────
    // Memory Size
    // ────────────────────────────────────────────────────────────────────────────

    // Total memory in bytes (1024 for 1K, 4096 for 4K)
    size_t totalMemoryBytes() const noexcept { return is4K_ ? 4096 : 1024; }

    // Total blocks in card (64 for 1K, 256 for 4K)
    int totalBlocks() const noexcept { return is4K_ ? 256 : 64; }

    // Total sectors (16 for 1K, 40 for 4K)
    int sectorCount() const noexcept { return is4K_ ? 40 : 16; }

    // ────────────────────────────────────────────────────────────────────────────
    // Sector Information
    // ────────────────────────────────────────────────────────────────────────────

    // Blocks per sector (4 for normal, 16 for extended)
    int blocksPerSector(int sector) const noexcept;

    // First block index in sector
    int firstBlockOfSector(int sector) const noexcept;

    // Last block index in sector (trailer block)
    int lastBlockOfSector(int sector) const noexcept;

    // Trailer block of sector (last block)
    int trailerBlockOfSector(int sector) const noexcept;

    // ────────────────────────────────────────────────────────────────────────────
    // Block Information
    // ────────────────────────────────────────────────────────────────────────────

    // Which sector contains this block
    int sectorFromBlock(int block) const noexcept;

    // Position within sector (0 = first, n-1 = trailer)
    int blockIndexInSector(int block) const noexcept;

    // Is this the manufacturer block (block 0)?
    bool isManufacturerBlock(int block) const noexcept { return block == 0; }

    // Is this a trailer block?
    bool isTrailerBlock(int block) const noexcept;

    // Is this a data block (not manufacturer, not trailer)?
    bool isDataBlock(int block) const noexcept;

    // Byte offset in card memory for block (block * 16)
    size_t blockByteOffset(int block) const noexcept { return static_cast<size_t>(block) * 16; }

    // ────────────────────────────────────────────────────────────────────────────
    // Validation
    // ────────────────────────────────────────────────────────────────────────────

    // Check if block number is valid
    bool isValidBlock(int block) const noexcept;

    // Check if sector number is valid
    bool isValidSector(int sector) const noexcept;

    // Throw if block invalid
    void validateBlock(int block) const;

    // Throw if sector invalid
    void validateSector(int sector) const;

private:
    bool is4K_;
    CardTopologyData data_;

    // ────────────────────────────────────────────────────────────────────────────
    // Internal Helpers
    // ────────────────────────────────────────────────────────────────────────────

    // Check if sector is extended (16 blocks) - only for 4K
    bool isExtendedSector(int sector) const noexcept;
};

#endif // CARDTOPOLOGY_H
