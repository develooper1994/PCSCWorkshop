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
		std::string text = "Mustafa Selcuk Caglar 10/08/1994";
		// Hesapla: Mifare Classic için sector hesaplama (1K ve 4K uyumlu)
		auto sectorFromPage = [](BYTE page) -> int {
			int p = static_cast<int>(page);
			// Ýlk 128 blok (0..127): sektör baþýna 4 blok (sektör 0..31)
			if (p < 128) return p / 4;
			// Sonraki bloklar (128..255): sektör 32..39, sektör baþýna 16 blok
			return 32 + (p - 128) / 16;
			};
		// Print original and original as hex
		std::cout << "Original " << text << " || Size: " << text.size() << " bytes ("
			<< ((text.size() + 15) / 16) << " pages) starting at page "
			<< (int)startPage << " (sector " << sectorFromPage(startPage) << ")\n";

		if (acr1281u.isAuthRequested()) {
			BYTE key[16] = { (BYTE)0xFF }; // default key for Mifare Classic (16 bytes)
			const BYTE keyNumber = 0x01;
			acr1281u.loadKey(key, KeyStructure::NonVolatile, keyNumber); // Load default key for Mifare Classic
			// acr1281u.authKey<ACR1281UReader>(startPage, KeyType::A, keyNumber); // Authenticate with Key A
			/*BYTE data5[5] = {0x01, 0x00, startPage, 0x60, 0x01}; // fixed: create local array for C++
			acr1281u.authKey2(data5); // Alternative auth with data5
			*/
		}

		auto readBack = acr1281u.readPage(startPage);
		// auto readBack = acr1281u.readData(startPage, text.size(), 16);
		std::cout << "Read back: " << std::string(readBack.begin(), readBack.end()) << '\n';
		printHex(readBack.data(), (DWORD)readBack.size());

		acr1281u.writeData(startPage, text, 16);

		readBack = acr1281u.readData(startPage, text.size());
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
