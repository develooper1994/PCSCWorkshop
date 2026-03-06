#ifndef CARD_H
#define CARD_H

#include "Topology/Topology.h"
#include "../Reader.h"
#include "Topology/SectorConfig.h"
#include <string>
#include <vector>
#include <memory>  // unique_ptr için

// ============================================================
// Card — Abstract base class for all card types
// ============================================================
// Provides common interface and functionality for all smart cards.
// Derived classes (MifareClassic, DESFire, etc.) implement
// card-specific operations.
// ============================================================
class Card {
public:
	// ── Virtual destructor for proper cleanup ──
	virtual ~Card() noexcept = default;

	// ── Non-copyable (cards maintain reader state) ──
	Card(const Card&) = delete;
	Card& operator=(const Card&) = delete;

	// ── Movable ──
	Card(Card&&) noexcept = default;
	Card& operator=(Card&&) noexcept = default;

	// ════════════════════════════════════════════════════════
	//  Pure virtual interface (must be implemented by derived classes)
	// ════════════════════════════════════════════════════════

	// Get card type identifier
	virtual CardType getCardType() const noexcept = 0;

	// Get total memory capacity in bytes
	virtual size_t getMemorySize() const noexcept = 0;

	// Read data from card at specified address/block
	virtual BYTEV read(size_t address, size_t length) = 0;

	// Write data to card at specified address/block
	virtual void write(size_t address, const BYTEV& data) = 0;

	// ════════════════════════════════════════════════════════
	//  Common operations (can be overridden if needed)
	// ════════════════════════════════════════════════════════

	// Get card UID/serial number
	virtual BYTEV getUID() const;

	// Describe logical topology (blocks/sectors/applications)
	virtual CardTopology getTopology() const;

	// Human-readable card type text
	static const char* describeCardType(CardType type) noexcept;

	// Authenticate to card (default implementation delegates to reader)
	virtual void authenticate(const BYTE* key, size_t keyLen);

	// Check if card is connected and responsive
	virtual bool isConnected() const noexcept;

	// Reset card state (clear authentication, caches, etc.)
	virtual void reset();

	// ════════════════════════════════════════════════════════
	//  Convenience overloads (non-virtual)
	// ════════════════════════════════════════════════════════

	// Write string data
	void write(size_t address, const std::string& text);

	// Read and return as string
	std::string readString(size_t address, size_t length);

protected:
	// ── Protected constructor (only derived classes can instantiate) ──
	explicit Card(Reader& r) noexcept;

	// ── Access to reader for derived classes ──
	Reader& reader() noexcept;
	const Reader& reader() const noexcept;

private:
	// ── Core state ──
	Reader& reader_;
};

#endif // !CARD_H
