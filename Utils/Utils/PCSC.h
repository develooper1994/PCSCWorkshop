#ifndef PCSC_WORKSHOP1_PCSC_H
#define PCSC_WORKSHOP1_PCSC_H

#include "DESFire.h"
#include "Readers.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>

// ============================================================
// PCSC — Tum PC/SC islemlerini tek sinifta yoneten facade.
//
// Sorumluluklar:
//   1. Context yonetimi   (SCardEstablishContext / SCardReleaseContext)
//   2. Reader kesfetme    (SCardListReaders / SCardFreeMemory)
//   3. Kart baglantisi    (SCardConnect / SCardDisconnect)
//   4. APDU iletimi       (SCardTransmit — CardConnection uzerinden)
//   5. Ust-seviye erisim  (CardConnection&, DESFire, Reader testleri)
// ============================================================

class PCSC {
public:
    PCSC() = default;
    ~PCSC();

    // Kopyalanamaz, tasinabilir
    PCSC(const PCSC&) = delete;
    PCSC& operator=(const PCSC&) = delete;
    PCSC(PCSC&&) noexcept;
    PCSC& operator=(PCSC&&) noexcept;

    // ---- 1. Context yonetimi ----
    bool establishContext();
    void releaseContext();
    bool hasContext() const;
    SCARDCONTEXT context() const;

    // ---- 2. Reader kesfetme ----
    struct ReaderList {
        std::vector<std::wstring> names;
        bool ok;
    };
    ReaderList listReaders() const;
    bool chooseReader(size_t defaultIndex = 1);
    const std::wstring& readerName() const;

    // ---- 3. Kart baglantisi ----
    bool connectToCard(int retryMs = 500, int maxRetries = 0);
    void disconnectCard();
    bool isConnected() const;

    // ---- 4. APDU iletimi ----
    BYTEV transmit(const BYTEV& apdu) const;
    BYTEV sendCommand(const BYTEV& apdu, bool followChaining = true) const;

    // ---- 5. Ust-seviye erisim ----
    /// Alt siniflar (Reader, DESFire vb.) icin aktif CardConnection referansi
    CardConnection& cardConnection();
    const CardConnection& cardConnection() const;

    // ---- Hazir akis ----
    /// establishContext + chooseReader + connectToCard + runTests
    int run();

private:
    SCARDCONTEXT hContext_{0};
    std::wstring readerName_;
    std::unique_ptr<CardConnection> card_;

    void runTests();
    void cleanup();
};

#endif // PCSC_WORKSHOP1_PCSC_H
