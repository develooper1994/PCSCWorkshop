#include "CardTopology.h"

// ════════════════════════════════════════════════════════════════════════════════
// Construction
// ════════════════════════════════════════════════════════════════════════════════

CardTopology::CardTopology(bool is4K)
    : is4K_(is4K) {
}

// ════════════════════════════════════════════════════════════════════════════════
// Sector Information
// ════════════════════════════════════════════════════════════════════════════════

int CardTopology::blocksPerSector(int sector) const noexcept {
    if (!is4K_) {
        return 4;  // 1K: all sectors have 4 blocks
    }
    // 4K: sectors 0-31 have 4 blocks, sectors 32-39 have 16 blocks
    return sector < 32 ? 4 : 16;
}

int CardTopology::firstBlockOfSector(int sector) const noexcept {
    if (!is4K_) {
        return sector * 4;
    }
    // 4K: sectors 0-31 use blocks 0-127, sectors 32-39 use blocks 128-255
    if (sector < 32) {
        return sector * 4;
    }
    return 128 + (sector - 32) * 16;
}

int CardTopology::lastBlockOfSector(int sector) const noexcept {
    return firstBlockOfSector(sector) + blocksPerSector(sector) - 1;
}

int CardTopology::trailerBlockOfSector(int sector) const noexcept {
    return lastBlockOfSector(sector);
}

// ════════════════════════════════════════════════════════════════════════════════
// Block Information
// ════════════════════════════════════════════════════════════════════════════════

int CardTopology::sectorFromBlock(int block) const noexcept {
    if (!is4K_) {
        return block / 4;
    }
    // 4K: blocks 0-127 are in sectors 0-31, blocks 128-255 are in sectors 32-39
    if (block < 128) {
        return block / 4;
    }
    return 32 + (block - 128) / 16;
}

int CardTopology::blockIndexInSector(int block) const noexcept {
    int sector = sectorFromBlock(block);
    int firstBlock = firstBlockOfSector(sector);
    return block - firstBlock;
}

bool CardTopology::isTrailerBlock(int block) const noexcept {
    int sector = sectorFromBlock(block);
    return block == trailerBlockOfSector(sector);
}

bool CardTopology::isDataBlock(int block) const noexcept {
    return !isManufacturerBlock(block) && !isTrailerBlock(block);
}

// ════════════════════════════════════════════════════════════════════════════════
// Validation
// ════════════════════════════════════════════════════════════════════════════════

bool CardTopology::isValidBlock(int block) const noexcept {
    return block >= 0 && block < totalBlocks();
}

bool CardTopology::isValidSector(int sector) const noexcept {
    return sector >= 0 && sector < sectorCount();
}

void CardTopology::validateBlock(int block) const {
    if (!isValidBlock(block)) {
        throw std::out_of_range("Block out of range");
    }
}

void CardTopology::validateSector(int sector) const {
    if (!isValidSector(sector)) {
        throw std::out_of_range("Sector out of range");
    }
}

// ════════════════════════════════════════════════════════════════════════════════
// Internal Helpers
// ════════════════════════════════════════════════════════════════════════════════

bool CardTopology::isExtendedSector(int sector) const noexcept {
    return is4K_ && sector >= 32;
}
