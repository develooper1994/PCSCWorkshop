#include "PCSC.h"
#include <iostream>
#include <utility>
#include "Exceptions/GenericExceptions.h"

// ============================================================
// Helper: Convert SCARD error codes to user-friendly messages
// ============================================================
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
        std::cerr << "SCardEstablishContext failed: " 
                  << getSCardErrorMessage(result) << std::endl;
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
        std::cerr << "SCardListReaders failed: " 
                  << getSCardErrorMessage(rc) << std::endl;
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
    if (callback) callback(*this);
    return 0;
}

// ============================================================
// Temizlik
// ============================================================

void PCSC::cleanup() {
    disconnectCard();
    releaseContext();
}
