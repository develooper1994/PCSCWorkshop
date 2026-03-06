#ifndef CARDTOPOLOGY_H
#define CARDTOPOLOGY_H

#include "../CardDataTypes.h"
#include <stdexcept>

// ════════════════════════════════════════════════════════════════════════════════
// CardLayoutTopology - Pure Card Layout Information
// ════════════════════════════════════════════════════════════════════════════════
//
// Responsible for:
// - Card type (Classic 1K, Classic 4K, Ultralight)
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
// Classic 1K Layout:
// - Blocks: 64 (0-63), Sectors: 16 (0-15), Each sector: 4 blocks
//
// Classic 4K Layout:
// - Blocks: 256 (0-255), Sectors: 40 (0-39)
// - Sectors 0-31: 4 blocks each, Sectors 32-39: 16 blocks each
//
// Ultralight Layout:
// - Virtual blocks: 4 (0-3), her biri 4 page = 16 byte
// - Sektör yok, trailer yok
// - Tüm kart tek sektör (sector 0) olarak modellenir → readCard() uyumlu
//
// ════════════════════════════════════════════════════════════════════════════════

class CardLayoutTopology {
public:
    // ────────────────────────────────────────────────────────────────────────────
    // Constructor
    // ────────────────────────────────────────────────────────────────────────────

    explicit CardLayoutTopology(CardType ct = CardType::MifareClassic1K);

    // Backward-compatible: bool is4K → CardType
    explicit CardLayoutTopology(bool is4K);

    // ────────────────────────────────────────────────────────────────────────────
    // Card Type Queries
    // ────────────────────────────────────────────────────────────────────────────

    CardType cardType() const noexcept { return cardType_; }
    bool is4K() const noexcept { return cardType_ == CardType::MifareClassic4K; }
    bool is1K() const noexcept { return cardType_ == CardType::MifareClassic1K; }
    bool isUltralight() const noexcept { return cardType_ == CardType::MifareUltralight; }
    bool isClassic() const noexcept { return is1K() || is4K(); }

    // ────────────────────────────────────────────────────────────────────────────
    // Memory Size
    // ────────────────────────────────────────────────────────────────────────────

    // Total memory in bytes
    size_t totalMemoryBytes() const noexcept;

    // Total blocks in card (64 for 1K, 256 for 4K, 4 for Ultralight)
    int totalBlocks() const noexcept;

    // Total sectors (16 for 1K, 40 for 4K, 1 for Ultralight)
    int sectorCount() const noexcept;

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

    // Is this a trailer block? (always false for Ultralight)
    bool isTrailerBlock(int block) const noexcept;

    // Does this card type have trailer blocks?
    bool hasTrailers() const noexcept { return isClassic(); }

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
    CardType cardType_;

    // ────────────────────────────────────────────────────────────────────────────
    // Internal Helpers
    // ────────────────────────────────────────────────────────────────────────────

    // Check if sector is extended (16 blocks) - only for 4K
    bool isExtendedSector(int sector) const noexcept;
};

#endif // CARDTOPOLOGY_H
