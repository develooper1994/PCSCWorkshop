#include "SectorConfig.h"
#include "Types.h"
#include <cstring>
#include <stdexcept>
#include <cstdint>

// ════════════════════════════════════════════════════════
// Constructors
// ════════════════════════════════════════════════════════
SectorConfig::SectorConfig() noexcept {
	keyA.kt = KeyType::A;
	keyA.ks = KeyStructure::NonVolatile;
	keyA.slot = 0x01;

	keyB.kt = KeyType::B;
	keyB.ks = KeyStructure::NonVolatile;
	keyB.slot = 0x02;

	accessBits = makeSectorAccessBits(*this);
}

SectorConfig::SectorConfig(bool aR, bool aW, bool bR, bool bW) noexcept {
	// Initialize KeyInfo with permissions
	keyA.kt = KeyType::A;
	keyA.readable = aR;
	keyA.writable = aW;
	keyA.ks = KeyStructure::NonVolatile;
	keyA.slot = 0x01;
	// Default key (FF FF FF FF FF FF)
	std::array<BYTE, 6> defaultKeyA = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	keyA.key = defaultKeyA;
	
	keyB.kt = KeyType::B;
	keyB.readable = bR;
	keyB.writable = bW;
	keyB.ks = KeyStructure::NonVolatile;
	keyB.slot = 0x02;
	// Default key (FF FF FF FF FF FF)
	keyB.key = defaultKeyA;

	accessBits = makeSectorAccessBits(*this);
}

SectorConfig::SectorConfig(const KeyInfo& kA, const KeyInfo& kB) noexcept : keyA(kA), keyB(kB) {
	keyA.kt = KeyType::A;
	keyA.ks = KeyStructure::NonVolatile;
	if (keyA.slot == 0x00) keyA.slot = 0x01;

	keyB.kt = KeyType::B;
	keyB.ks = KeyStructure::NonVolatile;
	if (keyB.slot == 0x00) keyB.slot = 0x02;

	accessBits = makeSectorAccessBits(*this);
}

// ════════════════════════════════════════════════════════
// Access-bits codec (static helpers)
// ════════════════════════════════════════════════════════
SectorConfig SectorConfig::parseAccessBitsToConfig(const BYTE ab[4]) noexcept {
	BYTE c1[4], c2[4], c3[4];
	extractAccessBits(ab, c1, c2, c3);
	SectorConfig cfg = configFromC1C2C3(c1[0], c2[0], c3[0]);
	cfg.accessBits = { ab[0], ab[1], ab[2], ab[3] };
	return cfg;
}

bool SectorConfig::validateAccessBits(const BYTE a[4]) noexcept {
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

void SectorConfig::extractAccessBits(const BYTE ab[4],
									   BYTE c1[4], BYTE c2[4], BYTE c3[4]) noexcept {
	BYTE b6 = ab[0], b7 = ab[1];
	c1[0] = (b6 >> 4) & 1; c1[1] = (b6 >> 5) & 1;
	c1[2] = (b6 >> 6) & 1; c1[3] = (b6 >> 7) & 1;
	c2[0] = (b6 >> 0) & 1; c2[1] = (b6 >> 1) & 1;
	c2[2] = (b6 >> 2) & 1; c2[3] = (b6 >> 3) & 1;
	c3[0] = (b7 >> 4) & 1; c3[1] = (b7 >> 5) & 1;
	c3[2] = (b7 >> 6) & 1; c3[3] = (b7 >> 7) & 1;
}

SectorConfig SectorConfig::configFromC1C2C3(BYTE c1, BYTE c2, BYTE c3) noexcept {
	SectorConfig cfg;
	
	// Set KeyType for keys
	cfg.keyA.kt = KeyType::A;
	cfg.keyB.kt = KeyType::B;

	cfg.keyA.slot = 0x01;
	cfg.keyB.slot = 0x02;

	// Standard key access rules in Mifare Classic:
	// Data blocks (0-2 of each sector):
	//   C1=0,C2=0,C3=0: KeyA read-only,  KeyB read+write
	//   C1=0,C2=1,C3=0: KeyA read-only,  KeyB read-only
	//   C1=1,C2=0,C3=0: KeyA read+write, KeyB read+write
	//   C1=1,C2=1,C3=0: KeyA read+write, KeyB read-only
	
	if (c1 == 0 && c2 == 0 && c3 == 0) {
		// KeyA read-only, KeyB read+write
		cfg.keyA.readable = true;  cfg.keyA.writable = false;
		cfg.keyB.readable = true;  cfg.keyB.writable = true;
	} else if (c1 == 0 && c2 == 1 && c3 == 0) {
		// KeyA read-only, KeyB read-only
		cfg.keyA.readable = true;  cfg.keyA.writable = false;
		cfg.keyB.readable = true;  cfg.keyB.writable = false;
	} else if (c1 == 1 && c2 == 0 && c3 == 0) {
		// KeyA read+write, KeyB read+write
		cfg.keyA.readable = true;  cfg.keyA.writable = true;
		cfg.keyB.readable = true;  cfg.keyB.writable = true;
	} else if (c1 == 1 && c2 == 1 && c3 == 0) {
		// KeyA read+write, KeyB read-only
		cfg.keyA.readable = true;  cfg.keyA.writable = true;
		cfg.keyB.readable = true;  cfg.keyB.writable = false;
	} else {
		// Default: safe configuration
		cfg.keyA.readable = true;  cfg.keyA.writable = false;
		cfg.keyB.readable = true;  cfg.keyB.writable = true;
	}
	return cfg;
}

void SectorConfig::mapDataBlock(const SectorConfig& cfg,
								  BYTE& c1, BYTE& c2, BYTE& c3) {
	const bool keyA_canRead  = cfg.keyA.readable;
	const bool keyA_canWrite = cfg.keyA.writable;
	const bool keyB_canRead  = cfg.keyB.readable;
	const bool keyB_canWrite = cfg.keyB.writable;
	
	if ( keyA_canRead && !keyA_canWrite &&  keyB_canRead &&  keyB_canWrite) { c1=0; c2=1; c3=0; return; }
	else if ( keyA_canRead &&  keyA_canWrite &&  keyB_canRead &&  keyB_canWrite) { c1=0; c2=0; c3=0; return; }
	else if ( keyA_canRead &&  keyA_canWrite &&  keyB_canRead && !keyB_canWrite) { c1=1; c2=0; c3=0; return; }
	else if ( keyA_canRead && !keyA_canWrite &&  keyB_canRead && !keyB_canWrite) { c1=1; c2=1; c3=0; return; }
	else throw std::runtime_error("Unsupported access combination");
}

ACCESSBYTES SectorConfig::makeSectorAccessBits(const SectorConfig& dataCfg) noexcept {
	const int SMALL_SECTOR_PAGES = 4;
	BYTE c1[SMALL_SECTOR_PAGES], c2[SMALL_SECTOR_PAGES], c3[SMALL_SECTOR_PAGES];
	for (int i = 0; i < SMALL_SECTOR_PAGES - 1; ++i)
		mapDataBlock(dataCfg, c1[i], c2[i], c3[i]);

	// trailer: access bits are read-only (never writable)
	c1[3] = 0; c2[3] = 1; c3[3] = 0;

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

BLOCK SectorConfig::buildTrailer(const BYTE keyA[6],
									 const BYTE access[4],
									 const BYTE keyB[6]) noexcept {
	BLOCK t;
	std::memcpy(t.data(),      keyA,   6);
	std::memcpy(t.data() + 6,  access, 4);
	std::memcpy(t.data() + 10, keyB,   6);
	return t;
}
