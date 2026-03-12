#include "CardInterface.h"
#include "CardModel/CardMemoryLayout.h"
#include "CardModel/CardTopology.h"
#include "CardModel/DesfireMemoryLayout.h"
#include "CardProtocol/AccessControl.h"
#include "CardProtocol/KeyManagement.h"
#include "CardProtocol/AuthenticationState.h"
#include "Result.h"
#include <iostream>
#include <iomanip>

// ════════════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ════════════════════════════════════════════════════════════════════════════════

CardInterface::~CardInterface() = default;

CardInterface::CardInterface(CardType ct) : cardType_(ct) {
    memory_ = std::make_unique<CardMemoryLayout>(ct);
    topology_ = std::make_unique<CardLayoutTopology>(ct);

    if (ct == CardType::MifareDesfire) {
        desfire_ = std::make_unique<DesfireMemoryLayout>();
    } else {
        accessControl_ = std::make_unique<AccessControl>(*memory_);
        keyMgmt_ = std::make_unique<KeyManagement>(*memory_);
        authState_ = std::make_unique<AuthenticationState>(*memory_);
    }
}

CardInterface::CardInterface(bool is4K)
    : CardInterface(is4K ? CardType::MifareClassic4K
                         : CardType::MifareClassic1K) {}

// ════════════════════════════════════════════════════════════════════════════════
// Memory Management
// ════════════════════════════════════════════════════════════════════════════════

void CardInterface::loadMemory(const BYTE* data, size_t size) {
    if (isDesfire()) {
		PcscError::make(CardError::NotDesfire,
            "DESFire: use DesfireMemoryLayout, not loadMemory()").throwIfError();
        return;
    }
    else if (size != memory_->memorySize()) {
		PcscError::make(CardError::InvalidData,
            "Invalid memory size for card type").throwIfError();
        return;
    }
    else std::memcpy(memory_->getRawMemory(), data, size);
}

const CardMemoryLayout& CardInterface::getMemory() const {
    return *memory_;
}

CardMemoryLayout& CardInterface::getMemoryMutable() {
    return *memory_;
}

BYTEV CardInterface::exportMemory() const {
    if (isDesfire()) {
		PcscError::make(CardError::NotDesfire,
            "DESFire: no flat memory export").throwIfError();
        return {};
    }
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
	if (isUltralight() || isDesfire()) return;
    topology_->validateSector(sector);
    authState_->markAuthenticated(sector, kt);
}

bool CardInterface::isAuthenticated(int sector) const {
	return isUltralight() ? true : authState_->isAuthenticated(sector);
}

bool CardInterface::isAuthenticatedWith(int sector, KeyType kt) const {
	return isUltralight() ? true : authState_->isAuthenticatedWith(sector, kt);
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
	return isUltralight() || isDesfire() ? true : accessControl_->canRead(block, kt);
}

bool CardInterface::canWrite(int block, KeyType kt) const {
	return isUltralight() || isDesfire() ? true : accessControl_->canWrite(block, kt);
}

bool CardInterface::canReadDataBlocks(int sector, KeyType kt) const {
	return isUltralight() || isDesfire() ? true : accessControl_->canReadDataBlock(sector, kt);
}

bool CardInterface::canWriteDataBlocks(int sector, KeyType kt) const {
	return isUltralight() || isDesfire() ? true : accessControl_->canWriteDataBlock(sector, kt);
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

bool CardInterface::isDesfire() const {
    return cardType_ == CardType::MifareDesfire;
}

size_t CardInterface::getTotalMemory() const {
    if (isDesfire()) return desfire_ ? desfire_->totalMemory : 0;
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
    std::cout << "============================================\n";
    std::cout << "  CARD INFORMATION\n";
    std::cout << "============================================\n";

    const char* typeName = "Unknown";
    switch (cardType_) {
        case CardType::MifareClassic1K:  typeName = "Classic 1K"; break;
        case CardType::MifareClassic4K:  typeName = "Classic 4K"; break;
        case CardType::MifareUltralight: typeName = "Ultralight"; break;
        case CardType::MifareDesfire:    typeName = "DESFire"; break;
        default: break;
    }
    std::cout << "Card Type: " << typeName << "\n"
			  << "Total Memory: " << getTotalMemory() << " bytes\n";
    if (!isDesfire()) {
        std::cout << "Total Blocks: " << getTotalBlocks() << "\n"
				  << "Total Sectors: " << getTotalSectors() << "\n";
    } else if (desfire_) {
        std::cout << "Applications: " << desfire_->applications.size() << "\n";
    }

    std::cout << "\nUID: ";
    KEYBYTES uid = getUID();
    for (BYTE b : uid) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)b << " ";
    }
    std::cout << std::dec << "\n";

    std::cout << "============================================\n";
}

void CardInterface::printAuthenticationStatus() const {
    if (authState_) authState_->printAuthenticationStatus();
}

KEYBYTES CardInterface::getUID() const {
    KEYBYTES uid{};
    if (isDesfire()) {
        if (desfire_) {
            for (int i = 0; i < 6; ++i)
                uid[i] = desfire_->versionInfo.uid[i];
        }
        return uid;
    }
    else if (isUltralight()) {
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

// ════════════════════════════════════════════════════════════════════════════════
// DESFire Memory Access
// ════════════════════════════════════════════════════════════════════════════════

const DesfireMemoryLayout& CardInterface::getDesfireMemory() const {
    if (!desfire_) {
		PcscError::make(CardError::NotDesfire, "Not a DESFire card").throwIfError();
    }
    return *desfire_;
}

DesfireMemoryLayout& CardInterface::getDesfireMemoryMutable() {
    if (!desfire_) {
		PcscError::make(CardError::NotDesfire, "Not a DESFire card").throwIfError();
    }
    return *desfire_;
}
