#include "PCSC.h"
#include <iostream>
#include <utility>
#include "Exceptions/GenericExceptions.h"

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

    LPWSTR rawBuffer = nullptr;
    DWORD readersLen = SCARD_AUTOALLOCATE;
    LONG rc = SCardListReadersW(hContext_, nullptr,
                                reinterpret_cast<LPWSTR>(&rawBuffer),
                                &readersLen);
    if (rc != SCARD_S_SUCCESS) {
        std::cerr << "SCardListReaders failed: 0x"
                  << std::hex << rc << std::dec << std::endl;
        return result;
    }

    // Multi-string parse: her string NUL ile biter, son string cift NUL
    wchar_t* p = rawBuffer;
    while (p && *p != L'\0') {
        result.names.emplace_back(p);
        std::wcout << L"[" << result.names.size() - 1
                   << L"] " << p << std::endl;
        p += wcslen(p) + 1;
    }

    // rawBuffer'i serbest birak
    if(rawBuffer != nullptr) SCardFreeMemory(hContext_, rawBuffer);

    if (result.names.empty()) {
        std::cerr << "No readers found.\n";
        return result;
    }

    result.ok = true;
    return result;
}

bool PCSC::chooseReader(size_t defaultIndex) {
    auto readers = listReaders();
    if (!readers.ok) return false;

    // Interaktif reader secimi
    if (readers.names.empty()) return false;
    if (defaultIndex >= readers.names.size()) defaultIndex = 0;

    size_t selected = defaultIndex;
    while (true) {
        std::wcout << L"Select reader index (0-"
                   << (readers.names.size() - 1)
                   << L", default " << defaultIndex << L"): ";
        std::wstring input;
        std::getline(std::wcin, input);
        if (input.empty()) {
            selected = defaultIndex;
            break;
        }
        try {
            selected = std::stoul(input);
            if (selected < readers.names.size()) break;
            std::wcout << L"Invalid index. Try again.\n";
        }
        catch (const std::exception&) {
            std::wcout << L"Invalid input. Try again.\n";
        }
    }

    readerName_ = readers.names[selected];
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
        throw pcsc::ReaderError("PCSC::transmit - card not connected");
    return card_->transmit(apdu);
}

BYTEV PCSC::sendCommand(const BYTEV& apdu, bool followChaining) const {
    if (!card_ || !card_->isConnected())
        throw pcsc::ReaderError("PCSC::sendCommand - card not connected");
    return card_->sendCommand(apdu, followChaining);
}

// ============================================================
// 5. Ust-seviye erisim
// ============================================================

CardConnection& PCSC::cardConnection() {
    if (!card_)
        throw pcsc::ReaderError("PCSC::cardConnection - no active connection");
    return *card_;
}

const CardConnection& PCSC::cardConnection() const {
    if (!card_)
        throw pcsc::ReaderError("PCSC::cardConnection - no active connection");
    return *card_;
}

// ============================================================
// Hazir akis
// ============================================================

int PCSC::run(TestCallback callback) {
    if (!establishContext()) return 1;
    if (!chooseReader())     return 1;
    if (!connectToCard(500)) return 1;

    if (callback) {
        callback(*this);
    }
    return 0;
}

// ============================================================
// Temizlik
// ============================================================

void PCSC::cleanup() {
    disconnectCard();
    releaseContext();
}
