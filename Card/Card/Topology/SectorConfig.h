#ifndef SECTORCONFIG_H
#define SECTORCONFIG_H

#include "../CardDataTypes.h"
#include <array>
#include <cstdint>
#include <string>

// ════════════════════════════════════════════════════════════════════════════════
// SectorConfig — Mifare Classic Sector Security Configuration
// ════════════════════════════════════════════════════════════════════════════════
// 
// Mifare Classic sector trailer layout (16 bytes):
//   [0-5]   = KeyA (6 bytes)
//   [6-9]   = Access Bytes (4 bytes)
//   [10-15] = KeyB (6 bytes)
//
// Access bytes encoding:
//   Byte 6:  [~C2(4bits) | ~C1(4bits)]
//   Byte 7:  [C1(4bits) | ~C3(4bits)]
//   Byte 8:  [~C2(4bits) | C2(4bits)]   // Note: Cn are 4-bit values (per block)
//   Byte 9:  = 0x00 (General Purpose Byte, unused in Classic)
//
// Each Cn (n=1,2,3) is a 4-bit value where each bit corresponds to one of the 4 blocks:
//   bit0 = block0, bit1 = block1, bit2 = block2, bit3 = trailer block
//
// ════════════════════════════════════════════════════════════════════════════════
class SectorConfig {
public:
	// ────────────────────────────────────────────────────────────────────────────
	// Instance Data
	// ────────────────────────────────────────────────────────────────────────────
	KeyInfo keyA;                    // Key A metadata + key data
	ACCESSBYTES accessBits;          // Generated access control bytes (4 bytes)
	KeyInfo keyB;                    // Key B metadata + key data

	// ────────────────────────────────────────────────────────────────────────────
	// Constructors
	// ────────────────────────────────────────────────────────────────────────────
	
	// Default constructor: Both keys FF...FF, standard permissions
	SectorConfig() noexcept;
	
	// Constructor with per-key read/write permissions
	// Creates default KeyInfo objects with specified permissions
	// @param aR keyA readable, @param aW keyA writable
	// @param bR keyB readable, @param bW keyB writable
	SectorConfig(bool aR, bool aW, bool bR, bool bW) noexcept;
	
	// Constructor from explicit KeyInfo objects
	SectorConfig(const KeyInfo& kA, const KeyInfo& kB) noexcept;

	// ────────────────────────────────────────────────────────────────────────────
	// Access Bits Codec (Static Utilities)
	// ────────────────────────────────────────────────────────────────────────────
	
	// Parse 4 raw access bytes → SectorConfig (decode permissions from card)
	static SectorConfig parseAccessBitsToConfig(const BYTE ab[4]) noexcept;
	
	// Verify access bytes validity (check consistency with complement bits)
	static bool validateAccessBits(const BYTE a[4]) noexcept;
	
	// Generate 4 access bytes from SectorConfig (encode permissions for card)
	static ACCESSBYTES makeSectorAccessBits(const SectorConfig& dataCfg) noexcept;
	
	// Build complete 16-byte trailer block from components
	static BLOCK buildTrailer(const BYTE keyA[6],
							  const BYTE access[4],
							  const BYTE keyB[6]) noexcept;

	// ────────────────────────────────────────────────────────────────────────────
	// Low-Level Access Bits Manipulation (for advanced use)
	// ────────────────────────────────────────────────────────────────────────────
	
	// Extract C1, C2, C3 nibbles from raw access bytes
	// Each nibble is 4 bits: [bit3=trailer, bit2=block2, bit1=block1, bit0=block0]
	static void extractAccessBits(const BYTE ab[4],
		BYTE c1[4], BYTE c2[4], BYTE c3[4]) noexcept;
	
	// Construct SectorConfig from raw C1, C2, C3 bits
	// Decodes standard Mifare Classic permission table
	static SectorConfig configFromC1C2C3(BYTE c1, BYTE c2, BYTE c3) noexcept;
	
	// Determine C1, C2, C3 bits for data blocks from SectorConfig
	// Encodes to standard Mifare Classic permission table
	static void mapDataBlock(const SectorConfig& cfg,
		BYTE& c1, BYTE& c2, BYTE& c3);
};

#endif // SECTORCONFIG_H
