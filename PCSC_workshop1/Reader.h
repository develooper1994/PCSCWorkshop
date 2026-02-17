#ifndef PCSC_WORKSHOP1_READER_H
#define PCSC_WORKSHOP1_READER_H

#include "CardConnection.h"
#include "Cipher.h"
#include <string>
#include <memory>

// Reader abstraction: each reader model can implement its own read/write APDUs
class Reader {
public:
    explicit Reader(CardConnection& c);
    virtual ~Reader();

    // Non-copyable, movable
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;
    Reader(Reader&&) noexcept;
    Reader& operator=(Reader&&) noexcept;

    // Core primitive that derived classes must implement (4 bytes)
    virtual void writePage(BYTE page, const BYTE* data4) = 0;
    virtual BYTEV readPage(BYTE page) = 0;

    // Encrypted write/read
    void writePageEncrypted(BYTE page, const BYTE* data4, const ICipher& cipher);
    BYTEV readPageDecrypted(BYTE page, const ICipher& cipher);

    // Convenience overloads for common container types
    void writePage(BYTE page, const BYTEV& data);
    void writePage(BYTE page, const std::string& s);

    void writePageEncrypted(BYTE page, const BYTEV& data, const ICipher& cipher);
    void writePageEncrypted(BYTE page, const std::string& s, const ICipher& cipher);

    // convenience: write multi-page data
    virtual void writeData(BYTE startPage, const BYTEV& data);
    void writeData(BYTE startPage, const std::string& s);

    virtual BYTEV readData(BYTE startPage, size_t length);

    // Multi-page encrypted write/read
    void writeDataEncrypted(BYTE startPage, const BYTEV& data, const ICipher& cipher);
    void writeDataEncrypted(BYTE startPage, const std::string& s, const ICipher& cipher);
    BYTEV readDataDecrypted(BYTE startPage, size_t length, const ICipher& cipher);

    // Read all pages until card returns error
    BYTEV readAll(BYTE startPage = 0);

protected:
    struct Impl;
    std::unique_ptr<Impl> pImpl;

    // Derived classes access CardConnection through this
    CardConnection& card();
    const CardConnection& card() const;
};

#endif // PCSC_WORKSHOP1_READER_H
