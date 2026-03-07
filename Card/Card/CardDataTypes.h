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
	A,                      // keyA (Mifare Classic)
	B,                      // keyB (Mifare Classic)
	ACCESS,                 // AccessBytes (Mifare Classic)
	AB,                     // [keyA(6 Byte)|-|keyB(6 Byte)|]
	MifareClassic_Trailer,  // [keyA(6)|Access(4)|keyB(6)] — full 16 bytes of trailer

	// DESFire key types — future
	// DESFire_Master,
	// DESFire_Application,
	// ...
};

enum class CardBlockKind {
	Unknown,
	Data,
	Manufacturer,
	Trailer
};

enum class Permission {
	// Mifare Classic + DESFire common
	None               = 0b00000,
	Read               = 0b00001,
	Write              = 0b00010,

	// DESFire specific
	ChangeAccessRights = 0b00100,  // Change application access rights (CAR)
	Delete             = 0b01000,  // Delete file / application
	CreateApplication  = 0b10000,  // Create application (master key only)

	// Common combinations
	ReadWrite          = Read | Write,
	ReadDelete         = Read | Delete,
	WriteDelete        = Write | Delete,
	ReadWriteDelete    = Read | Write | Delete,
	ReadChangeAccess   = Read | ChangeAccessRights,
	WriteChangeAccess  = Write | ChangeAccessRights,
	ReadWriteChangeAccess = Read | Write | ChangeAccessRights,
	Full               = Read | Write | ChangeAccessRights | Delete | CreateApplication,
};

inline bool hasPermission(Permission p, Permission flag) {
	return (static_cast<int>(p) & static_cast<int>(flag)) != 0;
}

// Auth amacı — ensureAuth doğru key'i seçmek için kullanır
enum class AuthPurpose {
	Read,
	Write
};



// represents a single key with its metadata
struct KeyInfo {
	KEYBYTES key{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	KeyType kt = KeyType::A;
	KeyStructure ks = KeyStructure::NonVolatile;
	BYTE slot = 0x01;
	std::string name;

	// Key'in veri erişim yetkisi.
	//   Mifare Classic : chooseKey() sector access bits'i kullanır,
	//                    bu alan yalnızca fallback'tir.
	//   DESFire / diğer: sector access bits yoktur,
	//                    chooseKey() doğrudan bu alana bakar.
	Permission permission = Permission::ReadWrite;

	bool canRead()  const { return hasPermission(permission, Permission::Read); }
	bool canWrite() const { return hasPermission(permission, Permission::Write); }
};

#endif // !CARDTYPES_H
