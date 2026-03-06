#ifndef TOPOLOGY_H
#define TOPOLOGY_H

#include "../CardDataTypes.h"
#include "SectorConfig.h"
#include <string>
#include <memory>
#include <vector>

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
	std::unique_ptr<SectorConfig> security;  // unique_ptr kullan

	// Copy constructor/assignment için özel işlem gerekir
	CardSectorNode() = default;

	CardSectorNode(const CardSectorNode& other) : index(other.index), name(other.name), blocks(other.blocks) {
		if (other.security) security = std::make_unique<SectorConfig>(*other.security);
	}

	CardSectorNode& operator=(const CardSectorNode& other) {
		if (this != &other) {
			index = other.index;
			name = other.name;
			blocks = other.blocks;
			if (other.security) security = std::make_unique<SectorConfig>(*other.security);
			else security.reset();
		}
		return *this;
	}

	CardSectorNode(CardSectorNode&&) = default;
	CardSectorNode& operator=(CardSectorNode&&) = default;
};

struct CardApplicationNode {
	size_t index = 0;
	std::string name;
	std::vector<CardSectorNode> sectors;
};

struct CardTopology {
	CardType cardType = CardType::Unknown;
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


#endif // !TOPOLOGY_H
