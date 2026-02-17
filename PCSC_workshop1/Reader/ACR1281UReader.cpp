#include "ACR1281UReader.h"
#include "../utils/PcscUtils.h"
#include "../Cipher/Ciphers.h"
#include <sstream>

using namespace std;

ACR1281UReader::ACR1281UReader(CardConnection& c) : Reader(c) {}
ACR1281UReader::ACR1281UReader(ACR1281UReader&&) noexcept = default;
ACR1281UReader::~ACR1281UReader() = default;
ACR1281UReader& ACR1281UReader::operator=(ACR1281UReader&&) noexcept = default;

void ACR1281UReader::writePage(BYTE page, const BYTE* data4) {
    if (!card().isConnected()) throw runtime_error("Card not connected");
    BYTEV apdu{ 0xFF, 0xD6, 0x00, page, 0x04 };
    apdu.insert(apdu.end(), data4, data4 + 4);

    auto resp = card().transmit(apdu);
    if (resp.size() < 2) throw runtime_error("Invalid response for write");
    BYTE sw1 = resp[resp.size()-2], sw2 = resp[resp.size()-1];
    if (!((sw1 == 0x90 || sw1 == 0x91) && sw2 == 0x00)) {
        stringstream ss;
        ss << "Write failed SW=0x" << hex << (int)sw1 << " 0x" << (int)sw2;
        throw runtime_error(ss.str());
    }
}

BYTEV ACR1281UReader::readPage(BYTE page) {
    if (!card().isConnected()) throw runtime_error("Card not connected");
    BYTEV apdu{ 0xFF, 0xB0, 0x00, page, 0x04};

    auto resp = card().transmit(apdu);
    if (resp.size() < 2) throw runtime_error("Invalid response for read");
    BYTE sw1 = resp[resp.size()-2], sw2 = resp[resp.size()-1];
    if (!((sw1 == 0x90 || sw1 == 0x91) && sw2 == 0x00)) {
        stringstream ss;
        ss << "Read failed SW=0x" << hex << (int)sw1 << " 0x" << (int)sw2;
        throw runtime_error(ss.str());
    }
    return BYTEV(resp.begin(), resp.end()-2);
}

/********************  TEST ********************/

void ACR1281UReader::testACR1281UReader(CardConnection& card) {
    std::cout << "\n--- "<< __func__  <<": ---\n";
    ACR1281UReader acr1281u(card);
    testACR1281UReaderUnsecured(acr1281u);
    testACR1281UReaderSecured(acr1281u);
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

void ACR1281UReader::testACR1281UReaderUnsecured(ACR1281UReader& acr1281u) {
    std::cout << "\n--- " << __func__ << ": ---\n";
    try {
        BYTE startPage = 4;
        std::string text = "Mustafa Selcuk Caglar 10/08/1994";

        std::cout << "Writing " << text.size() << " bytes ("
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

void ACR1281UReader::testACR1281UReaderXorCipher(ACR1281UReader& acr1281u) {
    std::cout << "\n--- " << __func__ << ": ---\n";
    try {
        XorCipher::Key4 key = {{ 0xDE, 0xAD, 0xBE, 0xEF }};
        XorCipher cipher(key);

        const ICipher& ic = cipher;
        if (!ic.test()) {
            std::cerr << "Cipher self-test FAILED \u2014 aborting secured RW test\n";
            return;
        }

        BYTE startPage = 4;
        std::string text = "Xor cipher test string"; // "Mustafa Selcuk Caglar 10/08/1994"
        size_t len = text.size();

        std::cout << "Original (" << len << " bytes, "
                  << ((len + 3) / 4) << " pages): " << text << '\n';

        // çoklu page þifreli yazma
        acr1281u.writeDataEncrypted(startPage, text, cipher);

        // Ham okuma — karttaki þifreli byte'larý göster
        auto raw = acr1281u.readData(startPage, len);
        std::cout << "On card (encrypted): "; printHex(raw.data(), (DWORD)raw.size());

        // çoklu page þifreli okuma ve çözme
        auto dec = acr1281u.readDataDecrypted(startPage, len, cipher);
        std::cout << "Decrypted: " << std::string(dec.begin(), dec.end()) << '\n';
    }
    catch (const std::exception& ex) {
        std::cerr << "Secured RW test failed: " << ex.what() << std::endl;
    }
}

void ACR1281UReader::testACR1281UReaderCaesarCipher(ACR1281UReader& acr1281u) {
    std::cout << "\n--- " << __func__ << ": ---\n";
    try {
        CaesarCipher cipher(3);
        const ICipher& ic = cipher;
        if (!ic.test()) {
            std::cerr << "Caesar cipher self-test FAILED \u2014 skipping.\n";
            return;
        }

        BYTE startPage = 4;
        std::string text = "Caesar cipher test string";
        size_t len = text.size();

        std::cout << "Original (" << len << " bytes, "
                  << ((len + 3) / 4) << " pages): " << text << '\n';

        acr1281u.writeDataEncrypted(startPage, text, cipher);
        auto raw = acr1281u.readData(startPage, len);
        std::cout << "On card (encrypted): "; printHex(raw.data(), (DWORD)raw.size());
        auto dec = acr1281u.readDataDecrypted(startPage, len, cipher);
        std::cout << "Decrypted: " << std::string(dec.begin(), dec.end()) << '\n';
    }
    catch (const std::exception& ex) {
        std::cerr << "Caesar secured RW test failed: " << ex.what() << std::endl;
    }
}

#ifdef _WIN32
void ACR1281UReader::testACR1281UReaderCngAES(ACR1281UReader& acr1281u) {
    std::cout << "\n--- " << __func__ << ": ---\n";
    try {
        std::vector<BYTE> key(16, 0x01);
        std::vector<BYTE> iv(16, 0x00);
        CngAES cipher(key, iv);
        if (!cipher.test()) {
            std::cerr << "CngAES self-test FAILED \u2014 skipping.\n";
            return;
        }

        BYTE startPage = 4;
        std::string text = "CngAES test data";
        size_t len = text.size();

        std::cout << "Original (" << len << " bytes): " << text << '\n';

        // Block cipher output is larger than input (padding).
        // Encrypt entire plaintext at once, write ciphertext blob to card,
        // read it back, then decrypt at once.
        auto enc = cipher.encrypt(reinterpret_cast<const BYTE*>(text.data()), len);
        std::cout << "Encrypted blob (" << enc.size() << " bytes): ";
        printHex(enc.data(), (DWORD)enc.size());

        acr1281u.writeData(startPage, enc);
        auto raw = acr1281u.readData(startPage, enc.size());
        std::cout << "On card: "; printHex(raw.data(), (DWORD)raw.size());

        auto dec = cipher.decrypt(raw.data(), raw.size());
        std::cout << "Decrypted: " << std::string(dec.begin(), dec.end()) << '\n';
    }
    catch (const std::exception& ex) {
        std::cerr << "CngAES secured RW test failed: " << ex.what() << std::endl;
    }
}

void ACR1281UReader::testACR1281UReaderCng3DES(ACR1281UReader& acr1281u) {
    std::cout << "\n--- " << __func__ << ": ---\n";
    try {
        std::vector<BYTE> key(24, 0x02);
        std::vector<BYTE> iv(8, 0x00);
        Cng3DES cipher(key, iv);
        if (!cipher.test()) {
            std::cerr << "Cng3DES self-test FAILED \u2014 skipping.\n";
            return;
        }

        BYTE startPage = 4;
        std::string text = "3DES test data!!";  // 16 bytes
        size_t len = text.size();

        std::cout << "Original (" << len << " bytes): " << text << '\n';

        // Encrypt entire plaintext, write ciphertext blob, read back, decrypt
        auto enc = cipher.encrypt(reinterpret_cast<const BYTE*>(text.data()), len);
        std::cout << "Encrypted blob (" << enc.size() << " bytes): ";
        printHex(enc.data(), (DWORD)enc.size());

        acr1281u.writeData(startPage, enc);
        auto raw = acr1281u.readData(startPage, enc.size());
        std::cout << "On card: "; printHex(raw.data(), (DWORD)raw.size());

        auto dec = cipher.decrypt(raw.data(), raw.size());
        std::cout << "Decrypted: " << std::string(dec.begin(), dec.end()) << '\n';
    }
    catch (const std::exception& ex) {
        std::cerr << "Cng3DES secured RW test failed: " << ex.what() << std::endl;
    }
}

void ACR1281UReader::testACR1281UReaderCngAESGcm(ACR1281UReader& acr1281u) {
    std::cout << "\n--- " << __func__ << ": ---\n";
    try {
        std::vector<BYTE> key(16, 0x03);
        CngAESGcm cipher(key);
        if (!cipher.test()) {
            std::cerr << "CngAESGcm self-test FAILED \u2014 skipping.\n";
            return;
        }

        BYTE startPage = 4;
        std::string text = "AES-GCM test str";
        std::string aadStr = "header";
        size_t len = text.size();

        std::cout << "Original (" << len << " bytes): " << text << '\n';

        // GCM output = nonce(12) + ciphertext(N) + tag(16).
        // Encrypt entire plaintext, write blob to card, read back, decrypt.
        auto enc = cipher.encrypt(
            reinterpret_cast<const BYTE*>(text.data()), len,
            reinterpret_cast<const BYTE*>(aadStr.data()), aadStr.size());
        std::cout << "Encrypted blob (" << enc.size() << " bytes): ";
        printHex(enc.data(), (DWORD)enc.size());

        acr1281u.writeData(startPage, enc);
        auto raw = acr1281u.readData(startPage, enc.size());
        std::cout << "On card: "; printHex(raw.data(), (DWORD)raw.size());

        auto dec = cipher.decrypt(
            raw.data(), raw.size(),
            reinterpret_cast<const BYTE*>(aadStr.data()), aadStr.size());
        std::cout << "Decrypted: " << std::string(dec.begin(), dec.end()) << '\n';
    }
    catch (const std::exception& ex) {
        std::cerr << "CngAESGcm secured RW test failed: " << ex.what() << std::endl;
    }
}
#endif
