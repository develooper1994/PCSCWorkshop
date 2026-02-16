#include "../Reader.h"
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

// --- Encrypted single-page write/read ---

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

// --- Plain convenience overloads ---

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

// --- Multi-page plain write/read ---

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
