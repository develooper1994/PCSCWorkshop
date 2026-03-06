#include "CardInterface.h"
#include "CardModel/CardMemoryLayout.h"
#include "CardModel/CardTopology.h"
#include "CardProtocol/AccessControl.h"
#include "CardProtocol/KeyManagement.h"
#include "CardProtocol/AuthenticationState.h"
#include <iostream>
#include <iomanip>

// ════════════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ════════════════════════════════════════════════════════════════════════════════

CardInterface::~CardInterface() = default;

CardInterface::CardInterface(CardType ct) : cardType_(ct) {
    memory_ = std::make_unique<CardMemoryLayout>(ct);
    topology_ = std::make_unique<CardLayoutTopology>(ct);

    // Classic-specific bileşenler; Ultralight'ta kullanılmaz ama
    // oluşturmak zararsızdır — facade kısa devre yapar.
    accessControl_ = std::make_unique<AccessControl>(*memory_);
    keyMgmt_ = std::make_unique<KeyManagement>(*memory_);
    authState_ = std::make_unique<AuthenticationState>(*memory_);
}

CardInterface::CardInterface(bool is4K)
    : CardInterface(is4K ? CardType::MifareClassic4K
                         : CardType::MifareClassic1K) {}

// ════════════════════════════════════════════════════════════════════════════════
// Memory Management
// ════════════════════════════════════════════════════════════════════════════════

void CardInterface::loadMemory(const BYTE* data, size_t size) {
    if (size != memory_->memorySize()) {
        throw std::invalid_argument("Invalid memory size for card type");
    }
    std::memcpy(memory_->getRawMemory(), data, size);
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
    if (isUltralight()) return;   // Ultralight'ta auth yok
    topology_->validateSector(sector);
    authState_->markAuthenticated(sector, kt);
}

bool CardInterface::isAuthenticated(int sector) const {
    if (isUltralight()) return true;
    return authState_->isAuthenticated(sector);
}

bool CardInterface::isAuthenticatedWith(int sector, KeyType kt) const {
    if (isUltralight()) return true;
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
    if (isUltralight()) return true;
    return accessControl_->canRead(block, kt);
}

bool CardInterface::canWrite(int block, KeyType kt) const {
    if (isUltralight()) return true;
    return accessControl_->canWrite(block, kt);
}

bool CardInterface::canReadDataBlocks(int sector, KeyType kt) const {
    if (isUltralight()) return true;
    return accessControl_->canReadDataBlock(sector, kt);
}

bool CardInterface::canWriteDataBlocks(int sector, KeyType kt) const {
    if (isUltralight()) return true;
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

CardType CardInterface::getCardType() const {
    return cardType_;
}

bool CardInterface::is4K() const {
    return cardType_ == CardType::MifareClassic4K;
}

bool CardInterface::is1K() const {
    return cardType_ == CardType::MifareClassic1K;
}

bool CardInterface::isUltralight() const {
    return cardType_ == CardType::MifareUltralight;
}

bool CardInterface::isClassic() const {
    return is1K() || is4K();
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

    const char* typeName = "Unknown";
    switch (cardType_) {
        case CardType::MifareClassic1K:  typeName = "Classic 1K"; break;
        case CardType::MifareClassic4K:  typeName = "Classic 4K"; break;
        case CardType::MifareUltralight: typeName = "Ultralight"; break;
        default: break;
    }
    std::cout << "Card Type: " << typeName << "\n";
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
    KEYBYTES uid{};
    if (isUltralight()) {
        // Ultralight UID: page0[0-2] + page1[0-3] = 7 byte, ilk 4'ü döndür
        const auto& pg0 = memory_->data.ultralight.detailed.serial0;
        uid[0] = pg0.serial0.sn0;
        uid[1] = pg0.serial0.sn1;
        uid[2] = pg0.serial0.sn2;
        const auto& pg1 = memory_->data.ultralight.detailed.serial1;
        uid[3] = pg1.serial1.sn3;
        uid[4] = pg1.serial1.sn4;
        uid[5] = pg1.serial1.sn5;
    } else {
        const MifareBlock& mfg = memory_->getBlock(0);
        std::copy(mfg.manufacturer.uid, mfg.manufacturer.uid + 4, uid.begin());
        uid[4] = 0;
        uid[5] = 0;
    }
    return uid;
}
