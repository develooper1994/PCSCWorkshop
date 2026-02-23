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

    void writePage(BYTE page, const BYTE* data4, BYTE LC = 0x04) override;
    BYTEV readPage(BYTE page, BYTE LC = 0x04) override;

protected:
    BYTE mapKeyStructure(KeyStructure structure) const override {
        switch (structure) {
            case KeyStructure::Volatile: return 0x00;
            case KeyStructure::NonVolatile: return 0x20;
        }
        throw std::runtime_error("Invalid key kind");
    }
    BYTE mapKeyKind(KeyType kind) const override {
        switch (kind) {
            case KeyType::A: return 0x60;
            case KeyType::B: return 0x61;
        }
        throw std::runtime_error("Invalid key kind");
    }
};

// Specialize the nested template Reader::KeyMapping for ACR1281UReader at namespace scope
template<>
struct ACR1281UReader::KeyMapping<ACR1281UReader>
{
    static constexpr BYTE map(KeyStructure s) {
        return (s == KeyStructure::Volatile) ? 0x00 : 0x20;
    }

    static constexpr BYTE map(KeyType t) {
        return (t == KeyType::A) ? 0x60 : 0x61;
    }
};

#endif // PCSC_WORKSHOP1_READER_ACR1281UREADER_H
