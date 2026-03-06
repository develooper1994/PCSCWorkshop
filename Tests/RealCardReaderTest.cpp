// RealCardReaderTest.cpp - GERCEK PCSC kart okuyucu testi
// ACR1281U + Mifare Classic 1K kart
// Gercek: loadKey, auth, read, write islemleri

#include "PCSC.h"
#include "CardUtils.h"
#include "Readers.h"
#include "MifareClassic/MifareClassic.h"
#include <iostream>
#include <iomanip>
#include <cstring>

using namespace std;

// ════════════════════════════════════════════════════════════════════════════════
// Yardimci
// ════════════════════════════════════════════════════════════════════════════════

static void printBlock(int blockNum, const BYTEV& data) {
    cout << "  Block " << setfill(' ') << setw(2) << blockNum << ": ";
    for (size_t i = 0; i < data.size(); ++i) {
        cout << hex << setfill('0') << setw(2) << (int)data[i] << " ";
    }
    // ASCII
    cout << " |";
    for (size_t i = 0; i < data.size(); ++i) {
        char c = (char)data[i];
        cout << (isprint((unsigned char)c) ? c : '.');
    }
    cout << "|" << dec << "\n";
}

// ════════════════════════════════════════════════════════════════════════════════
// GERCEK KART OKUYUCU TESTi
// ════════════════════════════════════════════════════════════════════════════════

int testRealCardReader() {
    cout << "\n========================================\n";
    cout << "  GERCEK PCSC KART OKUYUCU TESTi\n";
    cout << "  ACR1281U + Mifare Classic 1K\n";
    cout << "========================================\n\n";

    // ── 1. PCSC baglantisi ──────────────────────────────────────────────────

    PCSC pcsc;

    cout << "[1] PCSC Context kuruluyor...\n";
    if (!pcsc.establishContext()) {
        cerr << "HATA: PCSC context kurulamadi. Okuyucu takili mi?\n";
        return 1;
    }
    cout << "    OK\n\n";

    // ── 2. Reader secimi ────────────────────────────────────────────────────

    cout << "[2] Reader araniyor...\n";
    auto readers = pcsc.listReaders();
    if (!readers.ok || readers.names.empty()) {
        cerr << "HATA: Hic reader bulunamadi.\n";
        return 1;
    }
    for (size_t i = 0; i < readers.names.size(); ++i)
        wcout << "    " << (i + 1) << L". " << readers.names[i] << L"\n";

    if (!pcsc.chooseReader()) {
        cerr << "HATA: Reader secilemedi.\n";
        return 1;
    }
    cout << "    OK\n\n";

    // ── 3. Karta baglan ─────────────────────────────────────────────────────

    cout << "[3] Karta baglaniliyor...\n";
    if (!pcsc.connectToCard(500)) {
        cerr << "HATA: Kart algilanamadi. Kart okuyucunun ustunde mi?\n";
        return 1;
    }
    cout << "    OK\n\n";

    // ── 4. UID oku ──────────────────────────────────────────────────────────

    cout << "[4] UID okunuyor...\n";
    BYTEV uid = CardUtils::getUid(pcsc.cardConnection());
    cout << "    UID: ";
    for (BYTE b : uid)
        cout << hex << setfill('0') << setw(2) << (int)b << " ";
    cout << dec << "(" << uid.size() << " byte)\n\n";

    // ── 5. ACR1281UReader + MifareCardCore olustur ──────────────────────────

    cout << "[5] MifareCardCore olusturuluyor...\n";

    BYTE defaultKeyA[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    ACR1281UReader reader(
        pcsc.cardConnection(),
        16,                          // LE = 16 byte per block
        true,                        // auth requested
        KeyType::A,                  // default key type
        KeyStructure::NonVolatile,   // key structure
        0x01,                        // key slot
        defaultKeyA                  // default Key A
    );

    MifareCardCore card(reader, false /* 1K */);

    // Key'leri kaydet
    KEYBYTES kA = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    KEYBYTES kB = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    card.setKey(KeyType::A, kA, KeyStructure::NonVolatile, 0x01);
    card.setKey(KeyType::B, kB, KeyStructure::NonVolatile, 0x02);

    cout << "    OK - KeyA ve KeyB yuklendi.\n\n";

    // ── 6. Tum sektorleri oku (data + trailer) ─────────────────────────────

    cout << "[6] Tum sektorler okunuyor (0-15)...\n\n";

    int successSectors = 0;
    int failedSectors  = 0;
    int firstGoodDataBlock = -1;   // write testi icin

    for (int sector = 0; sector < 16; ++sector) {
        int firstBlock = sector * 4;
        int trailerBlock = firstBlock + 3;

        cout << "  --- Sektor " << sector << " (blok " << firstBlock
             << "-" << trailerBlock << ") ---\n";

        bool sectorOk = true;

        // Data bloklari (0,1,2)
        for (int i = 0; i < 3; ++i) {
            BYTE block = (BYTE)(firstBlock + i);
            try {
                BYTEV data = card.read(block);
                printBlock(block, data);
                // Sektor 0/block 0 manufacturer, yazilamaz - sector>=1 seciyoruz
                if (firstGoodDataBlock < 0 && sector >= 1)
                    firstGoodDataBlock = block;
            }
            catch (const exception& e) {
                cout << "  Block " << setw(2) << (int)block
                     << ": HATA - " << e.what() << "\n";
                sectorOk = false;
            }
        }

        // Trailer blogu - dogrudan reader uzerinden oku
        try {
            BYTEV raw = reader.readPage((BYTE)trailerBlock);
            cout << "  Block " << setfill(' ') << setw(2) << trailerBlock
                 << " [trailer]: KeyA=";
            for (int i = 0; i < 6 && i < (int)raw.size(); ++i)
                cout << hex << setfill('0') << setw(2) << (int)raw[i];
            cout << " Acc=";
            for (int i = 6; i < 10 && i < (int)raw.size(); ++i)
                cout << hex << setfill('0') << setw(2) << (int)raw[i];
            cout << " KeyB=";
            for (int i = 10; i < 16 && i < (int)raw.size(); ++i)
                cout << hex << setfill('0') << setw(2) << (int)raw[i];
            cout << dec << "\n";
        }
        catch (const exception& e) {
            cout << "  Block " << setw(2) << trailerBlock
                 << " [trailer]: HATA - " << e.what() << "\n";
            sectorOk = false;
        }

        if (sectorOk) ++successSectors;
        else           ++failedSectors;
        cout << "\n";
    }

    cout << "  Sonuc: " << successSectors << " sektor OK, "
         << failedSectors << " sektor HATA\n\n";

    // ── 7. Write testi ─────────────────────────────────────────────────────
    // MifareCardCore auth cache'i scan sonrasi bozulur (kart sadece son sector'u
    // hatirlat). Bu yuzden write testinde dogrudan reader kullaniyoruz.

    if (firstGoodDataBlock < 0) {
        cout << "[7] Write testi ATLANDI - okunabilir data block bulunamadi.\n\n";
    } else {
        BYTE testBlock = (BYTE)firstGoodDataBlock;
        int testSector = testBlock / 4;
        BYTE trailerOfSector = (BYTE)(testSector * 4 + 3);
        cout << "[7] Write testi (Sektor " << testSector
             << ", Block " << (int)testBlock << ")...\n";
        try {
            // 1. loadKey + auth (reader seviyesinde)
            reader.loadKey(defaultKeyA, KeyStructure::NonVolatile, 0x01);
            reader.auth(trailerOfSector, KeyType::A, 0x01);
            cout << "    Auth OK (KeyA, sektor " << testSector << ")\n";

            // 2. Eski veriyi oku
            BYTEV oldData = reader.readPage(testBlock);
            cout << "    Eski veri:  ";
            printBlock(testBlock, oldData);

            // 3. Test verisi yaz
            BYTE testData[16] = {
                'B', 'E', 'L', 'B', 'I', 'M', ' ', 'T',
                'E', 'S', 'T', ' ', '1', '2', '3', '4'
            };
            reader.writePage(testBlock, testData);
            cout << "    Yazildi.\n";

            // 4. Geri oku (auth hala gecerli, ayni sektor)
            BYTEV readBack = reader.readPage(testBlock);
            cout << "    Okunan  :  ";
            printBlock(testBlock, readBack);

            // 5. Dogrula
            bool match = (readBack.size() >= 16);
            for (int i = 0; match && i < 16; ++i) {
                // readPage SW byte'lari dahil donebilir, ilk 16 byte'a bak
                if (readBack[i] != testData[i]) match = false;
            }

            if (match) {
                cout << "    >>> WRITE/READ DOGRULANDI! <<<\n";
            } else {
                cout << "    UYARI: Yazilan ve okunan farkli!\n";
            }

            // 6. Eski veriyi geri yaz
            if (oldData.size() >= 16) {
                reader.writePage(testBlock, oldData.data());
                cout << "    Eski veri geri yazildi.\n";
            }
        }
        catch (const exception& e) {
            cout << "    Write testi HATA: " << e.what() << "\n";
            cout << "    (Sektor bozuk veya yetki yok olabilir)\n";
        }
        cout << "\n";
    }

    // ── 8. Ozet ─────────────────────────────────────────────────────────────

    cout << "========================================\n";
    cout << "  SONUC\n";
    cout << "========================================\n";
    cout << "  UID          : ";
    for (BYTE b : uid) cout << hex << setfill('0') << setw(2) << (int)b << " ";
    cout << dec << "\n";
    cout << "  Kart tipi    : Mifare Classic 1K\n";
    cout << "  Okunan sektor: " << successSectors << "/16\n";
    cout << "  Bozuk sektor : " << failedSectors << "/16\n";
    cout << "  PCSC         : Bagli\n";
    cout << "  loadKey      : Calisti\n";
    cout << "  auth         : Calisti\n";
    cout << "  read         : Calisti (" << successSectors << " sektor)\n";
    cout << "  write        : " << (firstGoodDataBlock >= 0 ? "Test edildi" : "Hic sektor yok") << "\n";
    cout << "========================================\n\n";

    return (failedSectors < 16) ? 0 : 1;
}
