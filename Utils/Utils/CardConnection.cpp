#include "CardConnection.h"
#include <iostream>

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
        std::cerr << "SCardConnect failed: 0x" << std::hex << rc << std::dec << std::endl;
        return false;
    }
    m_connected = true;
    std::wcout << L"Connected to: " << m_readerName << std::endl;
    return true;
}

bool CardConnection::waitAndConnect(int retryMs, int maxRetries) {
    int attempt = 0;
    while (true) {
        if (connect()) return true;
        ++attempt;
        if (maxRetries > 0 && attempt >= maxRetries) return false;
        std::wcout << L"Waiting for card...\n";
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

std::pair<BYTE, BYTE> CardConnection::getStatusWords(const BYTEV& resp) const {
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

    LONG r = SCardTransmit(m_hCard, &pci,
        cmd.data(), static_cast<DWORD>(cmd.size()),
        nullptr, recv, &recvLen);
    if (r != SCARD_S_SUCCESS)
        throw pcsc::ReaderError(std::string("SCardTransmit failed: ") + std::to_string(r));
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
            std::cerr << "Unexpected SW: ";
            printHex(&resp[resp.size() - 2], 2);
            throw pcsc::ReaderError("Card returned error status");
        }
    }

    return full;
}
