#include "ACR1281UReader.h"
#include "ACR1281UReaderTestHelpers.h"
#include "ACR1281UReaderTests.h"
#include "PcscUtils.h"
#include "CardUtils.h"
#include "Log/Log.h"
#include "Ciphers.h"
#include "CardIO.h"
#include "CardModel/TrailerConfig.h"
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

/********************  TESTS ********************/

 /********************  Mifare Classic ********************/
// CardIO ile: readCard -> readTrailer -> access config -> read/write
void testACR1281UReaderMifareClassicUnsecured(ACR1281UReader& acr1281u, BYTE startPage) {
	std::cout << "\n--- " << __func__ << ": (CardIO) ---\n";
	try {
		// 1) Hazýr metin
		std::string text = "MustafaMustafa77";
		std::cout << "Original: \"" << text << "\" size: " << text.size()
			<< " bytes, needs " << ((text.size() + 15) / 16)
			<< " pages. Starting at page: " << static_cast<int>(startPage) << "\n";

		// 2) CardIO oluþtur
		CardIO io(acr1281u, false);  // 1K kart

		// 3) Default key (factory)
		KEYBYTES defKey = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
		io.setDefaultKey(defKey, KeyStructure::NonVolatile, 0x01, KeyType::A);

		// 4) Tüm kartý oku
		std::cout << "\nReading card memory...\n";
		int okBlocks = io.readCard();
		std::cout << "OK: " << okBlocks << "/" << io.card().getTotalBlocks() << " blocks\n";

		// 5) UID
		KEYBYTES uid = io.card().getUID();
		std::cout << "UID: ";
		for (int i = 0; i < 4; ++i)
			std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)uid[i] << " ";
		std::cout << std::dec << "\n";

		// 6) Blok topology
		int sector = io.card().getSectorForBlock(startPage);
		int blockInSector = startPage - io.card().getFirstBlockOfSector(sector);
		std::cout << "Target block: " << (int)startPage
			<< " (sector " << sector << ", index " << blockInSector << ")\n";

		// 7) Trailer oku — access bits incele
		std::cout << "\nReading trailers for sector " << sector << "...\n";
		TrailerConfig tc = io.readTrailer(sector);
		SectorAccessConfig cfg = tc.access;
		DataBlockPermission dp = cfg.dataPermission(0);
		std::cout << "Data block: read=" << (dp.readA ? "A" : "") << (dp.readB ? "|B" : "")
			<< " write=" << (dp.writeA ? "A" : "") << (dp.writeB ? "|B" : "") << "\n";

		// 8) Okuma öncesi — boþ blok oku
		{
			std::cout << "\nReading block " << (int)startPage << " before write:\n";
			BYTEV before = io.readBlock(startPage);
			std::cout << "Bytes: ";
			size_t displayLen = before.size() < 16 ? before.size() : 16;
			for (size_t i = 0; i < displayLen; ++i)
				std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)before[i] << " ";
			std::cout << std::dec << "\n";
		}

		// 9) Veri yaz
		{
			std::cout << "\nWriting to block " << (int)startPage << ":\n";
			std::cout << "Text: \"" << text << "\"\n";
			BYTE payload[16] = {0};
			size_t copyLen = text.size() < 16 ? text.size() : 16;
			std::memcpy(payload, text.c_str(), copyLen);
			io.writeBlock(startPage, payload);
			std::cout << "Written OK\n";
		}

		// 10) Okuma sonrasý — veri oku ve doðrula
		{
			std::cout << "\nReading block " << (int)startPage << " after write:\n";
			BYTEV after = io.readBlock(startPage);
			std::string readBack(after.begin(), after.end());
			std::cout << "Read back: \"" << readBack << "\"\n";
			
			bool match = (after.size() >= 16);
			for (size_t i = 0; match && i < text.size(); ++i)
				if (after[i] != (BYTE)text[i]) match = false;
			
			if (match) {
				std::cout << ">>> WRITE/READ VERIFIED OK! <<<\n";
			} else {
				std::cout << "WARNING: Data mismatch!\n";
			}
		}

		// 11) Full dump - cardIO memory'den
		std::cout << "\nFull card memory dump (first 32 bytes):\n";
		const CardMemoryLayout& mem = io.card().getMemory();
		for (int i = 0; i < 2; ++i) {
			for (int j = 0; j < 16; ++j) {
				std::cout << std::hex << std::setfill('0') << std::setw(2)
					<< (int)mem.getRawMemory()[i*16 + j] << " ";
			}
			std::cout << std::dec << "\n";
		}

		std::cout << "\nTest completed successfully!\n";
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

