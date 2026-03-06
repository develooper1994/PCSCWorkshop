#include "MifareClassic.h"
#include "../Topology/SectorConfig.h"
#include "Utils.h"
#include "Reader.h"
#include <Log/Log.h>
#include <algorithm>
#include <iomanip>
#include <iostream>

// ════════════════════════════════════════════════════════════════════════════════
// Construction
// ════════════════════════════════════════════════════════════════════════════════

MifareCardCore::MifareCardCore(Reader& r, bool is4K)
	: Card(r),
	  is4KCard_(is4K),
	  numberOfSectors_(is4K ? 40 : 16),
	  sectorConfigs_(numberOfSectors_, SectorConfig{}) {
	
	// Initialize auth cache with default values
	authCache_.resize(numberOfSectors_);
	for (auto& auth : authCache_) {
		auth.slot = 0xFF;  // Invalid/not-authorized sentinel
	}

	// Register default keys (FF...FF for both A and B)
	const std::array<BYTE, 6> defaultKey = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	setKey(KeyType::A, defaultKey, KeyStructure::NonVolatile, 0x01);
	setKey(KeyType::B, defaultKey, KeyStructure::NonVolatile, 0x02);
}

// ════════════════════════════════════════════════════════════════════════════════
// Card Base Class Implementation
// ════════════════════════════════════════════════════════════════════════════════

CardType MifareCardCore::getCardType() const noexcept {
	return is4KCard_ ? CardType::MifareClassic4K : CardType::MifareClassic1K;
}

// ────────────────────────────────────────────────────────────────────────────
// getTopology(): Build detailed card layout for introspection
// ────────────────────────────────────────────────────────────────────────────
CardTopology MifareCardCore::getTopology() const {
	CardTopology topology;
	topology.cardType = getCardType();
	topology.sectors.reserve(numberOfSectors_);

	for (size_t sector = 0; sector < numberOfSectors_; ++sector) {
		CardSectorNode sectorNode;
		sectorNode.index = sector;
		sectorNode.name = "Sector " + std::to_string(sector);

		const int firstBlock = firstBlockOfSector(static_cast<int>(sector));
		const int blockCount = blocksPerSector(static_cast<int>(sector));
		sectorNode.blocks.reserve(static_cast<size_t>(blockCount));

		// Populate blocks
		for (int i = 0; i < blockCount; ++i) {
			const int absoluteBlock = firstBlock + i;
			const bool isManufacturer = absoluteBlock == 0;
			const bool isTrailer = i == blockCount - 1;

			CardBlockNode blockNode;
			blockNode.index = static_cast<size_t>(absoluteBlock);
			blockNode.sizeBytes = 16;
			blockNode.readable = true;
			blockNode.writable = !isManufacturer;
			blockNode.isMetadata = isManufacturer || isTrailer;

			if (isManufacturer) {
				blockNode.kind = CardBlockKind::Manufacturer;
				blockNode.name = "Manufacturer";
				blockNode.fields = {
					{0, 4, true,  false, "UID/BCC"},
					{4, 12, true, false, "ManufacturerData"}
				};
			} else if (isTrailer) {
				blockNode.kind = CardBlockKind::Trailer;
				blockNode.name = "Trailer";
				blockNode.fields = {
					{0,  6, false, true, "KeyA"},
					{6,  4, true,  true, "AccessBits+GPB"},
					{10, 6, false, true, "KeyB"}
				};
			} else {
				blockNode.kind = CardBlockKind::Data;
				blockNode.name = "Data";
				blockNode.fields = {{0, 16, true, true, "Payload"}};
			}

			sectorNode.blocks.push_back(blockNode);
		}

		topology.sectors.push_back(sectorNode);
	}

	return topology;
}

// ────────────────────────────────────────────────────────────────────────────
// Address-based read/write (inherited interface delegates to block operations)
// ────────────────────────────────────────────────────────────────────────────

BYTEV MifareCardCore::read(size_t address, size_t length) {
	// Convert byte address to block index and delegate
	return readLinear(static_cast<BYTE>(address / 16), length);
}

void MifareCardCore::write(size_t address, const BYTEV& data) {
	// Convert byte address to block index and delegate
	writeLinear(static_cast<BYTE>(address / 16), data);
}

void MifareCardCore::reset() {
	authCache_.clear();
	authCache_.resize(numberOfSectors_);
	for (auto& auth : authCache_) {
		auth.slot = 0xFF;
	}
}

// ════════════════════════════════════════════════════════════════════════════════
// Card Properties
// ════════════════════════════════════════════════════════════════════════════════

size_t MifareCardCore::getSectorCount() const noexcept {
	return numberOfSectors_;
}

// ════════════════════════════════════════════════════════════════════════════════
// Key Management
// ════════════════════════════════════════════════════════════════════════════════

void MifareCardCore::setKey(KeyType kt, const std::array<BYTE, 6>& key,
	KeyStructure ks, BYTE slot) {
	
	// Find existing key or create new
	auto it = std::find_if(keys_.begin(), keys_.end(),
		[kt](const KeyInfo& k) { return k.kt == kt; });

	if (it != keys_.end()) {
		// Update existing
		it->key = key;
		it->ks = ks;
		it->slot = slot;
	} else {
		// Add new
		KeyInfo ki;
		ki.kt = kt;
		ki.key = key;
		ki.ks = ks;
		ki.slot = slot;
		keys_.push_back(ki);
	}
}

void MifareCardCore::loadAllKeysToReader() {
	for (const auto& ki : keys_) {
		reader().loadKey(ki.key.data(), ki.ks, ki.slot);
	}
}

// ════════════════════════════════════════════════════════════════════════════════
// Sector Configuration Management
// ════════════════════════════════════════════════════════════════════════════════

void MifareCardCore::setSectorConfig(int sector, const SectorConfig& cfg) {
	validateSectorNumber(sector);
	sectorConfigs_[sector] = cfg;
	invalidateAuthForSector(sector);
}

void MifareCardCore::setAllSectorsConfig(const SectorConfig& cfg) {
	std::fill(sectorConfigs_.begin(), sectorConfigs_.end(), cfg);
	for (size_t i = 0; i < numberOfSectors_; ++i) {
		invalidateAuthForSector(static_cast<int>(i));
	}
}

void MifareCardCore::setAllSectorsConfig(const std::vector<SectorConfig>& configs) {
	if (configs.size() != numberOfSectors_) {
		throw std::invalid_argument("configs size mismatch");
	}
	sectorConfigs_ = configs;
	for (size_t i = 0; i < numberOfSectors_; ++i) {
		invalidateAuthForSector(static_cast<int>(i));
	}
}

// ════════════════════════════════════════════════════════════════════════════════
// Layout Information
// ════════════════════════════════════════════════════════════════════════════════

int MifareCardCore::blockToSector(BYTE block) const {
	return sectorFromBlock(block);
}

int MifareCardCore::blockIndexInSector(BYTE block) const {
	int sector = sectorFromBlock(block);
	int firstBlock = firstBlockOfSector(sector);
	return static_cast<int>(block) - firstBlock;
}

int MifareCardCore::firstBlockInSector(int sector) const {
	return firstBlockOfSector(sector);
}

int MifareCardCore::blocksInSector(int sector) const {
	return blocksPerSector(sector);
}

// ════════════════════════════════════════════════════════════════════════════════
// Authentication & Authorization
// ════════════════════════════════════════════════════════════════════════════════

// Check cache: is this sector already authenticated with this key?
bool MifareCardCore::isSectorAuthorized(int sector, KeyType kt, BYTE slot) const noexcept {
	if (sector < 0 || sector >= static_cast<int>(authCache_.size())) {
		return false;
	}
	const auto& cached = authCache_[sector];
	return cached.kt == kt && cached.slot == slot;
}

// Mark sector as authenticated with given key
void MifareCardCore::setAuthCache(int sector, KeyType kt, BYTE slot) {
	ensureAuthCacheSize(sector + 1);
	authCache_[sector].kt = kt;
	authCache_[sector].slot = slot;
}

// Invalidate auth for sector (forces re-auth next time)
void MifareCardCore::invalidateAuthForSector(int sector) noexcept {
	if (sector >= 0 && sector < static_cast<int>(authCache_.size())) {
		authCache_[sector].slot = 0xFF;  // Sentinel: invalid
	}
}

// Grow auth cache if needed
void MifareCardCore::ensureAuthCacheSize(int sector) {
	if (sector > static_cast<int>(authCache_.size())) {
		authCache_.resize(sector);
		for (auto& ki : authCache_) {
			ki.slot = 0xFF;
		}
	}
}

// Authenticate to a sector using specified key
void MifareCardCore::ensureAuthorized(int sector, KeyType kt, BYTE keySlot) {
	// Return immediately if already authenticated with same key
	if (isSectorAuthorized(sector, kt, keySlot)) {
		return;
	}

	// Need to authenticate
	BYTE block = trailerBlockOf(sector);
	reader().setKeyType(kt);
	reader().setKeyNumber(keySlot);
	reader().setAuthRequested(true);
	reader().performAuth(block);
	
	// Cache the auth
	setAuthCache(sector, kt, keySlot);
}

// Try any registered key for this sector
KeyType MifareCardCore::tryAnyAuthForSector(int sector, BYTE& outSlot) {
	const auto& keyA = findKeyOrThrow(KeyType::A);
	const auto& keyB = findKeyOrThrow(KeyType::B);

	// Try cached auth first
	if (isSectorAuthorized(sector, KeyType::A, keyA.slot)) {
		outSlot = keyA.slot;
		return KeyType::A;
	}
	if (isSectorAuthorized(sector, KeyType::B, keyB.slot)) {
		outSlot = keyB.slot;
		return KeyType::B;
	}

	// Try to authenticate with KeyA
	try {
		ensureAuthorized(sector, KeyType::A, keyA.slot);
		outSlot = keyA.slot;
		return KeyType::A;
	} catch (...) {}

	// Try to authenticate with KeyB
	try {
		ensureAuthorized(sector, KeyType::B, keyB.slot);
		outSlot = keyB.slot;
		return KeyType::B;
	} catch (...) {
		throw std::runtime_error("Authentication failed for both keys");
	}
}

// ════════════════════════════════════════════════════════════════════════════════
// Single-Block Operations
// ════════════════════════════════════════════════════════════════════════════════

BYTEV MifareCardCore::read(BYTE block) {
	validateBlockNumber(block);
	if (isTrailerBlock(block)) {
		checkTrailerBlock(block);
	}

	int sector = sectorFromBlock(block);
	KeyType kt = chooseKeyForOperation(sector, false);  // isWrite=false
	const KeyInfo& ki = findKeyOrThrow(kt);
	return readWithKey(block, kt, ki.slot);
}

void MifareCardCore::write(BYTE block, const BYTEV& data) {
	validateBlockNumber(block);
	if (isTrailerBlock(block)) {
		checkTrailerBlock(block);
	}
	if (isManufacturerBlock(block)) {
		checkManufacturerBlock(block);
	}

	int sector = sectorFromBlock(block);
	KeyType kt = chooseKeyForOperation(sector, true);  // isWrite=true
	const KeyInfo& ki = findKeyOrThrow(kt);
	writeWithKey(block, data, kt, ki.slot);
}

void MifareCardCore::write(BYTE block, const std::string& text) {
	write(block, BYTEV(text.begin(), text.end()));
}

BYTEV MifareCardCore::readWithKey(BYTE block, KeyType keyType, BYTE keySlot) {
	int sector = sectorFromBlock(block);
	ensureAuthorized(sector, keyType, keySlot);
	return reader().readPage(block);
}

void MifareCardCore::writeWithKey(BYTE block, const BYTEV& data, 
								   KeyType keyType, BYTE keySlot) {
	if (data.size() != 16) {
		throw std::invalid_argument("Block data must be exactly 16 bytes");
	}
	int sector = sectorFromBlock(block);
	ensureAuthorized(sector, keyType, keySlot);
	reader().writePage(block, data.data());
}

void MifareCardCore::writeWithKey(BYTE block, const std::string& text, 
								   KeyType keyType, BYTE keySlot) {
	writeWithKey(block, BYTEV(text.begin(), text.end()), keyType, keySlot);
}

BLOCK MifareCardCore::readBlock(BYTE block) {
	BYTEV data = read(block);
	BLOCK b;
	std::memcpy(b.data(), data.data(), 16);
	return b;
}

void MifareCardCore::writeBlock(BYTE block, const BYTE data[16]) {
	BYTEV v(data, data + 16);
	write(block, v);
}

// ════════════════════════════════════════════════════════════════════════════════
// Multi-Block Linear Operations
// ════════════════════════════════════════════════════════════════════════════════

BYTEV MifareCardCore::readLinear(BYTE startBlock, size_t length) {
	BYTEV result;
	size_t remaining = length;
	BYTE currentBlock = startBlock;

	while (remaining > 0) {
		BYTEV blockData = read(currentBlock);
		size_t copySize = (std::min)(remaining, blockData.size());
		result.insert(result.end(), blockData.begin(), blockData.begin() + copySize);
		remaining -= copySize;
		++currentBlock;
	}
	return result;
}

void MifareCardCore::writeLinear(BYTE startBlock, const BYTEV& data) {
	BYTE currentBlock = startBlock;
	size_t offset = 0;

	while (offset < data.size()) {
		BYTEV blockData(16, 0);
		size_t copySize = (std::min)(static_cast<size_t>(16), data.size() - offset);
		std::memcpy(blockData.data(), data.data() + offset, copySize);
		write(currentBlock, blockData);
		offset += copySize;
		++currentBlock;
	}
}

// ════════════════════════════════════════════════════════════════════════════════
// Trailer Block Operations (Sector Security Configuration)
// ════════════════════════════════════════════════════════════════════════════════

BLOCK MifareCardCore::readTrailer(int sector) {
	validateSectorNumber(sector);
	BYTE trailerBlock = trailerBlockOf(sector);
	return readBlock(trailerBlock);
}

std::vector<BLOCK> MifareCardCore::readAllTrailers() {
	std::vector<BLOCK> trailers;
	trailers.reserve(numberOfSectors_);
	for (size_t i = 0; i < numberOfSectors_; ++i) {
		trailers.push_back(readTrailer(static_cast<int>(i)));
	}
	return trailers;
}

void MifareCardCore::printAllTrailers() {
	std::cout << "Sector Trailers (KeyA | AccessBytes | KeyB):\n";
	for (size_t i = 0; i < numberOfSectors_; ++i) {
		BLOCK trailer = readTrailer(static_cast<int>(i));
		std::cout << "Sector " << std::setw(2) << i << ": ";
		for (size_t j = 0; j < 16; ++j) {
			std::cout << std::hex << std::setw(2) << std::setfill('0')
					  << static_cast<int>(trailer[j]) << " ";
		}
		std::cout << std::dec << "\n";
	}
}

void MifareCardCore::changeTrailer(int sector, const BYTE trailer16[16]) {
	validateSectorNumber(sector);
	
	// Validate access bytes
	if (!SectorConfig::validateAccessBits(trailer16 + 6)) {
		throw std::runtime_error("changeTrailer: invalid access bits in trailer");
	}

	// Authenticate and write
	const KeyInfo& ki = findKeyOrThrow(KeyType::B);
	ensureAuthorized(sector, KeyType::B, ki.slot);
	reader().writePage(trailerBlockOf(sector), trailer16);
	
	// Invalidate auth cache
	invalidateAuthForSector(sector);
}

// Simple apply: use any key we have
void MifareCardCore::applySectorConfigToCard(int sector) {
	validateSectorNumber(sector);
	
	const auto& cfg = sectorConfigs_[sector];
	
	// Determine key bytes to write
	const BYTE* keyAData = nullptr;
	const BYTE* keyBData = nullptr;
	
	// Use config keys if provided, else fallback to registered keys
	if (cfg.keyA.key[0] != 0 || cfg.keyA.key[1] != 0 || cfg.keyA.key[2] != 0) {
		keyAData = cfg.keyA.key.data();
	} else {
		requireBothKeys();
		keyAData = findKeyOrThrow(KeyType::A).key.data();
	}
	
	if (cfg.keyB.key[0] != 0 || cfg.keyB.key[1] != 0 || cfg.keyB.key[2] != 0) {
		keyBData = cfg.keyB.key.data();
	} else {
		requireBothKeys();
		keyBData = findKeyOrThrow(KeyType::B).key.data();
	}

	// Generate access bytes
	ACCESSBYTES access = SectorConfig::makeSectorAccessBits(cfg);
	if (!SectorConfig::validateAccessBits(access.data())) {
		throw std::runtime_error("Generated access bits are invalid");
	}

	// Build trailer
	BLOCK trailer = SectorConfig::buildTrailer(keyAData, access.data(), keyBData);

	// Authenticate with any key
	BYTE authSlot = 0xFF;
	tryAnyAuthForSector(sector, authSlot);

	// Write trailer
	reader().writePage(trailerBlockOf(sector), trailer.data());
	invalidateAuthForSector(sector);
}

// Strict apply: validate everything, rollback on failure
void MifareCardCore::applySectorConfigStrict(int sector, 
											  KeyType authKeyType, 
											  bool enableRollback) {
	validateSectorNumber(sector);
	
	const auto& cfg = sectorConfigs_[sector];
	
	// Determine key bytes
	const BYTE* keyAData = nullptr;
	const BYTE* keyBData = nullptr;
	
	if (cfg.keyA.key[0] != 0 || cfg.keyA.key[1] != 0 || cfg.keyA.key[2] != 0) {
		keyAData = cfg.keyA.key.data();
	} else {
		requireBothKeys();
		keyAData = findKeyOrThrow(KeyType::A).key.data();
	}
	
	if (cfg.keyB.key[0] != 0 || cfg.keyB.key[1] != 0 || cfg.keyB.key[2] != 0) {
		keyBData = cfg.keyB.key.data();
	} else {
		requireBothKeys();
		keyBData = findKeyOrThrow(KeyType::B).key.data();
	}

	const KeyInfo& authKey = findKeyOrThrow(authKeyType);

	// Generate access bytes
	auto accessBytes = SectorConfig::makeSectorAccessBits(cfg);
	if (!SectorConfig::validateAccessBits(accessBytes.data())) {
		throw std::runtime_error("Generated access bits invalid");
	}

	BYTE trailerBlock = trailerBlockOf(sector);

	// Save old trailer for rollback
	BLOCK oldTrailer{};
	if (enableRollback) {
		oldTrailer = readTrailer(sector);
	}

	// Build new trailer
	BLOCK newTrailer = SectorConfig::buildTrailer(keyAData, accessBytes.data(), keyBData);

	// Authenticate with specified key
	ensureAuthorized(sector, authKeyType, authKey.slot);

	// Try to write, rollback on failure
	try {
		reader().writePage(trailerBlock, newTrailer.data());
	} catch (...) {
		if (enableRollback) {
			try {
				ensureAuthorized(sector, authKeyType, authKey.slot);
				reader().writePage(trailerBlock, oldTrailer.data());
			} catch (...) {
				std::cerr << "Rollback failed for sector " << sector << "\n";
			}
		}
		throw;
	}

	invalidateAuthForSector(sector);
}

// ════════════════════════════════════════════════════════════════════════════════
// Batch Configuration Operations
// ════════════════════════════════════════════════════════════════════════════════

void MifareCardCore::applyAllSectorsConfig() {
	batchApply([this](int s) { applySectorConfigToCard(s); });
}

void MifareCardCore::applyAllSectorsConfig(const std::vector<SectorConfig>& configs) {
	setAllSectorsConfig(configs);
	applyAllSectorsConfig();
}

void MifareCardCore::applyAllSectorsConfigStrict(KeyType authKeyType, 
												  bool enableRollback) {
	batchApply([this, authKeyType, enableRollback](int s) {
		applySectorConfigStrict(s, authKeyType, enableRollback);
	});
}

void MifareCardCore::applyAllSectorsConfigStrict(const std::vector<SectorConfig>& configs,
												  KeyType authKeyType,
												  bool enableRollback) {
	setAllSectorsConfig(configs);
	applyAllSectorsConfigStrict(authKeyType, enableRollback);
}

// Load config from card (read trailers and parse)
void MifareCardCore::loadSectorConfigFromCard(int sector) {
	validateSectorNumber(sector);
	BLOCK trailer = readTrailer(sector);
	sectorConfigs_[sector] = SectorConfig::parseAccessBitsToConfig(trailer.data() + 6);
}

void MifareCardCore::loadAllSectorConfigsFromCard() {
	batchApply([this](int s) { loadSectorConfigFromCard(s); });
}

void MifareCardCore::loadSectorConfigsFromCard(const std::vector<int>& sectors) {
	for (int s : sectors) {
		loadSectorConfigFromCard(s);
	}
}

// ════════════════════════════════════════════════════════════════════════════════
// Helper Functions (Key Selection, Validation, etc.)
// ════════════════════════════════════════════════════════════════════════════════

KeyType MifareCardCore::chooseKeyForOperation(int sector, bool isWrite) const {
	const auto& cfg = sectorConfigs_[sector];
	if (isWrite) {
		return cfg.keyB.writable ? KeyType::B : KeyType::A;
	} else {
		return cfg.keyA.readable ? KeyType::A : KeyType::B;
	}
}

const KeyInfo& MifareCardCore::findKeyOrThrow(KeyType kt) const {
	auto it = std::find_if(keys_.begin(), keys_.end(),
		[kt](const KeyInfo& k) { return k.kt == kt; });
	if (it == keys_.end()) {
		throw std::runtime_error("Key not found for KeyType");
	}
	return *it;
}

bool MifareCardCore::hasKey(KeyType kt) const noexcept {
	return std::any_of(keys_.begin(), keys_.end(),
		[kt](const KeyInfo& k) { return k.kt == kt; });
}

void MifareCardCore::requireBothKeys() const {
	if (!hasKey(KeyType::A) || !hasKey(KeyType::B)) {
		throw std::runtime_error(
			"Both KeyType::A and KeyType::B must be set");
	}
}

void MifareCardCore::throwIfErrors(const std::vector<std::string>& errors,
									const char* header) {
	if (errors.empty()) return;
	std::string msg = header;
	for (const auto& e : errors) msg += e + "\n";
	throw std::runtime_error(msg);
}
