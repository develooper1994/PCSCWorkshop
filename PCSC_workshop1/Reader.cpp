#include "Reader.h"
#include <sstream>

// ============================================================
// Reader::Impl — hidden implementation of the base Reader
// ============================================================
struct Reader::Impl {
    CardConnection& card;

    explicit Impl(CardConnection& c) : card(c) {}
};

// ============================================================
// Reader — base class implementation
// ============================================================

Reader::Reader(CardConnection& c)
    : pImpl(std::make_unique<Impl>(c))
{}

Reader::~Reader() = default;

Reader::Reader(Reader&&) noexcept = default;
Reader& Reader::operator=(Reader&&) noexcept = default;

CardConnection& Reader::card() { return pImpl->card; }
const CardConnection& Reader::card() const { return pImpl->card; }

// --- Encrypted write/read (cipher dýþarýdan enjekte edilir) ---

void Reader::writePageEncrypted(BYTE page, const BYTE* data4, const ICipher& cipher) {
    auto enc = cipher.encrypt(data4, 4);
    BYTE buf[4] = {0,0,0,0};
    for (size_t i = 0; i < 4 && i < enc.size(); ++i) buf[i] = enc[i];
    writePage(page, buf);
}

BYTEV Reader::readPageDecrypted(BYTE page, const ICipher& cipher) {
    BYTEV raw = readPage(page);
    return cipher.decrypt(raw);
}

void Reader::writePageEncrypted(BYTE page, const BYTEV& data, const ICipher& cipher) {
    BYTE tmp[4] = {0,0,0,0};
    for (size_t i = 0; i < 4 && i < data.size(); ++i) tmp[i] = data[i];
    writePageEncrypted(page, tmp, cipher);
}

void Reader::writePageEncrypted(BYTE page, const std::string& s, const ICipher& cipher) {
    BYTE tmp[4] = {0,0,0,0};
    for (size_t i = 0; i < 4 && i < s.size(); ++i) tmp[i] = static_cast<BYTE>(s[i]);
    writePageEncrypted(page, tmp, cipher);
}

// --- Plain convenience overloads (unchanged) ---

void Reader::writePage(BYTE page, const BYTEV& data) {
    BYTE tmp[4] = {0,0,0,0};
    for (size_t i = 0; i < 4 && i < data.size(); ++i) tmp[i] = data[i];
    writePage(page, tmp);
}

void Reader::writePage(BYTE page, const std::string& s) {
    BYTE tmp[4] = {0,0,0,0};
    for (size_t i = 0; i < 4 && i < s.size(); ++i) tmp[i] = static_cast<BYTE>(s[i]);
    writePage(page, tmp);
}

void Reader::writeData(BYTE startPage, const BYTEV& data) {
    size_t total = data.size();
    size_t pages = (total + 3) / 4;
    for (size_t i = 0; i < pages; ++i) {
        BYTE page = static_cast<BYTE>(startPage + i);
        BYTE chunk[4] = { 0, 0, 0, 0 };
        for (size_t b = 0; b < 4; ++b) {
            size_t idx = i * 4 + b;
            if (idx < total) chunk[b] = data[idx];
        }
        writePage(page, chunk);
    }
}

void Reader::writeData(BYTE startPage, const std::string& s) {
    BYTEV data(s.begin(), s.end());
    writeData(startPage, data);
}

BYTEV Reader::readData(BYTE startPage, size_t length) {
    BYTEV out;
    size_t remaining = length;
    BYTE page = startPage;
    while (remaining > 0) {
        auto p = readPage(page);
        if (!p.empty()) out.insert(out.end(), p.begin(), p.end());
        remaining = (remaining > 4) ? (remaining - 4) : 0;
        ++page;
    }
    out.resize(length);
    return out;
}

// --- Multi-page encrypted write/read ---

void Reader::writeDataEncrypted(BYTE startPage, const BYTEV& data, const ICipher& cipher) {
    size_t total = data.size();
    size_t pages = (total + 3) / 4;
    for (size_t i = 0; i < pages; ++i) {
        BYTE page = static_cast<BYTE>(startPage + i);
        BYTE chunk[4] = { 0, 0, 0, 0 };
        for (size_t b = 0; b < 4; ++b) {
            size_t idx = i * 4 + b;
            if (idx < total) chunk[b] = data[idx];
        }
        writePageEncrypted(page, chunk, cipher);
    }
}

void Reader::writeDataEncrypted(BYTE startPage, const std::string& s, const ICipher& cipher) {
    BYTEV data(s.begin(), s.end());
    writeDataEncrypted(startPage, data, cipher);
}

BYTEV Reader::readDataDecrypted(BYTE startPage, size_t length, const ICipher& cipher) {
    BYTEV out;
    size_t remaining = length;
    BYTE page = startPage;
    while (remaining > 0) {
        auto p = readPageDecrypted(page, cipher);
        if (!p.empty()) out.insert(out.end(), p.begin(), p.end());
        remaining = (remaining > 4) ? (remaining - 4) : 0;
        ++page;
    }
    out.resize(length);
    return out;
}

// --- Read all pages until error ---

BYTEV Reader::readAll(BYTE startPage) {
    BYTEV out;
    BYTE page = startPage;
    while (true) {
        try {
            auto p = readPage(page);
            if (p.empty()) break;
            out.insert(out.end(), p.begin(), p.end());
            ++page;
        }
        catch (...) {
            break;
        }
    }
    return out;
}

// ============================================================
// ACR1281UReader::Impl — hidden implementation details
// ============================================================
struct ACR1281UReader::Impl {
    // ACR1281U-specific state can be added here in the future
};

// ============================================================
// ACR1281UReader — implementation
// ============================================================

ACR1281UReader::ACR1281UReader(CardConnection& c)
    : Reader(c), pImplACR(std::make_unique<Impl>())
{}

ACR1281UReader::~ACR1281UReader() = default;

ACR1281UReader::ACR1281UReader(ACR1281UReader&&) noexcept = default;
ACR1281UReader& ACR1281UReader::operator=(ACR1281UReader&&) noexcept = default;

void ACR1281UReader::writePage(BYTE page, const BYTE* data4) {
    if (!card().isConnected()) throw std::runtime_error("Card not connected");
    BYTEV apdu;
    apdu.reserve(5 + 4);
    apdu.push_back(0xFF);        // CLA
    apdu.push_back(0xD6);        // INS = UPDATE BINARY / vendor write
    apdu.push_back(0x00);        // P1
    apdu.push_back(page);        // P2 = page
    apdu.push_back(0x04);        // LC = 4
    apdu.insert(apdu.end(), data4, data4 + 4);

    auto resp = card().transmit(apdu);
    if (resp.size() < 2) throw std::runtime_error("Invalid response for write");
    BYTE sw1 = resp[resp.size()-2], sw2 = resp[resp.size()-1];
    if (!(sw1 == 0x90 && sw2 == 0x00)) {
        std::stringstream ss;
        ss << "Write failed SW=0x" << std::hex << (int)sw1 << " 0x" << (int)sw2;
        throw std::runtime_error(ss.str());
    }
}

BYTEV ACR1281UReader::readPage(BYTE page) {
    if (!card().isConnected()) throw std::runtime_error("Card not connected");
    BYTEV apdu;
    apdu.reserve(5);
    apdu.push_back(0xFF);    // CLA
    apdu.push_back(0xB0);    // INS = READ BINARY / vendor read
    apdu.push_back(0x00);    // P1
    apdu.push_back(page);    // P2 = page
    apdu.push_back(0x04);    // Le = 4

    auto resp = card().transmit(apdu);
    if (resp.size() < 2) throw std::runtime_error("Invalid response for read");
    BYTE sw1 = resp[resp.size()-2], sw2 = resp[resp.size()-1];
    if (!(sw1 == 0x90 && sw2 == 0x00)) {
        std::stringstream ss;
        ss << "Read failed SW=0x" << std::hex << (int)sw1 << " 0x" << (int)sw2;
        throw std::runtime_error(ss.str());
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
