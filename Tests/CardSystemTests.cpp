// CardSystemTests.cpp - Card System Test Suite
// Tests for CardMemoryLayout, CardTopology, KeyManagement, AccessControl,
// AuthenticationState, CardInterface, TrailerConfig, AccessBitsCodec, CardSimulation

#include "../Card/Card/CardModel/CardMemoryLayout.h"
#include "../Card/Card/CardModel/CardTopology.h"
#include "../Card/Card/CardModel/TrailerConfig.h"
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
        if (cardMem.cardType != CardType::MifareClassic1K) return false;
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
        if (cardMem.cardType != CardType::MifareClassic4K) return false;
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
// AccessBitsCodec Tests — encode / decode / verify
// ════════════════════════════════════════════════════════════════════════════════

bool testAccessBitsCodec() {
    try {
        // 1) Factory default decode: FF 07 80 69
        ACCESSBYTES factory = {0xFF, 0x07, 0x80, 0x69};
        SectorAccessConfig cfg = AccessBitsCodec::decode(factory);

        // data bloklar: C1=0, C2=0, C3=0
        for (int i = 0; i < 3; ++i) {
            if (cfg.dataBlock[i].c1 || cfg.dataBlock[i].c2 || cfg.dataBlock[i].c3)
                return false;
        }
        // trailer: C1=0, C2=0, C3=1
        if (cfg.trailer.c1 || cfg.trailer.c2 || !cfg.trailer.c3) return false;
        if (cfg.gpb != 0x69) return false;

        // 2) Round-trip: encode(decode(x)) == x
        ACCESSBYTES reEncoded = AccessBitsCodec::encode(cfg);
        if (reEncoded != factory) return false;

        // 3) Verify — factory default valid
        if (!AccessBitsCodec::verify(factory)) return false;

        // 4) Bozuk bits → verify false
        ACCESSBYTES corrupt = {0xFF, 0xFF, 0x80, 0x69};  // C1 inverted'ler tutmuyor
        if (AccessBitsCodec::verify(corrupt)) return false;

        // 5) Her SectorMode encode → decode round-trip
        SectorMode modes[] = {
            SectorMode::TRANSPORT, SectorMode::READ_ONLY_AB,
            SectorMode::READ_AB_WRITE_B, SectorMode::VALUE_BLOCK,
            SectorMode::KEY_B_ONLY, SectorMode::FROZEN
        };
        for (auto mode : modes) {
            SectorAccessConfig mc = sectorModeToConfig(mode);
            ACCESSBYTES enc = AccessBitsCodec::encode(mc);
            if (!AccessBitsCodec::verify(enc)) return false;
            SectorAccessConfig dec = AccessBitsCodec::decode(enc);
            // data bloklar eşleşmeli
            for (int i = 0; i < 3; ++i) {
                if (dec.dataBlock[i].c1 != mc.dataBlock[i].c1) return false;
                if (dec.dataBlock[i].c2 != mc.dataBlock[i].c2) return false;
                if (dec.dataBlock[i].c3 != mc.dataBlock[i].c3) return false;
            }
            if (dec.trailer.c1 != mc.trailer.c1) return false;
            if (dec.trailer.c2 != mc.trailer.c2) return false;
            if (dec.trailer.c3 != mc.trailer.c3) return false;
        }

        // 6) READ_AB_WRITE_B: data C=100 → readA=true, writeA=false, writeB=true
        {
            SectorAccessConfig rw = sectorModeToConfig(SectorMode::READ_AB_WRITE_B);
            DataBlockPermission dp = rw.dataPermission(0);
            if (!dp.readA || !dp.readB) return false;
            if (dp.writeA) return false;
            if (!dp.writeB) return false;
        }

        // 7) FROZEN: data C=111 → hiçbir erişim yok
        {
            SectorAccessConfig fr = sectorModeToConfig(SectorMode::FROZEN);
            DataBlockPermission dp = fr.dataPermission(0);
            if (dp.readA || dp.readB || dp.writeA || dp.writeB) return false;
            if (dp.incrementA || dp.decrementB) return false;
        }

        return true;
    }
    catch (const exception& e) {
        cerr << "Exception: " << e.what() << "\n";
        return false;
    }
}

// ════════════════════════════════════════════════════════════════════════════════
// TrailerConfig Tests — fromBlock / toBlock / isValid
// ════════════════════════════════════════════════════════════════════════════════

bool testTrailerConfig() {
    try {
        // 1) Factory default oluştur
        TrailerConfig def = TrailerConfig::factoryDefault();
        if (def.keyA != KEYBYTES{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}) return false;
        if (def.keyB != KEYBYTES{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}) return false;
        if (!def.isValid()) return false;

        ACCESSBYTES ab = def.accessBitsRaw();
        if (ab != ACCESSBYTES{0xFF, 0x07, 0x80, 0x69}) return false;

        // 2) toBlock → fromBlock round-trip
        MifareBlock blk = def.toBlock();
        TrailerConfig parsed = TrailerConfig::fromBlock(blk);
        if (parsed.keyA != def.keyA) return false;
        if (parsed.keyB != def.keyB) return false;
        if (parsed.accessBitsRaw() != def.accessBitsRaw()) return false;

        // 3) Özel key'lerle oluştur
        TrailerConfig custom;
        custom.keyA = {0xA0,0xA1,0xA2,0xA3,0xA4,0xA5};
        custom.keyB = {0xB0,0xB1,0xB2,0xB3,0xB4,0xB5};
        custom.access = sectorModeToConfig(SectorMode::READ_AB_WRITE_B);
        if (!custom.isValid()) return false;

        MifareBlock cblk = custom.toBlock();
        // keyA bytes 0-5
        if (cblk.trailer.keyA[0] != 0xA0 || cblk.trailer.keyA[5] != 0xA5)
            return false;
        // keyB bytes 10-15
        if (cblk.trailer.keyB[0] != 0xB0 || cblk.trailer.keyB[5] != 0xB5)
            return false;

        // Parse geri
        TrailerConfig cparsed = TrailerConfig::fromBlock(cblk);
        if (cparsed.keyA != custom.keyA) return false;
        if (cparsed.keyB != custom.keyB) return false;

        // 4) Permission tablosu doğrulama — trailer C=001
        TrailerPermission tp = def.access.trailerPermission();
        if (!tp.keyAWriteA) return false;     // A, KeyA yazabilir
        if (tp.keyAWriteB) return false;      // B, KeyA yazamaz
        if (!tp.accReadA) return false;       // A, access bits okuyabilir
        if (!tp.accWriteA) return false;      // A, access bits yazabilir (001'de A yazar)
        if (!tp.keyBReadA) return false;      // A, KeyB okuyabilir
        if (tp.keyBReadB) return false;       // B, KeyB okuyamaz

        return true;
    }
    catch (const exception& e) {
        cerr << "Exception: " << e.what() << "\n";
        return false;
    }
}

// ════════════════════════════════════════════════════════════════════════════════
// Card Simulation Test — Gerçek kart olmadan tam senaryo
// ════════════════════════════════════════════════════════════════════════════════
//
// Senaryo:  Fabrika kartı → veri yaz → key değiştir → izin kısıtla → doğrula
//
// Bu test PCSC/reader gerektirmez. Sadece CardInterface + TrailerConfig kullanır.
//

bool testCardSimulation() {
    try {
        // ── 1. Fabrika kartı simüle et ──────────────────────────────────────

        CardInterface card(false);  // 1K

        // Factory trailer'ları yaz — her sektörün trailer bloğuna
        CardMemoryLayout& mem = card.getMemoryMutable();
        for (int s = 0; s < 16; ++s) {
            TrailerConfig def = TrailerConfig::factoryDefault();
            MifareBlock tblk = def.toBlock();
            int trailerBlock = s * 4 + 3;
            memcpy(mem.getRawMemory() + trailerBlock * 16, tblk.raw, 16);
        }

        // Manufacturer bloğuna UID yaz
        BYTE uid[4] = {0xDE, 0xAD, 0xBE, 0xEF};
        memcpy(mem.data.card1K.blocks[0].manufacturer.uid, uid, 4);

        // UID doğrula
        KEYBYTES gotUid = card.getUID();
        if (gotUid[0] != 0xDE || gotUid[1] != 0xAD ||
            gotUid[2] != 0xBE || gotUid[3] != 0xEF) return false;

        // ── 2. Factory default izinler doğru mu? ────────────────────────────

        // data=000: A|B read, A|B write
        if (!card.canRead(1, KeyType::A)) return false;
        if (!card.canWrite(1, KeyType::A)) return false;
        if (!card.canRead(1, KeyType::B)) return false;
        if (!card.canWrite(1, KeyType::B)) return false;

        // trailer=001: A reads access bits
        if (!card.canRead(3, KeyType::A)) return false;   // trailer acc read A
        if (card.canRead(3, KeyType::B)) return false;    // B can't read acc

        // ── 3. Veri yaz — sektör 4, blok 16-17 ─────────────────────────────

        BYTE data1[16] = {'S','I','M','U','L','A','T','E','D',' ','D','A','T','A','!',0};
        BYTE data2[16] = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
                          0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10};
        memcpy(mem.getRawMemory() + 16 * 16, data1, 16);  // block 16
        memcpy(mem.getRawMemory() + 17 * 16, data2, 16);  // block 17

        // Doğrula — getBlock
        const MifareBlock& blk16 = card.getBlock(16);
        if (blk16.raw[0] != 'S' || blk16.raw[8] != 'D') return false;
        const MifareBlock& blk17 = card.getBlock(17);
        if (blk17.raw[0] != 0x01 || blk17.raw[15] != 0x10) return false;

        // ── 4. Key değiştir — sektör 4 trailer'ına özel key yaz ─────────────

        TrailerConfig s4cfg;
        s4cfg.keyA = {0xA0,0xA1,0xA2,0xA3,0xA4,0xA5};
        s4cfg.keyB = {0xB0,0xB1,0xB2,0xB3,0xB4,0xB5};
        s4cfg.access = sectorModeToConfig(SectorMode::READ_AB_WRITE_B);
        // C1=1,C2=0,C3=0 → data: A|B read, B write
        // trailer: C1=1,C2=0,C3=0 → accRead=A|B, accWrite=never, KeyB write=B
        MifareBlock s4trailer = s4cfg.toBlock();
        memcpy(mem.getRawMemory() + 19 * 16, s4trailer.raw, 16); // block 19

        // Doğrula — key memory'de var
        KEYBYTES cardKeyB = card.getCardKey(4, KeyType::B);
        if (cardKeyB != KEYBYTES{0xB0,0xB1,0xB2,0xB3,0xB4,0xB5}) return false;

        // ── 5. İzin kısıtlandı mı? ─────────────────────────────────────────

        // Sektör 4 artık READ_AB_WRITE_B: data C=100
        if (!card.canRead(16, KeyType::A)) return false;   // A okuyabilir
        if (!card.canRead(16, KeyType::B)) return false;   // B okuyabilir
        if (card.canWrite(16, KeyType::A)) return false;   // A YAZAMAZ
        if (!card.canWrite(16, KeyType::B)) return false;  // B yazabilir

        // Sektör 0 hâlâ factory: A|B read+write
        if (!card.canWrite(1, KeyType::A)) return false;

        // ── 6. Auth simülasyonu ─────────────────────────────────────────────

        card.authenticate(4, KeyType::A);
        if (!card.isAuthenticated(4)) return false;
        if (!card.isAuthenticatedWith(4, KeyType::A)) return false;
        if (card.isAuthenticatedWith(4, KeyType::B)) return false;

        // Sektör değişince önceki auth geçersiz kalmalı (Mifare davranışı)
        card.authenticate(5, KeyType::B);
        // Sektör 4 hâlâ valid — timeout-based, açıkça deauth edilmeli
        card.deauthenticate(4);
        if (card.isAuthenticated(4)) return false;
        if (!card.isAuthenticated(5)) return false;

        // ── 7. Farklı bloklar için farklı access ───────────────────────────

        // Sektör 6'yı VALUE_BLOCK moduna al: data C=110
        //   read=A|B, write=B, inc=B, dec=A|B
        SectorAccessConfig valCfg = sectorModeToConfig(SectorMode::VALUE_BLOCK);
        TrailerConfig s6tc = TrailerConfig::factoryDefault();
        s6tc.access = valCfg;
        MifareBlock s6t = s6tc.toBlock();
        memcpy(mem.getRawMemory() + 27 * 16, s6t.raw, 16); // block 27 = sector 6 trailer

        DataBlockPermission dp6 = valCfg.dataPermission(0);
        if (!dp6.readA || !dp6.readB) return false;
        if (dp6.writeA) return false;
        if (!dp6.writeB) return false;
        if (dp6.incrementA) return false;
        if (!dp6.incrementB) return false;
        if (!dp6.decrementA || !dp6.decrementB) return false;

        // ── 8. Sektör 8'i FROZEN yap ───────────────────────────────────────

        SectorAccessConfig frozenCfg = sectorModeToConfig(SectorMode::FROZEN);
        TrailerConfig s8tc = TrailerConfig::factoryDefault();
        s8tc.access = frozenCfg;
        MifareBlock s8t = s8tc.toBlock();
        memcpy(mem.getRawMemory() + 35 * 16, s8t.raw, 16); // block 35

        if (card.canRead(32, KeyType::A)) return false;    // FROZEN: okuma yok
        if (card.canRead(32, KeyType::B)) return false;
        if (card.canWrite(32, KeyType::A)) return false;
        if (card.canWrite(32, KeyType::B)) return false;

        // ── 9. exportMemory → yeniden loadMemory → doğrula ─────────────────

        BYTEV exported = card.exportMemory();
        if (exported.size() != 1024) return false;

        CardInterface card2(false);
        card2.loadMemory(exported.data(), 1024);

        // UID korunmuş mu?
        KEYBYTES uid2 = card2.getUID();
        if (uid2[0] != 0xDE || uid2[3] != 0xEF) return false;

        // Sektör 4'ün access kısıtlamaları korunmuş mu?
        if (card2.canWrite(16, KeyType::A)) return false;   // hâlâ yazamaz
        if (!card2.canWrite(16, KeyType::B)) return false;  // B hâlâ yazabilir

        // Sektör 8 hâlâ frozen?
        if (card2.canRead(32, KeyType::A)) return false;

        // Veri korunmuş mu?
        const MifareBlock& vblk = card2.getBlock(16);
        if (vblk.raw[0] != 'S' || vblk.raw[14] != '!') return false;

        // ── 10. Topology tutarlılık ─────────────────────────────────────────

        if (card2.getSectorForBlock(16) != 4) return false;
        if (card2.getTrailerBlockOfSector(4) != 19) return false;
        if (!card2.isTrailerBlock(19)) return false;
        if (!card2.isDataBlock(16)) return false;
        if (!card2.isManufacturerBlock(0)) return false;
        if (card2.getTotalBlocks() != 64) return false;
        if (card2.getTotalSectors() != 16) return false;

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
    recordTest("AccessBits Codec", testAccessBitsCodec());
    recordTest("Trailer Config", testTrailerConfig());
    recordTest("Card Simulation", testCardSimulation());
    
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
