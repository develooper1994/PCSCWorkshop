#include "SectorConfig.h"
#include "Types.h"
#include <cstring>
#include <stdexcept>
#include <cstdint>

// ════════════════════════════════════════════════════════════════════════════════
// Constructors
// ════════════════════════════════════════════════════════════════════════════════

// Default: FF...FF keys, standard read/write permissions
SectorConfig::SectorConfig() noexcept {
	keyA.kt = KeyType::A;
	keyA.ks = KeyStructure::NonVolatile;
	keyA.slot = 0x01;
	// keyA.key defaults to {0,0,0,0,0,0} → display FF...FF

	keyB.kt = KeyType::B;
	keyB.ks = KeyStructure::NonVolatile;
	keyB.slot = 0x02;
	// keyB.key defaults to {0,0,0,0,0,0} → display FF...FF

	accessBits = makeSectorAccessBits(*this);
}

// Constructor with per-key read/write flags
SectorConfig::SectorConfig(bool aR, bool aW, bool bR, bool bW) noexcept {
	keyA.kt = KeyType::A;
	keyA.readable = aR;
	keyA.writable = aW;
	keyA.ks = KeyStructure::NonVolatile;
	keyA.slot = 0x01;
	keyA.key = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	
	keyB.kt = KeyType::B;
	keyB.readable = bR;
	keyB.writable = bW;
	keyB.ks = KeyStructure::NonVolatile;
	keyB.slot = 0x02;
	keyB.key = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

	accessBits = makeSectorAccessBits(*this);
}

// Constructor from explicit KeyInfo objects
SectorConfig::SectorConfig(const KeyInfo& kA, const KeyInfo& kB) noexcept 
	: keyA(kA), keyB(kB) {
	
	keyA.kt = KeyType::A;
	keyA.ks = KeyStructure::NonVolatile;
	if (keyA.slot == 0x00) keyA.slot = 0x01;

	keyB.kt = KeyType::B;
	keyB.ks = KeyStructure::NonVolatile;
	if (keyB.slot == 0x00) keyB.slot = 0x02;

	accessBits = makeSectorAccessBits(*this);
}

// ════════════════════════════════════════════════════════════════════════════════
// Access Bits Parsing & Generation (Public Interface)
// ════════════════════════════════════════════════════════════════════════════════

// Read 4 access bytes from card trailer → extract permissions
SectorConfig SectorConfig::parseAccessBitsToConfig(const BYTE ab[4]) noexcept {
	BYTE c1[4], c2[4], c3[4];
	extractAccessBits(ab, c1, c2, c3);
	SectorConfig cfg = configFromC1C2C3(c1[0], c2[0], c3[0]);
	cfg.accessBits = { ab[0], ab[1], ab[2], ab[3] };
	return cfg;
}

// Verify access bytes consistency (check complement bits)
bool SectorConfig::validateAccessBits(const BYTE a[4]) noexcept {
	BYTE c1[4], c2[4], c3[4];
	extractAccessBits(a, c1, c2, c3);

	// Validate byte 7: bits [3:0] must be ~C1
	BYTE b7 = a[1];
	BYTE expected_b7_lo = static_cast<BYTE>(
		((~c1[0] & 1)) | ((~c1[1] & 1) << 1) |
		((~c1[2] & 1) << 2) | ((~c1[3] & 1) << 3));
	if ((b7 & 0x0F) != expected_b7_lo) return false;

	// Validate byte 8: bits [7:4] must be ~C2
	BYTE b8 = a[2];
	BYTE expected_b8_hi = static_cast<BYTE>(
		((~c2[0] & 1)) | ((~c2[1] & 1) << 1) |
		((~c2[2] & 1) << 2) | ((~c2[3] & 1) << 3));
	if (((b8 >> 4) & 0x0F) != expected_b8_hi) return false;

	// Validate byte 8: bits [3:0] must be ~C3
	BYTE expected_b8_lo = static_cast<BYTE>(
		((~c3[0] & 1)) | ((~c3[1] & 1) << 1) |
		((~c3[2] & 1) << 2) | ((~c3[3] & 1) << 3));
	if ((b8 & 0x0F) != expected_b8_lo) return false;

	return true;
}

// Generate access bytes from SectorConfig permissions
ACCESSBYTES SectorConfig::makeSectorAccessBits(const SectorConfig& dataCfg) noexcept {
	// For each of the 4 blocks, determine C1, C2, C3 bits
	const int BLOCK_COUNT = 4;
	BYTE c1[BLOCK_COUNT], c2[BLOCK_COUNT], c3[BLOCK_COUNT];
	
	// Blocks 0-2: data blocks (use SectorConfig permissions)
	for (int i = 0; i < BLOCK_COUNT - 1; ++i) {
		mapDataBlock(dataCfg, c1[i], c2[i], c3[i]);
	}

	// Block 3: trailer block (always read-protected)
	// C1=0, C2=1, C3=0 → readable by key A/B, writable with key B only
	c1[3] = 0; c2[3] = 1; c3[3] = 0;

	// Pack nibbles into access bytes
	auto packNibble = [](const BYTE v[BLOCK_COUNT]) -> BYTE {
		return static_cast<BYTE>((v[3]<<3)|(v[2]<<2)|(v[1]<<1)|v[0]);
	};
	BYTE n1 = packNibble(c1), n2 = packNibble(c2), n3 = packNibble(c3);

	// Encode into bytes 6, 7, 8
	BYTE b6 = static_cast<BYTE>((n1 << 4) | (n2 & 0x0F));
	BYTE b7 = static_cast<BYTE>((n3 << 4) | (static_cast<BYTE>(~n1) & 0x0F));
	BYTE b8 = static_cast<BYTE>(((static_cast<BYTE>(~n2) & 0x0F) << 4)
								| (static_cast<BYTE>(~n3) & 0x0F));
	
	return {{ b6, b7, b8, 0x00 }};
}

// Build complete sector trailer (16 bytes) from components
BLOCK SectorConfig::buildTrailer(const BYTE keyA[6],
								 const BYTE access[4],
								 const BYTE keyB[6]) noexcept {
	BLOCK t;
	std::memcpy(t.data(),       keyA,   6);   // Bytes 0-5:   KeyA
	std::memcpy(t.data() + 6,   access, 4);   // Bytes 6-9:   Access bytes
	std::memcpy(t.data() + 10,  keyB,   6);   // Bytes 10-15: KeyB
	return t;
}

// ════════════════════════════════════════════════════════════════════════════════
// Low-Level Access Bits Manipulation
// ════════════════════════════════════════════════════════════════════════════════

// Extract C1, C2, C3 nibbles from raw access bytes
void SectorConfig::extractAccessBits(const BYTE ab[4],
									   BYTE c1[4], BYTE c2[4], BYTE c3[4]) noexcept {
	BYTE b6 = ab[0], b7 = ab[1], b8 = ab[2];
	
	// Byte 6: [~C2(4) | ~C1(4)] → extract C2 from low nibble, C1 from high nibble
	c1[0] = (b6 >> 4) & 1; c1[1] = (b6 >> 5) & 1;
	c1[2] = (b6 >> 6) & 1; c1[3] = (b6 >> 7) & 1;
	c2[0] = (b6 >> 0) & 1; c2[1] = (b6 >> 1) & 1;
	c2[2] = (b6 >> 2) & 1; c2[3] = (b6 >> 3) & 1;
	
	// Byte 7 low: [~C3(4)] → skip, use b8 instead
	// Byte 8: [~C2(4) | C3(4)] → extract C3 from high nibble, ~C2 from low nibble
	c3[0] = (b8 >> 4) & 1; c3[1] = (b8 >> 5) & 1;
	c3[2] = (b8 >> 6) & 1; c3[3] = (b8 >> 7) & 1;
}

// Decode C1, C2, C3 → standard Mifare Classic permission table
SectorConfig SectorConfig::configFromC1C2C3(BYTE c1, BYTE c2, BYTE c3) noexcept {
	SectorConfig cfg;
	
	cfg.keyA.kt = KeyType::A;
	cfg.keyB.kt = KeyType::B;
	cfg.keyA.slot = 0x01;
	cfg.keyB.slot = 0x02;

	// Standard Mifare Classic access control rules for data blocks
	if (c1 == 0 && c2 == 0 && c3 == 0) {
		// Mode 0: KeyA R, KeyB RW
		cfg.keyA.readable = true;  cfg.keyA.writable = false;
		cfg.keyB.readable = true;  cfg.keyB.writable = true;
	} else if (c1 == 0 && c2 == 1 && c3 == 0) {
		// Mode 1: KeyA R, KeyB R
		cfg.keyA.readable = true;  cfg.keyA.writable = false;
		cfg.keyB.readable = true;  cfg.keyB.writable = false;
	} else if (c1 == 1 && c2 == 0 && c3 == 0) {
		// Mode 2: KeyA RW, KeyB RW
		cfg.keyA.readable = true;  cfg.keyA.writable = true;
		cfg.keyB.readable = true;  cfg.keyB.writable = true;
	} else if (c1 == 1 && c2 == 1 && c3 == 0) {
		// Mode 3: KeyA RW, KeyB R
		cfg.keyA.readable = true;  cfg.keyA.writable = true;
		cfg.keyB.readable = true;  cfg.keyB.writable = false;
	} else {
		// Fallback: safe defaults (Mode 0)
		cfg.keyA.readable = true;  cfg.keyA.writable = false;
		cfg.keyB.readable = true;  cfg.keyB.writable = true;
	}
	
	return cfg;
}

// Encode SectorConfig → C1, C2, C3 bits
void SectorConfig::mapDataBlock(const SectorConfig& cfg,
								  BYTE& c1, BYTE& c2, BYTE& c3) {
	const bool keyA_R = cfg.keyA.readable;
	const bool keyA_W = cfg.keyA.writable;
	const bool keyB_R = cfg.keyB.readable;
	const bool keyB_W = cfg.keyB.writable;
	
	// Match against standard permission combinations
	if (keyA_R && !keyA_W && keyB_R && !keyB_W) {
		c1=1; c2=1; c3=0;  // Mode 3: KeyA RW, KeyB R
	} else if (keyA_R && !keyA_W && keyB_R && keyB_W) {
		c1=0; c2=0; c3=0;  // Mode 0: KeyA R, KeyB RW
	} else if (keyA_R && keyA_W && keyB_R && keyB_W) {
		c1=1; c2=0; c3=0;  // Mode 2: KeyA RW, KeyB RW
	} else if (keyA_R && keyA_W && keyB_R && !keyB_W) {
		c1=0; c2=1; c3=0;  // Mode 1: KeyA R, KeyB R
	} else {
		throw std::runtime_error("Unsupported access combination for data block");
	}
}
