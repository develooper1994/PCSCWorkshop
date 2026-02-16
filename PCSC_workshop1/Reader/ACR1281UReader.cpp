#include "ACR1281UReader.h"
#include "../PcscUtils.h"
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

void ACR1281UReader::testACR1281UReaderSecured(ACR1281UReader& acr1281u) {
    std::cout << "\n--- " << __func__ << ": ---\n";
    try {
        XorCipher::Key4 key = {{ 0xDE, 0xAD, 0xBE, 0xEF }};
        XorCipher cipher(key);

        const ICipher& ic = cipher;
        if (!ic.test()) {
            std::cerr << "Cipher self-test FAILED — aborting secured RW test\n";
            return;
        }

        BYTE startPage = 4;
        std::string text = "Mustafa Selcuk Caglar 10/08/1994";
        size_t len = text.size();

        std::cout << "Original (" << len << " bytes, "
                  << ((len + 3) / 4) << " pages): " << text << '\n';

        // Çoklu page þifreli yazma
        acr1281u.writeDataEncrypted(startPage, text, cipher);

        // Ham okuma — karttaki þifreli byte'larý göster
        auto raw = acr1281u.readData(startPage, len);
        std::cout << "On card (encrypted): "; printHex(raw.data(), (DWORD)raw.size());

        // Çoklu page þifreli okuma ve çözme
        auto dec = acr1281u.readDataDecrypted(startPage, len, cipher);
        std::cout << "Decrypted: " << std::string(dec.begin(), dec.end()) << '\n';
    }
    catch (const std::exception& ex) {
        std::cerr << "Secured RW test failed: " << ex.what() << std::endl;
    }
}
