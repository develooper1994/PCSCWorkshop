#ifndef CARDCONNECTION_H
#define CARDCONNECTION_H

#include "PcscUtils.h"
#include <thread>
#include <chrono>
#include "Exceptions/GenericExceptions.h"

// ============================================================
// Kart iletiþim katmaný
// ============================================================

class CardConnection {
public:
    CardConnection(SCARDCONTEXT ctx, const std::wstring& readerName);
    ~CardConnection();

    bool connect();
    bool waitAndConnect(int retryMs = 500, int maxRetries = 0);
    void disconnect();

    bool isConnected() const;
    void checkConnected() const;
    void checkResponseSize(const BYTEV& resp, size_t minSize = 2) const;
    std::pair<BYTE, BYTE> getStatusWords(const BYTEV& resp) const;
    SCARDHANDLE handle() const;
    DWORD protocol() const;

    BYTEV transmit(const BYTEV& cmd) const;
    BYTEV sendCommand(BYTEV cmd, bool followChaining = true) const;

private:
    SCARDCONTEXT m_context;
    std::wstring m_readerName;
    SCARDHANDLE  m_hCard;
    DWORD        m_activeProtocol;
    bool         m_connected;
};

#endif // PCSC_WORKSHOP1_CARDCONNECTION_H