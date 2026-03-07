#include "CardTopology.h"

// ════════════════════════════════════════════════════════════════════════════════
// Construction
// ════════════════════════════════════════════════════════════════════════════════

CardLayoutTopology::CardLayoutTopology(CardType ct)
    : cardType_(ct) {
}

CardLayoutTopology::CardLayoutTopology(bool is4K)
    : cardType_(is4K ? CardType::MifareClassic4K : CardType::MifareClassic1K) {
}

// ════════════════════════════════════════════════════════════════════════════════
// Memory Size
// ════════════════════════════════════════════════════════════════════════════════

size_t CardLayoutTopology::totalMemoryBytes() const noexcept {
    switch (cardType_) {
        case CardType::MifareClassic4K:  return 4096;
        case CardType::MifareUltralight: return 64;
        case CardType::MifareDesfire:    return 0;   // runtime: GetVersion
        default:                         return 1024;
    }
}

int CardLayoutTopology::totalBlocks() const noexcept {
    switch (cardType_) {
        case CardType::MifareClassic4K:  return 256;
        case CardType::MifareUltralight: return 4;
        case CardType::MifareDesfire:    return 0;   // virtual blocks via DesfireMemoryLayout
        default:                         return 64;
    }
}

int CardLayoutTopology::sectorCount() const noexcept {
    switch (cardType_) {
        case CardType::MifareClassic4K:  return 40;
        case CardType::MifareUltralight: return 1;   // tüm kart = 1 sanal sektör
        case CardType::MifareDesfire:    return 0;   // app-based, no sectors
        default:                         return 16;
    }
}

// ════════════════════════════════════════════════════════════════════════════════
// Sector Information
// ════════════════════════════════════════════════════════════════════════════════

int CardLayoutTopology::blocksPerSector(int sector) const noexcept {
    if (isUltralight()) return 4;           // tek sektör, 4 blok
    if (isDesfire())    return 0;           // no sectors
    if (!is4K()) return 4;                  // 1K: tüm sektörler 4 blok
    return sector < 32 ? 4 : 16;           // 4K: karma
}

int CardLayoutTopology::firstBlockOfSector(int sector) const noexcept {
    if (isUltralight()) return 0;
    if (isDesfire())    return -1;          // no sectors
    if (!is4K()) return sector * 4;
    if (sector < 32) return sector * 4;
    return 128 + (sector - 32) * 16;
}

int CardLayoutTopology::lastBlockOfSector(int sector) const noexcept {
    return firstBlockOfSector(sector) + blocksPerSector(sector) - 1;
}

int CardLayoutTopology::trailerBlockOfSector(int sector) const noexcept {
    if (isUltralight()) return -1;          // Ultralight'ta trailer yok
    if (isDesfire())    return -1;          // DESFire'da trailer yok
    return lastBlockOfSector(sector);
}

// ════════════════════════════════════════════════════════════════════════════════
// Block Information
// ════════════════════════════════════════════════════════════════════════════════

int CardLayoutTopology::sectorFromBlock(int block) const noexcept {
    if (isUltralight()) return 0;           // tüm bloklar sektör 0
    if (isDesfire())    return -1;          // no sectors
    if (!is4K()) return block / 4;
    if (block < 128) return block / 4;
    return 32 + (block - 128) / 16;
}

int CardLayoutTopology::blockIndexInSector(int block) const noexcept {
    int sector = sectorFromBlock(block);
    int firstBlock = firstBlockOfSector(sector);
    return block - firstBlock;
}

bool CardLayoutTopology::isTrailerBlock(int block) const noexcept {
    if (isUltralight()) return false;       // Ultralight'ta trailer yok
    if (isDesfire())    return false;       // DESFire'da trailer yok
    int sector = sectorFromBlock(block);
    return block == trailerBlockOfSector(sector);
}

bool CardLayoutTopology::isDataBlock(int block) const noexcept {
    return !isManufacturerBlock(block) && !isTrailerBlock(block);
}

// ════════════════════════════════════════════════════════════════════════════════
// Validation
// ════════════════════════════════════════════════════════════════════════════════

bool CardLayoutTopology::isValidBlock(int block) const noexcept {
    return block >= 0 && block < totalBlocks();
}

bool CardLayoutTopology::isValidSector(int sector) const noexcept {
    return sector >= 0 && sector < sectorCount();
}

void CardLayoutTopology::validateBlock(int block) const {
    if (!isValidBlock(block)) {
        throw std::out_of_range("Block out of range");
    }
}

void CardLayoutTopology::validateSector(int sector) const {
    if (!isValidSector(sector)) {
        throw std::out_of_range("Sector out of range");
    }
}

// ════════════════════════════════════════════════════════════════════════════════
// Internal Helpers
// ════════════════════════════════════════════════════════════════════════════════

bool CardLayoutTopology::isExtendedSector(int sector) const noexcept {
    return is4K() && sector >= 32;
}
