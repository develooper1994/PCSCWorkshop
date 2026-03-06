#include "MifareCardSimulator.h"
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <cstring>

// ════════════════════════════════════════════════════════════════════════════════
// Construction & Initialization
// ════════════════════════════════════════════════════════════════════════════════

MifareCardSimulator::MifareCardSimulator(bool is4K)
    : is4K_(is4K),
      sectorCount_(is4K ? 40 : 16),
      memory_(is4K ? 4096 : 1024, 0) {
    
    // Register default keys (FF...FF)
    KEYBYTES defaultKey = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    setKey(KeyType::A, defaultKey, 0x01);
    setKey(KeyType::B, defaultKey, 0x02);
    
    // Initialize card with default layout
    initializeCard();
}

void MifareCardSimulator::initializeCard() {
    // Fill all data blocks with 0x00
    std::fill(memory_.begin(), memory_.end(), 0x00);
    
    // Set manufacturer block
    initializeManufacturerBlock();
    
    // Set all trailers with default keys and access bits
    initializeTrailers();
}

void MifareCardSimulator::initializeManufacturerBlock() {
    // Block 0: Manufacturer block (read-only)
    // Bytes 0-3: UID
    // Bytes 4-15: Manufacturer data
    
    BLOCK uid;
    uid[0] = 0x11;
    uid[1] = 0x22;
    uid[2] = 0x33;
    uid[3] = 0x44;
    uid[4] = 0x55;  // BCC
    
    std::memcpy(memory_.data(), uid.data(), 16);
}

void MifareCardSimulator::initializeTrailers() {
    // Set all sector trailers with default keys (FF...FF)
    // Access bits: 0xFF 0x07 0x80 0x69 (standard, KeyB writable)
    
    const BYTE defaultTrailer[16] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // KeyA
        0xFF, 0x07, 0x80, 0x69,              // Access bytes + GPB
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF   // KeyB
    };
    
    for (size_t sector = 0; sector < sectorCount_; ++sector) {
        int trailerBlock = trailerBlockOfSector(sector);
        size_t offset = blockOffset(trailerBlock);
        std::memcpy(memory_.data() + offset, defaultTrailer, 16);
    }
}

// ════════════════════════════════════════════════════════════════════════════════
// Key Management
// ════════════════════════════════════════════════════════════════════════════════

void MifareCardSimulator::setKey(KeyType kt, const KEYBYTES& key, BYTE slot) noexcept {
    keys_[kt] = { key, slot };
}

const KEYBYTES& MifareCardSimulator::getKey(KeyType kt) const {
    auto it = keys_.find(kt);
    if (it == keys_.end()) {
        throw std::runtime_error("Key not found");
    }
    return it->second.first;
}

bool MifareCardSimulator::hasKey(KeyType kt) const noexcept {
    return keys_.find(kt) != keys_.end();
}

// ════════════════════════════════════════════════════════════════════════════════
// Card Content Management
// ════════════════════════════════════════════════════════════════════════════════

BLOCK MifareCardSimulator::readBlock(int block) const {
    validateBlock(block);
    
    if (isManufacturerBlock(block)) {
        // Manufacturer block is always readable
    } else if (isTrailerBlock(block)) {
        // Trailer readable but keys are masked (depends on auth)
        // For simulation, we allow reading trailers
    } else {
        // Data block requires authentication
        int sector = sectorFromBlock(block);
        checkAuthorization(block, KeyType::A);  // Simple check
    }
    
    size_t offset = blockOffset(block);
    BLOCK result;
    std::memcpy(result.data(), memory_.data() + offset, 16);
    return result;
}

void MifareCardSimulator::writeBlock(int block, const BYTE data[16]) {
    validateBlock(block);
    
    // Check if block is writeable
    if (isManufacturerBlock(block)) {
        throw std::runtime_error("Cannot write to manufacturer block");
    }
    
    if (isTrailerBlock(block)) {
        throw std::runtime_error("Use writeTrailer() for trailer blocks");
    }
    
    // Data block write requires authentication
    int sector = sectorFromBlock(block);
    checkAuthorization(block, KeyType::B);  // Write requires specific key
    
    size_t offset = blockOffset(block);
    std::memcpy(memory_.data() + offset, data, 16);
}

BLOCK MifareCardSimulator::readTrailer(int sector) const {
    validateSector(sector);
    int trailerBlock = trailerBlockOfSector(sector);
    size_t offset = blockOffset(trailerBlock);
    
    BLOCK result;
    std::memcpy(result.data(), memory_.data() + offset, 16);
    return result;
}

void MifareCardSimulator::writeTrailer(int sector, const BYTE trailer[16]) {
    validateSector(sector);
    
    // Trailer write requires authentication with KeyB
    checkAuthorization(trailerBlockOfSector(sector), KeyType::B);
    
    int trailerBlock = trailerBlockOfSector(sector);
    size_t offset = blockOffset(trailerBlock);
    std::memcpy(memory_.data() + offset, trailer, 16);
    
    // Invalidate auth after trailer write
    authState_.erase(sector);
}

std::vector<BYTE> MifareCardSimulator::dumpMemory() const {
    return memory_;
}

void MifareCardSimulator::restoreMemory(const std::vector<BYTE>& dump) {
    if (dump.size() != memory_.size()) {
        throw std::invalid_argument("Memory dump size mismatch");
    }
    memory_ = dump;
    authState_.clear();  // Reset auth after restore
}

// ════════════════════════════════════════════════════════════════════════════════
// Authentication & Authorization
// ════════════════════════════════════════════════════════════════════════════════

void MifareCardSimulator::authenticate(int sector, KeyType kt, BYTE keySlot) {
    validateSector(sector);
    
    // Check if key matches
    if (!hasKey(kt)) {
        throw std::runtime_error("Key not registered");
    }
    
    const auto& keyInfo = keys_[kt];
    if (keyInfo.second != keySlot) {
        throw std::runtime_error("Key slot mismatch");
    }
    
    // Get key from trailer and compare
    BLOCK trailer = readTrailer(sector);
    const BYTE* trailerKeyPos = nullptr;
    
    if (kt == KeyType::A) {
        trailerKeyPos = trailer.data();  // Bytes 0-5
    } else {
        trailerKeyPos = trailer.data() + 10;  // Bytes 10-15
    }
    
    // Compare keys
    if (std::memcmp(trailerKeyPos, keyInfo.first.data(), 6) != 0) {
        throw std::runtime_error("Authentication failed: key mismatch");
    }
    
    // Authentication successful
    authState_[sector] = kt;
}

bool MifareCardSimulator::isAuthenticated(int sector, KeyType kt, BYTE keySlot) const noexcept {
    auto it = authState_.find(sector);
    if (it == authState_.end()) {
        return false;
    }
    return it->second == kt;
}

void MifareCardSimulator::clearAuthentication() noexcept {
    authState_.clear();
}

// ════════════════════════════════════════════════════════════════════════════════
// Card Properties Access
// ════════════════════════════════════════════════════════════════════════════════

BYTEV MifareCardSimulator::getUID() const {
    BYTEV uid;
    uid.assign(memory_.begin(), memory_.begin() + 4);
    return uid;
}

void MifareCardSimulator::setUID(const BYTEV& uid) {
    if (uid.size() != 4) {
        throw std::invalid_argument("UID must be 4 bytes");
    }
    std::memcpy(memory_.data(), uid.data(), 4);
}

BYTEV MifareCardSimulator::getManufacturerData() const {
    BYTEV data;
    data.assign(memory_.begin() + 4, memory_.begin() + 16);
    return data;
}

void MifareCardSimulator::setManufacturerData(const BYTEV& data) {
    if (data.size() != 12) {
        throw std::invalid_argument("Manufacturer data must be 12 bytes");
    }
    std::memcpy(memory_.data() + 4, data.data(), 12);
}

// ════════════════════════════════════════════════════════════════════════════════
// Layout Helpers
// ════════════════════════════════════════════════════════════════════════════════

int MifareCardSimulator::sectorFromBlock(int block) const noexcept {
    return (!is4K_ || block < 128) ? block / 4 : 32 + (block - 128) / 16;
}

int MifareCardSimulator::blocksPerSector(int sector) const noexcept {
    return (!is4K_ || sector < 32) ? 4 : 16;
}

int MifareCardSimulator::firstBlockOfSector(int sector) const noexcept {
    return (!is4K_ || sector < 32) ? sector * 4 : 128 + (sector - 32) * 16;
}

int MifareCardSimulator::trailerBlockOfSector(int sector) const noexcept {
    return firstBlockOfSector(sector) + blocksPerSector(sector) - 1;
}

bool MifareCardSimulator::isManufacturerBlock(int block) const noexcept {
    return block == 0;
}

bool MifareCardSimulator::isTrailerBlock(int block) const noexcept {
    int sector = sectorFromBlock(block);
    return block == trailerBlockOfSector(sector);
}

size_t MifareCardSimulator::blockOffset(int block) const noexcept {
    return static_cast<size_t>(block) * 16;
}

// ════════════════════════════════════════════════════════════════════════════════
// Validation & Authorization Checks
// ════════════════════════════════════════════════════════════════════════════════

void MifareCardSimulator::validateBlock(int block) const {
    int maxBlock = is4K_ ? 255 : 63;
    if (block < 0 || block > maxBlock) {
        throw std::out_of_range("Block number out of range");
    }
}

void MifareCardSimulator::validateSector(int sector) const {
    if (sector < 0 || sector >= static_cast<int>(sectorCount_)) {
        throw std::out_of_range("Sector number out of range");
    }
}

void MifareCardSimulator::checkAuthorization(int block, KeyType requiredKey) const {
    int sector = sectorFromBlock(block);
    
    auto it = authState_.find(sector);
    if (it == authState_.end()) {
        throw std::runtime_error("Sector not authenticated");
    }
    
    if (it->second != requiredKey) {
        throw std::runtime_error("Wrong key type for operation");
    }
}

// ════════════════════════════════════════════════════════════════════════════════
// Debug & Inspection
// ════════════════════════════════════════════════════════════════════════════════

void MifareCardSimulator::printMemory() const {
    std::cout << "=== Mifare " << (is4K_ ? "4K" : "1K") << " Card Memory ===\n";
    for (size_t i = 0; i < memory_.size(); i += 16) {
        std::cout << "Block " << std::setw(3) << (i / 16) << ": ";
        for (size_t j = 0; j < 16 && i + j < memory_.size(); ++j) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(memory_[i + j]) << " ";
        }
        std::cout << std::dec << "\n";
    }
}

void MifareCardSimulator::printSector(int sector) const {
    validateSector(sector);
    std::cout << "=== Sector " << sector << " ===\n";
    
    int firstBlock = firstBlockOfSector(sector);
    int blockCount = blocksPerSector(sector);
    
    for (int i = 0; i < blockCount; ++i) {
        int block = firstBlock + i;
        size_t offset = blockOffset(block);
        
        std::string blockType;
        if (block == 0) blockType = "[MFGR]";
        else if (block == trailerBlockOfSector(sector)) blockType = "[TRLR]";
        else blockType = "[DATA]";
        
        std::cout << "Block " << std::setw(3) << block << " " << blockType << ": ";
        for (size_t j = 0; j < 16; ++j) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(memory_[offset + j]) << " ";
        }
        std::cout << std::dec << "\n";
    }
}

void MifareCardSimulator::printAllTrailers() const {
    std::cout << "=== All Sector Trailers ===\n";
    for (size_t i = 0; i < sectorCount_; ++i) {
        BLOCK trailer = readTrailer(i);
        std::cout << "Sector " << std::setw(2) << i << ": ";
        for (size_t j = 0; j < 16; ++j) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(trailer[j]) << " ";
        }
        std::cout << std::dec << "\n";
    }
}
