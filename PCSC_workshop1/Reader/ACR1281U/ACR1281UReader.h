#ifndef PCSC_WORKSHOP1_READER_ACR1281UREADER_H
#define PCSC_WORKSHOP1_READER_ACR1281UREADER_H

#include "../Reader.h"

// ACR1281U specific reader declaration (implementation in separate file)
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

private:
    static void testACR1281UReaderSecured(ACR1281UReader& acr1281u); // Unified secured runner that invokes the per-cipher tests
    static void testACR1281UReaderUnsecured(ACR1281UReader& acr1281u);

    // Per-cipher test routines. The old secured method is split into per-cipher
    // testers; the unified runner is still named testACR1281UReaderSecured.
    static void testACR1281UReaderXorCipher(ACR1281UReader& acr1281u);
    static void testACR1281UReaderCaesarCipher(ACR1281UReader& acr1281u);
#ifdef _WIN32
    static void testACR1281UReaderCngAES(ACR1281UReader& acr1281u);
    static void testACR1281UReaderCng3DES(ACR1281UReader& acr1281u);
    static void testACR1281UReaderCngAESGcm(ACR1281UReader& acr1281u);
#endif
};

#endif // PCSC_WORKSHOP1_READER_ACR1281UREADER_H
