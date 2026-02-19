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

Reader::Reader(CardConnection& c) : pImpl(std::make_unique<Impl>(c)){}
Reader::Reader(Reader&&) noexcept = default;
Reader::~Reader() = default;
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

// --- AAD-capable single-page operations ---

void Reader::writePageEncryptedAAD(BYTE page, const BYTE* data4, const ICipher& cipher, const BYTE* aad, size_t aad_len) {
    // Prefer AAD-capable encrypt; ICipher default forwards to non-AAD if unsupported
    auto enc = cipher.encrypt(data4, 4, aad, aad_len);
    BYTE buf[4] = {0,0,0,0};
    for (size_t i = 0; i < 4 && i < enc.size(); ++i) buf[i] = enc[i];
    writePage(page, buf);
}

BYTEV Reader::readPageDecryptedAAD(BYTE page, const ICipher& cipher, const BYTE* aad, size_t aad_len) {
    BYTEV raw = readPage(page);
    return cipher.decrypt(raw.data(), raw.size(), aad, aad_len);
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

void Reader::writePageEncryptedAAD(BYTE page, const BYTEV& data, const ICipher& cipher, const BYTEV& aad) {
    BYTE tmp[4] = {0,0,0,0};
    for (size_t i = 0; i < 4 && i < data.size(); ++i) tmp[i] = data[i];
    writePageEncryptedAAD(page, tmp, cipher, aad.data(), aad.size());
}

void Reader::writePageEncryptedAAD(BYTE page, const std::string& s, const ICipher& cipher, const BYTEV& aad) {
    BYTE tmp[4] = {0,0,0,0};
    for (size_t i = 0; i < 4 && i < s.size(); ++i) tmp[i] = static_cast<BYTE>(s[i]);
    writePageEncryptedAAD(page, tmp, cipher, aad.data(), aad.size());
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

    size_t pages = (encLen + 3) / 4;
    BYTEV raw;
    for (size_t i = 0; i < pages; ++i) {
        try {
            auto p = readPage(static_cast<BYTE>(startPage + i));
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

    size_t pages = (encLen + 3) / 4;
    BYTEV raw;
    for (size_t i = 0; i < pages; ++i) {
        try {
            auto p = readPage(static_cast<BYTE>(startPage + i));
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
