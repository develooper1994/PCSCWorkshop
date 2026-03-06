# MIFARE Classic Card Management System

## Overview
Complete, production-ready Mifare Classic card management library for PCSC card readers. Supports 1K and 4K card variants with comprehensive permission checking and authentication management.

---

## ✨ Features

### Zero-Copy Memory Architecture
- Single 1K/4K buffer with 5 simultaneous views
- Direct pointer access (no copying)
- Views: raw bytes | blocks | sectors | detailed structure

### Type-Safe Design
- Union-based memory structure
- Compile-time constants
- No virtual functions
- Const correctness

### Complete Functionality
- ✅ 1K and 4K card support
- ✅ Sector-level topology queries
- ✅ Block type identification
- ✅ Host and card key management
- ✅ Mifare Classic permission matrix
- ✅ Authentication session caching with timeout
- ✅ Access control (KeyA/KeyB discrimination)
- ✅ Memory import/export

---

## 📁 Architecture

```
Card/
├── CardInterface.h/cpp              [High-Level API]
│
├── CardModel/                       [Memory Layer]
│   ├── CardDataTypes.h              (Enums, type definitions)
│   ├── BlockDefinition.h            (16-byte block union)
│   ├── CardMemoryLayout.h           (1K/4K memory layout)
│   ├── CardTopology.h/cpp           (Layout calculations)
│   ├── SectorDefinition.h           (Sector metadata)
│   └── AccessBits.h/cpp             (C1 C2 C3 codec)
│
└── CardProtocol/                    [Business Logic]
    ├── AccessControl.h/cpp          (Permission matrix)
    ├── KeyManagement.h/cpp          (Key storage & reading)
    └── AuthenticationState.h/cpp    (Session caching)

Tests/
├── main.cpp                         [Test runner with modes]
├── CardSystemTests.cpp              [7 test suites]
└── RealCardReaderTest.cpp           [Real card reader demo]
```

---

## 🚀 Usage Examples

### Basic Card Operations
```cpp
// Create 1K card interface
CardInterface card(false);

// Load memory from raw bytes
card.loadMemory(rawBytes, 1024);

// Register keys
KEYBYTES keyA = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
card.registerKey(KeyType::A, keyA, KeyStructure::NonVolatile, 0x01);

// Authenticate sector
card.authenticate(5, KeyType::A);

// Check permissions
if (card.canReadDataBlocks(5, KeyType::A)) {
    // Read data from sector 5
}

// Get card information
cout << "Card: " << card.getTotalMemory() << " bytes\n";
cout << "UID: " << card.getUID() << "\n";
```

### Topology Queries
```cpp
CardInterface card(false);

// Block to sector mapping
int sector = card.getSectorForBlock(20);

// Get sector boundaries
int firstBlock = card.getFirstBlockOfSector(5);
int trailerBlock = card.getTrailerBlockOfSector(5);

// Check block type
if (card.isManufacturerBlock(0)) { ... }
if (card.isTrailerBlock(3)) { ... }
if (card.isDataBlock(1)) { ... }
```

### Key Management
```cpp
// Register host key
KEYBYTES myKey = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
card.registerKey(KeyType::A, myKey, KeyStructure::NonVolatile, 0x01, "My Key");

// Get registered key
KEYBYTES key = card.getKey(KeyType::A, 0x01);

// Read card's trailer key
KEYBYTES cardKeyA = card.getCardKey(5, KeyType::A);
```

### Authentication Sessions
```cpp
// Authenticate sector
card.authenticate(0, KeyType::A);

// Check authentication status
if (card.isAuthenticated(0)) {
    if (card.isAuthenticatedWith(0, KeyType::A)) {
        // Access granted with KeyA
    }
}

// Clear authentication
card.deauthenticate(0);
```

---

## 🧪 Testing

### Test Modes
```bash
Tests.exe          # Interactive menu
Tests.exe 1        # All tests (cipher + card system)
Tests.exe 2        # Cipher tests only
Tests.exe 3        # Card system tests only
Tests.exe 4        # Real card reader test
```

### Test Results
```
✅ Cipher round-trip tests ......... PASSED
✅ CNG cipher tests ................ PASSED
✅ Card Memory Layout (1K) ......... PASSED
✅ Card Memory Layout (4K) ......... PASSED
✅ Card Topology ................... PASSED
✅ Key Management .................. PASSED
✅ Access Control .................. PASSED
✅ Authentication State ............ PASSED
✅ Card Interface .................. PASSED

Total: 9 tests, All PASSED ✅
```

---

## 📊 Supported Card Variants

### Mifare Classic 1K
- 64 blocks total
- 16 sectors × 4 blocks
- 1024 bytes total memory
- Default access: MODE_0

### Mifare Classic 4K
- 256 blocks total
- 32 normal sectors (4 blocks each)
- 8 extended sectors (16 blocks each)
- 4096 bytes total memory

---

## 🔐 Permission Matrix

### Default Mode (MODE_0)
```
Data Blocks:
  KeyA: Read only
  KeyB: Read/Write

Trailer Block:
  KeyA: No access
  KeyB: Read/Write
```

### Access Bits Encoding
- C1 C2 C3: 3-bit permission codes
- GPB: General Purpose Byte
- Complete codec in AccessBits.h/cpp

---

## 🔑 Key Management

### Key Validation
- Keys must not be all 0x00 (invalid)
- Keys must not be all 0xFF (reserved)
- Other values are acceptable
- Validation: `KeyManagement::isValidKey(key)`

### Host vs Card Keys
- **Host Keys**: Registered by application, used for authentication
- **Card Keys**: Stored in sector trailer, read directly from memory

### Multiple Slots
- Support multiple keys per type
- Slot-based organization (0x01, 0x02, etc.)
- Default key selection (first registered)

---

## 📈 Authentication Sessions

### Features
- Per-sector caching
- Configurable timeout (default: 5000ms)
- Automatic expiry
- Refresh capability
- Batch operations

### Usage
```cpp
// Configure timeout
authState.setDefaultTimeout(std::chrono::milliseconds(10000));

// Mark authenticated
card.authenticate(sector, KeyType::A);

// Check status
bool auth = card.isAuthenticated(sector);

// Get remaining time
auto remaining = authState.getTimeRemaining(sector);

// Clear expired sessions
authState.removeExpiredSessions();
```

---

## 🔧 Configuration

### Memory Layout
```cpp
CardInterface card(false);  // 1K
CardInterface card(true);   // 4K
```

### Authentication Timeout
```cpp
authState.setDefaultTimeout(std::chrono::milliseconds(5000));
```

### Caching
```cpp
authState.enableCaching(true);   // Enable (default)
authState.enableCaching(false);  // Disable
```

---

## 📋 API Reference

### CardInterface (Main API)

**Memory Management**
- `loadMemory(data, size)` - Load from raw bytes
- `exportMemory()` - Export to BYTEV
- `getMemory()` - Read-only access
- `getMemoryMutable()` - Mutable access

**Key Management**
- `registerKey(type, key, structure, slot, name)`
- `getKey(type, slot)`
- `getCardKey(sector, type)`

**Authentication**
- `authenticate(sector, keyType)`
- `isAuthenticated(sector)`
- `isAuthenticatedWith(sector, keyType)`
- `deauthenticate(sector)`
- `clearAuthentication()`

**Access Control**
- `canRead(block, keyType)`
- `canWrite(block, keyType)`
- `canReadDataBlocks(sector, keyType)`
- `canWriteDataBlocks(sector, keyType)`

**Topology**
- `getSectorForBlock(block)`
- `getFirstBlockOfSector(sector)`
- `getLastBlockOfSector(sector)`
- `getTrailerBlockOfSector(sector)`
- `isManufacturerBlock(block)`
- `isTrailerBlock(block)`
- `isDataBlock(block)`

**Information**
- `is1K()`, `is4K()`
- `getTotalMemory()`, `getTotalBlocks()`, `getTotalSectors()`
- `getUID()`
- `printCardInfo()`, `printAuthenticationStatus()`

---

## 🎯 Design Principles

1. **Zero-Copy**: Single buffer with multiple views
2. **Type-Safe**: Compile-time checks, no unsafe casts
3. **Simple API**: High-level interface hiding complexity
4. **Testable**: Comprehensive test suite
5. **Modular**: Independent components
6. **Extensible**: Easy to add new features

---

## 📝 License & Credits

Part of PCSC Workshop project - Educational and commercial use supported.

---

## 🤝 Integration

Ready to integrate with:
- ✅ PCSC card readers
- ✅ Mifare readers (ACR1281U, etc.)
- ✅ Custom card readers
- ✅ NFC frameworks
- ✅ RFID systems

---

## 📞 Support

For issues, questions, or contributions:
1. Check test output: `Tests.exe 3`
2. Review API reference
3. Examine test cases in CardSystemTests.cpp
4. Check real card reader test: `Tests.exe 4`

---

**Version**: 1.0.0  
**Status**: Production Ready ✅  
**Last Updated**: 2025-03-06
