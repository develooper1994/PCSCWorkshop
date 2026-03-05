#include "CardConnection.h"
#include "PcscUtils.h"
#include "Exceptions/GenericExceptions.h"
#include "Log/Log.h"
#include <iostream>

// Helper function to convert SCARD error codes to user-friendly messages
namespace {
	std::string getSCardErrorMessage(LONG errorCode) {
		switch (errorCode) {
			case SCARD_E_NO_READERS_AVAILABLE:
				return "No smart card readers found. Please connect a card reader.";
			case SCARD_E_NO_SMARTCARD:
				return "No smart card detected. Please insert a card into the reader.";
			case SCARD_W_REMOVED_CARD:
				return "Smart card has been removed from the reader.";
			case SCARD_E_READER_UNAVAILABLE:
				return "Card reader is unavailable or disconnected.";
			case SCARD_E_SHARING_VIOLATION:
				return "Card reader is being used by another application.";
			case SCARD_E_CARD_UNSUPPORTED:
				return "The smart card is not supported.";
			case SCARD_E_INVALID_HANDLE:
				return "Invalid handle or context.";
			case SCARD_E_INSUFFICIENT_BUFFER:
				return "Buffer is too small for the requested operation.";
			case SCARD_E_UNKNOWN_CARD:
				return "Unknown card type.";
			case SCARD_E_PROTO_MISMATCH:
				return "Protocol mismatch between card and reader.";
			case SCARD_E_NOT_READY:
				return "Card reader is not ready.";
			case SCARD_E_CANCELLED:
				return "Operation was cancelled.";
			case SCARD_E_TIMEOUT:
				return "Operation timed out.";
			case SCARD_W_UNPOWERED_CARD:
				return "Card is not powered.";
			case SCARD_W_UNRESPONSIVE_CARD:
				return "Card is not responding.";
			case SCARD_W_UNSUPPORTED_CARD:
				return "Card is not supported.";
			case SCARD_E_INVALID_PARAMETER:
				return "Invalid parameter provided.";
			case SCARD_E_INVALID_VALUE:
				return "Invalid value provided.";
			case SCARD_E_SERVICE_STOPPED:
				return "Smart Card service has stopped.";
			default: {
				std::ostringstream oss;
				oss << "Smart card error: 0x" << std::hex << std::uppercase << errorCode;
				return oss.str();
			}
		}
	}
}

CardConnection::CardConnection(SCARDCONTEXT ctx, const std::wstring& readerName)
	: m_context(ctx), m_readerName(readerName), m_hCard(0), m_activeProtocol(0), m_connected(false) {}

CardConnection::~CardConnection() {
	disconnect();
}

bool CardConnection::connect() {
	LONG rc = SCardConnect(
		m_context,
		m_readerName.c_str(),
		SCARD_SHARE_SHARED,
		SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
		&m_hCard,
		&m_activeProtocol);

	if (rc != SCARD_S_SUCCESS) {
		LOG_CONN_ERROR("SCardConnect failed: " + getSCardErrorMessage(rc));
		return false;
	}
	m_connected = true;
	
	// wide string'i normal string'e çevir (güvenli)
	std::wstring wideReaderName = m_readerName;
	std::string narrowReaderName(wideReaderName.begin(), wideReaderName.end());
	LOG_CONN_INFO("Connected to: " + narrowReaderName);
	
	return true;
}

bool CardConnection::waitAndConnect(int retryMs, int maxRetries) {
	int attempt = 0;
	while (true) {
		if (connect()) return true;
		++attempt;
		if (maxRetries > 0 && attempt >= maxRetries) return false;
		LOG_CONN_DEBUG("Waiting for card...");
		std::this_thread::sleep_for(std::chrono::milliseconds(retryMs));
	}
}

void CardConnection::disconnect() {
	if (m_connected) {
		SCardDisconnect(m_hCard, SCARD_UNPOWER_CARD);
		m_connected = false;
	}
}

bool CardConnection::isConnected() const { return m_connected; }

void CardConnection::checkConnected() const { if (!m_connected) throw pcsc::ReaderError("Card not connected"); }

void CardConnection::checkResponseSize(const BYTEV& resp, size_t minSize) const {
	if (resp.size() < minSize)
		throw pcsc::ReaderError("Response too short: expected at least " + std::to_string(minSize) + " bytes");
}

STATUS CardConnection::getStatusWords(const BYTEV& resp) const {
	checkResponseSize(resp, 2);
	return { resp[resp.size() - 2], resp[resp.size() - 1] };
}

SCARDHANDLE CardConnection::handle() const { return m_hCard; }

DWORD CardConnection::protocol() const { return m_activeProtocol; }

BYTEV CardConnection::transmit(const BYTEV& cmd) const {
	BYTE recv[512];
	DWORD recvLen = sizeof(recv);
	SCARD_IO_REQUEST pci = (m_activeProtocol == SCARD_PROTOCOL_T0)
		? *SCARD_PCI_T0
		: *SCARD_PCI_T1;

	// Debug: hex yazdır
	{
		std::ostringstream oss;
		oss << "APDU send: ";
		for (auto b : cmd) oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (int)b << ' ';
		LOG_PCSC_DEBUG(oss.str());
	}

	LONG r = SCardTransmit(m_hCard, &pci,
		cmd.data(), static_cast<DWORD>(cmd.size()),
		nullptr, recv, &recvLen);

	if (r != SCARD_S_SUCCESS) {
		std::string errorMsg = "SCardTransmit failed: " + getSCardErrorMessage(r);
		LOG_PCSC_ERROR(errorMsg);
		throw pcsc::ReaderError(errorMsg);
	}

	// Debug: alınan byte'ları yazdır
	{
		std::ostringstream oss;
		oss << "APDU recvLen=" << recvLen << " recv: ";
		for (DWORD i = 0; i < recvLen; ++i) oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (int)recv[i] << ' ';
		LOG_PCSC_DEBUG(oss.str());
	}

	return BYTEV(recv, recv + recvLen);
}

BYTEV CardConnection::sendCommand(BYTEV cmd, bool followChaining) const {
	BYTEV full;

	while (true) {
		BYTEV resp = transmit(cmd);
		auto sw = getStatusWords(resp);
		BYTE sw1 = sw.first, sw2 = sw.second;

		if (resp.size() > 2)
			full.insert(full.end(), resp.begin(), resp.end() - 2);

		if (sw2 == 0xAF && (sw1 == 0x91 || sw1 == 0x90)) {
			if (!followChaining) break;
			cmd = { 0x90, 0xAF, 0x00, 0x00, 0x00 };
			continue;
		}
		else if (sw2 == 0x00 && (sw1 == 0x91 || sw1 == 0x90)) break;
		else {
			std::ostringstream oss;
			oss << "Unexpected SW: ";
			for (size_t i = resp.size() - 2; i < resp.size(); ++i) {
				oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (int)resp[i] << " ";
			}
			LOG_PCSC_ERROR(oss.str());
			throw pcsc::ReaderError("Card returned error status");
		}
	}

	return full;
}
