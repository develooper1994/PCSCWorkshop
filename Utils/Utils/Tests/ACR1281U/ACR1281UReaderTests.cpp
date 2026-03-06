#include "Card/MifareClassic/MifareClassic.h"
#include "ACR1281UReader.h"
#include "ACR1281UReaderTestHelpers.h"
#include "ACR1281UReaderTests.h"
#include "PcscUtils.h"
#include "Ciphers.h"
#include <iostream>
#include <vector>
#include <string>
#include <CardUtils.h>
#include <Log/Log.h>

/********************  TESTS ********************/

 /********************  Mifare Classic ********************/ 
// loadkey -> auth block -> read/write block
void testACR1281UReaderMifareClassicUnsecured(ACR1281UReader& acr1281u, BYTE startPage) {
	std::cout << "\n--- " << __func__ << ": ---\n";
	try {
		// 1) hazýr metin
		std::string text = "MustafaMustafa77"; // "Mustafa Selcuk Caglar 10/08/1994";

		// 2) prepare reader & card core
		// create a MifareCardCore that wraps acr1281u; assume 1K (false). If you have 4K card, pass true.
		MifareCardCore card(acr1281u, false);  // 1K kart
		card.loadAllKeysToReader();

		// Keys are already loaded in constructor with default values
		// If you want to use different keys, call setKey here:
		// const BYTE customKey[6] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
		// card.setKey(KeyType::A, std::array<BYTE, 6>{...}, KeyStructure::NonVolatile, 0x01);

		// 3) print info using card's layout helpers
		std::cout << "Original: \"" << text << "\" size: " << text.size()
			<< " bytes, needs " << ((text.size() + 15) / 16)
			<< " pages. starting page: " << static_cast<int>(startPage)
			<< " (sector " << card.blockToSector(startPage)
			<< ", block index in sector: " << card.blockIndexInSector(startPage) << ")\n";

		// 4) Read sector trailers for all sectors and print their access bits (for debugging)
		card.printAllTrailers();

		// 5) ensure default sector config: KeyA read-only, KeyB read+write
		MifareCardCore::SectorKeyConfig defaultCfg(true, false, true, true);
		std::vector<MifareCardCore::SectorKeyConfig> sectorConfigs(16, defaultCfg);
		sectorConfigs[0].keyB.writable = false; // Sector 0: KeyA read-only, KeyB read-only
		card.applyAllSectorsConfigStrict(sectorConfigs);

		// 6) Try a pre-read of the start page using card (safe)
		{
			std::cout << "Reading bytes in block " << static_cast<int>(startPage) << ":\n";
			auto result = card.read(static_cast<BYTE>(startPage));
			std::string sread(result.begin(), result.end());
			std::cout << "Empty Read (single block) : \"" << sread << "\"\n";
			printHex(result);
		}

		// 7) Write the bytes at startPage:
		{
			std::cout << "\nWriting bytes in block " << int(startPage) << ":\n";
			card.write(static_cast<BYTE>(startPage), text);
		}

		// 8) Read back same length starting at startPage
		{
			std::cout << "Reading bytes in block " << static_cast<int>(startPage) << ":\n";
			auto result = card.read(static_cast<BYTE>(startPage));
			std::cout << "Read back (reassembled): \"" << std::string(result.begin(), result.end()) << "\"\n";
			printHex(result);
		}

		// 9) Optionally, print full dump using reader's own readAll (if you prefer original behaviour)
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
	auto uid = CardUtils::getUid(cardConnection);
	std::cout << "UID: " << std::string(uid.begin(), uid.end()) << "\n";
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

