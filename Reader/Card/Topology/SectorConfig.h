#ifndef SECTORKEYCONFIG_H
#define SECTORKEYCONFIG_H

#include "../CardDataTypes.h"
#include <array>
#include <cstdint>
#include <string>

// represents a single key with its metadata; for KeyType::ACCESS, the key field is ignored and access bits should be stored in the key field of the KeyInfo with kt=KeyType::ACCESS
struct KeyInfo {
	KEYBYTES key{};
	bool readable = true;
	bool writable = true;
	KeyType kt = KeyType::A;
	KeyStructure ks = KeyStructure::NonVolatile;
	BYTE slot = 0x00;
	std::string name;
};

// ════════════════════════════════════════════════════════
// [keyA(6 Byte)|AccessBytes(4 Byte)|keyB(6 Byte)|]
// SectorKeyConfig — sector permission descriptor (host-side)
// Security configuration for a sector, including KeyA and KeyB permissions and key data (optional)
// ════════════════════════════════════════════════════════
class SectorConfig {
public:
	// Key information (includes readable/writable flags)
	KeyInfo keyA;
	ACCESSBYTES accessBits; // Generated from key permissions
	KeyInfo keyB;

	// Default constructor
	SectorConfig() noexcept;
	
	// Constructor with permission flags (for backward compatibility)
	// Creates default KeyInfo objects with specified permissions
	SectorConfig(bool aR, bool aW, bool bR, bool bW) noexcept;
	
	// Constructor with KeyInfo
	SectorConfig(const KeyInfo& kA, const KeyInfo& kB) noexcept;

	// ════════════════════════════════════════════════════════
	//  Access-bits codec (static helpers)
	// ════════════════════════════════════════════════════════
	static SectorConfig parseAccessBitsToConfig(const BYTE ab[4]) noexcept;
	static bool validateAccessBits(const BYTE a[4]) noexcept;
	static ACCESSBYTES makeSectorAccessBits(const SectorConfig& dataCfg) noexcept;
	static BLOCK buildTrailer(const BYTE keyA[6],
							  const BYTE access[4],
							  const BYTE keyB[6]) noexcept;

	static void extractAccessBits(const BYTE ab[4],
		BYTE c1[4], BYTE c2[4], BYTE c3[4]) noexcept;
	static SectorConfig configFromC1C2C3(BYTE c1, BYTE c2, BYTE c3) noexcept;
	static void mapDataBlock(const SectorConfig& cfg,
		BYTE& c1, BYTE& c2, BYTE& c3);
};

#endif // SECTORKEYCONFIG_H
