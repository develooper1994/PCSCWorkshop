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
// Test Result Tracking
// ════════════════════════════════════════════════════════════════════════════════

struct TestResult {
    std::string name;
    bool passed;
    std::string error;
    
    TestResult(const std::string& n, bool p, const std::string& e = "")
        : name(n), passed(p), error(e) {}
};

static std::vector<TestResult> testResults;

void recordTest(const std::string& name, bool passed, const std::string& error = "") {
    testResults.push_back(TestResult(name, passed, error));
    cout << "  " << name << "... " << (passed ? "PASSED" : "FAILED");
    if (!error.empty()) cout << " (" << error << ")";
    cout << "\n";
}

// ════════════════════════════════════════════════════════════════════════════════
// Card Memory Layout Tests
// ════════════════════════════════════════════════════════════════════════════════

bool testCardMemoryLayout1K() {
    try {
        CardMemoryLayout cardMem(false);
        if (cardMem.is4K != false) return false;
        if (cardMem.memorySize() != 1024) return false;
        if (cardMem.getRawMemory() == nullptr) return false;
        return true;
    }
    catch (const exception& e) {
        cerr << "Exception: " << e.what() << "\n";
        return false;
    }
}

bool testCardMemoryLayout4K() {
    try {
        CardMemoryLayout cardMem(true);
        if (cardMem.is4K != true) return false;
        if (cardMem.memorySize() != 4096) return false;
        if (cardMem.getRawMemory() == nullptr) return false;
        return true;
    }
    catch (const exception& e) {
        cerr << "Exception: " << e.what() << "\n";
        return false;
    }
}

// ════════════════════════════════════════════════════════════════════════════════
// Card Topology Tests
// ════════════════════════════════════════════════════════════════════════════════

bool testCardTopology() {
    try {
        CardLayoutTopology topo(false);
        if (topo.sectorCount() != 16) return false;
        if (topo.totalBlocks() != 64) return false;
        if (topo.blocksPerSector(0) != 4) return false;
        if (topo.sectorFromBlock(0) != 0) return false;
        if (topo.trailerBlockOfSector(0) != 3) return false;
        if (!topo.isTrailerBlock(3)) return false;
        if (!topo.isDataBlock(1)) return false;
        
        CardLayoutTopology topo4k(true);
        if (topo4k.sectorCount() != 40) return false;
        if (topo4k.totalBlocks() != 256) return false;
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
        auto& trailer = cardMem.data.card1K.detailed.sector[0].trailerBlock;
        for (int i = 0; i < 6; ++i) {
            trailer.trailer.keyA[i] = 0xFF;
            trailer.trailer.keyB[i] = 0x00;
        }
        
        KeyManagement keyMgr(cardMem);
        KEYBYTES testKeyA = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
        keyMgr.registerKey(KeyType::A, testKeyA, KeyStructure::NonVolatile, 0x01, "Test");
        
        if (!keyMgr.hasKey(KeyType::A, 0x01)) return false;
        if (keyMgr.getKey(KeyType::A, 0x01) != testKeyA) return false;
        if (keyMgr.getCardKeyA(0)[0] != 0xFF) return false;
        
        KEYBYTES validKey = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE};
        if (!KeyManagement::isValidKey(validKey)) return false;
        
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
        // Factory default: C1=0,C2=0,C3=0 → data: A|B read, A|B write
        if (!access.canReadDataBlock(0, KeyType::A)) return false;
        if (!access.canReadDataBlock(0, KeyType::B)) return false;
        if (!access.canWriteDataBlock(0, KeyType::A)) return false;
        if (!access.canWriteDataBlock(0, KeyType::B)) return false;

        // Trailer: C1=0,C2=0,C3=1 → accRead: A only, accWrite: A only
        if (!access.canReadTrailer(0, KeyType::A)) return false;
        if (access.canReadTrailer(0, KeyType::B)) return false;
        if (!access.canWriteTrailer(0, KeyType::A)) return false;
        if (access.canWriteTrailer(0, KeyType::B)) return false;
        
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
        
        authState.markAuthenticated(0, KeyType::A);
        if (!authState.isAuthenticated(0)) return false;
        if (!authState.isAuthenticatedWith(0, KeyType::A)) return false;
        
        authState.markAuthenticated(5, KeyType::B);
        auto authenticated = authState.getAuthenticatedSectors();
        if (authenticated.size() != 2) return false;
        
        authState.invalidate(0);
        if (authState.isAuthenticated(0)) return false;
        
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
        CardInterface card(false);
        if (!card.is1K()) return false;
        if (card.is4K()) return false;
        if (card.getTotalMemory() != 1024) return false;
        if (card.getTotalBlocks() != 64) return false;
        
        CardMemoryLayout& mem = card.getMemoryMutable();
        mem.data.card1K.blocks[0].manufacturer.uid[0] = 0x12;
        
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
        
        KEYBYTES keyA = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
        card.registerKey(KeyType::A, keyA, KeyStructure::NonVolatile, 0x01);
        
        card.authenticate(0, KeyType::A);
        if (!card.isAuthenticated(0)) return false;
        
        if (card.getSectorForBlock(3) != 0) return false;
        if (card.getTrailerBlockOfSector(0) != 3) return false;
        
        BYTEV exported = card.exportMemory();
        if (exported.size() != 1024) return false;
        if (exported[0] != 0x12) return false;
        
        return true;
    }
    catch (const exception& e) {
        cerr << "Exception: " << e.what() << "\n";
        return false;
    }
}

// ════════════════════════════════════════════════════════════════════════════════
// MAIN TEST RUNNER - All Card System Tests
// ════════════════════════════════════════════════════════════════════════════════

int runCardSystemTests() {
    cout << "\n=== Card System Tests ===\n\n";
    
    testResults.clear();
    
    // Run all tests
    recordTest("Card Memory Layout (1K)", testCardMemoryLayout1K());
    recordTest("Card Memory Layout (4K)", testCardMemoryLayout4K());
    recordTest("Card Topology", testCardTopology());
    recordTest("Key Management", testKeyManagement());
    recordTest("Access Control", testAccessControl());
    recordTest("Authentication State", testAuthenticationState());
    recordTest("Card Interface", testCardInterface());
    
    // Summary
    cout << "\n=== Test Summary ===\n";
    int passed = 0;
    int failed = 0;
    
    for (const auto& result : testResults) {
        if (result.passed) {
            passed++;
        } else {
            failed++;
        }
    }
    
    cout << "Total: " << testResults.size() << " tests\n";
    cout << "Passed: " << passed << "\n";
    cout << "Failed: " << failed << "\n";
    
    if (failed > 0) {
        cout << "\nFailed tests:\n";
        for (const auto& result : testResults) {
            if (!result.passed) {
                cout << "  - " << result.name << "\n";
            }
        }
    }
    
    return (failed == 0) ? 0 : 1;
}
