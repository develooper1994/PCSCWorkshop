#ifndef CARDTYPES_H
#define CARDTYPES_H

#include "Types.h"
#include <array>

// Type aliases for Mifare sector operations
using ACCESSBYTES = std::array<BYTE, 4>;
using KEYBYTES = std::array<BYTE, 6>;
using BLOCK = std::array<BYTE, 16>;
using SECTOR = std::array<BLOCK, 4>;
using MIFARECLASSIC1k = std::array<BLOCK, 16>;
using MIFARECLASSIC4k = std::array<BLOCK, 40>;

struct TrailerFields {
	// 16 bytes: [keyA(6 bytes)|AccessBytes(4 bytes)|keyB(6 bytes)|]
	KEYBYTES	keyA{};
	ACCESSBYTES accessBytes{};
	KEYBYTES	keyB{};
};

union TrailerData {
	TrailerFields fields;
	std::array<BYTE, 16> raw;

	TrailerData() : raw{} {} // Default constructor initializes raw to zeros
};

enum class CardType {
	Unknown,
	MifareClassic1K,
	MifareClassic4K,
	MifareUltralight,
	MifareDesfire,
};

enum class KeyStructure : BYTE {
	Volatile,
	NonVolatile
};

enum class KeyType {
	A, 		// keyA
	B, 		// keyB
	ACCESS, // AccessBytes
	AB, 	// [keyA(6 Byte)|-|keyB(6 Byte)|]
	ALL, 	// [keyA(6 Byte)|AccessBytes(4 Byte)|keyB(6 Byte)|]
};

enum class CardBlockKind {
	Unknown,
	Data,
	Manufacturer,
	Trailer
};

#endif // !CARDTYPES_H
