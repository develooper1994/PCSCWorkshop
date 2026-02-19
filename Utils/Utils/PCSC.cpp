#include "PCSC.h"
#include <iostream>
#include <utility>

// ============================================================
// Destructor & move semantics
// ============================================================

PCSC::~PCSC() { cleanup(); }

PCSC::PCSC(PCSC&& other) noexcept
    : hContext_(other.hContext_),
      readerName_(std::move(other.readerName_)),
      card_(std::move(other.card_))
{
    other.hContext_ = 0;
}

PCSC& PCSC::operator=(PCSC&& other) noexcept {
    if (this != &other) {
        cleanup();
        hContext_    = other.hContext_;
        readerName_  = std::move(other.readerName_);
        card_        = std::move(other.card_);
        other.hContext_ = 0;
    }
    return *this;
}

// ============================================================
// 1. Context yonetimi
// ============================================================

bool PCSC::establishContext() {
    if (hContext_) return true; // zaten acik
    LONG result = SCardEstablishContext(SCARD_SCOPE_USER, nullptr, nullptr, &hContext_);
    if (result != SCARD_S_SUCCESS) {
        std::cerr << "SCardEstablishContext failed: 0x"
                  << std::hex << result << std::dec << std::endl;
        hContext_ = 0;
        return false;
    }
    return true;
}

void PCSC::releaseContext() {
    if (hContext_) {
        SCardReleaseContext(hContext_);
        hContext_ = 0;
    }
}

bool PCSC::hasContext() const { return hContext_ != 0; }

SCARDCONTEXT PCSC::context() const { return hContext_; }

// ============================================================
// 2. Reader kesfetme
// ============================================================

PCSC::ReaderList PCSC::listReaders() const {
    ReaderList result{{}, false};
    if (!hContext_) {
        std::cerr << "No valid context for listReaders.\n";
        return result;
    }

    // PcscUtils.h icindeki global listReaders fonksiyonunu cagir
    auto raw = ::listReaders(hContext_);
    if (!raw.ok) {
        std::cerr << "No smartcard readers found or listReaders failed.\n";
        return result;
    }

    result.names = raw.names;
    result.ok    = true;

    // rawBuffer'i serbest birak
    SCardFreeMemory(hContext_, raw.rawBuffer);
    return result;
}

bool PCSC::chooseReader(size_t defaultIndex) {
    auto readers = listReaders();
    if (!readers.ok) return false;

    size_t index = selectReader(readers.names, defaultIndex);
    if (index >= readers.names.size()) {
        std::cerr << "Invalid reader index selected.\n";
        return false;
    }

    readerName_ = readers.names[index];
    return true;
}

const std::wstring& PCSC::readerName() const { return readerName_; }

// ============================================================
// 3. Kart baglantisi
// ============================================================

bool PCSC::connectToCard(int retryMs, int maxRetries) {
    if (!hContext_ || readerName_.empty()) {
        std::cerr << "Context or reader not ready.\n";
        return false;
    }

    card_ = std::make_unique<CardConnection>(hContext_, readerName_);
    if (!card_->waitAndConnect(retryMs, maxRetries)) {
        std::cerr << "Failed to connect to card.\n";
        card_.reset();
        return false;
    }
    return true;
}

void PCSC::disconnectCard() {
    if (card_) {
        try { card_->disconnect(); }
        catch (...) {}
        card_.reset();
    }
}

bool PCSC::isConnected() const {
    return card_ && card_->isConnected();
}

// ============================================================
// 4. APDU iletimi
// ============================================================

BYTEV PCSC::transmit(const BYTEV& apdu) const {
    if (!card_ || !card_->isConnected())
        throw std::runtime_error("PCSC::transmit — card not connected");
    return card_->transmit(apdu);
}

BYTEV PCSC::sendCommand(const BYTEV& apdu, bool followChaining) const {
    if (!card_ || !card_->isConnected())
        throw std::runtime_error("PCSC::sendCommand — card not connected");
    return card_->sendCommand(apdu, followChaining);
}

// ============================================================
// 5. Ust-seviye erisim
// ============================================================

CardConnection& PCSC::cardConnection() {
    if (!card_)
        throw std::runtime_error("PCSC::cardConnection — no active connection");
    return *card_;
}

const CardConnection& PCSC::cardConnection() const {
    if (!card_)
        throw std::runtime_error("PCSC::cardConnection — no active connection");
    return *card_;
}

// ============================================================
// Hazir akis
// ============================================================

int PCSC::run() {
    if (!establishContext()) return 1;
    if (!chooseReader())     return 1;
    if (!connectToCard(500)) return 1;

    runTests();
    return 0;
}

void PCSC::runTests() {
    if (!card_) return;
    // 1) DESFire bilgilerini oku
    DESFire::testDESFire(*card_);
    // 2) ACR1281U okuyucu yardimci sinifi kullanarak ornek read/write testi
    ACR1281UReader::testACR1281UReader(*card_);
}

// ============================================================
// Temizlik
// ============================================================

void PCSC::cleanup() {
    disconnectCard();
    releaseContext();
}
