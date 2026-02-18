#include "utils/DESFire.h"
#include "Reader/Readers.h"
#include <iostream>
#include <memory>
#include <string>

class SmartCardApp {
  public:
    SmartCardApp() = default;
    ~SmartCardApp() { cleanup(); }

    // Uygulama akýþýný çalýþtýr. Ýþlem çýkýþ kodunu döndürür.
    int run() {
        if (!establishContext()) return 1;
        if (!chooseReader()) return 1;
        if (!connectToCard(500)) return 1;

        runTests();
        return 0;
    }

  private:
    SCARDCONTEXT hContext_{0};
    std::wstring readerName_;
    std::unique_ptr<CardConnection> card_;

    bool establishContext() {
        LONG result = SCardEstablishContext(SCARD_SCOPE_USER, nullptr, nullptr, &hContext_);
        if (result != SCARD_S_SUCCESS) {
            std::cerr << "SCardEstablishContext failed: 0x" << std::hex << result << std::dec << std::endl;
            return false;
        }
        return true;
    }

    bool chooseReader() {
        auto readers = listReaders(hContext_);
        if (!readers.ok) {
            std::cerr << "No smartcard readers found or listReaders failed.\n";
            return false;
        }

        size_t index = selectReader(readers.names, 1);
        if (index >= readers.names.size()) {
            std::cerr << "Invalid reader index selected.\n";
            SCardFreeMemory(hContext_, readers.rawBuffer);
            return false;
        }

        readerName_ = readers.names[index];
        // listReaders tarafýndan ayrýlan tamponu serbest býrak
        SCardFreeMemory(hContext_, readers.rawBuffer);
        return true;
    }

    bool connectToCard(int retryMs = 500) {
        card_ = std::make_unique<CardConnection>(hContext_, readerName_);
        if (!card_)
            return false;
        if (!card_->waitAndConnect(retryMs)) {
            std::cerr << "Failed to connect to card within wait period.\n";
            return false;
        }
        return true;
    }

    void runTests() {
        if (!card_) return;
        // 1) DESFire bilgilerini oku
        DESFire::testDESFire(*card_);
        // 2) ACR1281U okuyucu yardýmcý sýnýfý kullanarak örnek read/write testi
        ACR1281UReader::testACR1281UReader(*card_);
    }

    void cleanup() {
        if (card_) {
            try { card_->disconnect(); }
            catch (...) {}
            card_.reset();
        }
        if (hContext_) {
            SCardReleaseContext(hContext_);
            hContext_ = 0;
        }
    }
};

//#define BUILD_MAIN_APP

#ifdef BUILD_MAIN_APP
int main() {
    SmartCardApp app;
    return app.run();
}
#endif