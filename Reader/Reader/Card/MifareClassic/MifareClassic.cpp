#include "MifareClassic.h"

// ════════════════════════════════════════════════════════
//  SectorKeyConfig
// ════════════════════════════════════════════════════════
MifareCardCore::SectorKeyConfig::SectorKeyConfig(bool aR, bool aW,
												  bool bR, bool bW)
	: keyA_canRead(aR), keyA_canWrite(aW),
	  keyB_canRead(bR), keyB_canWrite(bW) {}

// ════════════════════════════════════════════════════════
//  Construction
// ════════════════════════════════════════════════════════
MifareCardCore::MifareCardCore(Reader& r, bool is4K)
	: reader_(r),
	  is4KCard_(is4K),
	  numberOfSectors_(is4K ? 40 : 16),
	  sectorConfigs_(numberOfSectors_, SectorKeyConfig{})
{
	// -> load key into reader and also give it to the card (slot 0x01 used in older examples)
	const std::array<BYTE, 6> defaultKey6 = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	const BYTE keySlotA = 0x01; // as in your earlier example
	const BYTE keySlotB = 0x02; // as in your earlier example

	// Also register the key into card so it knows which slot to use for auth decisions.
	// setKey signature might be different in your core; adjust if necessary.
	setKey(MifareCardCore::KeyType::A, std::array<BYTE, 6>{defaultKey6[0], defaultKey6[1], defaultKey6[2], defaultKey6[3], defaultKey6[4], defaultKey6[5]},
		KeyStructure::NonVolatile, keySlotA);
	setKey(MifareCardCore::KeyType::B, std::array<BYTE, 6>{defaultKey6[0], defaultKey6[1], defaultKey6[2], defaultKey6[3], defaultKey6[4], defaultKey6[5]},
		/*keyStructure*/ KeyStructure::NonVolatile, keySlotB);
	// If your ACR1281UReader expects storage type (NonVolatile/Volatile), use that value:
}

int MifareCardCore::getSectorCount() const {
	return numberOfSectors_;
}

// ════════════════════════════════════════════════════════
//  Key management
// ════════════════════════════════════════════════════════
void MifareCardCore::setKey(KeyType kt, const std::array<BYTE, 6>& key,
							KeyStructure ks, BYTE slot) {
	KeyInfo ki;
	ki.key = key;  ki.ks = ks;  ki.slot = slot;  ki.kt = kt;
	auto it = std::find_if(keys_.begin(), keys_.end(),
						   [kt](const KeyInfo& k) { return k.kt == kt; });
	if (it != keys_.end()) *it = ki;
	else keys_.push_back(ki);
}

// ════════════════════════════════════════════════════════
//  Sector config (host-side permission model)
// ════════════════════════════════════════════════════════
void MifareCardCore::setSectorConfig(int sector, const SectorKeyConfig& cfg) {
	validateSectorNumber(sector);
	sectorConfigs_[sector] = cfg;
}

void MifareCardCore::setAllSectorsConfig(const SectorKeyConfig& cfg) {
	std::fill(sectorConfigs_.begin(), sectorConfigs_.end(), cfg);
}

void MifareCardCore::setAllSectorsConfig(const std::map<int, SectorKeyConfig>& configs) {
	for (const auto& p : configs) {
		validateSectorNumber(p.first);
		sectorConfigs_[p.first] = p.second;
	}
}

void MifareCardCore::setAllSectorsConfig(const std::vector<SectorKeyConfig>& configs) {
	if (configs.size() > static_cast<size_t>(numberOfSectors_))
		throw std::out_of_range("configs vector size exceeds number of sectors");
	for (size_t i = 0; i < configs.size(); ++i)
		sectorConfigs_[static_cast<int>(i)] = configs[i];
}

// ════════════════════════════════════════════════════════
//  Layout helpers (public)
// ════════════════════════════════════════════════════════
int MifareCardCore::blockToSector(BYTE block) const {
	int b = static_cast<int>(block);
	if (!is4KCard_) {
		if (b < 0 || b >= 64)
			throw std::out_of_range("Invalid block for 1K card");
		return b / 4;
	}
	if (b < 0 || b >= 256)
		throw std::out_of_range("Invalid block for 4K card");
	return b < 128 ? b / 4 : 32 + (b - 128) / 16;
}

int MifareCardCore::blockIndexInSector(BYTE block) const {
	int b = static_cast<int>(block);
	if (!is4KCard_) {
		if (b < 0 || b >= 64)
			throw std::out_of_range("Invalid block for 1K card");
		return b % 4;
	}
	if (b < 0 || b >= 256)
		throw std::out_of_range("Invalid block for 4K card");
	return b < 128 ? b % 4 : (b - 128) % 16;
}

int MifareCardCore::firstBlockInSector(int sector) const {
	validateSectorNumber(sector);
	return firstBlockOfSector(sector);
}

int MifareCardCore::blocksInSector(int sector) const {
	validateSectorNumber(sector);
	return blocksPerSector(sector);
}

// ════════════════════════════════════════════════════════
//  Authentication
// ════════════════════════════════════════════════════════
void MifareCardCore::ensureAuthorized(int sector, KeyType kt, BYTE keySlot) {
	if (isSectorAuthorized(sector, kt, keySlot))
		return;

	const KeyInfo& ki = findKeyOrThrow(kt);
	reader_.loadKey(ki.key.data(), ki.ks, ki.slot);

	BYTE authBlock = static_cast<BYTE>(firstBlockOfSector(sector));
	reader_.setKeyType(kt);
	reader_.setKeyNumber(keySlot);
	reader_.setAuthRequested(true);
	try {
		reader_.performAuth(authBlock);
	} catch (...) {
		invalidateAuthForSector(sector);
		throw;
	}

	setAuthCache(sector, kt, ki.slot);
}

KeyType MifareCardCore::tryAnyAuthForSector(int sector, BYTE& outSlot) {
	for (const auto& ki : keys_) {
		try {
			ensureAuthorized(sector, ki.kt, ki.slot);
			outSlot = ki.slot;
			return ki.kt;
		} catch (...) { /* next key */ }
	}
	throw std::runtime_error("No loaded key could authenticate for this sector");
}

// ════════════════════════════════════════════════════════
//  Single-block read / write  (cache-aware)
// ════════════════════════════════════════════════════════

// Auto key selection versions (recommended)
BYTEV MifareCardCore::read(BYTE block) {
	validateBlockNumber(block);
	if (isTrailerBlock(block))
		throw std::runtime_error("read: trailer block - use readTrailer");

	int sector = blockToSector(block);
	KeyType chosen = chooseKeyForOperation(sector, false);
	const KeyInfo& ki = findKeyOrThrow(chosen);

	if (isSectorAuthorized(sector, chosen, ki.slot)) {
		try { return reader_.readPage(block); }
		catch (const pcsc::AuthFailedError&) {
			invalidateAuthForSector(sector);
		}
	}
	ensureAuthorized(sector, chosen, ki.slot);
	return reader_.readPage(block);
}

void MifareCardCore::write(BYTE block, const BYTEV& data) {
	validateBlockNumber(block);
	checkTrailerBlock(block);
	checkManufacturerBlock(block);

	int sector = blockToSector(block);
	KeyType chosen = chooseKeyForOperation(sector, true);
	const KeyInfo& ki = findKeyOrThrow(chosen);

	if (isSectorAuthorized(sector, chosen, ki.slot)) {
		try { reader_.writePage(block, data.data()); return; }
		catch (const pcsc::AuthFailedError&) {
			invalidateAuthForSector(sector);
		}
	}
	ensureAuthorized(sector, chosen, ki.slot);
	reader_.writePage(block, data.data());
}

void MifareCardCore::write(BYTE block, const std::string& text) {
	write(block, BYTEV(text.begin(), text.end()));
}

// Explicit key specification versions (for advanced use)
BYTEV MifareCardCore::readWithKey(BYTE block, KeyType keyType, BYTE keySlot) {
	int sector = blockToSector(block);

	if (isSectorAuthorized(sector, keyType, keySlot)) {
		try { return reader_.readPage(block); }
		catch (const pcsc::AuthFailedError&) {
			invalidateAuthForSector(sector);
		}
	}
	ensureAuthorized(sector, keyType, keySlot);
	return reader_.readPage(block);
}

void MifareCardCore::writeWithKey(BYTE block, const BYTEV& data, KeyType keyType, BYTE keySlot) {
	int sector = blockToSector(block);

	if (isSectorAuthorized(sector, keyType, keySlot)) {
		try { reader_.writePage(block, data.data()); return; }
		catch (const pcsc::AuthFailedError&) {
			invalidateAuthForSector(sector);
		}
	}
	ensureAuthorized(sector, keyType, keySlot);
	reader_.writePage(block, data.data());
}

void MifareCardCore::writeWithKey(BYTE block, const std::string& text,
								   KeyType keyType, BYTE keySlot) {
	writeWithKey(block, BYTEV(text.begin(), text.end()), keyType, keySlot);
}

// ════════════════════════════════════════════════════════
//  Convenience single-block (auto key selection)
// ════════════════════════════════════════════════════════
MifareCardCore::BLOCK MifareCardCore::readBlock(BYTE block) {
	validateBlockNumber(block);
	if (isTrailerBlock(block))
		throw std::runtime_error("readBlock: trailer block - use readTrailer");

	int sector   = sectorFromBlock(block);
	KeyType chosen = chooseKeyForOperation(sector, false);
	const KeyInfo& ki = findKeyOrThrow(chosen);
	ensureAuthorized(sector, chosen, ki.slot);

	BYTEV page = reader_.readPage(block);
	if (page.size() < 16) throw std::runtime_error("readBlock: short read");
	BLOCK out;
	std::memcpy(out.data(), page.data(), 16);
	return out;
}

void MifareCardCore::writeBlock(BYTE block, const BYTE data[16]) {
	validateBlockNumber(block);
	checkTrailerBlock(block);
	checkManufacturerBlock(block);

	int sector   = sectorFromBlock(block);
	KeyType chosen = chooseKeyForOperation(sector, true);
	const KeyInfo& ki = findKeyOrThrow(chosen);
	ensureAuthorized(sector, chosen, ki.slot);

	reader_.writePage(block, data);
}

// ════════════════════════════════════════════════════════
//  Linear (multi-block) read / write
// ════════════════════════════════════════════════════════
BYTEV MifareCardCore::readLinear(BYTE startBlock, size_t length) {
	BYTEV out;
	out.reserve(length);
	size_t remaining = length;
	int blk = static_cast<int>(startBlock);

	while (remaining > 0) {
		validateBlockNumber(blk);
		if (isTrailerBlock(static_cast<BYTE>(blk)))
			throw std::runtime_error("readLinear: encountered trailer block");

		int sector   = sectorFromBlock(static_cast<BYTE>(blk));
		KeyType chosen = chooseKeyForOperation(sector, false);
		const KeyInfo& ki = findKeyOrThrow(chosen);
		ensureAuthorized(sector, chosen, ki.slot);

		BYTEV page = reader_.readPage(static_cast<BYTE>(blk));
		if (page.size() < 16)
			throw std::runtime_error("readLinear: short page read");

		size_t take = std::min<size_t>(16, remaining);
		out.insert(out.end(), page.begin(), page.begin() + take);
		remaining -= take;
		++blk;
	}
	return out;
}

void MifareCardCore::writeLinear(BYTE startBlock, const BYTEV& data) {
	size_t remaining = data.size();
	size_t offset = 0;
	int blk = static_cast<int>(startBlock);

	while (remaining > 0) {
		validateBlockNumber(blk);
		if (isTrailerBlock(static_cast<BYTE>(blk)))
			throw std::runtime_error("writeLinear: encountered trailer block");
		if (isManufacturerBlock(static_cast<BYTE>(blk)))
			throw std::runtime_error("writeLinear: manufacturer block is read-only");

		int sector   = sectorFromBlock(static_cast<BYTE>(blk));
		KeyType chosen = chooseKeyForOperation(sector, true);
		const KeyInfo& ki = findKeyOrThrow(chosen);
		ensureAuthorized(sector, chosen, ki.slot);

		BYTE page[16];
		std::memset(page, 0, sizeof(page));
		size_t take = std::min<size_t>(16, remaining);
		std::memcpy(page, data.data() + offset, take);
		reader_.writePage(static_cast<BYTE>(blk), page);

		remaining -= take;
		offset    += take;
		++blk;
	}
}

// ════════════════════════════════════════════════════════
//  Trailer operations
// ════════════════════════════════════════════════════════
MifareCardCore::BLOCK MifareCardCore::readTrailer(int sector) {
	validateSectorNumber(sector);
	BYTE trailer = trailerBlockOf(sector);

	KeyType chosen = chooseKeyForOperation(sector, false);
	const KeyInfo& ki = findKeyOrThrow(chosen);
	ensureAuthorized(sector, chosen, ki.slot);

	BYTEV page = reader_.readPage(trailer);
	if (page.size() < 16)
		throw std::runtime_error("readTrailer: short read");
	BLOCK out;
	std::memcpy(out.data(), page.data(), 16);
	return out;
}

std::vector<MifareCardCore::BLOCK> MifareCardCore::readAllTrailers() {
	std::vector<BLOCK> trailers;
	for (int s = 0; s < numberOfSectors_; ++s) {
		try {
			trailers.push_back(readTrailer(s));
		} catch (const std::exception& e) {
			std::cerr << "Failed to read trailer for sector " << s
					  << ": " << e.what() << std::endl;
			trailers.emplace_back(); // push empty block on failure
		}
	}
	return trailers;
}

void MifareCardCore::printAllTrailers() {
	for (int sector = 0; sector < getSectorCount(); ++sector) {
		try {
			BLOCK trailer = readTrailer(sector);
			// Print raw trailer bytes (uses existing printHex overload that accepts a vector<BYTE>)
			std::vector<BYTE> tvec(trailer.begin(), trailer.end());
			std::cout << "Sector " << sector << " trailer: ";
			printHex(tvec);
			// If trailer has expected layout (KeyA[0..5], AccessBits[6..8], GP/Reserved[9], KeyB[10..15])
			if (tvec.size() >= 10)
				std::cout << " AccessBits: 0x" << std::hex << static_cast<int>(tvec[6])
						  << " 0x" << std::hex << static_cast<int>(tvec[7])
						  << " 0x" << std::hex << static_cast<int>(tvec[8])
						  << std::dec << " GP: " << static_cast<int>(tvec[9]);
		} catch (const std::exception& e) {
			std::cerr << "Failed to read trailer for sector " << sector
					  << ": " << e.what() << std::endl;
			std::cout << "<missing>";
		}
		std::cout << std::endl;
	}
}

void MifareCardCore::changeTrailer(int sector, const BYTE trailer16[16]) {
	validateSectorNumber(sector);
	if (!validateAccessBits(trailer16 + 6))
		throw std::runtime_error("changeTrailer: invalid access bits");

	const KeyInfo& ki = findKeyOrThrow(KeyType::B);
	ensureAuthorized(sector, KeyType::B, ki.slot);

	reader_.writePage(trailerBlockOf(sector), trailer16);
	invalidateAuthForSector(sector);
}

void MifareCardCore::applySectorConfigToCard(int sector) {
	validateSectorNumber(sector);
	requireBothKeys();

	ACCESSBYTES access = makeSectorAccessBits(sectorConfigs_[sector]);
	if (!validateAccessBits(access.data()))
		throw std::runtime_error("Generated access bits are invalid");

	const KeyInfo& ka = findKeyOrThrow(KeyType::A);
	const KeyInfo& kb = findKeyOrThrow(KeyType::B);
	BLOCK trailer = buildTrailer(ka.key.data(), access.data(), kb.key.data());

	BYTE usedSlot = 0xFF;
	tryAnyAuthForSector(sector, usedSlot);

	reader_.writePage(trailerBlockOf(sector), trailer.data());
	invalidateAuthForSector(sector);
}

void MifareCardCore::applySectorConfigStrict(int sector,
											  KeyType authKeyType,
											  bool enableRollback) {
	validateSectorNumber(sector);
	requireBothKeys();
	const KeyInfo& authKey = findKeyOrThrow(authKeyType);

	ACCESSBYTES access = makeSectorAccessBits(sectorConfigs_[sector]);
	if (!validateAccessBits(access.data()))
		throw std::runtime_error("Generated access bits invalid");

	BYTE trailerBlock = trailerBlockOf(sector);

	BLOCK oldTrailer{};
	if (enableRollback)
		oldTrailer = readTrailer(sector);

	const KeyInfo& ka = findKeyOrThrow(KeyType::A);
	const KeyInfo& kb = findKeyOrThrow(KeyType::B);
	BLOCK newTrailer = buildTrailer(ka.key.data(), access.data(), kb.key.data());

	ensureAuthorized(sector, authKeyType, authKey.slot);

	try {
		reader_.writePage(trailerBlock, newTrailer.data());
	} catch (...) {
		if (enableRollback) {
			try {
				ensureAuthorized(sector, authKeyType, authKey.slot);
				reader_.writePage(trailerBlock, oldTrailer.data());
			} catch (...) {
				std::cerr << "Rollback failed. trailerBlock: "
						  << static_cast<int>(trailerBlock) << std::endl;
			}
		}
		throw;
	}
	invalidateAuthForSector(sector);
}

// ════════════════════════════════════════════════════════
//  Batch apply / load config helpers
// ════════════════════════════════════════════════════════
void MifareCardCore::applyAllSectorsConfig() {
	batchApply([this](int s) { applySectorConfigToCard(s); });
}

void MifareCardCore::applyAllSectorsConfig(const std::map<int, SectorKeyConfig>& configs) {
	setAllSectorsConfig(configs);
	batchApply([this](int s) { applySectorConfigToCard(s); });
}

void MifareCardCore::applyAllSectorsConfig(const std::vector<SectorKeyConfig>& configs) {
	setAllSectorsConfig(configs);
	batchApply([this](int s) { applySectorConfigToCard(s); });
}

void MifareCardCore::applyAllSectorsConfigStrict(KeyType authKeyType,
												  bool enableRollback) {
	batchApply([this, authKeyType, enableRollback](int s) {
		applySectorConfigStrict(s, authKeyType, enableRollback);
	});
}

void MifareCardCore::applyAllSectorsConfigStrict(
		const std::map<int, SectorKeyConfig>& configs,
		KeyType authKeyType,
		bool enableRollback) {
	setAllSectorsConfig(configs);
	batchApply([this, authKeyType, enableRollback](int s) {
		applySectorConfigStrict(s, authKeyType, enableRollback);
	});
}

void MifareCardCore::applyAllSectorsConfigStrict(
		const std::vector<SectorKeyConfig>& configs,
		KeyType authKeyType,
		bool enableRollback) {
	setAllSectorsConfig(configs);
	batchApply([this, authKeyType, enableRollback](int s) {
		applySectorConfigStrict(s, authKeyType, enableRollback);
	});
}

void MifareCardCore::loadSectorConfigFromCard(int sector) {
	validateSectorNumber(sector);
	BLOCK trailer = readTrailer(sector);
	const BYTE* ab = trailer.data() + 6;
	if (!validateAccessBits(ab))
		throw std::runtime_error("Invalid access bits in sector "
								 + std::to_string(sector));
	sectorConfigs_[sector] = parseAccessBitsToConfig(ab);
}

void MifareCardCore::loadAllSectorConfigsFromCard() {
	batchApply([this](int s) { loadSectorConfigFromCard(s); });
}

void MifareCardCore::loadSectorConfigsFromCard(const std::vector<int>& sectors) {
	std::vector<std::string> errors;
	for (int s : sectors) {
		try { loadSectorConfigFromCard(s); }
		catch (const std::exception& e) {
			errors.push_back("Sector " + std::to_string(s) + ": " + e.what());
		}
	}
	throwIfErrors(errors, "Failed to load config from some sectors:\n");
}

// ════════════════════════════════════════════════════════
//  Private helpers
// ════════════════════════════════════════════════════════
bool MifareCardCore::isSectorAuthorized(int sector, KeyType kt, BYTE slot) const {
	if (sector < 0 || sector >= static_cast<int>(authCache_.size()))
		return false;
	const KeyInfo& ci = authCache_[sector];
	return ci.slot != 0xFF && ci.kt == kt && ci.slot == slot;
}

void MifareCardCore::setAuthCache(int sector, KeyType kt, BYTE slot) {
	ensureAuthCacheSize(sector);
	authCache_[sector].kt   = kt;
	authCache_[sector].slot = slot;
}

void MifareCardCore::invalidateAuthForSector(int sector) {
	if (sector >= 0 && sector < static_cast<int>(authCache_.size()))
		authCache_[sector].slot = 0xFF;   // sentinel: not authorized
}

void MifareCardCore::ensureAuthCacheSize(int sector) {
	if (static_cast<int>(authCache_.size()) <= sector) {
		KeyInfo empty;
		empty.slot = 0xFF;  // sentinel
		authCache_.resize(sector + 1, empty);
	}
}

int MifareCardCore::sectorFromBlock(BYTE block) const {
	int b = static_cast<int>(block);
	return (!is4KCard_ || b < 128) ? b / 4 : 32 + (b - 128) / 16;
}

int MifareCardCore::blocksPerSector(int sector) const {
	return (!is4KCard_ || sector < 32) ? 4 : 16;
}

int MifareCardCore::firstBlockOfSector(int sector) const {
	return (!is4KCard_ || sector < 32) ? sector * 4 : 128 + (sector - 32) * 16;
}

BYTE MifareCardCore::trailerBlockOf(int sector) const {
	return static_cast<BYTE>(firstBlockOfSector(sector)
							 + blocksPerSector(sector) - 1);
}

bool MifareCardCore::isTrailerBlock(BYTE block) const {
	return block == trailerBlockOf(sectorFromBlock(block));
}

bool MifareCardCore::isManufacturerBlock(BYTE block) const {
	return block == 0;
}

void MifareCardCore::checkTrailerBlock(BYTE block) const {
	if (isTrailerBlock(block))
		throw std::runtime_error("writeBlock: trailer block - use changeTrailer");
}

void MifareCardCore::checkManufacturerBlock(BYTE block) const {
	if (isManufacturerBlock(block))
		throw std::runtime_error("writeBlock: manufacturer block is read-only");
}

void MifareCardCore::validateBlockNumber(int block) const {
	if (block < 0) throw std::out_of_range("block < 0");
	int maxBlocks = is4KCard_ ? (128 + (40 - 32) * 16) : (16 * 4);
	if (block >= maxBlocks) throw std::out_of_range("block out of range");
}

void MifareCardCore::validateSectorNumber(int sector) const {
	if (sector < 0 || sector >= numberOfSectors_)
		throw std::out_of_range("sector out of range");
}

KeyType MifareCardCore::chooseKeyForOperation(int sector, bool isWrite) const {
	validateSectorNumber(sector);
	const SectorKeyConfig& cfg = sectorConfigs_[sector];
	if (isWrite) {
		if (cfg.keyB_canWrite) return KeyType::B;
		if (cfg.keyA_canWrite) return KeyType::A;
		throw std::runtime_error("no key allowed to write in this sector");
	}
	if (cfg.keyA_canRead) return KeyType::A;
	if (cfg.keyB_canRead) return KeyType::B;
	throw std::runtime_error("no key allowed to read in this sector");
}

const KeyInfo& MifareCardCore::findKeyOrThrow(KeyType kt) const {
	auto it = std::find_if(keys_.begin(), keys_.end(),
						   [kt](const KeyInfo& k) { return k.kt == kt; });
	if (it == keys_.end())
		throw std::runtime_error("key not set for requested KeyType");
	return *it;
}

bool MifareCardCore::hasKey(KeyType kt) const {
	return std::any_of(keys_.begin(), keys_.end(),
					   [kt](const KeyInfo& k) { return k.kt == kt; });
}

void MifareCardCore::requireBothKeys() const {
	if (!hasKey(KeyType::A) || !hasKey(KeyType::B))
		throw std::runtime_error(
			"Both KeyType::A and KeyType::B must be set before changing trailer");
}

void MifareCardCore::throwIfErrors(const std::vector<std::string>& errors,
									const char* header) {
	if (errors.empty()) return;
	std::string msg = header;
	for (const auto& e : errors) msg += e + "\n";
	throw std::runtime_error(msg);
}

// ════════════════════════════════════════════════════════
//  Access-bits codec (static helpers)
// ════════════════════════════════════════════════════════
MifareCardCore::SectorKeyConfig MifareCardCore::parseAccessBitsToConfig(const BYTE ab[4]) {
	BYTE c1[4], c2[4], c3[4];
	extractAccessBits(ab, c1, c2, c3);
	return configFromC1C2C3(c1[0], c2[0], c3[0]);
}

bool MifareCardCore::validateAccessBits(const BYTE a[4]) {
	BYTE c1[4], c2[4], c3[4];
	extractAccessBits(a, c1, c2, c3);

	BYTE b7 = a[1], b8 = a[2];
	BYTE expected_b7_lo = static_cast<BYTE>(
		((~c1[0] & 1)) | ((~c1[1] & 1) << 1) |
		((~c1[2] & 1) << 2) | ((~c1[3] & 1) << 3));
	if ((b7 & 0x0F) != expected_b7_lo) return false;

	BYTE expected_b8_hi = static_cast<BYTE>(
		((~c2[0] & 1)) | ((~c2[1] & 1) << 1) |
		((~c2[2] & 1) << 2) | ((~c2[3] & 1) << 3));
	if (((b8 >> 4) & 0x0F) != expected_b8_hi) return false;

	BYTE expected_b8_lo = static_cast<BYTE>(
		((~c3[0] & 1)) | ((~c3[1] & 1) << 1) |
		((~c3[2] & 1) << 2) | ((~c3[3] & 1) << 3));
	if ((b8 & 0x0F) != expected_b8_lo) return false;

	return true;
}

void MifareCardCore::extractAccessBits(const BYTE ab[4],
									   BYTE c1[4], BYTE c2[4], BYTE c3[4]) {
	BYTE b6 = ab[0], b7 = ab[1];
	c1[0] = (b6 >> 4) & 1; c1[1] = (b6 >> 5) & 1;
	c1[2] = (b6 >> 6) & 1; c1[3] = (b6 >> 7) & 1;
	c2[0] = (b6 >> 0) & 1; c2[1] = (b6 >> 1) & 1;
	c2[2] = (b6 >> 2) & 1; c2[3] = (b6 >> 3) & 1;
	c3[0] = (b7 >> 4) & 1; c3[1] = (b7 >> 5) & 1;
	c3[2] = (b7 >> 6) & 1; c3[3] = (b7 >> 7) & 1;
}

MifareCardCore::SectorKeyConfig MifareCardCore::configFromC1C2C3(BYTE c1, BYTE c2, BYTE c3) {
	SectorKeyConfig cfg;
	if      (c1==0 && c2==0 && c3==0) { cfg = {true,  true,  true,  true};  }
	else if (c1==0 && c2==1 && c3==0) { cfg = {true,  false, true,  true};  }
	else if (c1==1 && c2==0 && c3==0) { cfg = {true,  true,  true,  false}; }
	else if (c1==1 && c2==1 && c3==0) { cfg = {true,  false, true,  false}; }
	else if (c1==0 && c2==0 && c3==1) { cfg = {false, false, true,  true};  }
	else if (c1==0 && c2==1 && c3==1) { cfg = {false, false, false, true};  }
	else if (c1==1 && c2==0 && c3==1) { cfg = {false, false, true,  true};  }
	else if (c1==1 && c2==1 && c3==1) { cfg = {false, false, false, false}; }
	else                               { cfg = {true,  false, true,  true};  }
	return cfg;
}

void MifareCardCore::mapDataBlock(const SectorKeyConfig& cfg,
								  BYTE& c1, BYTE& c2, BYTE& c3) {
	if ( cfg.keyA_canRead && !cfg.keyA_canWrite &&  cfg.keyB_canRead &&  cfg.keyB_canWrite) { c1=0; c2=1; c3=0; return; }
	if ( cfg.keyA_canRead &&  cfg.keyA_canWrite &&  cfg.keyB_canRead &&  cfg.keyB_canWrite) { c1=0; c2=0; c3=0; return; }
	if ( cfg.keyA_canRead &&  cfg.keyA_canWrite &&  cfg.keyB_canRead && !cfg.keyB_canWrite) { c1=1; c2=0; c3=0; return; }
	if ( cfg.keyA_canRead && !cfg.keyA_canWrite &&  cfg.keyB_canRead && !cfg.keyB_canWrite) { c1=1; c2=1; c3=0; return; }
	throw std::runtime_error("Unsupported access combination");
}

MifareCardCore::ACCESSBYTES MifareCardCore::makeSectorAccessBits(const SectorKeyConfig& dataCfg) {
	BYTE c1[SMALL_SECTOR_PAGES], c2[SMALL_SECTOR_PAGES], c3[SMALL_SECTOR_PAGES];
	for (int i = 0; i < SMALL_SECTOR_PAGES - 1; ++i)
		mapDataBlock(dataCfg, c1[i], c2[i], c3[i]);

	// trailer: KeyA never readable, KeyB can write keys+access
	c1[3] = 1; c2[3] = 1; c3[3] = 0;

	auto nibble = [](const BYTE v[4]) -> BYTE {
		return static_cast<BYTE>((v[3]<<3)|(v[2]<<2)|(v[1]<<1)|v[0]);
	};
	BYTE n1 = nibble(c1), n2 = nibble(c2), n3 = nibble(c3);

	BYTE b6 = static_cast<BYTE>((n1 << 4) | (n2 & 0x0F));
	BYTE b7 = static_cast<BYTE>((n3 << 4) | (static_cast<BYTE>(~n1) & 0x0F));
	BYTE b8 = static_cast<BYTE>(((static_cast<BYTE>(~n2) & 0x0F) << 4)
								| (static_cast<BYTE>(~n3) & 0x0F));
	return {{ b6, b7, b8, 0x00 }};
}

MifareCardCore::BLOCK MifareCardCore::buildTrailer(const BYTE keyA[6],
													const BYTE access[4],
													const BYTE keyB[6]) noexcept {
	BLOCK t;
	std::memcpy(t.data(),      keyA,   6);
	std::memcpy(t.data() + 6,  access, 4);
	std::memcpy(t.data() + 10, keyB,   6);
	return t;
}
