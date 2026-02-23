#include "Reader.h"
#include <sstream>
#include <vector>
#include <cstring>

// ============================================================
// Reader::Impl — hidden implementation of the base Reader
// ============================================================
struct Reader::Impl {
    CardConnection& card;
    BYTE LC = 0x04;
    bool isAuthRequested = false;
    KeyType keyType = KeyType::A; // default to Key A
    KeyStructure keyStructure = KeyStructure::NonVolatile; // default to non-volatile
    BYTE keyNumber = 0x01;
    BYTE keyA[6] = { (BYTE)0xFF, (BYTE)0xFF, (BYTE)0xFF, (BYTE)0xFF, (BYTE)0xFF, (BYTE)0xFF }; // default key for Mifare Classic (6 bytes)
    BYTE keyB[6] = { (BYTE)0xFF, (BYTE)0xFF, (BYTE)0xFF, (BYTE)0xFF, (BYTE)0xFF, (BYTE)0xFF };

    // Primary ctor: initialize with CardConnection reference and optional parameters
    explicit Impl(CardConnection& c,
                  BYTE lc = 0x04,
                  bool authRequested = false,
                  KeyType kt = KeyType::A,
                  KeyStructure ks = KeyStructure::NonVolatile,
                  BYTE keyNum = 0x01,
                  const BYTE* keya = nullptr,
                  const BYTE* keyb = nullptr)
        : card(c), LC(lc), isAuthRequested(authRequested), keyType(kt), keyStructure(ks), keyNumber(keyNum)
    {
        if (isAuthRequested) {
            if (keya) std::memcpy(keyA, keya, 6);
            if (keyb) std::memcpy(keyB, keyb, 6);
        }
        else {
            // If auth is not requested, zero out keys for safety
            std::memset(keyA, 0, 6);
            std::memset(keyB, 0, 6);
		}
    }

    // Non-copyable because of reference member
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    // Movable: allow move construction/assignment
    Impl(Impl&& other) noexcept
        : card(other.card), LC(other.LC), isAuthRequested(other.isAuthRequested), keyType(other.keyType), keyStructure(other.keyStructure), keyNumber(other.keyNumber)
    {
        std::memcpy(keyA, other.keyA, 6);
    }

    Impl& operator=(Impl&& other) noexcept {
        if (this != &other) {
            // card is a reference — cannot rebind; this assignment should not be used in practice
            LC = other.LC;
            isAuthRequested = other.isAuthRequested;
            keyType = other.keyType;
            keyStructure = other.keyStructure;
            keyNumber = other.keyNumber;
            std::memcpy(keyA, other.keyA, 6);
        }
        return *this;
    }
};

// ============================================================
// Reader — base class implementation
// ============================================================

Reader::Reader(CardConnection& c) : pImpl(std::make_unique<Impl>(c)) {}
Reader::Reader(CardConnection& c, BYTE lc, bool authRequested, KeyType kt, KeyStructure ks, BYTE keyNumber, const BYTE key[6])
    : pImpl(std::make_unique<Impl>(c, lc, authRequested, kt, ks, keyNumber, key)) {}
Reader::Reader(CardConnection& c, BYTE lc, bool authRequested, KeyType kt, KeyStructure ks, BYTE keyNumber, const BYTE keyA[6], const BYTE keyB[6])
    : pImpl(std::make_unique<Impl>(c, lc, authRequested, kt, ks, keyNumber, keyA, keyB)) {}
Reader::Reader(Reader&&) noexcept = default;
Reader::~Reader() = default;
Reader& Reader::operator=(Reader&&) noexcept = default;

CardConnection& Reader::card() { return pImpl->card; }
const CardConnection& Reader::card() const { return pImpl->card; }

// New accessors to expose Impl members
bool Reader::isAuthRequested() const { return pImpl->isAuthRequested; }
void Reader::setAuthRequested(bool v) { pImpl->isAuthRequested = v; }

BYTE Reader::getLC() const { return pImpl->LC; }
void Reader::setLC(BYTE lc) { pImpl->LC = lc; }

KeyType Reader::keyType() const { return pImpl->keyType; }
void Reader::setKeyType(KeyType kt) { pImpl->keyType = kt; }

KeyStructure Reader::keyStructure() const { return pImpl->keyStructure; }
void Reader::setKeyStructure(KeyStructure ks) { pImpl->keyStructure = ks; }

BYTE Reader::getKeyNumber() const { return pImpl->keyNumber; }
void Reader::setKeyNumber(BYTE key) { pImpl->keyNumber = key; }

void Reader::getKey(BYTE out[6]) const {
    if (out == nullptr) return;
    for (int i = 0; i < 6; ++i) out[i] = pImpl->keyA[i];
}

void Reader::setKey(const BYTE* key) {
    if (!key) return;
    for (int i = 0; i < 6; ++i) pImpl->keyA[i] = key[i];
}

// Provide default implementation that forwards to the simple readPage
BYTEV Reader::readPage(BYTE page, BYTE len) {
    // Adjust LC temporarily if different from default
    BYTE oldLC = getLC();
    if (len != oldLC) setLC(len);
    BYTEV res = readPage(page);
    if (len != oldLC) setLC(oldLC);
    return res;
}

// --- Encrypted single-page write/read ---

void Reader::writePageEncrypted(BYTE page, const BYTE* data4, const ICipher& cipher) {
    auto enc = cipher.encrypt(data4, 4);
    BYTEV buf(getLC());
    for (size_t i = 0; i < getLC() && i < enc.size(); ++i) buf[i] = enc[i];
    writePage(page, buf.data());
}

BYTEV Reader::readPageDecrypted(BYTE page, const ICipher& cipher) {
    BYTEV raw = readPage(page);
    return cipher.decrypt(raw);
}

// --- AAD-capable single-page operations ---

void Reader::writePageEncryptedAAD(BYTE page, const BYTE* data4, const ICipher& cipher, const BYTE* aad, size_t aad_len) {
    // Prefer AAD-capable encrypt; ICipher default forwards to non-AAD if unsupported
    auto enc = cipher.encrypt(data4, 4, aad, aad_len);
    BYTEV buf(getLC());
    for (size_t i = 0; i < getLC() && i < enc.size(); ++i) buf[i] = enc[i];
    writePage(page, buf.data());
}

BYTEV Reader::readPageDecryptedAAD(BYTE page, const ICipher& cipher, const BYTE* aad, size_t aad_len) {
    BYTEV raw = readPage(page);
    return cipher.decrypt(raw.data(), raw.size(), aad, aad_len);
}

// --- Plain convenience overloads ---

void Reader::writePage(BYTE page, const BYTEV& data) {
    BYTEV tmp(getLC());
    for (size_t i = 0; i < getLC() && i < data.size(); ++i) tmp[i] = data[i];
    writePage(page, tmp.data());
}

void Reader::writePage(BYTE page, const std::string& s) {
    BYTEV tmp(getLC());
    for (size_t i = 0; i < getLC() && i < s.size(); ++i) tmp[i] = static_cast<BYTE>(s[i]);
    writePage(page, tmp.data());
}

void Reader::writePageEncrypted(BYTE page, const BYTEV& data, const ICipher& cipher) {
    BYTEV tmp(getLC());
    for (size_t i = 0; i < getLC() && i < data.size(); ++i) tmp[i] = data[i];
    writePageEncrypted(page, tmp.data(), cipher);
}

void Reader::writePageEncrypted(BYTE page, const std::string& s, const ICipher& cipher) {
    BYTEV tmp(getLC());
    for (size_t i = 0; i < getLC() && i < s.size(); ++i) tmp[i] = static_cast<BYTE>(s[i]);
    writePageEncrypted(page, tmp.data(), cipher);
}

void Reader::writePageEncryptedAAD(BYTE page, const BYTEV& data, const ICipher& cipher, const BYTEV& aad) {
    BYTEV tmp(getLC());
    for (size_t i = 0; i < getLC() && i < data.size(); ++i) tmp[i] = data[i];
    writePageEncryptedAAD(page, tmp.data(), cipher, aad.data(), aad.size());
}

void Reader::writePageEncryptedAAD(BYTE page, const std::string& s, const ICipher& cipher, const BYTEV& aad) {
    BYTEV tmp(getLC());
    for (size_t i = 0; i < getLC() && i < s.size(); ++i) tmp[i] = static_cast<BYTE>(s[i]);
    writePageEncryptedAAD(page, tmp.data(), cipher, aad.data(), aad.size());
}

// --- Multi-page plain write/read ---

void Reader::writeData(BYTE startPage, const BYTEV& data) {
    size_t total = data.size();
    size_t pages = (total + getLC()-1) / getLC();
    for (size_t i = 0; i < pages; ++i) {
        BYTE page = static_cast<BYTE>(startPage + i);
        BYTEV chunk(getLC());
        for (size_t b = 0; b < getLC(); ++b) {
            size_t idx = i * getLC() + b;
            if (idx < total) chunk[b] = data[idx];
        }
        writePage(page, chunk);
    }
}

void Reader::writeData(BYTE startPage, const std::string& s) {
    BYTEV data(s.begin(), s.end());
    writeData(startPage, data);
}

// New overloads that allow custom LC
void Reader::writeData(BYTE startPage, const BYTEV& data, BYTE lc) {
    BYTE old = getLC();
    if (old == lc) { writeData(startPage, data); return; }
    setLC(lc);
    writeData(startPage, data);
    setLC(old);
}

void Reader::writeData(BYTE startPage, const std::string& s, BYTE lc) {
    BYTEV data(s.begin(), s.end());
    writeData(startPage, data, lc);
}

BYTEV Reader::readData(BYTE startPage, size_t length) {
    BYTEV out;
    size_t remaining = length;
    BYTE page = startPage;
    while (remaining > 0) {
        auto p = readPage(page);
        if (!p.empty()) out.insert(out.end(), p.begin(), p.end());
        remaining = (remaining > getLC()) ? (remaining - getLC()) : 0;
        ++page;
    }
    out.resize(length);
    return out;
}

// --- Multi-page encrypted write/read ---

void Reader::writeDataEncrypted(BYTE startPage, const BYTEV& data, const ICipher& cipher) {
    // Encrypt the entire blob first, then write raw encrypted bytes page-by-page
    auto encrypted = cipher.encrypt(data);
    writeData(startPage, encrypted);
}

void Reader::writeDataEncrypted(BYTE startPage, const std::string& s, const ICipher& cipher) {
    BYTEV data(s.begin(), s.end());
    writeDataEncrypted(startPage, data, cipher);
}

BYTEV Reader::readDataDecrypted(BYTE startPage, size_t length, const ICipher& cipher) {
    // For block ciphers with PKCS7 padding, encrypted size = ((length / blockSize) + 1) * blockSize
    // AES blockSize=16, 3DES blockSize=8. We don't know the block size, so try common sizes.
    // For XorCipher/CaesarCipher (stream-like), encrypted size == length.
    // Strategy: try to encrypt a same-length dummy to determine the actual encrypted size,
    // or simply read enough pages and let the decrypt handle trimming.
    
    // First pass: try with exact length (for stream ciphers)
    // Second pass: try with additional blocks (for block ciphers: +16 covers AES, +8 covers 3DES)
    // We try encrypted sizes in order and return the first successful decrypt.
    
    // Determine how many bytes were actually written by doing a trial encrypt
    BYTEV dummy(length, 0);
    size_t encLen;
    try {
        auto trial = cipher.encrypt(dummy);
        encLen = trial.size();
    }
    catch (...) {
        encLen = length; // fallback for stream ciphers
    }

    size_t pages = (encLen + getLC() -1) / getLC();
    BYTEV raw;
    for (size_t i = 0; i < pages; ++i) {
        try {
            auto p = readPage(static_cast<BYTE>(startPage + i), getLC());
            raw.insert(raw.end(), p.begin(), p.end());
        }
        catch (...) {
            break;
        }
    }

    if (raw.empty()) return raw;

    // Trim to exact encrypted size (pages may have trailing zeros)
    if (raw.size() > encLen) raw.resize(encLen);

    auto decrypted = cipher.decrypt(raw);
    if (decrypted.size() > length) decrypted.resize(length);
    return decrypted;
}

// --- Multi-page AAD-capable encrypted write/read ---

void Reader::writeDataEncryptedAAD(BYTE startPage, const BYTEV& data, const ICipher& cipher, const BYTE* aad, size_t aad_len) {
    // Encrypt the entire blob with AAD, then write raw encrypted bytes page-by-page
    auto encrypted = cipher.encrypt(data.data(), data.size(), aad, aad_len);
    writeData(startPage, encrypted);
}

void Reader::writeDataEncryptedAAD(BYTE startPage, const std::string& s, const ICipher& cipher, const BYTE* aad, size_t aad_len) {
    BYTEV data(s.begin(), s.end());
    writeDataEncryptedAAD(startPage, data, cipher, aad, aad_len);
}

BYTEV Reader::readDataDecryptedAAD(BYTE startPage, size_t length, const ICipher& cipher, const BYTE* aad, size_t aad_len) {
    // Determine encrypted blob size by trial encrypt
    BYTEV dummy(length, 0);
    size_t encLen;
    try {
        auto trial = cipher.encrypt(dummy.data(), dummy.size(), aad, aad_len);
        encLen = trial.size();
    }
    catch (...) {
        encLen = length;
    }

    size_t pages = (encLen + getLC()-1) / getLC();
    BYTEV raw;
    for (size_t i = 0; i < pages; ++i) {
        try {
            auto p = readPage(static_cast<BYTE>(startPage + i), getLC());
            raw.insert(raw.end(), p.begin(), p.end());
        }
        catch (...) {
            break;
        }
    }

    if (raw.empty()) return raw;

    if (raw.size() > encLen) raw.resize(encLen);

    auto decrypted = cipher.decrypt(raw.data(), raw.size(), aad, aad_len);
    if (decrypted.size() > length) decrypted.resize(length);
    return decrypted;
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

void Reader::authNew(const BYTE* data5) {
    if (!card().isConnected()) throw std::runtime_error("Card not connected");
    BYTE authLC = 0x05; // 5 bytes
    BYTEV apdu{ 0xFF, 0x86, 0x00, 0x00, authLC };
    apdu.insert(apdu.end(), data5, data5 + authLC);

    auto resp = card().transmit(apdu);
    if (resp.size() < 2) throw std::runtime_error("Invalid response for write");
    BYTE sw1 = resp[resp.size() - 2], sw2 = resp[resp.size() - 1];
    if (!((sw1 == 0x90 || sw1 == 0x91) && sw2 == 0x00)) {
        std::stringstream ss;
        ss << "Write failed SW=0x" << std::hex << (int)sw1 << " 0x" << (int)sw2;
        throw std::runtime_error(ss.str());
    }
}
