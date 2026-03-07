#include "Reader.h"
#include "PcscCommands.h"
#include <sstream>
#include <vector>
#include <cstring>

// ============================================================
// Reader::Impl — minimal: connection + block size
// ============================================================
struct Reader::Impl {
	CardConnection& cardConnection;
	BYTE LE = 0x04;

	explicit Impl(CardConnection& c, BYTE le = 0x04)
		: cardConnection(c), LE(le) {}

	Impl(const Impl&) = delete;
	Impl& operator=(const Impl&) = delete;
	Impl(Impl&& other) noexcept
		: cardConnection(other.cardConnection), LE(other.LE) {}
	Impl& operator=(Impl&&) = delete;
};

// ============================================================
// Construction
// ============================================================
Reader::Reader(CardConnection& c)
	: pImpl(std::make_unique<Impl>(c)) {}
Reader::Reader(CardConnection& c, BYTE blockSize)
	: pImpl(std::make_unique<Impl>(c, blockSize)) {}
Reader::Reader(Reader&&) noexcept = default;
Reader::~Reader() = default;
Reader& Reader::operator=(Reader&&) noexcept = default;

// ============================================================
// Accessors
// ============================================================
CardConnection& Reader::cardConnection() noexcept { return pImpl->cardConnection; }
const CardConnection& Reader::cardConnection() const noexcept { return pImpl->cardConnection; }
BYTE Reader::getLE() const noexcept { return pImpl->LE; }
void Reader::setLE(BYTE le) noexcept { pImpl->LE = le; }

// ============================================================
// padToBlock — validate size and zero-pad to block boundary
// ============================================================
BYTEV Reader::padToBlock(const BYTE* data, size_t dataLen) const {
	if (dataLen > getLE()) {
		std::stringstream ss;
		ss << "Data size (" << dataLen << " bytes) exceeds block size ("
		   << static_cast<int>(getLE()) << " bytes)";
		throw std::invalid_argument(ss.str());
	}
	BYTEV tmp(getLE(), 0x00);
	std::memcpy(tmp.data(), data, dataLen);
	return tmp;
}

// ============================================================
// Exception-free core — tryXxx metotları
// ============================================================
Result<ReaderResponse> Reader::tryTransmit(const BYTEV& apdu) {
	if (!cardConnection().isConnected())
		return {ReaderResponse{}, {ErrorCode::NotConnected}};

	auto raw = cardConnection().transmit(apdu);

	if (raw.size() < 2)
		return {ReaderResponse{}, {ErrorCode::ResponseTooShort}};

	auto sw = cardConnection().getStatusWords(raw);
	BYTEV data(raw.begin(), raw.end() - 2);
	return {ReaderResponse{std::move(data), sw}, PcscError{}};
}

Result<BYTEV> Reader::tryReadPage(BYTE page, const BYTEV* customApdu) {
	auto apdu = customApdu ? *customApdu
						   : PcscCommands::readBinary(page, getLE());
	auto result = tryTransmit(apdu);
	if (!result) return {BYTEV{}, result.error};

	auto err = PcscCommands::evaluateRead(result.value.sw);
	if (!err.ok()) return {BYTEV{}, err};

	return {std::move(result.value.data), PcscError{}};
}

Result<void> Reader::tryWritePage(BYTE page, const BYTE* data, const BYTEV* customApdu) {
	auto apdu = customApdu ? *customApdu
						   : PcscCommands::updateBinary(page, data, getLE());
	auto result = tryTransmit(apdu);
	if (!result) return {result.error};

	return {PcscCommands::evaluateWrite(result.value.sw)};
}

Result<void> Reader::tryClearPage(BYTE page) {
	BYTEV zeros(getLE(), 0x00);
	return tryWritePage(page, zeros.data());
}

Result<void> Reader::tryLoadKey(const BYTE* key, KeyStructure ks, BYTE keyNumber) {
	auto apdu = PcscCommands::loadKey(mapKeyStructure(ks), keyNumber, key);
	auto result = tryTransmit(apdu);
	if (!result) return {result.error};

	return {PcscCommands::evaluateLoadKey(result.value.sw)};
}

Result<void> Reader::tryAuth(BYTE blockNumber, KeyType keyType, BYTE keyNumber) {
	auto apdu = PcscCommands::authLegacy(blockNumber, mapKeyKind(keyType), keyNumber);
	auto result = tryTransmit(apdu);
	if (!result) return {result.error};

	return {PcscCommands::evaluateAuth(result.value.sw)};
}

Result<void> Reader::tryAuthNew(BYTE blockNumber, KeyType keyType, BYTE keyNumber) {
	BYTE ktVal = mapKeyKind(keyType);
	BYTE data5[5] = {0x01, 0x00, blockNumber, ktVal, keyNumber};
	return tryAuthNew(data5);
}

Result<void> Reader::tryAuthNew(const BYTE data[5]) {
	auto apdu = PcscCommands::authGeneral(data);
	auto result = tryTransmit(apdu);
	if (!result) return {result.error};

	return {PcscCommands::evaluateAuth(result.value.sw)};
}

// ============================================================
// Throwing API — tryXxx + unwrap()
// ============================================================
ReaderResponse Reader::transmit(const BYTEV& apdu) {
	return tryTransmit(apdu).unwrap();
}

BYTEV Reader::readPage(BYTE page, const BYTEV* customApdu) {
	return tryReadPage(page, customApdu).unwrap();
}

void Reader::writePage(BYTE page, const BYTE* data, const BYTEV* customApdu) {
	tryWritePage(page, data, customApdu).unwrap();
}

void Reader::clearPage(BYTE page) {
	tryClearPage(page).unwrap();
}

void Reader::loadKey(const BYTE* key, KeyStructure ks, BYTE keyNumber) {
	tryLoadKey(key, ks, keyNumber).unwrap();
}

void Reader::auth(BYTE blockNumber, KeyType keyType, BYTE keyNumber) {
	tryAuth(blockNumber, keyType, keyNumber).unwrap();
}

void Reader::authNew(BYTE blockNumber, KeyType keyType, BYTE keyNumber) {
	tryAuthNew(blockNumber, keyType, keyNumber).unwrap();
}

void Reader::authNew(const BYTE data[5]) {
	tryAuthNew(data).unwrap();
}

void Reader::writePage(BYTE page, const BYTEV& data) {
	auto tmp = padToBlock(data.data(), data.size());
	writePage(page, tmp.data());
}

void Reader::writePage(BYTE page, const std::string& s) {
	auto tmp = padToBlock(reinterpret_cast<const BYTE*>(s.data()), s.size());
	writePage(page, tmp.data());
}

void Reader::writePage(BYTE page, const BYTE* data, BYTE len) {
	LEGuard guard(*this, len);
	writePage(page, data);
}

BYTEV Reader::readPage(BYTE page, BYTE len) {
	LEGuard guard(*this, len);
	return readPage(page);
}

// ============================================================
// Multi-page I/O
// ============================================================
void Reader::writeData(BYTE startPage, const BYTEV& data) {
	size_t total = data.size();
	size_t pages = (total + getLE() - 1) / getLE();
	for (size_t i = 0; i < pages; ++i) {
		BYTE page = static_cast<BYTE>(startPage + i);
		BYTEV chunk(getLE(), 0x00);
		for (size_t b = 0; b < getLE(); ++b) {
			size_t idx = i * getLE() + b;
			if (idx < total) chunk[b] = data[idx];
		}
		writePage(page, chunk);
	}
}

void Reader::writeData(BYTE startPage, const std::string& s) {
	writeData(startPage, BYTEV(s.begin(), s.end()));
}

void Reader::writeData(BYTE startPage, const BYTEV& data, BYTE le) {
	LEGuard guard(*this, le);
	writeData(startPage, data);
}

void Reader::writeData(BYTE startPage, const std::string& s, BYTE le) {
	writeData(startPage, BYTEV(s.begin(), s.end()), le);
}

BYTEV Reader::readData(BYTE startPage, size_t length) {
	BYTEV out;
	size_t remaining = length;
	BYTE page = startPage;
	while (remaining > 0) {
		auto p = readPage(page);
		if (!p.empty()) out.insert(out.end(), p.begin(), p.end());
		remaining = (remaining > getLE()) ? (remaining - getLE()) : 0;
		++page;
	}
	out.resize(length);
	return out;
}

BYTEV Reader::readAll(BYTE startPage) {
	BYTEV out;
	BYTE page = startPage;
	while (true) {
		auto result = tryReadPage(page);
		if (!result || result.value.empty()) break;
		out.insert(out.end(), result.value.begin(), result.value.end());
		++page;
	}
	return out;
}
