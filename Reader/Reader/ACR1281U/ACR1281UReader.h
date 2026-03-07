#ifndef PCSC_WORKSHOP1_READER_ACR1281UREADER_H
#define PCSC_WORKSHOP1_READER_ACR1281UREADER_H

#include "../Reader.h"

class ACR1281UReader : public Reader {
public:
	explicit ACR1281UReader(CardConnection& c);
	ACR1281UReader(CardConnection& c, BYTE blockSize);
	~ACR1281UReader() override;

	ACR1281UReader(const ACR1281UReader&) = delete;
	ACR1281UReader& operator=(const ACR1281UReader&) = delete;
	ACR1281UReader(ACR1281UReader&&) noexcept;
	ACR1281UReader& operator=(ACR1281UReader&&) noexcept;

	ReaderType getReaderType() const noexcept override { return ReaderType::ACR1281U; }

protected:
	BYTE mapKeyStructure(KeyStructure structure) const noexcept override {
		switch (structure) {
			case KeyStructure::Volatile:    return 0x00;
			case KeyStructure::NonVolatile: return 0x20;
			default: return 0x00;
		}
	}
	BYTE mapKeyKind(KeyType kind) const noexcept override {
		switch (kind) {
			case KeyType::A: return 0x60;
			case KeyType::B: return 0x61;
			default: return 0x60;
		}
	}
};

#endif // PCSC_WORKSHOP1_READER_ACR1281UREADER_H
