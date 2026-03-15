#include "PCSC.h"
#include "Log/Log.h"
#include <iostream>
#include <utility>
#include <iomanip>

// ============================================================
// SCARD hata mesajları
// ============================================================
namespace {
	std::string getSCardErrorMessage(LONG errorCode) {
		switch (errorCode) {
			case SCARD_E_NO_READERS_AVAILABLE: return "No smart card readers found.";
			case SCARD_E_NO_SMARTCARD:         return "No smart card detected.";
			case SCARD_W_REMOVED_CARD:         return "Smart card removed.";
			case SCARD_E_READER_UNAVAILABLE:   return "Reader unavailable.";
			case SCARD_E_SHARING_VIOLATION:    return "Reader in use by another app.";
			case SCARD_E_CARD_UNSUPPORTED:     return "Card not supported.";
			case SCARD_E_INVALID_HANDLE:       return "Invalid handle.";
			case SCARD_E_UNKNOWN_CARD:         return "Unknown card type.";
			case SCARD_E_PROTO_MISMATCH:       return "Protocol mismatch.";
			case SCARD_E_NOT_READY:            return "Reader not ready.";
			case SCARD_E_CANCELLED:            return "Operation cancelled.";
			case SCARD_E_TIMEOUT:              return "Operation timed out.";
			case SCARD_W_UNPOWERED_CARD:       return "Card not powered.";
			case SCARD_W_UNRESPONSIVE_CARD:    return "Card not responding.";
			case SCARD_E_SERVICE_STOPPED:      return "Smart Card service stopped.";
			default: {
				std::ostringstream oss;
				oss << "Smart card error: 0x" << std::hex << std::uppercase << errorCode;
				return oss.str();
			}
		}
	}
}

// ============================================================
// Lifecycle
// ============================================================

PCSC::~PCSC() { cleanup(); }

PCSC::PCSC(PCSC&& o) noexcept
	: hContext_(o.hContext_), readerName_(std::move(o.readerName_)),
	  hCard_(o.hCard_), activeProtocol_(o.activeProtocol_), connected_(o.connected_)
{
	o.hContext_ = 0; o.hCard_ = 0; o.connected_ = false;
}

PCSC& PCSC::operator=(PCSC&& o) noexcept {
	if (this != &o) {
		cleanup();
		hContext_        = o.hContext_;
		readerName_      = std::move(o.readerName_);
		hCard_           = o.hCard_;
		activeProtocol_  = o.activeProtocol_;
		connected_       = o.connected_;
		o.hContext_ = 0; o.hCard_ = 0; o.connected_ = false;
	}
	return *this;
}

void PCSC::cleanup() {
	disconnect();
	releaseContext();
}

// ============================================================
// 1. Context yönetimi
// ============================================================

bool PCSC::establishContext() {
	if (hContext_) return true;
	LONG rc = SCardEstablishContext(SCARD_SCOPE_USER, nullptr, nullptr, &hContext_);
	if (rc != SCARD_S_SUCCESS) {
		std::cerr << "SCardEstablishContext: " << getSCardErrorMessage(rc) << std::endl;
		hContext_ = 0;
		return false;
	}
	return true;
}

void PCSC::releaseContext() {
	if (hContext_) { SCardReleaseContext(hContext_); hContext_ = 0; }
}

bool PCSC::hasContext() const { return hContext_ != 0; }
SCARDCONTEXT PCSC::context() const { return hContext_; }

// ============================================================
// 2. Reader keşfetme
// ============================================================

PCSC::ReaderList PCSC::listReaders() const {
	ReaderList result{{}, false};
	if (!hContext_) { std::cerr << "No valid context.\n"; return result; }

#ifdef _WIN32
	LPWSTR rawBuffer = nullptr;
	DWORD readersLen = SCARD_AUTOALLOCATE;
	LONG rc = SCardListReadersW(hContext_, nullptr,
								reinterpret_cast<LPWSTR>(&rawBuffer), &readersLen);
	if (rc != SCARD_S_SUCCESS) {
		std::cerr << "SCardListReaders: " << getSCardErrorMessage(rc) << std::endl;
		return result;
	}

	wchar_t* p = rawBuffer;
	while (p && *p != L'\0') {
		result.names.emplace_back(p);
		std::wcout << L"[" << result.names.size() - 1 << L"] " << p << std::endl;
		p += wcslen(p) + 1;
	}
	if (rawBuffer) SCardFreeMemory(hContext_, rawBuffer);
#else
	// Linux/macOS: pcsclite — narrow string API
	DWORD readersLen = 0;
	LONG rc = SCardListReaders(hContext_, nullptr, nullptr, &readersLen);
	if (rc != SCARD_S_SUCCESS) {
		std::cerr << "SCardListReaders: " << getSCardErrorMessage(rc) << std::endl;
		return result;
	}
	std::vector<char> buf(readersLen);
	rc = SCardListReaders(hContext_, nullptr, buf.data(), &readersLen);
	if (rc != SCARD_S_SUCCESS) {
		std::cerr << "SCardListReaders: " << getSCardErrorMessage(rc) << std::endl;
		return result;
	}
	char* p = buf.data();
	while (p && *p != '\0') {
		std::string narrow(p);
		result.names.emplace_back(narrow.begin(), narrow.end());
		std::cout << "[" << result.names.size() - 1 << "] " << narrow << std::endl;
		p += narrow.size() + 1;
	}
#endif

	if (result.names.empty()) { std::cerr << "No readers found.\n"; return result; }
	result.ok = true;
	return result;
}

bool PCSC::chooseReader(size_t defaultIndex) {
	auto readers = listReaders();
	if (!readers.ok) return false;
	if (readers.names.empty()) return false;
	if (defaultIndex >= readers.names.size()) defaultIndex = 0;

	size_t selected = defaultIndex;
	while (true) {
		std::wcout << L"Select reader (0-" << (readers.names.size() - 1)
				   << L", default " << defaultIndex << L"): ";
		std::wstring input;
		std::getline(std::wcin, input);
		if (input.empty()) { selected = defaultIndex; break; }
		try {
			selected = std::stoul(input);
			if (selected < readers.names.size()) break;
			std::wcout << L"Invalid index.\n";
		}
		catch (const std::exception&) { std::wcout << L"Invalid input.\n"; }
	}
	readerName_ = readers.names[selected];
	return true;
}

const std::wstring& PCSC::readerName() const { return readerName_; }

// ============================================================
// 3. Kart bağlantısı
// ============================================================

bool PCSC::connectOnce() {
	std::string narrowName(readerName_.begin(), readerName_.end());
#ifdef _WIN32
	LONG rc = SCardConnect(hContext_, readerName_.c_str(),
						   SCARD_SHARE_SHARED,
						   SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
						   &hCard_, &activeProtocol_);
#else
	// pcsclite uses narrow strings
	LONG rc = SCardConnect(hContext_, narrowName.c_str(),
						   SCARD_SHARE_SHARED,
						   SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
						   &hCard_, &activeProtocol_);
#endif
	if (rc != SCARD_S_SUCCESS) {
		LOG_CONN_ERROR("SCardConnect: " + getSCardErrorMessage(rc));
		return false;
	}
	connected_ = true;
	LOG_CONN_INFO("Connected to: " + narrowName);
	return true;
}

bool PCSC::connectToCard(int retryMs, int maxRetries) {
	if (!hContext_ || readerName_.empty()) {
		std::cerr << "Context or reader not ready.\n";
		return false;
	}
	int attempt = 0;
	while (true) {
		if (connectOnce()) return true;
		++attempt;
		if (maxRetries > 0 && attempt >= maxRetries) return false;
		LOG_CONN_DEBUG("Waiting for card...");
		std::this_thread::sleep_for(std::chrono::milliseconds(retryMs));
	}
}

void PCSC::disconnect() {
	if (connected_) {
		SCardDisconnect(hCard_, SCARD_UNPOWER_CARD);
		hCard_ = 0;
		connected_ = false;
	}
}

bool PCSC::isConnected() const { return connected_; }
SCARDHANDLE PCSC::handle() const { return hCard_; }
DWORD PCSC::protocol() const { return activeProtocol_; }

// ============================================================
// 4. Transport
// ============================================================

BYTEV PCSC::transmit(const BYTEV& cmd) const {
	return tryTransmit(cmd).unwrap();
}

PcscResultByteV PCSC::tryTransmit(const BYTEV& cmd) const {
	BYTE recv[512];
	DWORD recvLen = sizeof(recv);
	SCARD_IO_REQUEST pci = (activeProtocol_ == SCARD_PROTOCOL_T0)
		? *SCARD_PCI_T0 : *SCARD_PCI_T1;

	{
		std::ostringstream oss;
		oss << "APDU send: ";
		for (auto b : cmd) oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (int)b << ' ';
		LOG_PCSC_DEBUG(oss.str());
	}

	LONG r = SCardTransmit(hCard_, &pci,
		cmd.data(), static_cast<DWORD>(cmd.size()),
		nullptr, recv, &recvLen);

	if (r != SCARD_S_SUCCESS) {
		std::string msg = "SCardTransmit: " + getSCardErrorMessage(r);
		LOG_PCSC_ERROR(msg);
		return PcscResultByteV::Err(PcscError::make(ConnectionError::NotConnected, msg));
	}

	{
		std::ostringstream oss;
		oss << "APDU recv(" << recvLen << "): ";
		for (DWORD i = 0; i < recvLen; ++i) oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (int)recv[i] << ' ';
		LOG_PCSC_DEBUG(oss.str());
	}

	return PcscResultByteV::Ok(BYTEV(recv, recv + recvLen));
}

BYTEV PCSC::sendCommand(BYTEV cmd, bool followChaining) const {
	return trySendCommand(std::move(cmd), followChaining).unwrap();
}

PcscResultByteV PCSC::trySendCommand(BYTEV cmd, bool followChaining) const {
	BYTEV full;
	while (true) {
		auto txResult = tryTransmit(cmd);
		if (!txResult) return txResult;
		BYTEV resp = std::move(txResult.unwrap());
		auto swResult = tryGetStatusWords(resp);
		if (!swResult) swResult.unwrap_error();
		auto sw = swResult.unwrap();

		if (resp.size() > 2)
			full.insert(full.end(), resp.begin(), resp.end() - 2);

		if (sw.sw2 == 0xAF && (sw.sw1 == 0x91 || sw.sw1 == 0x90)) {
			if (!followChaining) break;
			cmd = { 0x90, 0xAF, 0x00, 0x00, 0x00 };
			continue;
		}
		else if (sw.sw2 == 0x00 && (sw.sw1 == 0x91 || sw.sw1 == 0x90)) break;
		else {
			LOG_PCSC_ERROR("Unexpected SW: " + sw.toHexFormatted());
			return PcscResultByteV::Err(Error<PcscError>(ConnectionError::Unknown)
			                                .detail("Card returned error: " + sw.toHexFormatted())
			                                .meta("sw", sw));
		}
	}
	return PcscResultByteV::Ok(std::move(full));
}


// ============================================================
// 5. SW ayrıştırma
// ============================================================

StatusWord PCSC::getStatusWords(const BYTEV& resp) const {
	return tryGetStatusWords(resp).unwrap();
}

PcscResultStatusWord PCSC::tryGetStatusWords(const BYTEV& resp) const {
	return resp.size() < 2 ?
		PcscResultStatusWord{Error<PcscError>(ConnectionError::ResponseTooShort)} :
		PcscResultStatusWord{StatusWord(resp[resp.size() - 2], resp[resp.size() - 1])};
}
