#include "CardInterface.h"
#include <iostream>
#include <iomanip>

// ════════════════════════════════════════════════════════════════════════════════
// Construction
// ════════════════════════════════════════════════════════════════════════════════

CardInterface::CardInterface(bool is4K) : is4K_(is4K) {
    memory_ = std::make_unique<CardMemoryLayout>(is4K);
    topology_ = std::make_unique<CardTopology>(is4K);
    accessControl_ = std::make_unique<AccessControl>(*memory_);
    keyMgmt_ = std::make_unique<KeyManagement>(*memory_);
    authState_ = std::make_unique<AuthenticationState>(*memory_);
}

// ════════════════════════════════════════════════════════════════════════════════
// Memory Management
// ════════════════════════════════════════════════════════════════════════════════

void CardInterface::loadMemory(const BYTE* data, size_t size) {
    size_t expectedSize = is4K_ ? 4096 : 1024;
    if (size != expectedSize) {
        throw std::invalid_argument("Invalid memory size for card type");
    }
    
    if (is4K_) {
        std::memcpy(memory_->data.card4K.raw, data, 4096);
    } else {
        std::memcpy(memory_->data.card1K.raw, data, 1024);
    }
}

const CardMemoryLayout& CardInterface::getMemory() const {
    return *memory_;
}

CardMemoryLayout& CardInterface::getMemoryMutable() {
    return *memory_;
}

BYTEV CardInterface::exportMemory() const {
    const BYTE* rawPtr = memory_->getRawMemory();
    size_t size = memory_->memorySize();
    return BYTEV(rawPtr, rawPtr + size);
}

// ════════════════════════════════════════════════════════════════════════════════
// Key Management
// ════════════════════════════════════════════════════════════════════════════════

void CardInterface::registerKey(KeyType kt, const KEYBYTES& key,
                                KeyStructure structure, BYTE slot,
                                const std::string& name) {
    keyMgmt_->registerKey(kt, key, structure, slot, name);
}

const KEYBYTES& CardInterface::getKey(KeyType kt, BYTE slot) const {
    return keyMgmt_->getKey(kt, slot);
}

KEYBYTES CardInterface::getCardKey(int sector, KeyType kt) const {
    return keyMgmt_->getCardKey(sector, kt);
}

// ════════════════════════════════════════════════════════════════════════════════
// Authentication
// ════════════════════════════════════════════════════════════════════════════════

void CardInterface::authenticate(int sector, KeyType kt) {
    topology_->validateSector(sector);
    authState_->markAuthenticated(sector, kt);
}

bool CardInterface::isAuthenticated(int sector) const {
    return authState_->isAuthenticated(sector);
}

bool CardInterface::isAuthenticatedWith(int sector, KeyType kt) const {
    return authState_->isAuthenticatedWith(sector, kt);
}

void CardInterface::deauthenticate(int sector) {
    authState_->invalidate(sector);
}

void CardInterface::clearAuthentication() {
    authState_->clearAll();
}

// ════════════════════════════════════════════════════════════════════════════════
// Access Control
// ════════════════════════════════════════════════════════════════════════════════

bool CardInterface::canRead(int block, KeyType kt) const {
    return accessControl_->canRead(block, kt);
}

bool CardInterface::canWrite(int block, KeyType kt) const {
    return accessControl_->canWrite(block, kt);
}

bool CardInterface::canReadDataBlocks(int sector, KeyType kt) const {
    return accessControl_->canReadDataBlock(sector, kt);
}

bool CardInterface::canWriteDataBlocks(int sector, KeyType kt) const {
    return accessControl_->canWriteDataBlock(sector, kt);
}

// ════════════════════════════════════════════════════════════════════════════════
// Topology Queries
// ════════════════════════════════════════════════════════════════════════════════

MifareBlock& CardInterface::getBlock(int blockNum) {
    topology_->validateBlock(blockNum);
    return memory_->getBlock(blockNum);
}

const MifareBlock& CardInterface::getBlock(int blockNum) const {
    topology_->validateBlock(blockNum);
    return memory_->getBlock(blockNum);
}

int CardInterface::getSectorForBlock(int block) const {
    return topology_->sectorFromBlock(block);
}

int CardInterface::getFirstBlockOfSector(int sector) const {
    return topology_->firstBlockOfSector(sector);
}

int CardInterface::getLastBlockOfSector(int sector) const {
    return topology_->lastBlockOfSector(sector);
}

int CardInterface::getTrailerBlockOfSector(int sector) const {
    return topology_->trailerBlockOfSector(sector);
}

bool CardInterface::isManufacturerBlock(int block) const {
    return topology_->isManufacturerBlock(block);
}

bool CardInterface::isTrailerBlock(int block) const {
    return topology_->isTrailerBlock(block);
}

bool CardInterface::isDataBlock(int block) const {
    return topology_->isDataBlock(block);
}

// ════════════════════════════════════════════════════════════════════════════════
// Card Metadata
// ════════════════════════════════════════════════════════════════════════════════

bool CardInterface::is4K() const {
    return is4K_;
}

bool CardInterface::is1K() const {
    return !is4K_;
}

size_t CardInterface::getTotalMemory() const {
    return memory_->memorySize();
}

int CardInterface::getTotalBlocks() const {
    return topology_->totalBlocks();
}

int CardInterface::getTotalSectors() const {
    return topology_->sectorCount();
}

// ════════════════════════════════════════════════════════════════════════════════
// Utility / Introspection
// ════════════════════════════════════════════════════════════════════════════════

void CardInterface::printCardInfo() const {
    std::cout << "════════════════════════════════════════════\n";
    std::cout << "  CARD INFORMATION\n";
    std::cout << "════════════════════════════════════════════\n";
    std::cout << "Card Type: " << (is4K_ ? "4K" : "1K") << "\n";
    std::cout << "Total Memory: " << getTotalMemory() << " bytes\n";
    std::cout << "Total Blocks: " << getTotalBlocks() << "\n";
    std::cout << "Total Sectors: " << getTotalSectors() << "\n";
    
    std::cout << "\nUID: ";
    KEYBYTES uid = getUID();
    for (BYTE b : uid) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)b << " ";
    }
    std::cout << std::dec << "\n";
    
    std::cout << "════════════════════════════════════════════\n";
}

void CardInterface::printAuthenticationStatus() const {
    authState_->printAuthenticationStatus();
}

KEYBYTES CardInterface::getUID() const {
    KEYBYTES uid;
    const MifareBlock& mfg = memory_->getBlock(0);
    std::copy(mfg.manufacturer.uid, mfg.manufacturer.uid + 4, uid.begin());
    uid[4] = 0;
    uid[5] = 0;
    return uid;
}
