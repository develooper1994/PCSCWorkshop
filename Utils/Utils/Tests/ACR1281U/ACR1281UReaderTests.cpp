#include "Card/MifareClassic/MifareClassic.h"
#include "ACR1281UReader.h"
#include "ACR1281UReaderTestHelpers.h"
#include "ACR1281UReaderTests.h"
#include "PcscUtils.h"
#include "Ciphers.h"
#include <iostream>
#include <vector>
#include <string>

/********************  TESTS ********************/

/********************  Mifare Classic ********************/
// loadkey -> auth block -> read/write block
void testACR1281UReaderMifareClassic(CardConnection& cardConnection, BYTE startPage) {
	std::cout << "\n--- " << __func__ << ": ---\n";
	ACR1281UReader acr1281u(cardConnection);
	testACR1281UReaderMifareClassicUnsecured(acr1281u, startPage);
}

void testACR1281UReaderMifareClassicUnsecured(ACR1281UReader& acr1281u, BYTE startPage) {
    std::cout << "\n--- " << __func__ << ": ---\n";
    try {
        constexpr int SMALL_SECTOR_COUNT = 32;
        constexpr int SMALL_SECTOR_PAGES = 4;
        constexpr int SMALL_SECTORS_TOTAL_PAGES = SMALL_SECTOR_COUNT * SMALL_SECTOR_PAGES; // 128
        constexpr int LARGE_SECTOR_PAGES = 16;

        // 1) hazýr metin
        std::string text = "Mustafa";

        // 2) sectorFromPage lambda (1K ve 4K uyumlu)
        auto sectorFromPage = [](BYTE page) -> int {
            const int p = static_cast<int>(page);
            return p < SMALL_SECTORS_TOTAL_PAGES
                ? p / SMALL_SECTOR_PAGES
                : SMALL_SECTOR_COUNT + (p - SMALL_SECTORS_TOTAL_PAGES) / LARGE_SECTOR_PAGES;
        };

        // 3) indexInsideSector lambda
        auto indexFromPage = [](BYTE page) -> int {
            const int p = static_cast<int>(page);
            return p < SMALL_SECTORS_TOTAL_PAGES
                ? p % SMALL_SECTOR_PAGES
                : (p - SMALL_SECTORS_TOTAL_PAGES) % LARGE_SECTOR_PAGES;
        };

        // 4) print info
        std::cout << "Original: \"" << text << "\" size: " << text.size()
            << " bytes, needs " << ((text.size() + 15) / 16)
            << " pages. starting page: " << int(startPage)
            << " (sector " << sectorFromPage(startPage) << ")\n";

        // 5) prepare reader & card core
        // create a MifareCardCore that wraps acr1281u; assume 1K (false). If you have 4K card, pass true.
        MifareCardCore<ACR1281UReader> card(acr1281u, /*is4K=*/ false);

        // 6) load default key into reader and also give it to the card (slot 0x01 used in older examples)
        const BYTE defaultKey6[6] = { 0xFF };
        const BYTE keySlot = 0x01; // as in your earlier example

        // If your ACR1281UReader expects storage type (NonVolatile/Volatile), use that value:
        // acr1281u.loadKey(defaultKey6, KeyStructure::NonVolatile, keySlot);
        // Also register the key into card so it knows which slot to use for auth decisions.
        // setKey signature might be different in your core; adjust if necessary.
        card.setKey(MifareCardCore<ACR1281UReader>::KeyType::A, std::array<BYTE, 6>{defaultKey6[0], defaultKey6[1], defaultKey6[2], defaultKey6[3], defaultKey6[4], defaultKey6[5]},
            /*keyStructure*/ KeyStructure::NonVolatile, keySlot);
        card.setKey(MifareCardCore<ACR1281UReader>::KeyType::B, std::array<BYTE, 6>{defaultKey6[0], defaultKey6[1], defaultKey6[2], defaultKey6[3], defaultKey6[4], defaultKey6[5]},
            /*keyStructure*/ KeyStructure::NonVolatile, keySlot);

        // 7) ensure default sector config: KeyA read-only, KeyB read+write
        MifareCardCore<ACR1281UReader>::SectorKeyConfig defaultCfg;
        defaultCfg.keyA_canRead = true;
        defaultCfg.keyA_canWrite = false;
        defaultCfg.keyB_canRead = true;
        defaultCfg.keyB_canWrite = true;

        // Apply for all sectors (1K => 16 sectors)
        for (int s = 0; s < 16; ++s) card.setSectorConfig(s, defaultCfg);

        // 8) Try a pre-read of the start page using card (safe)
        {
            int sector = sectorFromPage(startPage);
            int index = indexFromPage(startPage);

            // read single block via card: card.read(sector, index, out16, KeyType, slot)
            uint8_t out16[16] = { 0 };
            card.read(static_cast<uint8_t>(sector), static_cast<uint8_t>(index), out16, MifareCardCore<ACR1281UReader>::KeyType::A, keySlot);

            std::string sread(reinterpret_cast<char*>(out16), 16);
            std::cout << "Read back (single block) : \"" << sread << "\"\n";
            printHex(out16, 16);
        }

        // 9) Write the text starting at startPage: split into 16-byte chunks and write sequentially
        {
            std::vector<BYTE> data(text.begin(), text.end());
            size_t pagesNeeded = (data.size() + 15) / 16;

            for (size_t i = 0; i < pagesNeeded; ++i) {
                BYTE page = static_cast<BYTE>(static_cast<int>(startPage) + static_cast<int>(i));
                int sector = sectorFromPage(page);
                int index = indexFromPage(page);

                BYTE pageBuf[16];
                std::fill(std::begin(pageBuf), std::end(pageBuf), 0);
                size_t copy = std::min<size_t>(16, data.size() - i * 16);
                memcpy(pageBuf, data.data() + i * 16, copy);

                // Write using KeyB (KeyB default allowed to write)
                card.write(static_cast<uint8_t>(sector), static_cast<uint8_t>(index), pageBuf, MifareCardCore<ACR1281UReader>::KeyType::B, keySlot);
            }
        }

        // 10) Read back same length starting at startPage
        {
            std::vector<BYTE> readBack;
            size_t bytesToRead = text.size();
            size_t pagesToRead = (bytesToRead + 15) / 16;
            readBack.reserve(pagesToRead * 16);

            for (size_t i = 0; i < pagesToRead; ++i) {
                BYTE page = static_cast<BYTE>(static_cast<int>(startPage) + static_cast<int>(i));
                int sector = sectorFromPage(page);
                int index = indexFromPage(page);

                BYTE out16[16];
                card.read(static_cast<uint8_t>(sector), static_cast<uint8_t>(index), out16, MifareCardCore<ACR1281UReader>::KeyType::A, keySlot);

                size_t take = std::min<size_t>(16, bytesToRead - i * 16);
                readBack.insert(readBack.end(), out16, out16 + take);
            }
            std::cout << "Read back (reassembled): \"" << std::string(readBack.begin(), readBack.end()) << "\"\n";
        }

        // 11) Optionally, print full dump using reader's own readAll (if you prefer original behaviour)
        std::cout << "\nreadAll from page 0 (reader helper):\n";
        auto all = acr1281u.readAll(0); // using your original helper
        std::cout << "Total bytes read: " << all.size() << "\n";
        printHex(all.data(), static_cast<DWORD>(all.size()));
    }
    catch (const std::exception& ex) {
        std::cerr << "RW test failed: " << ex.what() << std::endl;
    }
}

/********************  Ultralight ********************/
void testACR1281UReaderUltralight(CardConnection& cardConnection, BYTE startPage) {
	std::cout << "\n--- " << __func__ << ": ---\n";
	ACR1281UReader acr1281u(cardConnection);
	testACR1281UReaderUltralightUnsecured(acr1281u, startPage);
	testACR1281UReaderUltralightSecured(acr1281u, startPage);
}

void testACR1281UReaderUltralightUnsecured(ACR1281UReader& acr1281u, BYTE startPage) {
	std::cout << "\n--- " << __func__ << ": ---\n";
	try {
		std::string text = "Mustafa Selcuk Caglar 10/08/1994";
		// Print original and original as hex
		std::cout << "Original " << text << " || Size: " << text.size() << " bytes ("
			<< ((text.size() + 3) / 4) << " pages) starting at page "
			<< (int)startPage << '\n';

		acr1281u.writeData(startPage, text);

		auto readBack = acr1281u.readData(startPage, text.size());
		std::cout << "Read back: " << std::string(readBack.begin(), readBack.end()) << '\n';

		std::cout << "\nreadAll from page 0:\n";
		auto all = acr1281u.readAll(0);
		std::cout << "Total bytes read: " << all.size()
			<< " (" << (all.size() / 4) << " pages)\n";
		printHex(all.data(), (DWORD)all.size());
	}
	catch (const std::exception& ex) {
		std::cerr << "RW test failed: " << ex.what() << std::endl;
	}
}

void testACR1281UReaderUltralightSecured(ACR1281UReader& acr1281u, BYTE startPage) {
	// Unified runner: call all available cipher-specific tests
	std::cout << "\n--- " << __func__ << ": invoking per-cipher secured tests ---\n";
	testACR1281UReaderXorCipher(acr1281u);
	testACR1281UReaderCaesarCipher(acr1281u);
#ifdef _WIN32
	testACR1281UReaderCng3DES(acr1281u);
	testACR1281UReaderCngAES(acr1281u);
	testACR1281UReaderCngAESGcm(acr1281u);
#endif
}

void testACR1281UReaderXorCipher(ACR1281UReader& acr1281u, BYTE startPage) {
	XorCipher::Key4 key = { { 0xDE, 0xAD, 0xBE, 0xEF } };
	XorCipher cipher(key);
	runPerPageTest(acr1281u, cipher, "XorCipher", "Xor cipher test string");
}

void testACR1281UReaderCaesarCipher(ACR1281UReader& acr1281u, BYTE startPage) {
	CaesarCipher cipher(3);
	runPerPageTest(acr1281u, cipher, "CaesarCipher", "Caesar cipher test string");
}

#ifdef _WIN32
void testACR1281UReaderCngAES(ACR1281UReader& acr1281u, BYTE startPage) {
	std::vector<BYTE> key(16, 0x01);
	std::vector<BYTE> iv(16, 0x00);
	CngAES cipher(key, iv);
	runBlobTest(acr1281u, cipher, "CngAES", "CngAES test data");
}

void testACR1281UReaderCng3DES(ACR1281UReader& acr1281u, BYTE startPage) {
	std::vector<BYTE> key(24, 0x02);
	std::vector<BYTE> iv(8, 0x00);
	Cng3DES cipher(key, iv);
	runBlobTest(acr1281u, cipher, "Cng3DES", "3DES test data!!");
}

void testACR1281UReaderCngAESGcm(ACR1281UReader& acr1281u, BYTE startPage) {
	std::vector<BYTE> key(16, 0x03);
	CngAESGcm cipher(key);
	runBlobTestAAD(acr1281u, cipher, "CngAESGcm", "AES-GCM test str", "header");
}
#endif
