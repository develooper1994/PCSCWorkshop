#ifndef CARDTYPES_H
#define CARDTYPES_H

#include "Types.h"
#include <array>
#include <string>

// Type aliases for Mifare sector operations
using ACCESSBYTES = std::array<BYTE, 4>;
using KEYBYTES = std::array<BYTE, 6>;
using BLOCK = std::array<BYTE, 16>;
using SECTOR = std::array<BLOCK, 4>;
using MIFARECLASSIC1k = std::array<BLOCK, 16>;
using MIFARECLASSIC4k = std::array<BLOCK, 40>;


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



// represents a single key with its metadata; for KeyType::ACCESS, the key field is ignored
struct KeyInfo {
	KEYBYTES key{};
	bool readable = true;
	bool writable = true;
	KeyType kt = KeyType::A;
	KeyStructure ks = KeyStructure::NonVolatile;
	BYTE slot = 0x00;
	std::string name;
};

enum class Permission {
	None       = 0b00,
	Read       = 0b01,
	Write      = 0b10,
	ReadWrite  = 0b11
};

#endif // !CARDTYPES_H
