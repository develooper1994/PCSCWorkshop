#pragma once
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

    // Encrypted write/read — cipher nesnesi üzerinden þifreler/çözer
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

protected:
    struct Impl;
    std::unique_ptr<Impl> pImpl;

    // Derived classes access CardConnection through this
    CardConnection& card();
    const CardConnection& card() const;
};

// ACR1281U specific reader
class ACR1281UReader : public Reader {
public:
    explicit ACR1281UReader(CardConnection& c);
    ~ACR1281UReader() override;

    // Non-copyable, movable
    ACR1281UReader(const ACR1281UReader&) = delete;
    ACR1281UReader& operator=(const ACR1281UReader&) = delete;
    ACR1281UReader(ACR1281UReader&&) noexcept;
    ACR1281UReader& operator=(ACR1281UReader&&) noexcept;

    void writePage(BYTE page, const BYTE* data4) override;
    BYTEV readPage(BYTE page) override;

    /********************  TEST ********************/
    static void testACR1281UReader(CardConnection& card);
    static void testACR1281UReaderUnsecured(ACR1281UReader& acr1281u);
    static void testACR1281UReaderSecured(ACR1281UReader& acr1281u);

private:
    struct Impl;
    std::unique_ptr<Impl> pImplACR;
};
