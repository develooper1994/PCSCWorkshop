#ifndef CARDINTERFACE_H
#define CARDINTERFACE_H

#include "CardModel/CardMemoryLayout.h"
#include "CardModel/CardTopology.h"
#include "CardProtocol/AccessControl.h"
#include "CardProtocol/KeyManagement.h"
#include "CardProtocol/AuthenticationState.h"
#include <memory>

// ════════════════════════════════════════════════════════════════════════════════
// CardInterface - Complete Card Management System
// ════════════════════════════════════════════════════════════════════════════════
//
// High-level unified interface combining:
// - CardMemoryLayout (physical memory)
// - CardLayoutTopology (layout queries)
// - AccessControl (permissions)
// - KeyManagement (key storage)
// - AuthenticationState (sessions)
//
// Usage:
// CardInterface card(false);  // 1K card
// card.loadMemory(rawBytes);
// card.authenticate(5, keyA);
// if (card.canRead(block)) { ... }
//
// ════════════════════════════════════════════════════════════════════════════════

class CardInterface {
public:
    // ────────────────────────────────────────────────────────────────────────────
    // Constructor
    // ────────────────────────────────────────────────────────────────────────────

    // Create interface for 1K (is4K=false) or 4K (is4K=true)
    explicit CardInterface(bool is4K = false);

    // ────────────────────────────────────────────────────────────────────────────
    // Memory Management
    // ────────────────────────────────────────────────────────────────────────────

    // Load card memory from raw bytes
    void loadMemory(const BYTE* data, size_t size);

    // Get reference to memory layout
    const CardMemoryLayout& getMemory() const;
    CardMemoryLayout& getMemoryMutable();

    // Export memory to raw bytes
    BYTEV exportMemory() const;

    // ────────────────────────────────────────────────────────────────────────────
    // Key Management
    // ────────────────────────────────────────────────────────────────────────────

    // Register a key
    void registerKey(KeyType kt, const KEYBYTES& key, KeyStructure structure,
                    BYTE slot = 0x01, const std::string& name = "");

    // Get registered key
    const KEYBYTES& getKey(KeyType kt, BYTE slot = 0x01) const;

    // Get card's trailer key
    KEYBYTES getCardKey(int sector, KeyType kt) const;

    // ────────────────────────────────────────────────────────────────────────────
    // Authentication
    // ────────────────────────────────────────────────────────────────────────────

    // Simulate authentication (mark sector as authenticated)
    void authenticate(int sector, KeyType kt);

    // Check if sector is authenticated
    bool isAuthenticated(int sector) const;

    // Check if sector is authenticated with specific key
    bool isAuthenticatedWith(int sector, KeyType kt) const;

    // Deauthenticate sector
    void deauthenticate(int sector);

    // Clear all authentication
    void clearAuthentication();

    // ────────────────────────────────────────────────────────────────────────────
    // Access Control
    // ────────────────────────────────────────────────────────────────────────────

    // Check if can perform operation on block
    bool canRead(int block, KeyType kt) const;
    bool canWrite(int block, KeyType kt) const;

    // Check if can perform operation on data blocks in sector
    bool canReadDataBlocks(int sector, KeyType kt) const;
    bool canWriteDataBlocks(int sector, KeyType kt) const;

    // ────────────────────────────────────────────────────────────────────────────
    // Topology Queries
    // ────────────────────────────────────────────────────────────────────────────

    // Get block information
    MifareBlock& getBlock(int blockNum);
    const MifareBlock& getBlock(int blockNum) const;

    // Get sector information
    int getSectorForBlock(int block) const;
    int getFirstBlockOfSector(int sector) const;
    int getLastBlockOfSector(int sector) const;
    int getTrailerBlockOfSector(int sector) const;

    // Check block type
    bool isManufacturerBlock(int block) const;
    bool isTrailerBlock(int block) const;
    bool isDataBlock(int block) const;

    // ────────────────────────────────────────────────────────────────────────────
    // Card Metadata
    // ────────────────────────────────────────────────────────────────────────────

    // Get card type
    bool is4K() const;
    bool is1K() const;

    // Get card size info
    size_t getTotalMemory() const;
    int getTotalBlocks() const;
    int getTotalSectors() const;

    // ────────────────────────────────────────────────────────────────────────────
    // Utility / Introspection
    // ────────────────────────────────────────────────────────────────────────────

    // Print complete card information
    void printCardInfo() const;

    // Print authentication status
    void printAuthenticationStatus() const;

    // Get UID from manufacturer block
    KEYBYTES getUID() const;

private:
    // Components
    std::unique_ptr<CardMemoryLayout> memory_;
    std::unique_ptr<CardLayoutTopology> topology_;
    std::unique_ptr<AccessControl> accessControl_;
    std::unique_ptr<KeyManagement> keyMgmt_;
    std::unique_ptr<AuthenticationState> authState_;

    bool is4K_;
};

#endif // CARDINTERFACE_H
