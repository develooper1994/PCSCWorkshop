
/********************  TESTS ********************/

void ACR1281UReader::testACR1281UReader(CardConnection& card) {
    std::cout << "\n--- " << __func__ << ": ---\n";
    ACR1281UReader acr1281u(card);
    testACR1281UReaderUnsecured(acr1281u);
    testACR1281UReaderSecured(acr1281u);
}

void ACR1281UReader::testACR1281UReaderUnsecured(ACR1281UReader& acr1281u) {
    std::cout << "\n--- " << __func__ << ": ---\n";
    try {
        BYTE startPage = 4;
        std::string text = "Mustafa Selcuk Caglar 10/08/1994";

        std::cout << "Original " << text.size() << " bytes ("
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

void ACR1281UReader::testACR1281UReaderSecured(ACR1281UReader& acr1281u) {
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

void ACR1281UReader::testACR1281UReaderXorCipher(ACR1281UReader& acr1281u) {
    XorCipher::Key4 key = { { 0xDE, 0xAD, 0xBE, 0xEF } };
    XorCipher cipher(key);
    runPerPageTest(acr1281u, cipher, "XorCipher", "Xor cipher test string");
}

void ACR1281UReader::testACR1281UReaderCaesarCipher(ACR1281UReader& acr1281u) {
    CaesarCipher cipher(3);
    runPerPageTest(acr1281u, cipher, "CaesarCipher", "Caesar cipher test string");
}

#ifdef _WIN32
void ACR1281UReader::testACR1281UReaderCngAES(ACR1281UReader& acr1281u) {
    std::vector<BYTE> key(16, 0x01);
    std::vector<BYTE> iv(16, 0x00);
    CngAES cipher(key, iv);
    runBlobTest(acr1281u, cipher, "CngAES", "CngAES test data");
}

void ACR1281UReader::testACR1281UReaderCng3DES(ACR1281UReader& acr1281u) {
    std::vector<BYTE> key(24, 0x02);
    std::vector<BYTE> iv(8, 0x00);
    Cng3DES cipher(key, iv);
    runBlobTest(acr1281u, cipher, "Cng3DES", "3DES test data!!");
}

void ACR1281UReader::testACR1281UReaderCngAESGcm(ACR1281UReader& acr1281u) {
    std::vector<BYTE> key(16, 0x03);
    CngAESGcm cipher(key);
    runBlobTestAAD(acr1281u, cipher, "CngAESGcm", "AES-GCM test str", "header");
}
#endif
