#include "SimulatedReader.h"
#include <iostream>
#include <iomanip>

// ════════════════════════════════════════════════════════════════════════════════
// Construction
// ════════════════════════════════════════════════════════════════════════════════

SimulatedReader::SimulatedReader(std::shared_ptr<MifareCardSimulator> card)
    : card_(card) {
    if (!card_) {
        throw std::invalid_argument("Card simulator cannot be null");
    }
}

// ════════════════════════════════════════════════════════════════════════════════
// Reader Interface Implementation
// ════════════════════════════════════════════════════════════════════════════════

void SimulatedReader::loadKey(const BYTE* key, KeyStructure ks, BYTE slot) {
    if (!key) {
        throw std::invalid_argument("Key pointer is null");
    }
    
    // Convert raw key to KEYBYTES and register with card
    KEYBYTES keyBytes;
    std::copy(key, key + 6, keyBytes.begin());
    card_->setKey(currentKeyType_, keyBytes, slot);
}

void SimulatedReader::setKeyType(KeyType kt) {
    currentKeyType_ = kt;
}

void SimulatedReader::setKeyNumber(BYTE slot) {
    currentKeySlot_ = slot;
}

void SimulatedReader::setAuthRequested(bool requested) {
    authRequested_ = requested;
}

void SimulatedReader::performAuth(BYTE block) {
    if (!authRequested_) {
        return;
    }
    
    currentBlock_ = block;
    currentSector_ = sectorFromBlock(block);
    
    // Attempt authentication with current key
    try {
        card_->authenticate(currentSector_, currentKeyType_, currentKeySlot_);
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("Authentication failed: ") + e.what());
    }
    
    authRequested_ = false;
}

BYTEV SimulatedReader::readPage(BYTE block) {
    if (currentSector_ == -1) {
        throw std::runtime_error("No sector authenticated");
    }
    
    BLOCK blockData = card_->readBlock(block);
    BYTEV result(blockData.begin(), blockData.end());
    return result;
}

void SimulatedReader::writePage(BYTE block, const BYTE* data) {
    if (!data) {
        throw std::invalid_argument("Data pointer is null");
    }
    
    if (currentSector_ == -1) {
        throw std::runtime_error("No sector authenticated");
    }
    
    card_->writeBlock(block, data);
}

bool SimulatedReader::isConnected() const noexcept {
    return true;  // Simulated reader is always connected
}

// ════════════════════════════════════════════════════════════════════════════════
// Test/Debug Methods
// ════════════════════════════════════════════════════════════════════════════════

void SimulatedReader::reset() noexcept {
    authRequested_ = false;
    currentBlock_ = -1;
    currentSector_ = -1;
    card_->clearAuthentication();
}

void SimulatedReader::printState() const {
    std::cout << "=== SimulatedReader State ===\n";
    std::cout << "Connected: " << (isConnected() ? "Yes" : "No") << "\n";
    std::cout << "Current Key Type: " << (currentKeyType_ == KeyType::A ? "A" : "B") << "\n";
    std::cout << "Current Key Slot: " << static_cast<int>(currentKeySlot_) << "\n";
    std::cout << "Auth Requested: " << (authRequested_ ? "Yes" : "No") << "\n";
    std::cout << "Current Block: " << currentBlock_ << "\n";
    std::cout << "Current Sector: " << currentSector_ << "\n";
}

// ════════════════════════════════════════════════════════════════════════════════
// Helper Methods
// ════════════════════════════════════════════════════════════════════════════════

int SimulatedReader::sectorFromBlock(int block) const noexcept {
    bool is4K = card_->is4K();
    return (!is4K || block < 128) ? block / 4 : 32 + (block - 128) / 16;
}
