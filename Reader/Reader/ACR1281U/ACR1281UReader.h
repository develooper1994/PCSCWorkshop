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
};

#endif // PCSC_WORKSHOP1_READER_ACR1281UREADER_H
