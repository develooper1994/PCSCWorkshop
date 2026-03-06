// CardSystemTests.cpp - Card System Test Suite
// Tests for CardMemoryLayout, CardTopology, KeyManagement, AccessControl, AuthenticationState, CardInterface

#include "../Card/Card/CardModel/CardMemoryLayout.h"
#include "../Card/Card/CardModel/CardTopology.h"
#include "../Card/Card/CardProtocol/AccessControl.h"
#include "../Card/Card/CardProtocol/KeyManagement.h"
#include "../Card/Card/CardProtocol/AuthenticationState.h"
#include "../Card/Card/CardInterface.h"
#include <iostream>
#include <cstring>

using namespace std;

// ════════════════════════════════════════════════════════════════════════════════
// Card Memory Layout Tests
// ════════════════════════════════════════════════════════════════════════════════

bool testCardMemoryLayout1K() {
    try {
        // Just test basic construction
        CardMemoryLayout cardMem(false);
        
        // Verify is4K flag
        if (cardMem.is4K != false) {
            cerr << "ERROR: is4K should be false\n";
            return false;
        }
        
        // Verify memory size
        if (cardMem.memorySize() != 1024) {
            cerr << "ERROR: memorySize should be 1024, got " << cardMem.memorySize() << "\n";
            return false;
        }
        
        // Try to access raw memory
        try {
            BYTE* raw = cardMem.getRawMemory();
            if (raw == nullptr) {
                cerr << "ERROR: getRawMemory returned null\n";
                return false;
            }
        }
        catch (...) {
            cerr << "ERROR: getRawMemory threw exception\n";
            return false;
        }
        
        return true;
    }
    catch (const exception& e) {
        cerr << "Exception in testCardMemoryLayout1K: " << e.what() << "\n";
        return false;
    }
}

bool testCardMemoryLayout4K() {
    try {
        // Just test basic construction
        CardMemoryLayout cardMem(true);
        
        // Verify is4K flag
        if (cardMem.is4K != true) {
            cerr << "ERROR: is4K should be true\n";
            return false;
        }
        
        // Verify memory size
        if (cardMem.memorySize() != 4096) {
            cerr << "ERROR: memorySize should be 4096, got " << cardMem.memorySize() << "\n";
            return false;
        }
        
        // Try to access raw memory
        try {
            BYTE* raw = cardMem.getRawMemory();
            if (raw == nullptr) {
                cerr << "ERROR: getRawMemory returned null\n";
                return false;
            }
        }
        catch (...) {
            cerr << "ERROR: getRawMemory threw exception\n";
            return false;
        }
        
        return true;
    }
    catch (const exception& e) {
        cerr << "Exception in testCardMemoryLayout4K: " << e.what() << "\n";
        return false;
    }
}

// ════════════════════════════════════════════════════════════════════════════════
// Card Topology Tests
// ════════════════════════════════════════════════════════════════════════════════

bool testCardTopology() {
    try {
        CardTopology topo(false);
        
        // Test 1K counts
        if (topo.sectorCount() != 16) return false;
        if (topo.totalBlocks() != 64) return false;
        if (topo.blocksPerSector(0) != 4) return false;
        
        // Test block to sector mapping
        if (topo.sectorFromBlock(0) != 0) return false;
        if (topo.sectorFromBlock(3) != 0) return false;
        if (topo.sectorFromBlock(4) != 1) return false;
        if (topo.sectorFromBlock(63) != 15) return false;
        
        // Test trailer queries
        if (topo.trailerBlockOfSector(0) != 3) return false;
        if (topo.trailerBlockOfSector(5) != 23) return false;
        if (topo.isTrailerBlock(3) != true) return false;
        if (topo.isTrailerBlock(2) != false) return false;
        
        // Test block identification
        if (topo.isManufacturerBlock(0) != true) return false;
        if (topo.isManufacturerBlock(1) != false) return false;
        if (topo.isDataBlock(1) != true) return false;
        if (topo.isDataBlock(3) != false) return false;
        
        // Test 4K
        CardTopology topo4k(true);
        if (topo4k.sectorCount() != 40) return false;
        if (topo4k.totalBlocks() != 256) return false;
        if (topo4k.blocksPerSector(0) != 4) return false;
        if (topo4k.blocksPerSector(32) != 16) return false;
        if (topo4k.blocksPerSector(39) != 16) return false;
        
        return true;
    }
    catch (const exception& e) {
        cerr << "Exception: " << e.what() << "\n";
        return false;
    }
}

// ════════════════════════════════════════════════════════════════════════════════
// Key Management Tests
// ════════════════════════════════════════════════════════════════════════════════

bool testKeyManagement() {
    try {
        CardMemoryLayout cardMem(false);
        
        // Initialize trailer with sample keys
        auto& trailer = cardMem.data.card1K.detailed.sector[0].trailerBlock;
        for (int i = 0; i < 6; ++i) {
            trailer.trailer.keyA[i] = 0xFF;
            trailer.trailer.keyB[i] = 0x00;
        }
        
        KeyManagement keyMgr(cardMem);
        
        // Test key registration
        KEYBYTES testKeyA = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
        keyMgr.registerKey(KeyType::A, testKeyA, KeyStructure::NonVolatile, 0x01, "Test");
        
        if (!keyMgr.hasKey(KeyType::A, 0x01)) return false;
        if (keyMgr.hasKey(KeyType::B, 0x01)) return false;
        
        const auto& retrieved = keyMgr.getKey(KeyType::A, 0x01);
        if (retrieved != testKeyA) return false;
        
        // Test card key reading
        KEYBYTES cardKeyA = keyMgr.getCardKeyA(0);
        if (cardKeyA[0] != 0xFF) return false;
        if (cardKeyA[5] != 0xFF) return false;
        
        KEYBYTES cardKeyB = keyMgr.getCardKeyB(0);
        if (cardKeyB[0] != 0x00) return false;
        if (cardKeyB[5] != 0x00) return false;
        
        // Test key validation
        KEYBYTES validKey = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE};
        if (!KeyManagement::isValidKey(validKey)) return false;
        
        KEYBYTES allZero = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        if (KeyManagement::isValidKey(allZero)) return false;
        
        KEYBYTES allFF = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        if (KeyManagement::isValidKey(allFF)) return false;
        
        return true;
    }
    catch (const exception& e) {
        cerr << "Exception: " << e.what() << "\n";
        return false;
    }
}

// ════════════════════════════════════════════════════════════════════════════════
// Access Control Tests
// ════════════════════════════════════════════════════════════════════════════════

bool testAccessControl() {
    try {
        CardMemoryLayout cardMem(false);
        
        // Initialize all trailers
        for (int sector = 0; sector < 16; ++sector) {
            auto& trailer = cardMem.data.card1K.detailed.sector[sector].trailerBlock;
            for (int i = 0; i < 6; ++i) {
                trailer.trailer.keyA[i] = 0xFF;
                trailer.trailer.keyB[i] = 0x00;
            }
            trailer.trailer.accessBits[0] = 0xFF;
            trailer.trailer.accessBits[1] = 0x07;
            trailer.trailer.accessBits[2] = 0x80;
            trailer.trailer.accessBits[3] = 0x69;
        }
        
        AccessControl access(cardMem);
        
        // Test data block permissions (MODE_0: KeyA read, KeyB r/w)
        if (!access.canReadDataBlock(0, KeyType::A)) return false;
        if (!access.canReadDataBlock(0, KeyType::B)) return false;
        if (access.canWriteDataBlock(0, KeyType::A)) return false;
        if (!access.canWriteDataBlock(0, KeyType::B)) return false;
        
        // Test trailer permissions
        if (access.canReadTrailer(0, KeyType::A)) return false;
        if (access.canReadTrailer(0, KeyType::B)) return false;
        if (access.canWriteTrailer(0, KeyType::A)) return false;
        if (!access.canWriteTrailer(0, KeyType::B)) return false;
        
        return true;
    }
    catch (const exception& e) {
        cerr << "Exception: " << e.what() << "\n";
        return false;
    }
}

// ════════════════════════════════════════════════════════════════════════════════
// Authentication State Tests
// ════════════════════════════════════════════════════════════════════════════════

bool testAuthenticationState() {
    try {
        CardMemoryLayout cardMem(false);
        AuthenticationState authState(cardMem);
        
        // Test authentication marking
        authState.markAuthenticated(0, KeyType::A);
        if (!authState.isAuthenticated(0)) return false;
        if (!authState.isAuthenticatedWith(0, KeyType::A)) return false;
        if (authState.isAuthenticatedWith(0, KeyType::B)) return false;
        
        // Test multiple authentications
        authState.markAuthenticated(5, KeyType::B);
        if (!authState.isAuthenticated(5)) return false;
        
        // Test session count
        auto authenticated = authState.getAuthenticatedSectors();
        if (authenticated.size() != 2) return false;
        
        // Test invalidation
        authState.invalidate(0);
        if (authState.isAuthenticated(0)) return false;
        
        // Test clearing all
        authState.clearAll();
        if (authState.countAuthenticated() != 0) return false;
        
        return true;
    }
    catch (const exception& e) {
        cerr << "Exception: " << e.what() << "\n";
        return false;
    }
}

// ════════════════════════════════════════════════════════════════════════════════
// Card Interface Tests
// ════════════════════════════════════════════════════════════════════════════════

bool testCardInterface() {
    try {
        // Create interface
        CardInterface card(false);
        
        // Verify card metadata
        if (!card.is1K()) return false;
        if (card.is4K()) return false;
        if (card.getTotalMemory() != 1024) return false;
        if (card.getTotalBlocks() != 64) return false;
        if (card.getTotalSectors() != 16) return false;
        
        // Initialize sample data
        CardMemoryLayout& mem = card.getMemoryMutable();
        mem.data.card1K.blocks[0].manufacturer.uid[0] = 0x12;
        mem.data.card1K.blocks[0].manufacturer.uid[1] = 0x34;
        
        // Initialize trailers
        for (int sector = 0; sector < 16; ++sector) {
            auto& trailer = mem.data.card1K.detailed.sector[sector].trailerBlock;
            for (int i = 0; i < 6; ++i) {
                trailer.trailer.keyA[i] = 0xFF;
                trailer.trailer.keyB[i] = 0x00;
            }
            trailer.trailer.accessBits[0] = 0xFF;
            trailer.trailer.accessBits[1] = 0x07;
            trailer.trailer.accessBits[2] = 0x80;
            trailer.trailer.accessBits[3] = 0x69;
        }
        
        // Test key management
        KEYBYTES keyA = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};  // Valid key (not all FF)
        card.registerKey(KeyType::A, keyA, KeyStructure::NonVolatile, 0x01);
        if (card.getKey(KeyType::A, 0x01) != keyA) return false;
        
        // Test authentication
        card.authenticate(0, KeyType::A);
        if (!card.isAuthenticated(0)) return false;
        if (!card.isAuthenticatedWith(0, KeyType::A)) return false;
        
        // Test topology queries
        if (card.getSectorForBlock(3) != 0) return false;
        if (card.getFirstBlockOfSector(0) != 0) return false;
        if (card.getTrailerBlockOfSector(0) != 3) return false;
        if (!card.isManufacturerBlock(0)) return false;
        if (!card.isTrailerBlock(3)) return false;
        if (!card.isDataBlock(1)) return false;
        
        // Test access control
        if (!card.canReadDataBlocks(0, KeyType::A)) return false;
        if (card.canWriteDataBlocks(0, KeyType::A)) return false;
        
        // Test memory export
        BYTEV exported = card.exportMemory();
        if (exported.size() != 1024) return false;
        if (exported[0] != 0x12) return false;
        if (exported[1] != 0x34) return false;
        
        return true;
    }
    catch (const exception& e) {
        cerr << "Exception: " << e.what() << "\n";
        return false;
    }
}
