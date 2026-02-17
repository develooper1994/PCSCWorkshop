#ifndef PCSC_WORKSHOP1_READER_ACR1281UREADER_H
#define PCSC_WORKSHOP1_READER_ACR1281UREADER_H

#include "Reader.h"

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
    static void testACR1281UReaderUnsecured(ACR1281UReader& acr1281u);
    static void testACR1281UReaderSecured(ACR1281UReader& acr1281u);
};

#endif // PCSC_WORKSHOP1_READER_ACR1281UREADER_H
