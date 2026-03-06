#ifndef TOPOLOGY_H
#define TOPOLOGY_H

#include "../CardDataTypes.h"
#include "SectorConfig.h"
#include <string>
#include <memory>
#include <vector>

// ════════════════════════════════════════════════════════════════════════════════
// Card Topology Description (Runtime Introspection)
// ════════════════════════════════════════════════════════════════════════════════
// These structures describe the logical structure of a card:
// - Which blocks exist, their permissions, and their purpose
// - Used for UI display, validation, and block organization
// ════════════════════════════════════════════════════════════════════════════════

// A field within a block (e.g., UID within manufacturer block, KeyA within trailer)
struct CardBlockField {
	size_t offset = 0;           // Offset within block (bytes)
	size_t sizeBytes = 0;        // Field size (bytes)
	bool readable = true;        // Can be read (depending on permissions)
	bool writable = true;        // Can be written (depending on permissions)
	std::string name;            // Field name ("UID", "KeyA", etc.)
};

// A single block (16 bytes) within a sector
struct CardBlockNode {
	size_t index = 0;                           // Absolute block number
	size_t sizeBytes = 0;                       // Block size (always 16 for Mifare)
	bool readable = true;                       // Block can be read
	bool writable = true;                       // Block can be written
	bool isMetadata = false;                    // Is metadata (UID, trailer)?
	CardBlockKind kind = CardBlockKind::Unknown; // Block type
	std::string name;                           // "Data", "Manufacturer", "Trailer"
	std::vector<CardBlockField> fields;         // Sub-fields within the block
};

// A sector (4 or 16 blocks) with its security configuration
struct CardSectorNode {
	size_t index = 0;                              // Sector number
	std::string name;                              // Human-readable label
	std::vector<CardBlockNode> blocks;             // All blocks in this sector
	std::unique_ptr<SectorConfig> security;        // Security configuration

	// Default constructor
	CardSectorNode() = default;

	// Copy constructor (must handle unique_ptr)
	CardSectorNode(const CardSectorNode& other) 
		: index(other.index), name(other.name), blocks(other.blocks) {
		if (other.security) {
			security = std::make_unique<SectorConfig>(*other.security);
		}
	}

	// Copy assignment (must handle unique_ptr)
	CardSectorNode& operator=(const CardSectorNode& other) {
		if (this != &other) {
			index = other.index;
			name = other.name;
			blocks = other.blocks;
			if (other.security) {
				security = std::make_unique<SectorConfig>(*other.security);
			} else {
				security.reset();
			}
		}
		return *this;
	}

	// Move operations (default)
	CardSectorNode(CardSectorNode&&) = default;
	CardSectorNode& operator=(CardSectorNode&&) = default;
};

// An application/area on a DESFire card (future use, not in Classic)
struct CardApplicationNode {
	size_t index = 0;
	std::string name;
	std::vector<CardSectorNode> sectors;
};

// Complete logical structure of a card
struct CardTopology {
	CardType cardType = CardType::Unknown;
	std::vector<CardSectorNode> sectors;        // Sectors (for Classic)
	std::vector<CardApplicationNode> applications; // Applications (for DESFire)

	// Check if card has applications (DESFire-style)
	bool hasApplications() const noexcept { 
		return !applications.empty(); 
	}

	// Total memory in bytes
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

	// Writable memory in bytes (excluding manufacturer block, trailers, etc.)
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

#endif // !TOPOLOGY_H
