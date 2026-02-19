#ifndef PCSC_WORKSHOP1_CARDCONNECTION_H
#define PCSC_WORKSHOP1_CARDCONNECTION_H

#include "PcscUtils.h"
#include <thread>
#include <chrono>
#include "Exceptions.h"

// ============================================================
// Kart iletiþim katmaný
// ============================================================

class CardConnection {
public:
    CardConnection(SCARDCONTEXT ctx, const std::wstring& readerName)
        : m_context(ctx), m_readerName(readerName),
          m_hCard(0), m_activeProtocol(0), m_connected(false)
    {}

    ~CardConnection() {
        disconnect();
    }

    bool connect() {
        LONG rc = SCardConnect(
            m_context,
            m_readerName.c_str(),
            SCARD_SHARE_SHARED,
            SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
            &m_hCard,
            &m_activeProtocol);

        if (rc != SCARD_S_SUCCESS) {
            std::cerr << "SCardConnect failed: 0x"
                      << std::hex << rc << std::dec << std::endl;
            return false;
        }
        m_connected = true;
        std::wcout << L"Connected to: " << m_readerName << std::endl;
        return true;
    }

    bool waitAndConnect(int retryMs = 500, int maxRetries = 0) {
        int attempt = 0;
        while (true) {
            if (connect()) return true;
            ++attempt;
            if (maxRetries > 0 && attempt >= maxRetries) return false;
            std::wcout << L"Waiting for card...\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(retryMs));
        }
    }

    void disconnect() {
        if (m_connected) {
            SCardDisconnect(m_hCard, SCARD_UNPOWER_CARD);
            m_connected = false;
        }
    }

    bool isConnected() const { return m_connected; }
    SCARDHANDLE handle() const { return m_hCard; }
    DWORD protocol() const { return m_activeProtocol; }

    BYTEV transmit(const BYTEV& cmd) const {
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

    BYTEV sendCommand(BYTEV cmd, bool followChaining = true) const {
        BYTEV full;

        while (true) {
            auto resp = transmit(cmd);
            if (resp.size() < 2)
                throw pcsc::ReaderError("Invalid response length");

            BYTE sw1 = resp[resp.size() - 2];
            BYTE sw2 = resp[resp.size() - 1];

            if (resp.size() > 2)
                full.insert(full.end(), resp.begin(), resp.end() - 2);

            if (sw2 == 0xAF && (sw1 == 0x91 || sw1 == 0x90)) {
                if (!followChaining) break;
                cmd = { 0x90, 0xAF, 0x00, 0x00, 0x00 };
                continue;
            }
            else if (sw2 == 0x00 && (sw1 == 0x91 || sw1 == 0x90)) {
                break;
            }
            else {
                std::cerr << "Unexpected SW: ";
                printHex(&resp[resp.size() - 2], 2);
                throw pcsc::ReaderError("Card returned error status");
            }
        }

        return full;
    }

private:
    SCARDCONTEXT m_context;
    std::wstring m_readerName;
    SCARDHANDLE  m_hCard;
    DWORD        m_activeProtocol;
    bool         m_connected;
};

#endif // PCSC_WORKSHOP1_CARDCONNECTION_H
