#include "MifareClassic.h"
#include "../Topology/SectorConfig.h"
#include "Utils.h"
#include "Reader.h"
#include <Log/Log.h>
#include <algorithm>

// ════════════════════════════════════════════════════════
//  Construction
// ════════════════════════════════════════════════════════
MifareCardCore::MifareCardCore(Reader& r, bool is4K)
	: Card(r),
	  is4KCard_(is4K),
	  numberOfSectors_(is4K ? 40 : 16),
	  sectorConfigs_(numberOfSectors_, SectorConfig{}) {
	// Load default keys into MifareCardCore's key registry
	// Keys will be loaded to reader on-demand when first authentication is needed
	const std::array<BYTE, 6> defaultKey6 = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	const BYTE keySlotA = 0x01;
	const BYTE keySlotB = 0x02;

	// Register keys - they will be loaded to reader lazily when needed
	setKey(KeyType::A, defaultKey6, KeyStructure::NonVolatile, keySlotA);
	setKey(KeyType::B, defaultKey6, KeyStructure::NonVolatile, keySlotB);
}

// Inline implementations are in header file
// See MifareClassic.h for: getCardType(), getSectorCount(), etc.

CardTopology MifareCardCore::getTopology() const {
	CardTopology topology;
	topology.cardType = getCardType();
	topology.sectors.reserve(numberOfSectors_);

	// Topology
	for (size_t sector = 0; sector < numberOfSectors_; ++sector) {
		CardSectorNode sectorNode;
		sectorNode.index = sector;
		sectorNode.name = "Sector " + std::to_string(sector);

		const int firstBlock = firstBlockOfSector(static_cast<int>(sector));
		const int blockCount = blocksPerSector(static_cast<int>(sector));
		sectorNode.blocks.reserve(static_cast<size_t>(blockCount));
		// Sector
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
			// Block
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

BYTEV MifareCardCore::read(size_t address, size_t length) {
	return readLinear(static_cast<BYTE>(address / 16), length);
}

void MifareCardCore::write(size_t address, const BYTEV& data) {
	writeLinear(static_cast<BYTE>(address / 16), data);
}

void MifareCardCore::reset() {
	authCache_.clear();
	authCache_.resize(numberOfSectors_, KeyInfo{});
}

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

void MifareCardCore::setSectorConfig(int sector, const SectorConfig& cfg) {
	validateSectorNumber(sector);
	sectorConfigs_[sector] = cfg;
	invalidateAuthForSector(sector);
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

void MifareCardCore::setKey(KeyType kt, const std::array<BYTE, 6>& key,
	KeyStructure ks, BYTE slot) {
	auto it = std::find_if(keys_.begin(), keys_.end(),
		[kt](const KeyInfo& k) { return k.kt == kt; });

	if (it != keys_.end()) {
		it->key = key;
		it->ks = ks;
		it->slot = slot;
	} else {
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

bool MifareCardCore::isSectorAuthorized(int sector, KeyType kt, BYTE slot) const noexcept {
	if (sector < 0 || sector >= static_cast<int>(authCache_.size())) {
		return false;
	}
	const auto& cached = authCache_[sector];
	return cached.kt == kt && cached.slot == slot;
}

void MifareCardCore::setAuthCache(int sector, KeyType kt, BYTE slot) {
	ensureAuthCacheSize(sector + 1);
	authCache_[sector].kt = kt;
	authCache_[sector].slot = slot;
}

void MifareCardCore::ensureAuthCacheSize(int sector) {
	if (sector >= static_cast<int>(authCache_.size())) {
		authCache_.resize(sector + 1, KeyInfo{});
		for (auto& ki : authCache_) {
			ki.slot = 0xFF;
		}
	}
}

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

void MifareCardCore::ensureAuthorized(int sector, KeyType kt, BYTE keySlot) {
	if (isSectorAuthorized(sector, kt, keySlot)) {
		return;
	}
	BYTE block = trailerBlockOf(sector);
	reader().setKeyType(kt);
	reader().setKeyNumber(keySlot);
	reader().setAuthRequested(true);
	reader().performAuth(block);
	setAuthCache(sector, kt, keySlot);
}

KeyType MifareCardCore::tryAnyAuthForSector(int sector, BYTE& outSlot) {
	const auto& keyA = findKeyOrThrow(KeyType::A);
	const auto& keyB = findKeyOrThrow(KeyType::B);

	if (isSectorAuthorized(sector, KeyType::A, keyA.slot)) {
		outSlot = keyA.slot;
		return KeyType::A;
	}
	if (isSectorAuthorized(sector, KeyType::B, keyB.slot)) {
		outSlot = keyB.slot;
		return KeyType::B;
	}

	try {
		ensureAuthorized(sector, KeyType::A, keyA.slot);
		outSlot = keyA.slot;
		return KeyType::A;
	} catch (...) {}

	try {
		ensureAuthorized(sector, KeyType::B, keyB.slot);
		outSlot = keyB.slot;
		return KeyType::B;
	} catch (...) {
		throw std::runtime_error("Authentication failed for both keys");
	}
}

BYTEV MifareCardCore::read(BYTE block) {
	validateBlockNumber(block);
	if (isTrailerBlock(block)) {
		checkTrailerBlock(block);
	}
	int sector = sectorFromBlock(block);
	KeyType kt = chooseKeyForOperation(sector, false);
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
	KeyType kt = chooseKeyForOperation(sector, true);
	const KeyInfo& ki = findKeyOrThrow(kt);
	writeWithKey(block, data, kt, ki.slot);
}

BYTEV MifareCardCore::readWithKey(BYTE block, KeyType keyType, BYTE keySlot) {
	int sector = sectorFromBlock(block);
	ensureAuthorized(sector, keyType, keySlot);
	return reader().readPage(block);
}

void MifareCardCore::writeWithKey(BYTE block, const BYTEV& data, KeyType keyType, BYTE keySlot) {
	if (data.size() != 16) {
		throw std::invalid_argument("Block data must be 16 bytes");
	}
	int sector = sectorFromBlock(block);
	ensureAuthorized(sector, keyType, keySlot);
	reader().writePage(block, data.data());
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

BLOCK MifareCardCore::readTrailer(int sector) {
	validateSectorNumber(sector);
	BYTE trailerBlock = trailerBlockOf(sector);
	return readBlock(trailerBlock);
}

std::vector<BLOCK> MifareCardCore::readAllTrailers() {
	std::vector<BLOCK> trailers;
	for (size_t i = 0; i < numberOfSectors_; ++i) {
		trailers.push_back(readTrailer(static_cast<int>(i)));
	}
	return trailers;
}

void MifareCardCore::printAllTrailers() {
	std::cout << "Sector Trailers:\n";
	for (size_t i = 0; i < numberOfSectors_; ++i) {
		BLOCK trailer = readTrailer(static_cast<int>(i));
		std::cout << "Sector " << i << ": ";
		for (size_t j = 0; j < 16; ++j) {
			std::cout << std::hex << static_cast<int>(trailer[j]) << " ";
		}
		std::cout << std::dec << "\n";
	}
}

void MifareCardCore::changeTrailer(int sector, const BYTE trailer16[16]) {
	validateSectorNumber(sector);
	if (!SectorConfig::validateAccessBits(trailer16 + 6))
		throw std::runtime_error("changeTrailer: invalid access bits");

	const KeyInfo& ki = findKeyOrThrow(KeyType::B);
	ensureAuthorized(sector, KeyType::B, ki.slot);

	reader().writePage(trailerBlockOf(sector), trailer16);
	invalidateAuthForSector(sector);
}

void MifareCardCore::applySectorConfigToCard(int sector) {
	validateSectorNumber(sector);
	
	const auto& cfg = sectorConfigs_[sector];
	
	// If keys are provided in SectorKeyConfig, use them; otherwise fallback to registered keys
	const BYTE* keyAData = nullptr;
	const BYTE* keyBData = nullptr;
	
	if (cfg.keyA.key[0] != 0 || cfg.keyA.key[1] != 0 || cfg.keyA.key[2] != 0) {
		// keyA is set in config
		keyAData = cfg.keyA.key.data();
	} else {
		// Fallback to registered keys
		requireBothKeys();
		const KeyInfo& ka = findKeyOrThrow(KeyType::A);
		keyAData = ka.key.data();
	}
	
	if (cfg.keyB.key[0] != 0 || cfg.keyB.key[1] != 0 || cfg.keyB.key[2] != 0) {
		// keyB is set in config
		keyBData = cfg.keyB.key.data();
	} else {
		// Fallback to registered keys
		requireBothKeys();
		const KeyInfo& kb = findKeyOrThrow(KeyType::B);
		keyBData = kb.key.data();
	}

	ACCESSBYTES access = SectorConfig::makeSectorAccessBits(cfg);
	if (!SectorConfig::validateAccessBits(access.data()))
		throw std::runtime_error("Generated access bits are invalid");

	BLOCK trailer = SectorConfig::buildTrailer(keyAData, access.data(), keyBData);

	BYTE usedSlot = 0xFF;
	tryAnyAuthForSector(sector, usedSlot);

	reader().writePage(trailerBlockOf(sector), trailer.data());
	invalidateAuthForSector(sector);
}

void MifareCardCore::applySectorConfigStrict(int sector, KeyType authKeyType, bool enableRollback) {
	validateSectorNumber(sector);
	
	const auto& cfg = sectorConfigs_[sector];
	
	// If keys are provided in SectorKeyConfig, use them; otherwise fallback to registered keys
	const BYTE* keyAData = nullptr;
	const BYTE* keyBData = nullptr;
	
	if (cfg.keyA.key[0] != 0 || cfg.keyA.key[1] != 0 || cfg.keyA.key[2] != 0) {
		keyAData = cfg.keyA.key.data();
	} else {
		requireBothKeys();
		const KeyInfo& ka = findKeyOrThrow(KeyType::A);
		keyAData = ka.key.data();
	}
	
	if (cfg.keyB.key[0] != 0 || cfg.keyB.key[1] != 0 || cfg.keyB.key[2] != 0) {
		keyBData = cfg.keyB.key.data();
	} else {
		requireBothKeys();
		const KeyInfo& kb = findKeyOrThrow(KeyType::B);
		keyBData = kb.key.data();
	}

	const KeyInfo& authKey = findKeyOrThrow(authKeyType);

	auto accessBytes = SectorConfig::makeSectorAccessBits(cfg);
	if (!SectorConfig::validateAccessBits(accessBytes.data()))
		throw std::runtime_error("Generated access bits invalid");

	BYTE trailerBlock = trailerBlockOf(sector);

	BLOCK oldTrailer{};
	if (enableRollback)
		oldTrailer = readTrailer(sector);

	BLOCK newTrailer = SectorConfig::buildTrailer(keyAData, accessBytes.data(), keyBData);

	ensureAuthorized(sector, authKeyType, authKey.slot);

	try {
		reader().writePage(trailerBlock, newTrailer.data());
	} catch (...) {
		if (enableRollback) {
			try {
				ensureAuthorized(sector, authKeyType, authKey.slot);
				reader().writePage(trailerBlock, oldTrailer.data());
			} catch (...) {
				std::cerr << "Rollback failed. trailerBlock: "
						  << static_cast<int>(trailerBlock) << std::endl;
			}
		}
		throw;
	}
	invalidateAuthForSector(sector);
}

void MifareCardCore::applyAllSectorsConfig(const std::vector<SectorConfig>& configs) {
	setAllSectorsConfig(configs);
	batchApply([this](int s) { applySectorConfigToCard(s); });
}

void MifareCardCore::applyAllSectorsConfigStrict(const std::vector<SectorConfig>& configs, KeyType authKeyType, bool enableRollback) {
	setAllSectorsConfig(configs);
	batchApply([this, authKeyType, enableRollback](int s) {
		applySectorConfigStrict(s, authKeyType, enableRollback);
	});
}

void MifareCardCore::loadSectorConfigFromCard(int sector) {
	validateSectorNumber(sector);
	BLOCK trailer = readTrailer(sector);
	sectorConfigs_[sector] = SectorConfig::parseAccessBitsToConfig(trailer.data() + 6);
}

void MifareCardCore::loadSectorConfigsFromCard(const std::vector<int>& sectors) {
	for (int s : sectors) {
		loadSectorConfigFromCard(s);
	}
}

void MifareCardCore::validateBlockNumber(int block) const {
	int maxBlock = is4KCard_ ? 255 : 63;
	if (block < 0 || block > maxBlock) {
		throw std::out_of_range("block out of range");
	}
}

void MifareCardCore::throwIfErrors(const std::vector<std::string>& errors, const char* header) {
	if (errors.empty()) return;
	std::string msg = header;
	for (const auto& e : errors) msg += e + "\n";
	throw std::runtime_error(msg);
}
