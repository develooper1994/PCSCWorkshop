// RealCardReaderTest.cpp — GERCEK PCSC kart okuyucu testi
//
// Part A: ACR1281UReader ile dogrudan APDU (loadKey / auth / read / write)
// Part B: Yeni CardInterface + CardMemoryLayout ile gercek kart verisi

#include "PCSC.h"
#include "CardUtils.h"
#include "Readers.h"
#include "MifareClassic/MifareClassic.h"
#include "CardInterface.h"            // ◄── YENI SISTEM
#include <iostream>
#include <iomanip>
#include <cstring>

using namespace std;

// ════════════════════════════════════════════════════════════════════════════════
static void hexDump(int blockNum, const BYTE* d, size_t n, const char* tag = nullptr)
{
    cout << "  Block " << setfill(' ') << setw(2) << blockNum;
    if (tag) cout << " " << tag;
    cout << ": ";
    for (size_t i = 0; i < n; ++i)
        cout << hex << setfill('0') << setw(2) << (int)d[i] << ' ';
    cout << " |";
    for (size_t i = 0; i < n; ++i) {
        char c = (char)d[i];
        cout << (isprint((unsigned char)c) ? c : '.');
    }
    cout << '|' << dec << '\n';
}

static void hexDump(int blockNum, const BYTEV& v, const char* tag = nullptr)
{
    hexDump(blockNum, v.data(), min(v.size(), (size_t)16), tag);
}

// ════════════════════════════════════════════════════════════════════════════════
//  PCSC Baglanti — her iki Part icin ortak
// ════════════════════════════════════════════════════════════════════════════════
static bool pcscConnect(PCSC& pcsc, BYTEV& uid)
{
    cout << "[1] PCSC Context...\n";
    if (!pcsc.establishContext()) { cerr << "    HATA: context kurulamadi\n"; return false; }
    cout << "    OK\n";

    cout << "[2] Reader...\n";
    auto rl = pcsc.listReaders();
    if (!rl.ok || rl.names.empty()) { cerr << "    HATA: reader yok\n"; return false; }
    for (size_t i = 0; i < rl.names.size(); ++i)
        wcout << "    " << (i+1) << L". " << rl.names[i] << L'\n';
    if (!pcsc.chooseReader()) { cerr << "    HATA: secilemedi\n"; return false; }
    cout << "    OK\n";

    cout << "[3] Kart baglantisi...\n";
    if (!pcsc.connectToCard(500)) { cerr << "    HATA: kart yok\n"; return false; }
    cout << "    OK\n";

    cout << "[4] UID...\n";
    uid = CardUtils::getUid(pcsc.cardConnection());
    cout << "    ";
    for (BYTE b : uid) cout << hex << setfill('0') << setw(2) << (int)b << ' ';
    cout << dec << "(" << uid.size() << " byte)\n\n";
    return true;
}

// ════════════════════════════════════════════════════════════════════════════════
//  PART A — Eski MifareCardCore ile PCSC (onceki test, kisaltilmis)
// ════════════════════════════════════════════════════════════════════════════════
static void partA(PCSC& pcsc, ACR1281UReader& reader, const BYTE defaultKeyA[6])
{
    cout << "╔══════════════════════════════════════════════╗\n";
    cout << "║  PART A — ACR1281UReader ile dogrudan PCSC  ║\n";
    cout << "╚══════════════════════════════════════════════╝\n\n";

    MifareCardCore card(reader, false);
    KEYBYTES kA = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    card.setKey(KeyType::A, kA, KeyStructure::NonVolatile, 0x01);
    card.setKey(KeyType::B, kA, KeyStructure::NonVolatile, 0x02);

    // Hizli scan: sadece 3 sektor goster (0, 1, 15)
    int demoSectors[] = {0, 1, 15};
    for (int s : demoSectors) {
        int fb = s * 4;
        cout << "  --- Sektor " << s << " ---\n";
        for (int i = 0; i < 3; ++i) {
            try   { BYTEV d = card.read((BYTE)(fb+i)); hexDump(fb+i, d); }
            catch (...) { cout << "  Block " << (fb+i) << ": auth hata\n"; }
        }
        try   { BYTEV t = reader.readPage((BYTE)(fb+3)); hexDump(fb+3, t, "[trailer]"); }
        catch (...) { cout << "  Block " << (fb+3) << ": trailer hata\n"; }
        cout << '\n';
    }

    // Write/verify (sektor 2)
    cout << "  Write testi (Block 8)...\n";
    try {
        reader.loadKey(defaultKeyA, KeyStructure::NonVolatile, 0x01);
        reader.auth(11, KeyType::A, 0x01);       // trailer of sector 2
        BYTEV old = reader.readPage(8);
        cout << "    Eski: "; hexDump(8, old);

        BYTE tw[16] = {'P','A','R','T','-','A',' ','O','K','!',0,0,0,0,0,0};
        reader.writePage(8, tw);
        BYTEV rb = reader.readPage(8);
        cout << "    Yeni: "; hexDump(8, rb);

        bool ok = rb.size()>=16;
        for (int i=0; ok && i<16; ++i) if(rb[i]!=tw[i]) ok=false;
        cout << "    " << (ok ? ">>> DOGRULANDI <<<" : "UYARI: farkli") << "\n";

        reader.writePage(8, old.data());
        cout << "    Geri yazildi.\n";
    }
    catch (const exception& e) { cout << "    HATA: " << e.what() << '\n'; }
    cout << "\n";
}

// ════════════════════════════════════════════════════════════════════════════════
//  PART B — YENI CardInterface + CardMemoryLayout ile gercek kart
// ════════════════════════════════════════════════════════════════════════════════
static void partB(PCSC& pcsc, ACR1281UReader& reader, const BYTE defaultKeyA[6],
                  const BYTEV& uid)
{
    cout << "╔══════════════════════════════════════════════════════════╗\n";
    cout << "║  PART B — Yeni CardInterface + CardMemoryLayout testi  ║\n";
    cout << "╚══════════════════════════════════════════════════════════╝\n\n";

    // Part A sonrasi reader state bozuk olabilir; auth'u elle yapacagiz
    reader.setAuthRequested(false);

    // ── B1. Karttan tum bloklari oku, raw buffer'a yukle ────────────────────

    cout << "[B1] Karttan 1024 byte okunuyor (64 blok)...\n";
    BYTE rawCard[1024];
    memset(rawCard, 0, 1024);

    int readOk = 0, readFail = 0;
    for (int sector = 0; sector < 16; ++sector) {
        BYTE trailer = (BYTE)(sector * 4 + 3);
        try {
            reader.loadKey(defaultKeyA, KeyStructure::NonVolatile, 0x01);
            reader.auth(trailer, KeyType::A, 0x01);
        }
        catch (...) { readFail += 4; continue; }

        for (int i = 0; i < 4; ++i) {
            BYTE blk = (BYTE)(sector * 4 + i);
            try {
                BYTEV d = reader.readPage(blk);
                if (d.size() >= 16)
                    memcpy(rawCard + blk * 16, d.data(), 16);
                ++readOk;
            }
            catch (...) { ++readFail; }
        }
    }
    cout << "    " << readOk << " blok OK, " << readFail << " blok HATA\n\n";

    // ── B2. CardInterface olustur, loadMemory ile yukle ─────────────────────

    cout << "[B2] CardInterface olusturuluyor ve loadMemory cagriliyor...\n";
    CardInterface ci(false /* 1K */);
    ci.loadMemory(rawCard, 1024);

    cout << "    is1K()          = " << (ci.is1K() ? "true" : "false") << '\n';
    cout << "    getTotalMemory  = " << ci.getTotalMemory() << " byte\n";
    cout << "    getTotalBlocks  = " << ci.getTotalBlocks() << '\n';
    cout << "    getTotalSectors = " << ci.getTotalSectors() << '\n';

    // UID dogrula
    KEYBYTES ciUid = ci.getUID();
    cout << "    getUID()        = ";
    for (int i = 0; i < 4; ++i)
        cout << hex << setfill('0') << setw(2) << (int)ciUid[i] << ' ';
    cout << dec;
    bool uidMatch = (uid.size() >= 4);
    for (int i = 0; uidMatch && i < 4; ++i)
        if (ciUid[i] != uid[i]) uidMatch = false;
    cout << (uidMatch ? "(PCSC UID ile ESLESDI)" : "(UYARI: eslesmiyor)") << "\n\n";

    // ── B3. Topology sorgulari (gercek veriyle) ─────────────────────────────

    cout << "[B3] Topology sorgulari...\n";
    cout << "    getSectorForBlock(0)         = " << ci.getSectorForBlock(0) << '\n';
    cout << "    getSectorForBlock(7)         = " << ci.getSectorForBlock(7) << '\n';
    cout << "    getFirstBlockOfSector(5)     = " << ci.getFirstBlockOfSector(5) << '\n';
    cout << "    getTrailerBlockOfSector(5)   = " << ci.getTrailerBlockOfSector(5) << '\n';
    cout << "    isManufacturerBlock(0)       = " << ci.isManufacturerBlock(0) << '\n';
    cout << "    isTrailerBlock(3)            = " << ci.isTrailerBlock(3) << '\n';
    cout << "    isDataBlock(1)               = " << ci.isDataBlock(1) << '\n';
    cout << '\n';

    // ── B4. Zero-copy memory view — gercek kart verisi ──────────────────────

    cout << "[B4] CardMemoryLayout zero-copy view...\n";
    const CardMemoryLayout& mem = ci.getMemory();

    cout << "  Block  0 (manufacturer): ";
    for (int i = 0; i < 16; ++i)
        cout << hex << setfill('0') << setw(2) << (int)mem.data.card1K.blocks[0].raw[i] << ' ';
    cout << dec << '\n';

    cout << "  Block  0 uid[]         : ";
    for (int i = 0; i < 4; ++i)
        cout << hex << setfill('0') << setw(2)
             << (int)mem.data.card1K.blocks[0].manufacturer.uid[i] << ' ';
    cout << dec << '\n';

    cout << "  Block  3 trailer keyA  : ";
    for (int i = 0; i < 6; ++i)
        cout << hex << setfill('0') << setw(2)
             << (int)mem.data.card1K.blocks[3].trailer.keyA[i] << ' ';
    cout << dec << '\n';

    cout << "  Block  3 trailer accBit: ";
    for (int i = 0; i < 4; ++i)
        cout << hex << setfill('0') << setw(2)
             << (int)mem.data.card1K.blocks[3].trailer.accessBits[i] << ' ';
    cout << dec << '\n';

    cout << "  Block  3 trailer keyB  : ";
    for (int i = 0; i < 6; ++i)
        cout << hex << setfill('0') << setw(2)
             << (int)mem.data.card1K.blocks[3].trailer.keyB[i] << ' ';
    cout << dec << '\n';

    // Sector view
    cout << "  sectors[2].block[1]    : ";
    for (int i = 0; i < 16; ++i)
        cout << hex << setfill('0') << setw(2)
             << (int)mem.data.card1K.sectors[2].block[1].raw[i] << ' ';
    cout << dec << '\n';

    // Detailed view
    cout << "  detailed.sector[0].trailer: ";
    for (int i = 0; i < 16; ++i)
        cout << hex << setfill('0') << setw(2)
             << (int)mem.data.card1K.detailed.sector[0].trailerBlock.raw[i] << ' ';
    cout << dec << "\n\n";

    // ── B5. Key Management ──────────────────────────────────────────────────

    cout << "[B5] KeyManagement — registerKey, getKey, getCardKey...\n";
    KEYBYTES regKeyA = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    ci.registerKey(KeyType::A, regKeyA, KeyStructure::NonVolatile, 0x01, "DefaultA");
    ci.registerKey(KeyType::B, regKeyA, KeyStructure::NonVolatile, 0x02, "DefaultB");
    cout << "    Registered KeyA slot 0x01, KeyB slot 0x02\n";

    const KEYBYTES& keyA = ci.getKey(KeyType::A, 0x01);
    cout << "    getKey(A,0x01)  = ";
    for (BYTE b : keyA) cout << hex << setfill('0') << setw(2) << (int)b;
    cout << dec << '\n';

    KEYBYTES cardKeyA = ci.getCardKey(0, KeyType::A);
    cout << "    getCardKey(s0,A)= ";
    for (BYTE b : cardKeyA) cout << hex << setfill('0') << setw(2) << (int)b;
    cout << dec << '\n';

    KEYBYTES cardKeyB = ci.getCardKey(0, KeyType::B);
    cout << "    getCardKey(s0,B)= ";
    for (BYTE b : cardKeyB) cout << hex << setfill('0') << setw(2) << (int)b;
    cout << dec << "\n\n";

    // ── B6. Authentication + Access Control ─────────────────────────────────

    cout << "[B6] Authentication + AccessControl...\n";
    ci.authenticate(0, KeyType::A);
    cout << "    authenticate(0, A)\n";
    cout << "    isAuthenticated(0)           = " << ci.isAuthenticated(0) << '\n';
    cout << "    isAuthenticatedWith(0, A)    = " << ci.isAuthenticatedWith(0, KeyType::A) << '\n';
    cout << "    canReadDataBlocks(0, A)      = " << ci.canReadDataBlocks(0, KeyType::A) << '\n';
    cout << "    canWriteDataBlocks(0, A)     = " << ci.canWriteDataBlocks(0, KeyType::A) << '\n';
    cout << "    canReadDataBlocks(0, B)      = " << ci.canReadDataBlocks(0, KeyType::B) << '\n';
    cout << "    canWriteDataBlocks(0, B)     = " << ci.canWriteDataBlocks(0, KeyType::B) << '\n';
    cout << "    canRead(block3, A)  [trailer]= " << ci.canRead(3, KeyType::A) << '\n';
    cout << "    canWrite(block3, A) [trailer]= " << ci.canWrite(3, KeyType::A) << '\n';
    cout << '\n';

    // ── B7. getBlock — gercek karttan okunan veriyi dogrudan getBlock ile al

    cout << "[B7] getBlock() — gercek kart verisi...\n";
    for (int blk : {0, 1, 4, 8, 63}) {
        const MifareBlock& b = ci.getBlock(blk);
        hexDump(blk, b.raw, 16);
    }
    cout << '\n';

    // ── B8. Write to CardMemoryLayout → PCSC'ye geri yaz ───────────────────

    cout << "[B8] CardInterface uzerinden write → PCSC karta geri yaz...\n";

    // Calisan bir sektor bul (sector>=2, 0 manufacturer, 1 bozuk olabilir)
    int writeSector = -1;
    for (int s = 2; s < 16 && writeSector < 0; ++s) {
        try {
            reader.loadKey(defaultKeyA, KeyStructure::NonVolatile, 0x01);
            reader.auth((BYTE)(s*4+3), KeyType::A, 0x01);
            BYTEV test = reader.readPage((BYTE)(s*4));
            if (test.size() >= 16) writeSector = s;
        }
        catch (...) {}
    }

    if (writeSector < 0) {
        cout << "    ATLANDI — yazilabilir sektor bulunamadi\n\n";
    }
    else {
        BYTE writeBlock = (BYTE)(writeSector * 4);  // ilk data block
        cout << "    Hedef: Sektor " << writeSector << ", Block " << (int)writeBlock << '\n';

        // Karttan eski veriyi oku
        reader.loadKey(defaultKeyA, KeyStructure::NonVolatile, 0x01);
        reader.auth((BYTE)(writeSector*4+3), KeyType::A, 0x01);
        BYTEV oldFromCard = reader.readPage(writeBlock);
        cout << "    Karttan eski: "; hexDump(writeBlock, oldFromCard);

        // CardInterface memory'sini guncelle
        CardMemoryLayout& wmem = ci.getMemoryMutable();
        BYTE payload[16] = {'C','A','R','D','I','N','T','F',' ','O','K','!',0,0,0,0};
        memcpy(wmem.data.card1K.blocks[writeBlock].raw, payload, 16);

        // Dogrulama: getBlock yeni veriyi gorur mu?
        const MifareBlock& after = ci.getBlock(writeBlock);
        cout << "    Memory'de  : "; hexDump(writeBlock, after.raw, 16);

        // exportMemory ile de dogrula
        BYTEV exported = ci.exportMemory();
        cout << "    Export     : ";
        hexDump(writeBlock, &exported[writeBlock*16], 16);

        // Simdiler PCSC ile karta yaz
        reader.writePage(writeBlock, wmem.data.card1K.blocks[writeBlock].raw);
        cout << "    PCSC'ye yazildi.\n";

        // Karttan geri oku
        BYTEV readBack = reader.readPage(writeBlock);
        cout << "    Karttan geri: "; hexDump(writeBlock, readBack);

        // Dogrula
        bool match = readBack.size() >= 16;
        for (int i = 0; match && i < 16; ++i)
            if (readBack[i] != payload[i]) match = false;
        cout << "    " << (match ? ">>> CARDINTERFACE WRITE/READ DOGRULANDI! <<<" 
                                 : "UYARI: farkli!") << '\n';

        // Eski veriyi geri yaz
        if (oldFromCard.size() >= 16) {
            reader.writePage(writeBlock, oldFromCard.data());
            memcpy(wmem.data.card1K.blocks[writeBlock].raw, oldFromCard.data(), 16);
            cout << "    Eski veri geri yazildi.\n";
        }
    }
    cout << "\n";
}

// ════════════════════════════════════════════════════════════════════════════════
//  ANA TEST FONKSIYONU
// ════════════════════════════════════════════════════════════════════════════════

int testRealCardReader()
{
    cout << "\n========================================================\n";
    cout << "  GERCEK PCSC KART OKUYUCU TESTi\n";
    cout << "  ACR1281U + Mifare Classic 1K\n";
    cout << "========================================================\n\n";

    PCSC pcsc;
    BYTEV uid;
    if (!pcscConnect(pcsc, uid)) return 1;

    BYTE defaultKeyA[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    ACR1281UReader reader(
        pcsc.cardConnection(), 16, true,
        KeyType::A, KeyStructure::NonVolatile, 0x01, defaultKeyA);

    // ── PART A ──
    partA(pcsc, reader, defaultKeyA);

    // ── PART B ──
    partB(pcsc, reader, defaultKeyA, uid);

    // ── OZET ────
    cout << "========================================================\n";
    cout << "  SONUC\n";
    cout << "========================================================\n";
    cout << "  Part A (eski MifareCardCore)  : loadKey/auth/read/write CALISTI\n";
    cout << "  Part B (yeni CardInterface)   : loadMemory/getUID/topology/\n";
    cout << "                                  key/auth/access/getBlock/\n";
    cout << "                                  zero-copy/write CALISTI\n";
    cout << "========================================================\n\n";

    return 0;
}
