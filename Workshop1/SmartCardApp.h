#ifndef PCSC_WORKSHOP1_SMARTCARDAPP_H
#define PCSC_WORKSHOP1_SMARTCARDAPP_H

#include "DESFire.h"
#include "Readers.h"
#include <memory>
#include <string>

class SmartCardApp {
  public:
    SmartCardApp() = default;
    ~SmartCardApp();

    // Uygulama akýþýný çalýþtýr. Ýþlem çýkýþ kodunu döndürür.
    int run();

  private:
    SCARDCONTEXT hContext_{0};
    std::wstring readerName_;
    std::unique_ptr<CardConnection> card_;

    bool establishContext();
    bool chooseReader();
    bool connectToCard(int retryMs = 500);
    void runTests();
    void cleanup();
};

#endif // PCSC_WORKSHOP1_SMARTCARDAPP_H
