#include "AccessControl.h"
#include <sstream>
#include <iomanip>
#include <cstring>

// ════════════════════════════════════════════════════════════════════════════════
// Construction
// ════════════════════════════════════════════════════════════════════════════════

AccessControl::AccessControl(const CardMemoryLayout& cardMemory)
    : cardMemory_(cardMemory) {
}

// ════════════════════════════════════════════════════════════════════════════════
// Permission Queries
// ════════════════════════════════════════════════════════════════════════════════

bool AccessControl::canReadDataBlock(int sector, KeyType kt) const {
    ACCESSBYTES bits = getAccessBits(sector);
    // TODO: Decode bits and check data block read permission
    // For now, assume default: KeyA read, KeyB read
    return true;
}

bool AccessControl::canWriteDataBlock(int sector, KeyType kt) const {
    ACCESSBYTES bits = getAccessBits(sector);
    // TODO: Decode bits and check data block write permission
    // For now, assume default: KeyA no write, KeyB write
    if (kt == KeyType::A) return false;
    return true;
}

bool AccessControl::canReadTrailer(int sector, KeyType kt) const {
    // TODO: Decode trailer read permission
    return false;  // Trailers usually not readable
}

bool AccessControl::canWriteTrailer(int sector, KeyType kt) const {
    ACCESSBYTES bits = getAccessBits(sector);
    // TODO: Decode trailer write permission
    // Typically only KeyB can write trailer
    return kt == KeyType::B;
}

// ════════════════════════════════════════════════════════════════════════════════
// Authorization Checks
// ════════════════════════════════════════════════════════════════════════════════

bool AccessControl::canRead(int block, KeyType kt) const {
    // TODO: Determine if block is data or trailer, then check permission
    // For now, generic check
    int sector = (cardMemory_.is4K) ? 
        (block < 128 ? block / 4 : 32 + (block - 128) / 16) :
        block / 4;
    
    bool isTrailer = (cardMemory_.is4K) ?
        (block < 128 ? block % 4 == 3 : block % 16 == 15) :
        block % 4 == 3;
    
    if (isTrailer) {
        return canReadTrailer(sector, kt);
    } else {
        return canReadDataBlock(sector, kt);
    }
}

bool AccessControl::canWrite(int block, KeyType kt) const {
    // TODO: Determine if block is data or trailer, then check permission
    int sector = (cardMemory_.is4K) ? 
        (block < 128 ? block / 4 : 32 + (block - 128) / 16) :
        block / 4;
    
    bool isTrailer = (cardMemory_.is4K) ?
        (block < 128 ? block % 4 == 3 : block % 16 == 15) :
        block % 4 == 3;
    
    if (isTrailer) {
        return canWriteTrailer(sector, kt);
    } else {
        return canWriteDataBlock(sector, kt);
    }
}

// ════════════════════════════════════════════════════════════════════════════════
// Permission Setting
// ════════════════════════════════════════════════════════════════════════════════

void AccessControl::setDataBlockPermissions(int sector,
                                            bool keyARead, bool keyAWrite,
                                            bool keyBRead, bool keyBWrite) {
    // TODO: Encode to access bits and write to trailer
}

void AccessControl::setTrailerPermissions(int sector,
                                          bool keyARead, bool keyAWrite,
                                          bool keyBRead, bool keyBWrite) {
    // TODO: Encode to access bits and write to trailer
}

void AccessControl::applySectorMode(int sector, StandardMode mode) {
    // TODO: Apply standard mode (MODE_0, MODE_1, etc.)
}

// ════════════════════════════════════════════════════════════════════════════════
// Introspection
// ════════════════════════════════════════════════════════════════════════════════

ACCESSBYTES AccessControl::getAccessBits(int sector) const {
    MifareBlock trailer = getTrailer(sector);
    ACCESSBYTES bits;
    std::memcpy(bits.data(), trailer.raw + 6, 4);
    return bits;
}

std::string AccessControl::describePermissions(int sector) const {
    std::ostringstream oss;
    ACCESSBYTES bits = getAccessBits(sector);
    oss << "Sector " << sector << " access bits: ";
    for (size_t i = 0; i < bits.size(); ++i) {
        oss << std::hex << std::setfill('0') << std::setw(2) << (int)bits[i] << " ";
    }
    return oss.str();
}

// ════════════════════════════════════════════════════════════════════════════════
// Internal Helpers
// ════════════════════════════════════════════════════════════════════════════════

MifareBlock AccessControl::getTrailer(int sector) const {
    // Get trailer block for sector from memory layout
    if (cardMemory_.is4K) {
        if (sector < 32) {
            return cardMemory_.data.card4K.detailedNormal.sector[sector].trailerBlock;
        } else {
            return cardMemory_.data.card4K.detailedExtended.sector[sector - 32].trailerBlock;
        }
    } else {
        return cardMemory_.data.card1K.detailed.sector[sector].trailerBlock;
    }
}

bool AccessControl::checkPermissionBit(const ACCESSBYTES& bits, int blockIndex,
                                       KeyType kt, bool isWrite) const {
    // TODO: Implement bit checking logic
    return true;
}

ACCESSBYTES AccessControl::encodeAccessBits(bool dataReadA, bool dataWriteA,
                                            bool dataReadB, bool dataWriteB,
                                            bool trailerReadA, bool trailerWriteA,
                                            bool trailerReadB, bool trailerWriteB) const {
    ACCESSBYTES bits = {0xFF, 0x07, 0x80, 0x69};  // Default MODE_0
    // TODO: Encode permissions to bits
    return bits;
}
