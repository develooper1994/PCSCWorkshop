#ifndef PCSC_WORKSHOP1_PCSC_H
#define PCSC_WORKSHOP1_PCSC_H

#include "PcscUtils.h"
#include "StatusWordHandler.h"
#include "Result.h"
#include "Exceptions/GenericExceptions.h"
#include <string>
#include <vector>
#include <thread>
#include <chrono>

// ════════════════════════════════════════════════════════════════════════════════
// PCSC — PC/SC iletişim katmanı (context + reader + transport)
// ════════════════════════════════════════════════════════════════════════════════
//
// Sorumluluklar:
//   1. Context yönetimi   (SCardEstablishContext / SCardReleaseContext)
//   2. Reader keşfetme    (SCardListReaders)
//   3. Kart bağlantısı    (SCardConnect / SCardDisconnect)
//   4. APDU iletimi       (SCardTransmit)
//   5. SW ayrıştırma      (getStatusWords)
//
// Kullanım:
//   PCSC pcsc;
//   pcsc.establishContext();
//   pcsc.chooseReader();
//   pcsc.connectToCard();
//
//   ACR1281UReader reader(pcsc, 16);
//   auto data = reader.readPage(4);
//
// ════════════════════════════════════════════════════════════════════════════════

class PCSC {
public:
    PCSC() = default;
    ~PCSC();

    PCSC(const PCSC&) = delete;
    PCSC& operator=(const PCSC&) = delete;
    PCSC(PCSC&&) noexcept;
    PCSC& operator=(PCSC&&) noexcept;

    // ── 1. Context yönetimi ─────────────────────────────────────────────────
    bool establishContext();
    void releaseContext();
    bool hasContext() const;
    SCARDCONTEXT context() const;

    // ── 2. Reader keşfetme ──────────────────────────────────────────────────
    struct ReaderList {
        std::vector<std::wstring> names;
        bool ok;
    };
    ReaderList listReaders() const;
    bool chooseReader(size_t defaultIndex = 1);
    const std::wstring& readerName() const;

    // ── 3. Kart bağlantısı ──────────────────────────────────────────────────
    bool connectToCard(int retryMs = 500, int maxRetries = 0);
    void disconnect();
    bool isConnected() const;

    // ── 4. Transport ────────────────────────────────────────────────────────
    BYTEV transmit(const BYTEV& cmd) const;
    BYTEV sendCommand(BYTEV cmd, bool followChaining = true) const;

    // ── 4b. Transport — Exception-free ──────────────────────────────────────
	PcscResultByteV tryTransmit(const BYTEV& cmd) const;
	PcscResultByteV trySendCommand(BYTEV cmd, bool followChaining = true) const;

    // ── 5. SW ayrıştırma ────────────────────────────────────────────────────
    StatusWord getStatusWords(const BYTEV& resp) const;
	PcscResultStatusWord tryGetStatusWords(const BYTEV& resp) const;

    // ── Handle erişimi ──────────────────────────────────────────────────────
    SCARDHANDLE handle() const;
    DWORD protocol() const;

    // ── Hazır akış ──────────────────────────────────────────────────────────
    template<typename F>
    int run(F&& callback) { callback(*this); return 0; }

private:
    SCARDCONTEXT hContext_        = 0;
    std::wstring readerName_;
    SCARDHANDLE  hCard_           = 0;
    DWORD        activeProtocol_  = 0;
    bool         connected_       = false;

    bool connectOnce();
    void cleanup();
};

#endif // PCSC_WORKSHOP1_PCSC_H
