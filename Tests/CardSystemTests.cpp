// CardSystemTests.cpp - Card System Test Suite
// Tests for CardMemoryLayout, CardTopology, KeyManagement, AccessControl,
// AuthenticationState, CardInterface, TrailerConfig, AccessBitsCodec, CardSimulation

#include "../Card/Card/CardModel/CardMemoryLayout.h"
#include "../Card/Card/CardModel/CardTopology.h"
#include "../Card/Card/CardModel/TrailerConfig.h"
#include "../Card/Card/CardModel/DesfireMemoryLayout.h"
#include "../Card/Card/CardProtocol/AccessControl.h"
#include "../Card/Card/CardProtocol/KeyManagement.h"
#include "../Card/Card/CardProtocol/AuthenticationState.h"
#include "../Card/Card/CardProtocol/DesfireCrypto.h"
#include "../Card/Card/CardProtocol/DesfireAuth.h"
#include "../Card/Card/CardProtocol/DesfireSession.h"
#include "../Card/Card/CardProtocol/DesfireCommands.h"
#include "../Card/Card/CardProtocol/DesfireSecureMessaging.h"
#include "../Card/Card/CardInterface.h"
#include "Crypto.h"
#include <iostream>
#include <cstring>
#include <thread>

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
// TEST: DESFire Memory Layout + CardInterface integration
// ════════════════════════════════════════════════════════════════════════════════

bool testDesfireMemoryLayout() {
    int line = 0;
    try {
#define DF_CHECK(cond) do { line = __LINE__; if (!(cond)) { cout << "    FAIL at line " << line << ": " #cond "\n"; return false; } } while(0)

        // ── 1. DesfireVersionInfo parse
        DesfireVersionInfo vi;
        vi.hwVendorID = 0x04;  // NXP
        vi.hwMajorVer = 1;
        vi.hwStorageSize = 0x18;  // 4KB
        vi.swMajorVer = 1;
        vi.uid[0] = 0x04; vi.uid[1] = 0xAB; vi.uid[2] = 0xCD;
        vi.uid[3] = 0xEF; vi.uid[4] = 0x12; vi.uid[5] = 0x34; vi.uid[6] = 0x56;

        DF_CHECK(DesfireVersionInfo::storageSizeToBytes(0x16) == 2048);
        DF_CHECK(DesfireVersionInfo::storageSizeToBytes(0x18) == 4096);
        DF_CHECK(DesfireVersionInfo::storageSizeToBytes(0x1A) == 8192);
        DF_CHECK(vi.totalCapacity() == 4096);
        DF_CHECK(vi.guessVariant() == DesfireVariant::EV1);

        // ── 2. DesfireMemoryLayout init
        DesfireMemoryLayout layout;
        layout.initFromVersion(vi);

        DF_CHECK(layout.totalMemory == 4096);
        DF_CHECK(layout.freeMemory == 4096);
        DF_CHECK(layout.variant == DesfireVariant::EV1);

        // ── 3. Application oluştur
        DesfireApplication app;
        app.aid = DesfireAID::fromUint(0x010203);
        app.keyConfig.keyCount = 3;
        app.keyConfig.keyType = DesfireKeyType::AES128;

        DesfireFile file;
        file.fileNo = 0;
        file.settings.fileType = DesfireFileType::StandardData;
        file.settings.standard.fileSize = 128;
        file.settings.access.readKey = 0;
        file.settings.access.writeKey = 1;
        file.allocate();

        DF_CHECK(file.data.size() == 128);
        app.files.push_back(file);

        DesfireFile vfile;
        vfile.fileNo = 1;
        vfile.settings.fileType = DesfireFileType::Value;
        vfile.settings.value.lowerLimit = 0;
        vfile.settings.value.upperLimit = 10000;
        vfile.settings.value.value = 500;
        vfile.allocate();

        DF_CHECK(vfile.data.size() == 4);
        app.files.push_back(vfile);

        layout.applications.push_back(app);

        // ── 4. Sorgu testi
        DF_CHECK(layout.hasApp(DesfireAID::fromUint(0x010203)));
        DF_CHECK(!layout.hasApp(DesfireAID::fromUint(0x999999)));

        auto* foundApp = layout.findApp(DesfireAID::fromUint(0x010203));
        DF_CHECK(foundApp != nullptr);
        DF_CHECK(foundApp->files.size() == 2);
        DF_CHECK(foundApp->hasFile(0));
        DF_CHECK(foundApp->hasFile(1));
        DF_CHECK(!foundApp->hasFile(5));
        DF_CHECK(foundApp->totalDataSize() == 132);

        // ── 5. Virtual block read/write
        layout.currentAID = DesfireAID::fromUint(0x010203);
        layout.currentFile = 0;

        DF_CHECK(layout.virtualBlockCount() == 8);

        BYTE testData[16] = {'D','E','S','F','i','r','e',' ','T','e','s','t','!',0,0,0};
        DF_CHECK(layout.writeVirtualBlock(0, testData));

        BYTE readBuf[16] = {};
        DF_CHECK(layout.readVirtualBlock(0, readBuf));
        DF_CHECK(memcmp(readBuf, testData, 16) == 0);
        DF_CHECK(!layout.readVirtualBlock(99, readBuf));

        // ── 6. AccessRights encode/decode
        DesfireAccessRights ar;
        ar.readKey = 0x02;
        ar.writeKey = 0x03;
        ar.readWriteKey = 0x0E;
        ar.changeKey = 0x00;

        auto encoded = ar.encode();
        auto decoded = DesfireAccessRights::decode(encoded[0], encoded[1]);

        DF_CHECK(decoded.readKey == 0x02);
        DF_CHECK(decoded.writeKey == 0x03);
        DF_CHECK(decoded.readWriteKey == 0x0E);
        DF_CHECK(decoded.changeKey == 0x00);
        DF_CHECK(!decoded.isFreeRead());
        DF_CHECK(!decoded.isDenyWrite());

        // ── 7. AID helpers
        DesfireAID picc = DesfireAID::picc();
        DF_CHECK(picc.isPICC());

        DesfireAID a = DesfireAID::fromUint(0xABCDEF);
        DF_CHECK(a.toUint() == 0xABCDEF);
        DF_CHECK(!a.isPICC());

        // ── 8. CardInterface DESFire integration
        CardInterface card(CardType::MifareDesfire);

        DF_CHECK(card.isDesfire());
        DF_CHECK(!card.isClassic());
        DF_CHECK(!card.isUltralight());
        DF_CHECK(card.getTotalBlocks() == 0);
        DF_CHECK(card.getTotalSectors() == 0);

        DesfireMemoryLayout& dfm = card.getDesfireMemoryMutable();
        dfm.initFromVersion(vi);
        dfm.applications.push_back(app);

        DF_CHECK(card.getTotalMemory() == 4096);

        KEYBYTES uid = card.getUID();
        DF_CHECK(uid[0] == 0x04);
        DF_CHECK(uid[1] == 0xAB);
        DF_CHECK(uid[5] == 0x34);

        bool loadThrew = false;
        try { card.loadMemory(nullptr, 100); }
        catch (const std::logic_error&) { loadThrew = true; }
        DF_CHECK(loadThrew);

        bool exportThrew = false;
        try { card.exportMemory(); }
        catch (const std::logic_error&) { exportThrew = true; }
        DF_CHECK(exportThrew);

        // ── 9. KeySettings bitfield
        DesfireKeyConfig kc;
        kc.keySettings = 0x0F;
        DF_CHECK(kc.allowChangeMasterKey());
        DF_CHECK(kc.allowFreeDirectoryList());
        DF_CHECK(kc.allowFreeCreateDelete());
        DF_CHECK(kc.allowConfigChangeable());

        kc.keySettings = 0x00;
        DF_CHECK(!kc.allowChangeMasterKey());
        DF_CHECK(!kc.allowFreeDirectoryList());

        // ── 10. Variant detection
        DesfireVersionInfo ev2vi;
        ev2vi.hwMajorVer = 1;
        ev2vi.swMajorVer = 2;
        DF_CHECK(ev2vi.guessVariant() == DesfireVariant::EV2);

        DesfireVersionInfo ev3vi;
        ev3vi.hwMajorVer = 1;
        ev3vi.swMajorVer = 3;
        DF_CHECK(ev3vi.guessVariant() == DesfireVariant::EV3);

        DesfireVersionInfo lightvi;
        lightvi.hwMajorVer = 0; lightvi.hwMinorVer = 0; lightvi.swMajorVer = 0;
        DF_CHECK(lightvi.guessVariant() == DesfireVariant::Light);

#undef DF_CHECK
        return true;
    }
    catch (const exception& e) {
        cout << "    Exception at line " << line << ": " << e.what() << "\n";
        return false;
    }
    catch (...) {
        cout << "    Unknown exception at line " << line << "\n";
        return false;
    }
}


// ════════════════════════════════════════════════════════════════════════════════
// TEST: DESFire Crypto + Auth (simulated 3-pass)
// ════════════════════════════════════════════════════════════════════════════════

bool testDesfireAuth() {
    int line = 0;
    try {
#define DA_CHECK(cond) do { line = __LINE__; if (!(cond)) { cout << "    FAIL at line " << line << ": " #cond "\n"; return false; } } while(0)

        // ── 1. BlockCipher AES round-trip (no padding) ───────────────────

        BYTEV aesKey(16, 0x00);
        BYTEV aesIV(16, 0x00);
        BYTEV plain(16, 0x42);

        BYTEV enc = crypto::block::encryptAesCbc(aesKey, aesIV, plain);
        DA_CHECK(enc.size() == 16);

        BYTEV dec = crypto::block::decryptAesCbc(aesKey, aesIV, enc);
        DA_CHECK(dec == plain);

        // Multi-block
        BYTEV plain32(32, 0xAB);
        BYTEV enc32 = crypto::block::encryptAesCbc(aesKey, aesIV, plain32);
        DA_CHECK(enc32.size() == 32);
        BYTEV dec32 = crypto::block::decryptAesCbc(aesKey, aesIV, enc32);
        DA_CHECK(dec32 == plain32);

        // ── 2. BlockCipher 2K3DES round-trip ─────────────────────────────

        BYTEV desKey(16, 0x00);
        BYTEV desIV(8, 0x00);
        BYTEV plainDES(8, 0x55);

        BYTEV encDES = crypto::block::encrypt2K3DesCbc(desKey, desIV, plainDES);
        DA_CHECK(encDES.size() == 8);
        BYTEV decDES = crypto::block::decrypt2K3DesCbc(desKey, desIV, encDES);
        DA_CHECK(decDES == plainDES);

        // ── 3. Rotate ───────────────────────────────────────────────────────

        BYTEV data = {0x01, 0x02, 0x03, 0x04};
        BYTEV rotL = DesfireCrypto::rotateLeft(data);
        DA_CHECK(rotL.size() == 4);
        DA_CHECK(rotL[0] == 0x02);
        DA_CHECK(rotL[1] == 0x03);
        DA_CHECK(rotL[2] == 0x04);
        DA_CHECK(rotL[3] == 0x01);

        BYTEV rotR = DesfireCrypto::rotateRight(rotL);
        DA_CHECK(rotR == data);

        // ── 4. Session key derivation (AES) ─────────────────────────────────

        BYTEV rndA = {
            0xA0,0xA1,0xA2,0xA3, 0xA4,0xA5,0xA6,0xA7,
            0xA8,0xA9,0xAA,0xAB, 0xAC,0xAD,0xAE,0xAF
        };
        BYTEV rndB = {
            0xB0,0xB1,0xB2,0xB3, 0xB4,0xB5,0xB6,0xB7,
            0xB8,0xB9,0xBA,0xBB, 0xBC,0xBD,0xBE,0xBF
        };

        BYTEV sk = DesfireCrypto::deriveSessionKey(rndA, rndB, DesfireKeyType::AES128);
        DA_CHECK(sk.size() == 16);
        // SK = A0A1A2A3 || B0B1B2B3 || ACADAEAF || BCBDBEBF
        DA_CHECK(sk[0]  == 0xA0); DA_CHECK(sk[3]  == 0xA3);
        DA_CHECK(sk[4]  == 0xB0); DA_CHECK(sk[7]  == 0xB3);
        DA_CHECK(sk[8]  == 0xAC); DA_CHECK(sk[11] == 0xAF);
        DA_CHECK(sk[12] == 0xBC); DA_CHECK(sk[15] == 0xBF);

        // ── 5. Session key derivation (2K3DES) ─────────────────────────────

        BYTEV rndA8 = {0xA0,0xA1,0xA2,0xA3, 0xA4,0xA5,0xA6,0xA7};
        BYTEV rndB8 = {0xB0,0xB1,0xB2,0xB3, 0xB4,0xB5,0xB6,0xB7};

        BYTEV skDES = DesfireCrypto::deriveSessionKey(rndA8, rndB8, DesfireKeyType::TwoDES);
        DA_CHECK(skDES.size() == 16);
        // SK = A0A1A2A3 || B0B1B2B3 || A4A5A6A7 || B4B5B6B7
        DA_CHECK(skDES[0]  == 0xA0); DA_CHECK(skDES[3]  == 0xA3);
        DA_CHECK(skDES[4]  == 0xB0); DA_CHECK(skDES[7]  == 0xB3);
        DA_CHECK(skDES[8]  == 0xA4); DA_CHECK(skDES[11] == 0xA7);
        DA_CHECK(skDES[12] == 0xB4); DA_CHECK(skDES[15] == 0xB7);

        // ── 6. APDU construction ────────────────────────────────────────────

        BYTEV authCmd = DesfireAuth::buildAuthCmd(0x00, DesfireKeyType::AES128);
        DA_CHECK(authCmd.size() == 7);
        DA_CHECK(authCmd[0] == 0x90);  // CLA
        DA_CHECK(authCmd[1] == 0xAA);  // INS = AUTH_AES
        DA_CHECK(authCmd[5] == 0x00);  // keyNo

        BYTEV authCmdDES = DesfireAuth::buildAuthCmd(0x02, DesfireKeyType::TwoDES);
        DA_CHECK(authCmdDES[1] == 0x1A);  // INS = AUTH_ISO
        DA_CHECK(authCmdDES[5] == 0x02);  // keyNo

        BYTEV payload16(32, 0xCC);
        BYTEV frame = DesfireAuth::buildAdditionalFrame(payload16);
        DA_CHECK(frame[0] == 0x90);
        DA_CHECK(frame[1] == 0xAF);
        DA_CHECK(frame[4] == 32);   // Lc
        DA_CHECK(frame.size() == 6 + 32);

        // ── 7. Simulated 3-pass auth (AES) ──────────────────────────────────
        //
        // Simulate card side: use known key, card generates RndB,
        // processes host payload, returns encrypted rotated RndA.
        //

        BYTEV simKey(16, 0x00);  // factory default key

        // Card's RndB (fixed for test)
        BYTEV simRndB = {
            0x10,0x11,0x12,0x13, 0x14,0x15,0x16,0x17,
            0x18,0x19,0x1A,0x1B, 0x1C,0x1D,0x1E,0x1F
        };

        // Card encrypts RndB and sends it
        BYTEV zeroIV16(16, 0);
        BYTEV encSimRndB = crypto::block::encryptAesCbc(simKey, zeroIV16, simRndB);

        // Simulated transmit: tracks which step we're on
        int simStep = 0;
        BYTEV capturedPayload;
        BYTEV simRndAcaptured;

        auto simTransmit = [&](const BYTEV& apdu) -> BYTEV {
            simStep++;
            if (simStep == 1) {
                // Auth command → return ek(RndB) + SW 91AF
                BYTEV resp;
                resp.insert(resp.end(), encSimRndB.begin(), encSimRndB.end());
                resp.push_back(0x91);
                resp.push_back(0xAF);
                return resp;
            }
            else if (simStep == 2) {
                // Additional frame → extract payload, simulate card verify

                // Extract encrypted payload from APDU
                // APDU: 90 AF 00 00 Lc [payload] 00
                size_t payloadLen = apdu[4];
                capturedPayload.assign(apdu.begin() + 5, apdu.begin() + 5 + payloadLen);

                // Card decrypts with IV = last block of encRndB
                size_t bs = 16;
                BYTEV cardIV(encSimRndB.end() - bs, encSimRndB.end());
                BYTEV decPayload = crypto::block::decryptAesCbc(simKey, cardIV, capturedPayload);

                // decPayload = RndA || rotL(RndB)
                simRndAcaptured.assign(decPayload.begin(), decPayload.begin() + 16);
                BYTEV gotRndBrot(decPayload.begin() + 16, decPayload.end());

                // Verify rotated RndB
                BYTEV expectedRndBrot = DesfireCrypto::rotateLeft(simRndB);
                if (gotRndBrot != expectedRndBrot)
                    throw std::runtime_error("Simulated card: RndB rotation mismatch");

                // Card encrypts rotL(RndA) and sends it
                BYTEV rndArot = DesfireCrypto::rotateLeft(simRndAcaptured);
                BYTEV cardIV2(capturedPayload.end() - bs, capturedPayload.end());
                BYTEV encRndArot = crypto::block::encryptAesCbc(simKey, cardIV2, rndArot);

                BYTEV resp;
                resp.insert(resp.end(), encRndArot.begin(), encRndArot.end());
                resp.push_back(0x91);
                resp.push_back(0x00);
                return resp;
            }
            throw std::runtime_error("Unexpected transmit call");
        };

        DesfireAuth auth;
        DesfireSession session;
        auth.authenticate(session, simKey, 0, DesfireKeyType::AES128, simTransmit);

        DA_CHECK(session.authenticated);
        DA_CHECK(session.authKeyNo == 0);
        DA_CHECK(session.sessionKey.size() == 16);
        DA_CHECK(session.iv.size() == 16);
        DA_CHECK(session.cmdCounter == 0);

        // Verify session key derivation matches
        BYTEV expectedSK = DesfireCrypto::deriveSessionKey(simRndAcaptured, simRndB,
                                                            DesfireKeyType::AES128);
        DA_CHECK(session.sessionKey == expectedSK);

        // ── 8. CMAC smoke test ──────────────────────────────────────────────

        BYTEV cmacKey(16, 0x00);
        BYTEV cmacData = {0x01, 0x02, 0x03, 0x04};
        BYTEV mac = crypto::block::cmacAes128(cmacKey, cmacData);
        DA_CHECK(mac.size() == 16);

        // Empty data CMAC
        BYTEV macEmpty = crypto::block::cmacAes128(cmacKey, BYTEV{});
        DA_CHECK(macEmpty.size() == 16);

        // Same input → same MAC
        BYTEV mac2 = crypto::block::cmacAes128(cmacKey, cmacData);
        DA_CHECK(mac == mac2);

        // Different input → different MAC
        BYTEV cmacData2 = {0x05, 0x06, 0x07, 0x08};
        BYTEV mac3 = crypto::block::cmacAes128(cmacKey, cmacData2);
        DA_CHECK(mac3 != mac);

        // ── 9. DesfireSession reset ─────────────────────────────────────────

        session.reset();
        DA_CHECK(!session.authenticated);
        DA_CHECK(session.sessionKey.empty());
        DA_CHECK(session.iv.empty());

#undef DA_CHECK
        return true;
    }
    catch (const exception& e) {
        cout << "    Exception at line " << line << ": " << e.what() << "\n";
        return false;
    }
    catch (...) {
        cout << "    Unknown exception at line " << line << "\n";
        return false;
    }
}


// ════════════════════════════════════════════════════════════════════════════════
// TEST: DESFire Commands (APDU construction, response parsing, simulated I/O)
// ════════════════════════════════════════════════════════════════════════════════

bool testDesfireCommands() {
    int line = 0;
    try {
#define DC_CHECK(cond) do { line = __LINE__; if (!(cond)) { cout << "    FAIL at line " << line << ": " #cond "\n"; return false; } } while(0)

        // ── 1. wrapCommand ──────────────────────────────────────────────────

        BYTEV cmd = DesfireCommands::wrapCommand(0x60);
        DC_CHECK(cmd.size() == 5);  // 90 60 00 00 00
        DC_CHECK(cmd[0] == 0x90);
        DC_CHECK(cmd[1] == 0x60);
        DC_CHECK(cmd[4] == 0x00);

        BYTEV cmdData = DesfireCommands::wrapCommand(0x5A, {0x01, 0x02, 0x03});
        DC_CHECK(cmdData.size() == 9);  // 90 5A 00 00 03 01 02 03 00
        DC_CHECK(cmdData[4] == 0x03);  // Lc
        DC_CHECK(cmdData[5] == 0x01);  // data[0]
        DC_CHECK(cmdData[8] == 0x00);  // Le

        // ── 2. SelectApplication APDU ───────────────────────────────────────

        DesfireAID testAid = DesfireAID::fromUint(0x010203);
        BYTEV selApp = DesfireCommands::selectApplication(testAid);
        DC_CHECK(selApp.size() == 9);
        DC_CHECK(selApp[1] == 0x5A);  // INS
        DC_CHECK(selApp[5] == testAid.aid[0]);
        DC_CHECK(selApp[6] == testAid.aid[1]);
        DC_CHECK(selApp[7] == testAid.aid[2]);

        // ── 3. ReadData / WriteData APDU ────────────────────────────────────

        BYTEV readCmd = DesfireCommands::readData(0x01, 0x000010, 0x000020);
        DC_CHECK(readCmd[1] == 0xBD);  // INS
        DC_CHECK(readCmd[5] == 0x01);  // fileNo
        // offset LE24: 0x10, 0x00, 0x00
        DC_CHECK(readCmd[6] == 0x10);
        DC_CHECK(readCmd[7] == 0x00);
        // length LE24: 0x20, 0x00, 0x00
        DC_CHECK(readCmd[9] == 0x20);

        BYTEV writePayload = {0xAA, 0xBB, 0xCC, 0xDD};
        BYTEV writeCmd = DesfireCommands::writeData(0x02, 0, writePayload);
        DC_CHECK(writeCmd[1] == 0x3D);  // INS
        DC_CHECK(writeCmd[5] == 0x02);  // fileNo

        // ── 4. LE24 helpers ─────────────────────────────────────────────────

        BYTE buf[3];
        DesfireCommands::writeLE24(buf, 0x123456);
        DC_CHECK(buf[0] == 0x56);
        DC_CHECK(buf[1] == 0x34);
        DC_CHECK(buf[2] == 0x12);
        DC_CHECK(DesfireCommands::readLE24(buf) == 0x123456);

        DesfireCommands::writeLE24(buf, 0);
        DC_CHECK(DesfireCommands::readLE24(buf) == 0);

        DesfireCommands::writeLE24(buf, 0xFFFFFF);
        DC_CHECK(DesfireCommands::readLE24(buf) == 0xFFFFFF);

        // ── 5. Response parsing ─────────────────────────────────────────────

        BYTEV respOK = {0xDE, 0xAD, 0x91, 0x00};
        DC_CHECK(DesfireCommands::isOK(respOK));
        DC_CHECK(!DesfireCommands::hasMore(respOK));
        DC_CHECK(DesfireCommands::statusCode(respOK) == 0x00);

        BYTEV dataOK = DesfireCommands::extractData(respOK);
        DC_CHECK(dataOK.size() == 2);
        DC_CHECK(dataOK[0] == 0xDE);

        BYTEV respMore = {0x01, 0x02, 0x03, 0x91, 0xAF};
        DC_CHECK(!DesfireCommands::isOK(respMore));
        DC_CHECK(DesfireCommands::hasMore(respMore));

        BYTEV respErr = {0x91, 0xAE};
        DC_CHECK(!DesfireCommands::isOK(respErr));

        // checkResponse should throw for error
        bool threw = false;
        try { DesfireCommands::checkResponse(respErr, "test"); }
        catch (const std::runtime_error&) { threw = true; }
        DC_CHECK(threw);

        // checkResponse should NOT throw for OK or MORE
        DesfireCommands::checkResponse(respOK, "test");
        DesfireCommands::checkResponse(respMore, "test");

        // ── 6. Multi-frame transceive (simulated) ──────────────────────────

        int frameCount = 0;
        auto simTransmit = [&](const BYTEV& apdu) -> BYTEV {
            frameCount++;
            if (frameCount == 1) {
                // First call: return partial data + AF
                return {0x10, 0x11, 0x12, 0x91, 0xAF};
            } else if (frameCount == 2) {
                // Second call (additional frame): return more data + AF
                return {0x20, 0x21, 0x91, 0xAF};
            } else {
                // Third call: final data + OK
                return {0x30, 0x91, 0x00};
            }
        };

        BYTEV initialCmd = DesfireCommands::wrapCommand(0x60);
        BYTEV allData = DesfireCommands::transceive(simTransmit, initialCmd);
        DC_CHECK(frameCount == 3);
        DC_CHECK(allData.size() == 6);  // 3 + 2 + 1
        DC_CHECK(allData[0] == 0x10);
        DC_CHECK(allData[3] == 0x20);
        DC_CHECK(allData[5] == 0x30);

        // ── 7. parseApplicationIDs ──────────────────────────────────────────

        BYTEV aidData = {0x01, 0x02, 0x03, 0xAA, 0xBB, 0xCC};
        auto aids = DesfireCommands::parseApplicationIDs(aidData);
        DC_CHECK(aids.size() == 2);
        DC_CHECK(aids[0].aid[0] == 0x01);
        DC_CHECK(aids[1].aid[0] == 0xAA);

        // ── 8. parseFileIDs ─────────────────────────────────────────────────

        BYTEV fileData = {0x00, 0x01, 0x02, 0x05};
        auto files = DesfireCommands::parseFileIDs(fileData);
        DC_CHECK(files.size() == 4);
        DC_CHECK(files[3] == 0x05);

        // ── 9. parseFileSettings ────────────────────────────────────────────

        // StandardData file: type=0x00, comm=0x00, access=2bytes, size=3bytes
        BYTEV fsData = {
            0x00,       // fileType = StandardData
            0x00,       // commMode = Plain
            0x23, 0xE0, // access rights (encoded)
            0x80, 0x00, 0x00  // fileSize = 128 (LE24)
        };
        auto fs = DesfireCommands::parseFileSettings(fsData);
        DC_CHECK(fs.fileType == DesfireFileType::StandardData);
        DC_CHECK(fs.commMode == DesfireCommMode::Plain);
        DC_CHECK(fs.standard.fileSize == 128);

        // ── 10. parseFreeMemory ─────────────────────────────────────────────

        BYTEV fmData = {0x00, 0x10, 0x00};  // 4096 LE24
        size_t freeMem = DesfireCommands::parseFreeMemory(fmData);
        DC_CHECK(freeMem == 4096);

        // ── 11. GetVersion simulated parse ──────────────────────────────────

        int gvStep = 0;
        auto gvTransmit = [&](const BYTEV& apdu) -> BYTEV {
            gvStep++;
            if (gvStep == 1) {
                // Part 1: HW info (7 bytes) + AF
                return {0x04, 0x01, 0x01, 0x01, 0x00, 0x18, 0x05, 0x91, 0xAF};
            } else if (gvStep == 2) {
                // Part 2: SW info (7 bytes) + AF
                return {0x04, 0x01, 0x01, 0x01, 0x00, 0x18, 0x05, 0x91, 0xAF};
            } else {
                // Part 3: UID(7) + batch(5) + prod(2) + OK
                return {0x04, 0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56,
                        0x00, 0x00, 0x00, 0x00, 0x00,
                        0x28, 0x19,
                        0x91, 0x00};
            }
        };

        auto vi = DesfireCommands::parseGetVersion(gvTransmit);
        DC_CHECK(vi.hwVendorID == 0x04);
        DC_CHECK(vi.hwStorageSize == 0x18);
        DC_CHECK(vi.swMajorVer == 0x01);
        DC_CHECK(vi.uid[0] == 0x04);
        DC_CHECK(vi.uid[1] == 0xAB);
        DC_CHECK(vi.productionWeek == 0x28);
        DC_CHECK(vi.productionYear == 0x19);
        DC_CHECK(vi.totalCapacity() == 4096);

#undef DC_CHECK
        return true;
    }
    catch (const exception& e) {
        cout << "    Exception at line " << line << ": " << e.what() << "\n";
        return false;
    }
    catch (...) {
        cout << "    Unknown exception at line " << line << "\n";
        return false;
    }
}


// ════════════════════════════════════════════════════════════════════════════════
// TEST: DESFire Management APDU + Secure Messaging
// ════════════════════════════════════════════════════════════════════════════════

bool testDesfireManagement() {
    int line = 0;
    try {
#define DM_CHECK(cond) do { line = __LINE__; if (!(cond)) { cout << "    FAIL at line " << line << ": " #cond "\n"; return false; } } while(0)

        // ── 1. CreateApplication APDU ───────────────────────────────────────

        DesfireAID appAid = DesfireAID::fromUint(0x010203);
        BYTEV caCmd = DesfireCommands::createApplication(
            appAid, 0x0F, 3, DesfireKeyType::AES128);
        DM_CHECK(caCmd[0] == 0x90);
        DM_CHECK(caCmd[1] == 0xCA);  // INS
        DM_CHECK(caCmd[5] == appAid.aid[0]);
        DM_CHECK(caCmd[6] == appAid.aid[1]);
        DM_CHECK(caCmd[7] == appAid.aid[2]);
        DM_CHECK(caCmd[8] == 0x0F);  // keySettings
        DM_CHECK((caCmd[9] & 0x0F) == 3);    // maxKeys
        DM_CHECK((caCmd[9] & 0x80) == 0x80); // AES flag

        // 2K3DES variant
        BYTEV caDesCmd = DesfireCommands::createApplication(
            appAid, 0x0B, 2, DesfireKeyType::TwoDES);
        DM_CHECK((caDesCmd[9] & 0x80) == 0x00);  // no AES flag
        DM_CHECK((caDesCmd[9] & 0x0F) == 2);

        // ── 2. DeleteApplication APDU ───────────────────────────────────────

        BYTEV daCmd = DesfireCommands::deleteApplication(appAid);
        DM_CHECK(daCmd[1] == 0xDA);
        DM_CHECK(daCmd[5] == appAid.aid[0]);

        // ── 3. CreateStdDataFile APDU ───────────────────────────────────────

        DesfireAccessRights ar;
        ar.readKey = 0x00; ar.writeKey = 0x00;
        ar.readWriteKey = 0x00; ar.changeKey = 0x00;

        BYTEV csfCmd = DesfireCommands::createStdDataFile(
            0x01, DesfireCommMode::Plain, ar, 256);
        DM_CHECK(csfCmd[1] == 0xCD);   // INS
        DM_CHECK(csfCmd[5] == 0x01);   // fileNo
        DM_CHECK(csfCmd[6] == 0x00);   // commMode = Plain
        // fileSize = 256 = 0x000100 LE
        DM_CHECK(csfCmd[9] == 0x00);   // LE byte 0
        DM_CHECK(csfCmd[10] == 0x01);  // LE byte 1

        // ── 4. CreateValueFile APDU ─────────────────────────────────────────

        BYTEV cvfCmd = DesfireCommands::createValueFile(
            0x02, DesfireCommMode::MAC, ar, 0, 10000, 500, true);
        DM_CHECK(cvfCmd[1] == 0xCC);   // INS
        DM_CHECK(cvfCmd[5] == 0x02);   // fileNo
        DM_CHECK(cvfCmd[6] == 0x01);   // commMode = MAC

        // ── 5. CreateLinearRecordFile APDU ──────────────────────────────────

        BYTEV clrCmd = DesfireCommands::createLinearRecordFile(
            0x03, DesfireCommMode::Full, ar, 32, 100);
        DM_CHECK(clrCmd[1] == 0xC1);   // INS
        DM_CHECK(clrCmd[6] == 0x03);   // commMode = Full

        // CyclicRecordFile
        BYTEV ccrCmd = DesfireCommands::createCyclicRecordFile(
            0x04, DesfireCommMode::Plain, ar, 16, 50);
        DM_CHECK(ccrCmd[1] == 0xC0);

        // ── 6. DeleteFile APDU ──────────────────────────────────────────────

        BYTEV dfCmd = DesfireCommands::deleteFile(0x01);
        DM_CHECK(dfCmd[1] == 0xDF);
        DM_CHECK(dfCmd[5] == 0x01);

        // ── 7. Transaction APDUs ────────────────────────────────────────────

        BYTEV crCmd = DesfireCommands::credit(0x02, 100);
        DM_CHECK(crCmd[1] == 0x0C);
        DM_CHECK(crCmd[5] == 0x02);
        // value=100 LE32: 0x64, 0x00, 0x00, 0x00
        DM_CHECK(crCmd[6] == 0x64);

        BYTEV dbCmd = DesfireCommands::debit(0x02, 50);
        DM_CHECK(dbCmd[1] == 0xDC);
        DM_CHECK(dbCmd[6] == 0x32);  // 50 = 0x32

        BYTEV ctCmd = DesfireCommands::commitTransaction();
        DM_CHECK(ctCmd[1] == 0xC7);
        DM_CHECK(ctCmd.size() == 5);  // no data

        BYTEV atCmd = DesfireCommands::abortTransaction();
        DM_CHECK(atCmd[1] == 0xA7);

        // ── 8. Key management APDUs ─────────────────────────────────────────

        BYTEV gksCmd = DesfireCommands::getKeySettings();
        DM_CHECK(gksCmd[1] == 0x45);

        BYTEV gkvCmd = DesfireCommands::getKeyVersion(0x02);
        DM_CHECK(gkvCmd[1] == 0x64);
        DM_CHECK(gkvCmd[5] == 0x02);

        BYTEV ckCmd = DesfireCommands::changeKey(0x01, BYTEV(32, 0xAA));
        DM_CHECK(ckCmd[1] == 0xC4);
        DM_CHECK(ckCmd[5] == 0x01);  // keyNo

        // FormatPICC
        BYTEV fpCmd = DesfireCommands::formatPICC();
        DM_CHECK(fpCmd[1] == 0xFC);

        // ── 9. Secure Messaging — CMAC + truncate ──────────────────────────

        // Create a fake session
        DesfireSession session;
        session.authenticated = true;
        session.keyType = DesfireKeyType::AES128;
        session.sessionKey = BYTEV(16, 0x42);
        session.iv = BYTEV(16, 0x00);

        // calculateCMAC should return 16 bytes and update IV
        BYTEV testData = {0x01, 0x02, 0x03};
        BYTEV mac1 = DesfireSecureMessaging::calculateCMAC(session, testData);
        DM_CHECK(mac1.size() == 16);
        DM_CHECK(session.iv == mac1);  // IV should be updated to CMAC

        // Same key + same input → same CMAC (AES-CMAC is deterministic)
        session.iv = BYTEV(16, 0x00);
        BYTEV mac1b = DesfireSecureMessaging::calculateCMAC(session, testData);
        DM_CHECK(mac1b == mac1);

        // Different input → different CMAC
        BYTEV testData2 = {0x04, 0x05, 0x06};
        session.iv = BYTEV(16, 0x00);
        BYTEV mac2 = DesfireSecureMessaging::calculateCMAC(session, testData2);
        DM_CHECK(mac2.size() == 16);
        DM_CHECK(mac1 != mac2);

        // truncateCMAC — odd-indexed bytes
        BYTEV fullMAC = {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
                         0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF};
        BYTEV trunc = DesfireSecureMessaging::truncateCMAC(fullMAC);
        DM_CHECK(trunc.size() == 8);
        DM_CHECK(trunc[0] == 0x11);
        DM_CHECK(trunc[1] == 0x33);
        DM_CHECK(trunc[2] == 0x55);
        DM_CHECK(trunc[3] == 0x77);
        DM_CHECK(trunc[4] == 0x99);
        DM_CHECK(trunc[5] == 0xBB);
        DM_CHECK(trunc[6] == 0xDD);
        DM_CHECK(trunc[7] == 0xFF);

        // ── 10. Secure Messaging — wrap/unwrap round-trip ───────────────────

        session.iv = BYTEV(16, 0x00);  // reset IV

        BYTEV cmdHeader = {0x90, 0xBD};  // ReadData CLA+INS
        BYTEV cmdBody = {0x01, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00}; // fileNo + offset + len

        // Wrap in MAC mode
        BYTEV wrappedCmd = DesfireSecureMessaging::wrapCommand(
            session, cmdHeader, cmdBody, DesfireCommMode::MAC);
        // Wrapped should be longer than original (8 CMAC bytes added)
        DM_CHECK(wrappedCmd.size() > cmdBody.size() + 5);

        // Simulate response with MAC
        BYTEV ivBeforeUnwrap = session.iv;
        BYTEV responsePayload = {0xDE, 0xAD, 0xBE, 0xEF};

        // Generate expected CMAC for this response
        BYTEV cmacInput = responsePayload;
        cmacInput.push_back(0x00);  // status code
        BYTEV respMAC = DesfireSecureMessaging::calculateCMAC(session, cmacInput);
        BYTEV respTrunc = DesfireSecureMessaging::truncateCMAC(respMAC);

        // Build complete response: data + truncated CMAC
        BYTEV fullResp = responsePayload;
        fullResp.insert(fullResp.end(), respTrunc.begin(), respTrunc.end());

        // Reset IV to what it was before CMAC calc (to simulate verifier)
        session.iv = ivBeforeUnwrap;

        BYTEV unwrapped = DesfireSecureMessaging::unwrapResponse(
            session, fullResp, 0x00, DesfireCommMode::MAC);
        DM_CHECK(unwrapped.size() == 4);
        DM_CHECK(unwrapped[0] == 0xDE);
        DM_CHECK(unwrapped[3] == 0xEF);

        // ── 11. Plain mode should not throw ─────────────────────────────────

        session.iv = BYTEV(16, 0x00);
        BYTEV plainWrap = DesfireSecureMessaging::wrapCommand(
            session, cmdHeader, cmdBody, DesfireCommMode::Plain);
        DM_CHECK(!plainWrap.empty());

        BYTEV plainUnwrap = DesfireSecureMessaging::unwrapResponse(
            session, responsePayload, 0x00, DesfireCommMode::Plain);
        DM_CHECK(plainUnwrap == responsePayload);

#undef DM_CHECK
        return true;
    }
    catch (const exception& e) {
        cout << "    Exception at line " << line << ": " << e.what() << "\n";
        return false;
    }
    catch (...) {
        cout << "    Unknown exception at line " << line << "\n";
        return false;
    }
}


// ════════════════════════════════════════════════════════════════════════════════
// TEST: DESFire Integration — Full Lifecycle (simulated card)
// ════════════════════════════════════════════════════════════════════════════════

bool testDesfireIntegration() {
    int line = 0;
    try {
#define DI_CHECK(cond) do { line = __LINE__; if (!(cond)) { cout << "    FAIL at line " << line << ": " #cond "\n"; return false; } } while(0)

        // ── 1. Session timeout — isValid / isExpired ────────────────────────

        {
            DesfireSession ses;
            DI_CHECK(!ses.isValid());         // not authenticated
            DI_CHECK(ses.isExpired());        // expired because not auth

            ses.authenticated = true;
            ses.sessionKey = BYTEV(16, 0x42);
            ses.touchAuthTime();
            DI_CHECK(ses.isValid());          // auth + no timeout
            DI_CHECK(!ses.isExpired());

            // Set very long timeout — should not expire
            ses.setTimeoutMs(60000);
            DI_CHECK(ses.isValid());
            DI_CHECK(!ses.isExpired());

            // Set timeout to 0 — infinite
            ses.setTimeoutMs(0);
            DI_CHECK(ses.isValid());

            // Set 1ms timeout, wait, should expire
            ses.setTimeoutMs(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            DI_CHECK(ses.isExpired());
            DI_CHECK(!ses.isValid());

            // Reset session — should not be valid
            ses.reset();
            DI_CHECK(!ses.isValid());
            DI_CHECK(ses.sessionKey.empty());
        }

        // ── 2. Session resetKeepApp ─────────────────────────────────────────

        {
            DesfireSession ses;
            ses.authenticated = true;
            ses.currentAID = DesfireAID::fromUint(0x123456);
            ses.sessionKey = BYTEV(16, 0xFF);
            ses.resetKeepApp();
            DI_CHECK(!ses.authenticated);
            DI_CHECK(ses.sessionKey.empty());
            DI_CHECK(ses.currentAID.toUint() == 0x123456);
        }

        // ── 3. Full 3-pass auth → session → timeout lifecycle ───────────────

        {
            BYTEV simKey(16, 0x00);
            BYTEV simRndB = {
                0x20,0x21,0x22,0x23, 0x24,0x25,0x26,0x27,
                0x28,0x29,0x2A,0x2B, 0x2C,0x2D,0x2E,0x2F
            };

            BYTEV zeroIV16(16, 0);
            BYTEV encSimRndB = crypto::block::encryptAesCbc(simKey, zeroIV16, simRndB);

            int step = 0;
            BYTEV capturedRndA;

            auto transmit = [&](const BYTEV& apdu) -> BYTEV {
                step++;
                if (step == 1) {
                    BYTEV resp;
                    resp.insert(resp.end(), encSimRndB.begin(), encSimRndB.end());
                    resp.push_back(0x91); resp.push_back(0xAF);
                    return resp;
                } else {
                    size_t payLen = apdu[4];
                    BYTEV payload(apdu.begin() + 5, apdu.begin() + 5 + payLen);
                    size_t bs = 16;
                    BYTEV cardIV(encSimRndB.end() - bs, encSimRndB.end());
                    BYTEV dec = crypto::block::decryptAesCbc(simKey, cardIV, payload);
                    capturedRndA.assign(dec.begin(), dec.begin() + 16);
                    BYTEV rndArot = DesfireCrypto::rotateLeft(capturedRndA);
                    BYTEV cardIV2(payload.end() - bs, payload.end());
                    BYTEV encRndArot = crypto::block::encryptAesCbc(simKey, cardIV2, rndArot);
                    BYTEV resp;
                    resp.insert(resp.end(), encRndArot.begin(), encRndArot.end());
                    resp.push_back(0x91); resp.push_back(0x00);
                    return resp;
                }
            };

            DesfireAuth auth;
            DesfireSession session;
            session.setTimeoutMs(100);  // 100ms timeout

            auth.authenticate(session, simKey, 0, DesfireKeyType::AES128, transmit);
            DI_CHECK(session.isValid());
            DI_CHECK(session.authenticated);
            DI_CHECK(!session.isExpired());

            // Session key check
            BYTEV expectedSK = DesfireCrypto::deriveSessionKey(capturedRndA, simRndB,
                                                                DesfireKeyType::AES128);
            DI_CHECK(session.sessionKey == expectedSK);

            // Wait for timeout
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            DI_CHECK(session.isExpired());
            DI_CHECK(!session.isValid());
            DI_CHECK(session.authenticated);  // flag still true, but isValid = false
        }

        // ── 4. DesfireAccessRights encode/decode round-trip ─────────────────

        {
            DesfireAccessRights ar;
            ar.readKey = 0x02;
            ar.writeKey = 0x03;
            ar.readWriteKey = 0x0E;  // free
            ar.changeKey = 0x00;

            auto encoded = ar.encode();
            DI_CHECK(encoded.size() == 2);

            auto decoded = DesfireAccessRights::decode(encoded[0], encoded[1]);
            DI_CHECK(decoded.readKey == 0x02);
            DI_CHECK(decoded.writeKey == 0x03);
            DI_CHECK(decoded.readWriteKey == 0x0E);
            DI_CHECK(decoded.changeKey == 0x00);
        }

        // ── 5. DesfireCommands full command set verification ────────────────

        {
            // SelectApp + GetVersion + GetAppIDs + GetFileIDs cover Faz 3
            // Management commands cover Faz 4

            // Verify all commands have correct INS bytes
            DI_CHECK(DesfireCommands::selectApplication(DesfireAID::picc())[1] == 0x5A);
            DI_CHECK(DesfireCommands::getVersion()[1] == 0x60);
            DI_CHECK(DesfireCommands::getApplicationIDs()[1] == 0x6A);
            DI_CHECK(DesfireCommands::getFileIDs()[1] == 0x6F);
            DI_CHECK(DesfireCommands::getFileSettings(0)[1] == 0xF5);
            DI_CHECK(DesfireCommands::readData(0,0,0)[1] == 0xBD);
            DI_CHECK(DesfireCommands::writeData(0,0,{})[1] == 0x3D);
            DI_CHECK(DesfireCommands::getValue(0)[1] == 0x6C);
            DI_CHECK(DesfireCommands::getFreeMemory()[1] == 0x6E);
            DI_CHECK(DesfireCommands::additionalFrame()[1] == 0xAF);

            // Management
            DesfireAccessRights ar{};
            DI_CHECK(DesfireCommands::createApplication(DesfireAID::picc(),0,1,DesfireKeyType::AES128)[1] == 0xCA);
            DI_CHECK(DesfireCommands::deleteApplication(DesfireAID::picc())[1] == 0xDA);
            DI_CHECK(DesfireCommands::createStdDataFile(0,DesfireCommMode::Plain,ar,1)[1] == 0xCD);
            DI_CHECK(DesfireCommands::createBackupDataFile(0,DesfireCommMode::Plain,ar,1)[1] == 0xCB);
            DI_CHECK(DesfireCommands::createValueFile(0,DesfireCommMode::Plain,ar,0,0,0,false)[1] == 0xCC);
            DI_CHECK(DesfireCommands::createLinearRecordFile(0,DesfireCommMode::Plain,ar,1,1)[1] == 0xC1);
            DI_CHECK(DesfireCommands::createCyclicRecordFile(0,DesfireCommMode::Plain,ar,1,1)[1] == 0xC0);
            DI_CHECK(DesfireCommands::deleteFile(0)[1] == 0xDF);
            DI_CHECK(DesfireCommands::changeKey(0,{})[1] == 0xC4);
            DI_CHECK(DesfireCommands::getKeySettings()[1] == 0x45);
            DI_CHECK(DesfireCommands::changeKeySettings({})[1] == 0x54);
            DI_CHECK(DesfireCommands::getKeyVersion(0)[1] == 0x64);
            DI_CHECK(DesfireCommands::credit(0,0)[1] == 0x0C);
            DI_CHECK(DesfireCommands::debit(0,0)[1] == 0xDC);
            DI_CHECK(DesfireCommands::commitTransaction()[1] == 0xC7);
            DI_CHECK(DesfireCommands::abortTransaction()[1] == 0xA7);
            DI_CHECK(DesfireCommands::formatPICC()[1] == 0xFC);
        }

        // ── 6. Secure Messaging — end-to-end MAC verification ───────────────

        {
            DesfireSession ses;
            ses.authenticated = true;
            ses.keyType = DesfireKeyType::AES128;
            ses.sessionKey = BYTEV(16, 0x55);
            ses.iv = BYTEV(16, 0x00);

            // Simulate: host sends command with MAC
            BYTEV cmdHeader = {0x90, 0xBD};
            BYTEV cmdBody = {0x01, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00};

            // Save IV state before wrap
            BYTEV ivBefore = ses.iv;
            BYTEV wrapped = DesfireSecureMessaging::wrapCommand(
                ses, cmdHeader, cmdBody, DesfireCommMode::MAC);

            // Wrapped APDU should have 8 extra bytes (CMAC)
            // Original: header(5) + body(7) + Le(1) = 13
            // With MAC: header(5) + body(7) + mac(8) + Le(1) = 21
            DI_CHECK(wrapped.size() == 21);

            // Simulate: card responds with data + MAC
            BYTEV respData = {0xAA, 0xBB, 0xCC};
            BYTEV statusSW = {0x00};

            // Calculate what card's MAC should be
            BYTEV saveIV = ses.iv;
            BYTEV cmacIn = respData;
            cmacIn.push_back(0x00);
            BYTEV fullMAC = DesfireSecureMessaging::calculateCMAC(ses, cmacIn);
            BYTEV truncMAC = DesfireSecureMessaging::truncateCMAC(fullMAC);

            // Build response with MAC
            BYTEV fullResp = respData;
            fullResp.insert(fullResp.end(), truncMAC.begin(), truncMAC.end());

            // Unwrap — must reset IV to what it was before CMAC calc
            ses.iv = saveIV;
            BYTEV result = DesfireSecureMessaging::unwrapResponse(
                ses, fullResp, 0x00, DesfireCommMode::MAC);
            DI_CHECK(result.size() == 3);
            DI_CHECK(result[0] == 0xAA);

            // Wrong MAC should throw
            ses.iv = saveIV;
            BYTEV badResp = respData;
            badResp.insert(badResp.end(), 8, 0xFF);  // bad MAC
            bool threw = false;
            try {
                DesfireSecureMessaging::unwrapResponse(
                    ses, badResp, 0x00, DesfireCommMode::MAC);
            } catch (const std::runtime_error&) { threw = true; }
            DI_CHECK(threw);
        }

        // ── 7. DesfireMemoryLayout — model consistency ──────────────────────

        {
            CardInterface card(CardType::MifareDesfire);
            DesfireMemoryLayout& dfm = card.getDesfireMemoryMutable();

            // Model init from version
            DesfireVersionInfo vi;
            vi.hwStorageSize = 0x18;  // 4KB
            vi.hwMajorVer = 1;
            vi.swMajorVer = 1;
            std::memset(vi.uid, 0x04, 7);
            dfm.initFromVersion(vi);

            DI_CHECK(card.getTotalMemory() == 4096);
            DI_CHECK(dfm.versionInfo.guessVariant() == DesfireVariant::EV1);

            // Topology checks
            DI_CHECK(card.isDesfire());
            DI_CHECK(!card.isClassic());
            DI_CHECK(card.getTotalSectors() == 0);
            DI_CHECK(card.getTotalBlocks() == 0);

            // DESFire guards — Classic-only ops
            DI_CHECK(!card.isTrailerBlock(0));  // DESFire'da trailer yok

            bool threwExport = false;
            try { card.exportMemory(); }
            catch (const std::logic_error&) { threwExport = true; }
            DI_CHECK(threwExport);
        }

        // ── 8. Regression — Classic API still works ─────────────────────────

        {
            CardInterface classic1K(CardType::MifareClassic1K);
            DI_CHECK(classic1K.isClassic());
            DI_CHECK(!classic1K.isDesfire());
            DI_CHECK(classic1K.getTotalSectors() == 16);
            DI_CHECK(classic1K.getTotalBlocks() == 64);

            // Trailer detection
            DI_CHECK(classic1K.isTrailerBlock(3));
            DI_CHECK(!classic1K.isTrailerBlock(2));

            // Memory access
            std::vector<BYTE> memBuf(1024, 0);
            classic1K.loadMemory(memBuf.data(), memBuf.size());
            BYTEV exported = classic1K.exportMemory();
            DI_CHECK(exported.size() == 16 * 64);  // 1K full memory

            CardInterface classic4K(CardType::MifareClassic4K);
            DI_CHECK(classic4K.getTotalSectors() == 40);
            DI_CHECK(classic4K.getTotalBlocks() == 256);
        }

#undef DI_CHECK
        return true;
    }
    catch (const exception& e) {
        cout << "    Exception at line " << line << ": " << e.what() << "\n";
        return false;
    }
    catch (...) {
        cout << "    Unknown exception at line " << line << "\n";
        return false;
    }
}


// ════════════════════════════════════════════════════════════════════════════════
// TEST: 3K3DES — cipher, auth, session key
// ════════════════════════════════════════════════════════════════════════════════

bool testDesfire3K3DES() {
    int line = 0;
    try {
#define D3_CHECK(cond) do { line = __LINE__; if (!(cond)) { cout << "    FAIL at line " << line << ": " #cond "\n"; return false; } } while(0)

        // ── 1. BlockCipher 3K3DES round-trip ─────────────────────────────

        BYTEV key24(24, 0x00);
        key24[0] = 0x11; key24[8] = 0x22; key24[16] = 0x33;
        BYTEV iv8(8, 0x00);
        BYTEV plain8(8, 0x42);

        BYTEV enc = crypto::block::encrypt3K3DesCbc(key24, iv8, plain8);
        D3_CHECK(enc.size() == 8);
        D3_CHECK(enc != plain8);

        BYTEV dec = crypto::block::decrypt3K3DesCbc(key24, iv8, enc);
        D3_CHECK(dec == plain8);

        // Multi-block
        BYTEV plain16(16, 0x55);
        BYTEV enc16 = crypto::block::encrypt3K3DesCbc(key24, iv8, plain16);
        D3_CHECK(enc16.size() == 16);
        BYTEV dec16 = crypto::block::decrypt3K3DesCbc(key24, iv8, enc16);
        D3_CHECK(dec16 == plain16);

        // Wrong key size should throw
        bool threw = false;
        try { crypto::block::encrypt3K3DesCbc(BYTEV(16, 0), iv8, plain8); }
        catch (const std::invalid_argument&) { threw = true; }
        D3_CHECK(threw);

        // ── 2. DesfireCrypto sizes ──────────────────────────────────────────

        D3_CHECK(DesfireCrypto::keySize(DesfireKeyType::ThreeDES) == 24);
        D3_CHECK(DesfireCrypto::nonceSize(DesfireKeyType::ThreeDES) == 16);
        D3_CHECK(DesfireCrypto::blockSize(DesfireKeyType::ThreeDES) == 8);  // DES cipher block = 8

        // ── 3. Session key derivation — 3K3DES ─────────────────────────────

        BYTEV rndA16(16, 0xAA);
        for (size_t i = 0; i < 16; i++) rndA16[i] = static_cast<BYTE>(i + 0x10);

        BYTEV rndB16(16, 0xBB);
        for (size_t i = 0; i < 16; i++) rndB16[i] = static_cast<BYTE>(i + 0x30);

        BYTEV sk = DesfireCrypto::deriveSessionKey(rndA16, rndB16, DesfireKeyType::ThreeDES);
        D3_CHECK(sk.size() == 24);

        // SK = RndA[0..3] || RndB[0..3] || RndA[6..9] || RndB[6..9] || RndA[12..15] || RndB[12..15]
        D3_CHECK(sk[0] == rndA16[0]);
        D3_CHECK(sk[3] == rndA16[3]);
        D3_CHECK(sk[4] == rndB16[0]);
        D3_CHECK(sk[7] == rndB16[3]);
        D3_CHECK(sk[8] == rndA16[6]);
        D3_CHECK(sk[11] == rndA16[9]);
        D3_CHECK(sk[12] == rndB16[6]);
        D3_CHECK(sk[15] == rndB16[9]);
        D3_CHECK(sk[16] == rndA16[12]);
        D3_CHECK(sk[19] == rndA16[15]);
        D3_CHECK(sk[20] == rndB16[12]);
        D3_CHECK(sk[23] == rndB16[15]);

        // ── 4. Simulated 3K3DES 3-pass auth ────────────────────────────────

        // 3K3DES: nonce=16 bytes, block=8 bytes (DES block), key=24 bytes
        // But DESFire 3K3DES auth uses 16-byte nonces with 8-byte DES blocks
        // So encRndB = 16 bytes (2 DES blocks)

        BYTEV simKey(24, 0x00);
        simKey[0] = 0xAB; simKey[8] = 0xCD; simKey[16] = 0xEF;

        BYTEV simRndB = {
            0x20,0x21,0x22,0x23, 0x24,0x25,0x26,0x27,
            0x28,0x29,0x2A,0x2B, 0x2C,0x2D,0x2E,0x2F
        };

        // blockSize for 3K3DES = 16 (uses two 8-byte DES blocks chained)
        // But 3K3DES operates on 8-byte blocks; nonce is 16 bytes.
        // Auth uses zero IV (8-byte for DES)
        BYTEV zeroIV8(8, 0);
        BYTEV encSimRndB = crypto::block::encrypt3K3DesCbc(simKey, zeroIV8, simRndB);
        D3_CHECK(encSimRndB.size() == 16);

        int step = 0;
        BYTEV capturedRndA;

        auto transmit = [&](const BYTEV& apdu) -> BYTEV {
            step++;
            if (step == 1) {
                BYTEV resp;
                resp.insert(resp.end(), encSimRndB.begin(), encSimRndB.end());
                resp.push_back(0x91); resp.push_back(0xAF);
                return resp;
            } else {
                // 3K3DES: blockSize from DesfireCrypto = 16, but DES physical = 8
                // Extract payload, decrypt, verify
                size_t payLen = apdu[4];
                BYTEV payload(apdu.begin() + 5, apdu.begin() + 5 + payLen);

                // IV for step 2: last 8 bytes of encRndB
                BYTEV cardIV(encSimRndB.end() - 8, encSimRndB.end());
                BYTEV dec = crypto::block::decrypt3K3DesCbc(simKey, cardIV, payload);
                capturedRndA.assign(dec.begin(), dec.begin() + 16);

                BYTEV rndArot = DesfireCrypto::rotateLeft(capturedRndA);
                BYTEV cardIV2(payload.end() - 8, payload.end());
                BYTEV encRndArot = crypto::block::encrypt3K3DesCbc(simKey, cardIV2, rndArot);

                BYTEV resp;
                resp.insert(resp.end(), encRndArot.begin(), encRndArot.end());
                resp.push_back(0x91); resp.push_back(0x00);
                return resp;
            }
        };

        DesfireAuth auth;
        DesfireSession session;
        auth.authenticate(session, simKey, 0, DesfireKeyType::ThreeDES, transmit);

        D3_CHECK(session.authenticated);
        D3_CHECK(session.sessionKey.size() == 24);

        // Verify session key derivation
        BYTEV expectedSK = DesfireCrypto::deriveSessionKey(capturedRndA, simRndB,
                                                            DesfireKeyType::ThreeDES);
        D3_CHECK(session.sessionKey == expectedSK);

        // ── 5. CreateApplication with 3K3DES flag ──────────────────────────

        BYTEV caCmd = DesfireCommands::createApplication(
            DesfireAID::fromUint(0x010203), 0x0F, 3, DesfireKeyType::ThreeDES);
        D3_CHECK((caCmd[9] & 0x40) == 0x40);  // 3K3DES flag
        D3_CHECK((caCmd[9] & 0x0F) == 3);     // maxKeys

#undef D3_CHECK
        return true;
    }
    catch (const exception& e) {
        cout << "    Exception at line " << line << ": " << e.what() << "\n";
        return false;
    }
    catch (...) {
        cout << "    Unknown exception at line " << line << "\n";
        return false;
    }
}


// ════════════════════════════════════════════════════════════════════════════════
// TEST: DESFire Record File Operations
// ════════════════════════════════════════════════════════════════════════════════

bool testDesfireRecordFiles() {
    int line = 0;
    try {
#define RF_CHECK(cond) do { line = __LINE__; if (!(cond)) { cout << "    FAIL at line " << line << ": " #cond "\n"; return false; } } while(0)

        // ── 1. ReadRecords APDU ─────────────────────────────────────────────

        BYTEV rrCmd = DesfireCommands::readRecords(0x02, 0, 10);
        RF_CHECK(rrCmd[0] == 0x90);
        RF_CHECK(rrCmd[1] == 0xBB);  // INS
        RF_CHECK(rrCmd[5] == 0x02);  // fileNo
        // offset = 0 LE24
        RF_CHECK(rrCmd[6] == 0x00);
        RF_CHECK(rrCmd[7] == 0x00);
        RF_CHECK(rrCmd[8] == 0x00);
        // count = 10 LE24
        RF_CHECK(rrCmd[9] == 10);
        RF_CHECK(rrCmd[10] == 0);
        RF_CHECK(rrCmd[11] == 0);

        // ── 2. ReadRecords — multi-byte indices ─────────────────────────────

        BYTEV rrCmd2 = DesfireCommands::readRecords(0x01, 256, 65535);
        RF_CHECK(rrCmd2[5] == 0x01);
        // 256 LE24 = {0x00, 0x01, 0x00}
        RF_CHECK(rrCmd2[6] == 0x00);
        RF_CHECK(rrCmd2[7] == 0x01);
        RF_CHECK(rrCmd2[8] == 0x00);
        // 65535 LE24 = {0xFF, 0xFF, 0x00}
        RF_CHECK(rrCmd2[9] == 0xFF);
        RF_CHECK(rrCmd2[10] == 0xFF);
        RF_CHECK(rrCmd2[11] == 0x00);

        // ── 3. ReadRecords — count=0 (read all) ────────────────────────────

        BYTEV rrAll = DesfireCommands::readRecords(0x03, 0, 0);
        RF_CHECK(rrAll[1] == 0xBB);
        RF_CHECK(rrAll[9] == 0);

        // ── 4. AppendRecord APDU ────────────────────────────────────────────

        BYTEV payload = {0x11, 0x22, 0x33, 0x44, 0x55};
        BYTEV arCmd = DesfireCommands::appendRecord(0x02, payload);
        RF_CHECK(arCmd[0] == 0x90);
        RF_CHECK(arCmd[1] == 0x3B);  // INS
        RF_CHECK(arCmd[5] == 0x02);  // fileNo
        // offset = 0 (always)
        RF_CHECK(arCmd[6] == 0x00);
        RF_CHECK(arCmd[7] == 0x00);
        RF_CHECK(arCmd[8] == 0x00);
        // length = 5 LE24
        RF_CHECK(arCmd[9] == 5);
        RF_CHECK(arCmd[10] == 0);
        RF_CHECK(arCmd[11] == 0);
        // record data
        RF_CHECK(arCmd[12] == 0x11);
        RF_CHECK(arCmd[13] == 0x22);
        RF_CHECK(arCmd[16] == 0x55);

        // ── 5. AppendRecord — empty payload ─────────────────────────────────

        BYTEV arEmpty = DesfireCommands::appendRecord(0x03, {});
        RF_CHECK(arEmpty[1] == 0x3B);
        RF_CHECK(arEmpty[5] == 0x03);
        // length = 0
        RF_CHECK(arEmpty[9] == 0);
        RF_CHECK(arEmpty[10] == 0);
        RF_CHECK(arEmpty[11] == 0);

        // ── 6. AppendRecord — large payload (1024 bytes) ────────────────────

        BYTEV large(1024, 0xBB);
        BYTEV arLarge = DesfireCommands::appendRecord(0x04, large);
        RF_CHECK(arLarge[1] == 0x3B);
        // 1024 = 0x000400 LE24 = {0x00, 0x04, 0x00}
        RF_CHECK(arLarge[9] == 0x00);
        RF_CHECK(arLarge[10] == 0x04);
        RF_CHECK(arLarge[11] == 0x00);
        RF_CHECK(arLarge[12] == 0xBB);

        // ── 7. Simulated multi-frame read ───────────────────────────────────

        int frame = 0;
        auto simTransmit = [&](const BYTEV& apdu) -> BYTEV {
            frame++;
            if (frame == 1) {
                // First frame: 2 records of 8 bytes each + more
                BYTEV resp(16, 0xAA);
                resp.push_back(0x91); resp.push_back(0xAF);
                return resp;
            } else {
                // Second frame: 1 record of 8 bytes + OK
                BYTEV resp(8, 0xBB);
                resp.push_back(0x91); resp.push_back(0x00);
                return resp;
            }
        };

        BYTEV readCmd = DesfireCommands::readRecords(0x02, 0, 3);
        BYTEV allData = DesfireCommands::transceive(simTransmit, readCmd);
        RF_CHECK(frame == 2);
        RF_CHECK(allData.size() == 24);   // 16 + 8
        RF_CHECK(allData[0] == 0xAA);
        RF_CHECK(allData[16] == 0xBB);

#undef RF_CHECK
        return true;
    }
    catch (const exception& e) {
        cout << "    Exception at line " << line << ": " << e.what() << "\n";
        return false;
    }
    catch (...) {
        cout << "    Unknown exception at line " << line << "\n";
        return false;
    }
}


// ════════════════════════════════════════════════════════════════════════════════
// MAIN TEST RUNNER
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
    recordTest("DESFire Memory Layout", testDesfireMemoryLayout());
    recordTest("DESFire Auth", testDesfireAuth());
    recordTest("DESFire Commands", testDesfireCommands());
    recordTest("DESFire Management", testDesfireManagement());
    recordTest("DESFire Integration", testDesfireIntegration());
    recordTest("DESFire 3K3DES", testDesfire3K3DES());
    recordTest("DESFire Record Files", testDesfireRecordFiles());
    
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
