#include "Reader.h"
#include "PcscCommands.h"
#include <sstream>
#include <vector>
#include <cstring>

// ============================================================
// Reader::Impl — minimal: connection + block size
// ============================================================
struct Reader::Impl {
	PCSC& pcsc;
	BYTE LE = 0x04;

	explicit Impl(PCSC& p, BYTE le = 0x04)
		: pcsc(p), LE(le) {}

	Impl(const Impl&) = delete;
	Impl& operator=(const Impl&) = delete;
	Impl(Impl&& other) noexcept
		: pcsc(other.pcsc), LE(other.LE) {}
	Impl& operator=(Impl&&) = delete;
};

// ============================================================
// Construction
// ============================================================
Reader::Reader(PCSC& p)
	: pImpl(std::make_unique<Impl>(p)) {}
Reader::Reader(PCSC& p, BYTE blockSize)
	: pImpl(std::make_unique<Impl>(p, blockSize)) {}
Reader::Reader(Reader&&) noexcept = default;
Reader::~Reader() = default;
Reader& Reader::operator=(Reader&&) noexcept = default;

// ============================================================
// Accessors
// ============================================================
PCSC& Reader::pcsc() noexcept { return pImpl->pcsc; }
const PCSC& Reader::pcsc() const noexcept { return pImpl->pcsc; }
BYTE Reader::getLE() const noexcept { return pImpl->LE; }
void Reader::setLE(BYTE le) noexcept { pImpl->LE = le; }

// ============================================================
// padToBlock — validate size and zero-pad to block boundary
// ============================================================
BYTEV Reader::padToBlock(const BYTE* data, size_t dataLen) const {
	if (dataLen > getLE()) {
		std::string detail = "Data size (" + std::to_string(dataLen)
			+ " bytes) exceeds block size ("
			+ std::to_string(static_cast<int>(getLE())) + " bytes)";
		PcscError::make(CardError::InvalidData, std::move(detail)).throwIfError();
		return {};
	}
	BYTEV tmp(getLE(), 0x00);
	std::memcpy(tmp.data(), data, dataLen);
	return tmp;
}

// ============================================================
// Exception-free core — tryXxx metotları
// ============================================================
Result<ReaderResponse, PcscError> Reader::tryTransmit(const BYTEV& apdu) {
	if (!pcsc().isConnected())
		return Result<ReaderResponse, PcscError>::Err(PcscError::from(ConnectionError::NotConnected, {}, {}));

	auto raw = pcsc().transmit(apdu);

	if (raw.size() < 2)
		return Result<ReaderResponse, PcscError>::Err(PcscError::from(ConnectionError::ResponseTooShort, {}, {}));

	auto sw = pcsc().getStatusWords(raw);
	BYTEV data(raw.begin(), raw.end() - 2);
	return Result<ReaderResponse, PcscError>::Ok(ReaderResponse{std::move(data), sw});
}

PcscResultByteV Reader::tryReadPage(BYTE page, const BYTEV* customApdu)
{
	auto apdu = customApdu ? *customApdu
						   : PcscCommands::readBinary(page, getLE());
	auto result = tryTransmit(apdu);
	if (!result) return PcscResultByteV::Err(std::move(result.error()));

	auto err = PcscCommands::evaluateRead(result.unwrap().sw);
	if (!err.is_ok()) return PcscResultByteV::Err(std::move(err.error()));

	return PcscResultByteV::Ok(std::move(result.unwrap().data));
}

PcscResultVoid Reader::tryWritePage(BYTE page, const BYTE* data, const BYTEV* customApdu)
{
	auto apdu = customApdu ? *customApdu
						   : PcscCommands::updateBinary(page, data, getLE());
	auto result = tryTransmit(apdu);
	if (!result) return PcscResultVoid::Err(std::move(result.error()));

	auto writeResult = PcscCommands::evaluateWrite(result.unwrap().sw);
	if (!writeResult) return PcscResultVoid::Err(std::move(writeResult.error()));

	return PcscResultVoid::Ok();
}

PcscResultVoid Reader::tryClearPage(BYTE page)
{
	BYTEV zeros(getLE(), 0x00);
	return tryWritePage(page, zeros.data());
}

PcscResultVoid Reader::tryLoadKey(const BYTE* key, KeyStructure ks, BYTE keyNumber)
{
	auto apdu = PcscCommands::loadKey(mapKeyStructure(ks), keyNumber, key);
	auto tx = tryTransmit(apdu);
	if (!tx) return PcscResultVoid::Err(std::move(tx.error()));

	return PcscCommands::evaluateLoadKey(tx.unwrap().sw);
}

PcscResultVoid Reader::tryAuth(BYTE blockNumber, KeyType keyType, BYTE keyNumber)
{
	auto apdu = PcscCommands::authLegacy(blockNumber, mapKeyKind(keyType), keyNumber);
	auto result = tryTransmit(apdu);
	if (!result) return PcscResultVoid::Err(std::move(result.error()));

	return PcscCommands::evaluateAuth(result.unwrap().sw);
}

PcscResultVoid Reader::tryAuthNew(BYTE blockNumber, KeyType keyType, BYTE keyNumber)
{
	BYTE ktVal = mapKeyKind(keyType);
	BYTE data5[5] = {0x01, 0x00, blockNumber, ktVal, keyNumber};
	return tryAuthNew(data5);
}

PcscResultVoid Reader::tryAuthNew(const BYTE data[5])
{
	auto apdu = PcscCommands::authGeneral(data);
	auto result = tryTransmit(apdu);
	if (!result) return PcscResultVoid::Err(std::move(result.error()));

	return PcscCommands::evaluateAuth(result.unwrap().sw);
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
		if (!result || result.unwrap().empty()) break;
		out.insert(out.end(), result.unwrap().begin(), result.unwrap().end());
		++page;
	}
	return out;
}
