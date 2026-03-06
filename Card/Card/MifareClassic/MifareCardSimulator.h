#ifndef MIFARECARDSIMULATOR_H
#define MIFARECARDSIMULATOR_H

#include "../CardDataTypes.h"
#include <array>
#include <vector>
#include <map>
#include <cstring>
#include <stdexcept>

// ════════════════════════════════════════════════════════════════════════════════
// MifareCardSimulator — In-Memory Mifare Classic Card Simulation
// ════════════════════════════════════════════════════════════════════════════════
//
// Provides a complete in-memory simulation of a Mifare Classic 1K/4K card.
// Includes:
// - Block storage (all sectors and blocks)
// - Authentication state management
// - Key tracking
// - Realistic read/write simulation
//
// Usage:
//   MifareCardSimulator card(false);  // false = 1K, true = 4K
//   card.setKey(KeyType::A, {...}, 0x01);
//   card.authenticate(0, KeyType::A, 0x01);
//   auto data = card.readBlock(0, 1);
//
// ════════════════════════════════════════════════════════════════════════════════
class MifareCardSimulator {
public:
    // ────────────────────────────────────────────────────────────────────────────
    // Constructor
    // ────────────────────────────────────────────────────────────────────────────
    
    // Create simulator for 1K (is4K=false) or 4K (is4K=true) card
    // Initializes with default manufacturer block and FF...FF keys
    explicit MifareCardSimulator(bool is4K = false);

    // ────────────────────────────────────────────────────────────────────────────
    // Card Layout Information
    // ────────────────────────────────────────────────────────────────────────────
    
    // Get card type
    bool is4K() const noexcept { return is4K_; }
    
    // Get number of sectors (16 for 1K, 40 for 4K)
    size_t sectorCount() const noexcept { return sectorCount_; }
    
    // Get total memory in bytes
    size_t memorySize() const noexcept { return is4K_ ? 4096 : 1024; }

    // ────────────────────────────────────────────────────────────────────────────
    // Key Management (Host-Side)
    // ────────────────────────────────────────────────────────────────────────────
    
    // Register a key for authentication
    void setKey(KeyType kt, const KEYBYTES& key, BYTE slot) noexcept;
    
    // Get registered key
    const KEYBYTES& getKey(KeyType kt) const;
    
    // Check if key is registered
    bool hasKey(KeyType kt) const noexcept;

    // ────────────────────────────────────────────────────────────────────────────
    // Card Content Management
    // ────────────────────────────────────────────────────────────────────────────
    
    // Read block from card memory
    // Throws if block invalid or authentication required
    BLOCK readBlock(int block) const;
    
    // Write block to card memory
    // Throws if block is manufacturer/trailer/invalid or auth required
    void writeBlock(int block, const BYTE data[16]);
    
    // Read sector trailer
    BLOCK readTrailer(int sector) const;
    
    // Write sector trailer (full 16 bytes with keys and access bits)
    void writeTrailer(int sector, const BYTE trailer[16]);
    
    // Get complete card memory (all blocks)
    std::vector<BYTE> dumpMemory() const;
    
    // Restore card memory from dump
    void restoreMemory(const std::vector<BYTE>& dump);

    // ────────────────────────────────────────────────────────────────────────────
    // Authentication & Authorization
    // ────────────────────────────────────────────────────────────────────────────
    
    // Authenticate to a sector with given key
    // Throws if authentication fails
    void authenticate(int sector, KeyType kt, BYTE keySlot);
    
    // Check if sector is authenticated
    bool isAuthenticated(int sector, KeyType kt, BYTE keySlot) const noexcept;
    
    // Clear authentication state (reset)
    void clearAuthentication() noexcept;

    // ────────────────────────────────────────────────────────────────────────────
    // Card Properties Access (Introspection)
    // ────────────────────────────────────────────────────────────────────────────
    
    // Get UID (from manufacturer block)
    BYTEV getUID() const;
    
    // Set UID (manufacturer block bytes 0-3)
    void setUID(const BYTEV& uid);
    
    // Get/set manufacturer data (manufacturer block bytes 4-15)
    BYTEV getManufacturerData() const;
    void setManufacturerData(const BYTEV& data);

    // ────────────────────────────────────────────────────────────────────────────
    // Debug & Inspection
    // ────────────────────────────────────────────────────────────────────────────
    
    // Print entire card memory (hex)
    void printMemory() const;
    
    // Print sector information
    void printSector(int sector) const;
    
    // Print all trailers
    void printAllTrailers() const;

private:
    // ────────────────────────────────────────────────────────────────────────────
    // Internal State
    // ────────────────────────────────────────────────────────────────────────────
    
    bool is4K_;
    size_t sectorCount_;
    
    // Card memory: all blocks
    std::vector<BYTE> memory_;
    
    // Registered keys (host-side)
    std::map<KeyType, std::pair<KEYBYTES, BYTE>> keys_;  // key type -> (key data, slot)
    
    // Authentication state: sector -> authenticated key type
    std::map<int, KeyType> authState_;

    // ────────────────────────────────────────────────────────────────────────────
    // Layout Helpers
    // ────────────────────────────────────────────────────────────────────────────
    
    int sectorFromBlock(int block) const noexcept;
    int blocksPerSector(int sector) const noexcept;
    int firstBlockOfSector(int sector) const noexcept;
    int trailerBlockOfSector(int sector) const noexcept;
    bool isManufacturerBlock(int block) const noexcept;
    bool isTrailerBlock(int block) const noexcept;
    
    // Byte offset in memory for given block
    size_t blockOffset(int block) const noexcept;
    
    // Validation
    void validateBlock(int block) const;
    void validateSector(int sector) const;
    
    // Authorization check
    void checkAuthorization(int block, KeyType requiredKey) const;

    // ────────────────────────────────────────────────────────────────────────────
    // Initialization
    // ────────────────────────────────────────────────────────────────────────────
    
    // Initialize card with default values
    void initializeCard();
    
    // Set manufacturer block (UID, BCC, etc.)
    void initializeManufacturerBlock();
    
    // Set all trailers with default keys (FF FF FF FF FF FF)
    void initializeTrailers();
};

#endif // MIFARECARDSIMULATOR_H
