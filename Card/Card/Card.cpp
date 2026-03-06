#include "Card.h"
#include "Reader.h"
#include "CardUtils.h"
#include <stdexcept>

// ════════════════════════════════════════════════════════
//  Construction
// ════════════════════════════════════════════════════════
Card::Card(Reader& r) noexcept : reader_(r) {}

// ════════════════════════════════════════════════════════
//  Protected reader access
// ════════════════════════════════════════════════════════
Reader& Card::reader() noexcept { return reader_; }
const Reader& Card::reader() const noexcept { return reader_; }

// ════════════════════════════════════════════════════════
//  Default implementations of common operations
// ════════════════════════════════════════════════════════

BYTEV Card::getUID() const {
	// Default implementation: request UID from reader/connection
	// Derived classes can override for card-specific behavior
	// CardConnection doesn't have getUID, return empty by default
	return CardUtils::getUid(reader_.cardConnection());
}

CardTopology Card::getTopology() const {
	CardTopology topology;
	topology.cardType = getCardType();
	return topology;
}

const char* Card::describeCardType(CardType type) noexcept {
	switch (type) {
	case CardType::MifareClassic1K: return "Mifare Classic 1K";
	case CardType::MifareClassic4K: return "Mifare Classic 4K";
	case CardType::MifareUltralight: return "Mifare Ultralight";
	case CardType::MifareDesfire: return "Mifare DESFire";
	default: return "Unknown";
	}
}

void Card::authenticate(const BYTE* key, size_t keyLen) {
	// Default implementation does nothing
	// Derived classes override with card-specific authentication
	(void)key;
	(void)keyLen;
}

bool Card::isConnected() const noexcept {
	// Default implementation: assume connected
	// Derived classes can override with card-specific checks
	return reader_.cardConnection().isConnected();
}

void Card::reset() {
	// Default implementation: reset reader authentication state
	reader_.setAuthRequested(true);
}

// ════════════════════════════════════════════════════════
//  Convenience overloads
// ════════════════════════════════════════════════════════

void Card::write(size_t address, const std::string& text) {
	BYTEV data(text.begin(), text.end());
	write(address, data);
}

std::string Card::readString(size_t address, size_t length) {
	BYTEV data = read(address, length);
	return std::string(data.begin(), data.end());
}
