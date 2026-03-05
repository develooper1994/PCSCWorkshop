#ifndef CARD_H
#define CARD_H

#include "../Reader.h"
#include <string>
#include <vector>

enum class CardBlockKind {
	Unknown,
	Data,
	Manufacturer,
	Trailer
};

struct CardBlockField {
	size_t offset = 0;
	size_t sizeBytes = 0;
	bool readable = true;
	bool writable = true;
	std::string name;
};

struct CardBlockNode {
	size_t index = 0;
	size_t sizeBytes = 0;
	bool readable = true;
	bool writable = true;
	bool isMetadata = false;
	CardBlockKind kind = CardBlockKind::Unknown;
	std::string name;
	std::vector<CardBlockField> fields;
};

struct CardSectorNode {
	size_t index = 0;
	std::string name;
	std::vector<CardBlockNode> blocks;
};

struct CardApplicationNode {
	size_t index = 0;
	std::string name;
	std::vector<CardSectorNode> sectors;
};

struct CardTopology {
	std::string cardType;
	std::vector<CardSectorNode> sectors;
	std::vector<CardApplicationNode> applications;

	bool hasApplications() const noexcept { return !applications.empty(); }

	size_t totalBytes() const noexcept {
		size_t total = 0;
		for (const auto& sector : sectors)
			for (const auto& block : sector.blocks)
				total += block.sizeBytes;
		for (const auto& app : applications)
			for (const auto& sector : app.sectors)
				for (const auto& block : sector.blocks)
					total += block.sizeBytes;
		return total;
	}

	size_t writableBytes() const noexcept {
		size_t total = 0;
		for (const auto& sector : sectors)
			for (const auto& block : sector.blocks)
				if (block.writable) total += block.sizeBytes;
		for (const auto& app : applications)
			for (const auto& sector : app.sectors)
				for (const auto& block : sector.blocks)
					if (block.writable) total += block.sizeBytes;
		return total;
	}
};

// ============================================================
// Card — Abstract base class for all card types
// ============================================================
// Provides common interface and functionality for all smart cards.
// Derived classes (MifareClassic, DESFire, etc.) implement
// card-specific operations.
// ============================================================
class Card
{
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

	// Get card type identifier (e.g., "Mifare Classic 1K", "DESFire EV2")
	virtual std::string getCardType() const = 0;

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
