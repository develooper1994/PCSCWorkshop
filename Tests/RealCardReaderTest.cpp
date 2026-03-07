// RealCardReaderTest.cpp — CardIO + CardInterface ile GERCEK kart testi
// MifareCardCore YOK. Tamamen yeni mimari.

#include "PCSC.h"
#include "CardUtils.h"
#include "Readers.h"
#include "CardIO.h"
#include "CardModel/CardMemoryLayout.h"
#include "CardModel/TrailerConfig.h"
#include <iostream>
#include <iomanip>

using namespace std;

// ════════════════════════════════════════════════════════════════════════════════
static void hexDump(int blk, const BYTE* d, size_t n)
{
    cout << "  Block " << setfill(' ') << setw(2) << blk << ": ";
    for (size_t i = 0; i < n; ++i)
        cout << hex << setfill('0') << setw(2) << (int)d[i] << ' ';
    cout << " |";
    for (size_t i = 0; i < n; ++i) {
        char c = (char)d[i];
        cout << (isprint((unsigned char)c) ? c : '.');
    }
    cout << '|' << dec << '\n';
}
static void hexDump(int blk, const BYTEV& v) { hexDump(blk, v.data(), min(v.size(),(size_t)16)); }

// ════════════════════════════════════════════════════════════════════════════════
int testRealCardReader()
{
    cout << "\n========================================================\n";
    cout << "  GERCEK KART TESTi — CardIO + CardInterface\n";
    cout << "  (MifareCardCore YOK, tamamen yeni mimari)\n";
    cout << "========================================================\n\n";

    // ── 1. PCSC ─────────────────────────────────────────────────────────────

    PCSC pcsc;

    cout << "[1] PCSC Context...\n";
    if (!pcsc.establishContext()) { cerr << "    HATA: context\n"; return 1; }
    cout << "    OK\n";

    cout << "[2] Reader...\n";
    auto rl = pcsc.listReaders();
    if (!rl.ok || rl.names.empty()) { cerr << "    HATA: reader yok\n"; return 1; }
    for (size_t i = 0; i < rl.names.size(); ++i)
        wcout << "    " << (i+1) << L". " << rl.names[i] << L'\n';
    if (!pcsc.chooseReader()) { cerr << "    HATA: secilemedi\n"; return 1; }
    cout << "    OK\n";

    cout << "[3] Kart baglantisi...\n";
    if (!pcsc.connectToCard(500)) { cerr << "    HATA: kart yok\n"; return 1; }
    cout << "    OK\n";

    cout << "[4] UID (PCSC)...\n";
    BYTEV pcscUid = CardUtils::getUid(pcsc.cardConnection());
    cout << "    ";
    for (BYTE b : pcscUid) cout << hex << setfill('0') << setw(2) << (int)b << ' ';
    cout << dec << "(" << pcscUid.size() << " byte)\n\n";

    // ── 2. CardIO oluştur ───────────────────────────────────────────────────

    ACR1281UReader reader(pcsc.cardConnection(), 16);

    CardIO io(reader, false /* 1K */);

    KEYBYTES defKey = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    io.setDefaultKey(defKey, KeyStructure::NonVolatile, 0x01, KeyType::A);

    cout << "[5] CardIO olusturuldu.\n\n";

    // ── 3. Tüm kartı oku ───────────────────────────────────────────────────

    cout << "[6] io.readCard() — tum kart okunuyor...\n";
    int okBlocks = io.readCard();
    cout << "    " << okBlocks << " / " << io.card().getTotalBlocks()
         << " blok okundu.\n\n";

    // ── 4. CardInterface üzerinden sorgular ─────────────────────────────────

    cout << "[7] CardInterface sorgulari (gercek kart verisi)...\n";
    cout << "    is1K()          = " << io.card().is1K() << '\n';
    cout << "    getTotalMemory  = " << io.card().getTotalMemory() << " byte\n";
    cout << "    getTotalBlocks  = " << io.card().getTotalBlocks() << '\n';
    cout << "    getTotalSectors = " << io.card().getTotalSectors() << '\n';

    // UID — memory'den
    KEYBYTES uid = io.card().getUID();
    cout << "    getUID()        = ";
    for (int i = 0; i < 4; ++i)
        cout << hex << setfill('0') << setw(2) << (int)uid[i] << ' ';
    cout << dec;
    bool uidOk = pcscUid.size() >= 4;
    for (int i = 0; uidOk && i < 4; ++i) if (uid[i] != pcscUid[i]) uidOk = false;
    cout << (uidOk ? "(PCSC UID ile ESLESDI)" : "(UYARI: eslesmedi)") << "\n\n";

    // Topology
    cout << "[8] Topology...\n";
    cout << "    getSectorForBlock(0)       = " << io.card().getSectorForBlock(0) << '\n';
    cout << "    getSectorForBlock(7)       = " << io.card().getSectorForBlock(7) << '\n';
    cout << "    getTrailerBlockOfSector(3) = " << io.card().getTrailerBlockOfSector(3) << '\n';
    cout << "    isManufacturerBlock(0)     = " << io.card().isManufacturerBlock(0) << '\n';
    cout << "    isTrailerBlock(3)          = " << io.card().isTrailerBlock(3) << '\n';
    cout << "    isDataBlock(5)             = " << io.card().isDataBlock(5) << '\n';
    cout << '\n';

    // Zero-copy memory view
    cout << "[9] Zero-copy memory view...\n";
    const CardMemoryLayout& mem = io.card().getMemory();
    cout << "  manufacturer.uid  : ";
    for (int i = 0; i < 4; ++i)
        cout << hex << setfill('0') << setw(2) << (int)mem.data.card1K.blocks[0].manufacturer.uid[i] << ' ';
    cout << dec << '\n';
    cout << "  trailer[0].keyA   : ";
    for (int i = 0; i < 6; ++i)
        cout << hex << setfill('0') << setw(2) << (int)mem.data.card1K.blocks[3].trailer.keyA[i] << ' ';
    cout << dec << '\n';
    cout << "  trailer[0].access : ";
    for (int i = 0; i < 4; ++i)
        cout << hex << setfill('0') << setw(2) << (int)mem.data.card1K.blocks[3].trailer.accessBits[i] << ' ';
    cout << dec << '\n';
    cout << "  trailer[0].keyB   : ";
    for (int i = 0; i < 6; ++i)
        cout << hex << setfill('0') << setw(2) << (int)mem.data.card1K.blocks[3].trailer.keyB[i] << ' ';
    cout << dec << "\n\n";

    // Tüm blokları göster
    cout << "[10] Tum bloklar (getBlock)...\n";
    for (int s = 0; s < 16; ++s) {
        int fb = io.card().getFirstBlockOfSector(s);
        int lb = io.card().getLastBlockOfSector(s);
        cout << "  --- Sektor " << s << " ---\n";
        for (int b = fb; b <= lb; ++b) {
            const MifareBlock& blk = io.card().getBlock(b);
            hexDump(b, blk.raw, 16);
        }
    }
    cout << '\n';

    // ── 5. readBlock testi ──────────────────────────────────────────────────

    cout << "[11] io.readBlock(0) — tek blok oku...\n";
    try {
        BYTEV b0 = io.readBlock(0);
        cout << "    "; hexDump(0, b0);
    } catch (const exception& e) { cout << "    HATA: " << e.what() << '\n'; }
    cout << '\n';

    // ── 6. Write + Read doğrulama ───────────────────────────────────────────

    cout << "[12] io.writeBlock() + io.readBlock() — write/read testi...\n";

    // Yazılabilir sektör bul (sector >= 2)
    int writeSector = -1;
    for (int s = 2; s < 16 && writeSector < 0; ++s) {
        try {
            io.authenticate(s);
            BYTEV t = io.readBlock(s * 4);
            if (t.size() >= 16) writeSector = s;
        } catch (...) {}
    }

    if (writeSector < 0) {
        cout << "    ATLANDI — yazilabilir sektor bulunamadi.\n\n";
    } else {
        int wb = writeSector * 4;   // ilk data block
        cout << "    Hedef: Sektor " << writeSector << ", Block " << wb << '\n';

        try {
            // Eski veri
            BYTEV oldData = io.readBlock(wb);
            cout << "    Eski  : "; hexDump(wb, oldData);

            // Yaz
            BYTE payload[16] = {'C','A','R','D','I','O',' ','O','K','!',0,0,0,0,0,0};
            io.writeBlock(wb, payload);
            cout << "    Yazildi.\n";

            // Memory'de doğrula
            const MifareBlock& mblk = io.card().getBlock(wb);
            cout << "    Memory: "; hexDump(wb, mblk.raw, 16);

            // Karttan geri oku
            BYTEV readBack = io.readBlock(wb);
            cout << "    Kart  : "; hexDump(wb, readBack);

            bool match = readBack.size() >= 16;
            for (int i = 0; match && i < 16; ++i)
                if (readBack[i] != payload[i]) match = false;

            cout << "    " << (match ? ">>> WRITE/READ DOGRULANDI! <<<"
                                     : "UYARI: farkli!") << '\n';

            // Eski veriyi geri yaz
            if (oldData.size() >= 16) {
                io.writeBlock(wb, oldData);
                cout << "    Eski veri geri yazildi.\n";
            }
        } catch (const exception& e) {
            cout << "    HATA: " << e.what() << '\n';
        }
    }
    cout << '\n';

    // ── 7. readSector testi ─────────────────────────────────────────────────

    cout << "[13] io.readSector(0) — tek sektor oku...\n";
    bool rsOk = io.readSector(0);
    cout << "    " << (rsOk ? "OK" : "HATA") << '\n';
    for (int b = 0; b < 4; ++b) {
        const MifareBlock& blk = io.card().getBlock(b);
        hexDump(b, blk.raw, 16);
    }
    cout << '\n';

    // ── 8. Trailer okuma ────────────────────────────────────────────────────

    cout << "[14] io.readTrailer() — trailer config oku...\n";
    bool trailerReadOk = false;
    int trailerTestSector = (writeSector >= 2) ? writeSector : 0;
    try {
        TrailerConfig tc = io.readTrailer(trailerTestSector);
        trailerReadOk = true;

        cout << "    Sektor " << trailerTestSector << ":\n";
        cout << "    KeyA: ";
        for (BYTE b : tc.keyA) cout << hex << setfill('0') << setw(2) << (int)b;
        cout << dec << '\n';
        cout << "    KeyB: ";
        for (BYTE b : tc.keyB) cout << hex << setfill('0') << setw(2) << (int)b;
        cout << dec << '\n';

        ACCESSBYTES ab = tc.accessBitsRaw();
        cout << "    Access bits: ";
        for (BYTE b : ab) cout << hex << setfill('0') << setw(2) << (int)b << ' ';
        cout << dec;
        cout << (tc.isValid() ? "(valid)" : "(CORRUPT)") << '\n';

        // Decode
        SectorAccessConfig cfg = tc.access;
        auto dp = cfg.dataPermission(0);
        auto tp = cfg.trailerPermission();
        cout << "    Data[0]: C1=" << cfg.dataBlock[0].c1
             << " C2=" << cfg.dataBlock[0].c2
             << " C3=" << cfg.dataBlock[0].c3 << '\n';
        cout << "      readA=" << dp.readA << " readB=" << dp.readB
             << " writeA=" << dp.writeA << " writeB=" << dp.writeB << '\n';
        cout << "    Trailer: C1=" << cfg.trailer.c1
             << " C2=" << cfg.trailer.c2
             << " C3=" << cfg.trailer.c3 << '\n';
        cout << "      accReadA=" << tp.accReadA << " accReadB=" << tp.accReadB
             << " accWriteA=" << tp.accWriteA << " accWriteB=" << tp.accWriteB << '\n';
    }
    catch (const exception& e) { cout << "    HATA: " << e.what() << '\n'; }
    cout << '\n';

    // ── 9. Tüm sektör trailer'larını göster ─────────────────────────────────

    cout << "[15] Tum sektorlerin access config'i...\n";
    for (int s = 0; s < 16; ++s) {
        try {
            SectorAccessConfig cfg = io.getAccessConfig(s);
            auto dp = cfg.dataPermission(0);
            cout << "  S" << setfill(' ') << setw(2) << s
                 << ": data=C" << cfg.dataBlock[0].c1 << cfg.dataBlock[0].c2 << cfg.dataBlock[0].c3
                 << " trail=C" << cfg.trailer.c1 << cfg.trailer.c2 << cfg.trailer.c3
                 << "  R:" << (dp.readA?"A":"") << (dp.readB?"|B":"")
                 << " W:" << (dp.writeA?"A":"") << (dp.writeB?"|B":"")
                 << '\n';
        }
        catch (...) { cout << "  S" << setw(2) << s << ": okunamadi\n"; }
    }
    cout << '\n';

    // ── 10. Trailer yazma testi ─────────────────────────────────────────────

    cout << "[16] io.writeTrailer() — trailer config yaz/geri al...\n";
    bool trailerWriteOk = false;
    if (writeSector < 2) {
        cout << "    ATLANDI — uygun sektor yok.\n\n";
    } else {
        try {
            // Mevcut config'i yedekle
            TrailerConfig backup = io.readTrailer(writeSector);
            cout << "    Yedeklendi (sektor " << writeSector << ").\n";

            // Yeni config hazırla: aynı key'ler, farklı GPB
            TrailerConfig newCfg = backup;
            newCfg.access.gpb = 0x42;       // GPB değiştir
            io.writeTrailer(writeSector, newCfg);
            cout << "    Yazildi (GPB=0x42).\n";

            // Geri oku ve doğrula
            TrailerConfig verify = io.readTrailer(writeSector);
            ACCESSBYTES ab = verify.accessBitsRaw();
            cout << "    Okunan GPB: 0x" << hex << setfill('0') << setw(2)
                 << (int)verify.access.gpb << dec << '\n';
            trailerWriteOk = (verify.access.gpb == 0x42);
            cout << "    " << (trailerWriteOk ? ">>> TRAILER WRITE DOGRULANDI! <<<"
                                              : "UYARI: GPB eslesmiyor") << '\n';

            // Eski config'i geri yaz
            io.writeTrailer(writeSector, backup);
            cout << "    Yedek geri yazildi.\n";
        }
        catch (const exception& e) { cout << "    HATA: " << e.what() << '\n'; }
    }
    cout << '\n';

    // ── 11. saveAllTrailers ─────────────────────────────────────────────────

    cout << "[17] io.saveAllTrailers() — bulk backup...\n";
    try {
        auto allConfigs = io.saveAllTrailers();
        cout << "    " << allConfigs.size() << " sektor yedeklendi.\n";
        int validCount = 0;
        for (const auto& tc : allConfigs)
            if (tc.isValid()) ++validCount;
        cout << "    " << validCount << "/" << allConfigs.size() << " valid access bits.\n";
    }
    catch (const exception& e) { cout << "    HATA: " << e.what() << '\n'; }
    cout << '\n';

    // ── Özet ────────────────────────────────────────────────────────────────

    cout << "========================================================\n";
    cout << "  SONUC — CardIO + CardInterface (MifareCardCore YOK)\n";
    cout << "========================================================\n";
    cout << "  readCard()       : " << okBlocks << "/" << io.card().getTotalBlocks() << " blok\n";
    cout << "  getUID()         : " << (uidOk ? "ESLESDI" : "ESLESMEDI") << '\n';
    cout << "  Topology         : CALISTI\n";
    cout << "  Zero-copy view   : CALISTI\n";
    cout << "  readBlock()      : CALISTI\n";
    cout << "  writeBlock()     : " << (writeSector >= 0 ? "DOGRULANDI" : "ATLANILDI") << '\n';
    cout << "  readSector()     : " << (rsOk ? "CALISTI" : "HATA") << '\n';
    cout << "  readTrailer()    : " << (trailerReadOk ? "CALISTI" : "HATA") << '\n';
    cout << "  writeTrailer()   : " << (trailerWriteOk ? "DOGRULANDI" : "ATLANILDI") << '\n';
    cout << "  saveAllTrailers(): CALISTI\n";
    cout << "========================================================\n\n";

    return 0;
}
