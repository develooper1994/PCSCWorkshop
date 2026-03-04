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
		const BYTE keySlotA = 0x01;
		const BYTE keySlotB = 0x02;

		// 3) print info using card's layout helpers
		std::cout << "Original: \"" << text << "\" size: " << text.size()
			<< " bytes, needs " << ((text.size() + 15) / 16)
			<< " pages. starting page: " << int(startPage)
			<< " (sector " << card.blockToSector(startPage)
			<< ", block index in sector: " << card.blockIndexInSector(startPage) << ")\n";

		// 4) load default keys
		const BYTE defaultKey6[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
		card.setKey(MifareCardCore::KeyType::A, std::array<BYTE, 6>{defaultKey6[0], defaultKey6[1], defaultKey6[2], defaultKey6[3], defaultKey6[4], defaultKey6[5]},
			KeyStructure::NonVolatile, keySlotA);
		card.setKey(MifareCardCore::KeyType::B, std::array<BYTE, 6>{defaultKey6[0], defaultKey6[1], defaultKey6[2], defaultKey6[3], defaultKey6[4], defaultKey6[5]},
			KeyStructure::NonVolatile, keySlotB);
		acr1281u.loadKey(defaultKey6, KeyStructure::NonVolatile, keySlotA);
		acr1281u.loadKey(defaultKey6, KeyStructure::NonVolatile, keySlotB);

		// 5) Read sector trailers for all sectors and print their access bits (for debugging)
		card.printAllTrailers();

		// 6) ensure default sector config: KeyA read-only, KeyB read+write
		MifareCardCore::SectorKeyConfig defaultCfg(true, false, true, true);
		std::vector<MifareCardCore::SectorKeyConfig> sectorConfigs(16, defaultCfg);
		sectorConfigs.at(0).keyB_canWrite = false; // Sector 0: KeyA read-only, KeyB read-only
		card.applyAllSectorsConfig(sectorConfigs);

		// 7) Try a pre-read of the start page using card (safe)
		{
			std::cout << "Reading bytes in block " << int(startPage) << ":\n";
			auto result = card.read(static_cast<BYTE>(startPage));
			std::string sread(result.begin(), result.end());
			std::cout << "Empty Read (single block) : \"" << sread << "\"\n";
			printHex(result);
		}

		// 8) Write the bytes at startPage:
		{
			std::cout << "\nWriting bytes in block " << int(startPage) << ":\n";
			card.write(static_cast<BYTE>(startPage), text);
		}

		// 9) Read back same length starting at startPage
		{
			std::cout << "Reading bytes in block " << int(startPage) << ":\n";
			auto result = card.read(static_cast<BYTE>(startPage));
			std::cout << "Read back (reassembled): \"" << std::string(result.begin(), result.end()) << "\"\n";
			printHex(result);
		}

		// 10) Optionally, print full dump using reader's own readAll (if you prefer original behaviour)
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

