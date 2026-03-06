# Mifare Classic Card Simulator - Usage Examples

## Overview

The Mifare Card Simulator allows you to test and develop Mifare Classic card applications **without hardware**. It provides:

- ✅ In-memory card simulation (1K or 4K)
- ✅ Realistic authentication and authorization
- ✅ Read/write operations with block protection
- ✅ Full memory introspection and debugging
- ✅ Integration with MifareCardCore

---

## Architecture

```
┌─────────────────────────────────┐
│   MifareCardCore                │  Your application
├─────────────────────────────────┤
│   SimulatedReader               │  Reader simulation
├─────────────────────────────────┤
│   MifareCardSimulator           │  Virtual card memory
└─────────────────────────────────┘
```

---

## Example 1: Basic Read/Write

```cpp
#include "Card/Card/MifareClassic/MifareCardSimulator.h"
#include "Card/Card/MifareClassic/SimulatedReader.h"
#include "Card/Card/MifareClassic/MifareClassic.h"

int main() {
    // Create simulated 1K card
    auto cardSim = std::make_shared<MifareCardSimulator>(false);
    
    // Create reader
    SimulatedReader reader(cardSim);
    
    // Create card interface
    MifareCardCore card(reader, false);
    
    // Read block 1 (after automatic auth)
    BYTEV data = card.read(1);
    std::cout << "Read " << data.size() << " bytes\n";
    
    return 0;
}
```

---

## Example 2: Write Data with Configuration

```cpp
#include "Card/Card/Topology/SectorConfigBuilder.h"

// Create custom sector config
auto config = SectorConfigBuilder()
    .keyA({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
    .keyB({0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
    .accessMode(SectorConfigBuilder::AccessMode::MODE_0)
    .build();

// Set and apply to sector 0
card.setSectorConfig(0, config);
card.applySectorConfigToCard(0);

// Write data to block 1
std::string text = "Hello Mifare!";
card.write(1, text);
```

---

## Example 3: Full Card Setup

```cpp
// Setup
auto cardSim = std::make_shared<MifareCardSimulator>(false);  // 1K
SimulatedReader reader(cardSim);
MifareCardCore card(reader, false);

// Set custom UID
cardSim->setUID({0x11, 0x22, 0x33, 0x44});

// Create standard config for all sectors
auto stdConfig = SectorConfigBuilder()
    .keyA({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
    .keyB({0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
    .accessMode(SectorConfigBuilder::AccessMode::MODE_0)
    .build();

card.setAllSectorsConfig(stdConfig);
card.applyAllSectorsConfig();

// Write data to sectors
for (int block = 1; block < 64; block += 4) {  // Skip trailers
    if (!cardSim->isTrailerBlock(block)) {
        card.write(block, "DATA");
    }
}

// Dump card to verify
cardSim->printMemory();
```

---

## Example 4: Authentication Testing

```cpp
// Set different keys for testing
auto cardSim = std::make_shared<MifareCardSimulator>(false);
SimulatedReader reader(cardSim);
MifareCardCore card(reader, false);

// Register custom keys
KEYBYTES keyA = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
KEYBYTES keyB = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

card.setKey(KeyType::A, keyA, KeyStructure::NonVolatile, 0x01);
card.setKey(KeyType::B, keyB, KeyStructure::NonVolatile, 0x02);

// Create config with custom keys
auto config = SectorConfigBuilder()
    .keyA(keyA)
    .keyB(keyB)
    .accessMode(SectorConfigBuilder::AccessMode::MODE_3)
    .build();

card.setSectorConfig(0, config);
card.applySectorConfigToCard(0);

// Read and verify
BYTEV data = card.read(1);
std::cout << "Authentication successful!\n";
```

---

## Example 5: Card Backup & Restore

```cpp
// Backup complete card memory
std::vector<BYTE> backup = cardSim->dumpMemory();
std::cout << "Card backed up: " << backup.size() << " bytes\n";

// Simulate some changes
card.write(10, "MODIFIED");

// Restore from backup
cardSim->restoreMemory(backup);
std::cout << "Card restored from backup\n";
```

---

## Example 6: Sector-by-Sector Configuration

```cpp
// Different config for different sectors
auto masterConfig = SectorConfigBuilder()
    .keyA({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
    .keyB({0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
    .accessMode(SectorConfigBuilder::AccessMode::MODE_3)  // KeyA RW, KeyB R
    .keyAProperties(KeyStructure::NonVolatile, 0x01, "MasterKey")
    .build();

auto dataConfig = SectorConfigBuilder()
    .keyA({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
    .keyB({0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
    .accessMode(SectorConfigBuilder::AccessMode::MODE_0)   // KeyA R, KeyB RW
    .keyBProperties(KeyStructure::NonVolatile, 0x02, "AdminKey")
    .build();

// Apply different configs
card.setSectorConfig(0, masterConfig);
card.applySectorConfigToCard(0);

for (int i = 1; i < 16; ++i) {
    card.setSectorConfig(i, dataConfig);
    card.applySectorConfigToCard(i);
}
```

---

## Example 7: Testing Edge Cases

```cpp
auto cardSim = std::make_shared<MifareCardSimulator>(false);
SimulatedReader reader(cardSim);

// Test: Manufacturer block (read-only)
try {
    BLOCK data = cardSim->readBlock(0);
    std::cout << "Read manufacturer block OK\n";
} catch (...) {
    std::cout << "Failed\n";
}

// Test: Trailer block writing
try {
    BYTE trailer[16] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0x07, 0x80, 0x69,
                        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    cardSim->writeTrailer(0, trailer);
    std::cout << "Trailer write OK\n";
} catch (const std::exception& e) {
    std::cout << "Trailer write failed: " << e.what() << "\n";
}

// Test: Invalid block
try {
    cardSim->readBlock(999);
} catch (const std::out_of_range& e) {
    std::cout << "Invalid block rejected: " << e.what() << "\n";
}
```

---

## Example 8: Debugging & Inspection

```cpp
// Print card memory
std::cout << "\n=== Card Memory ===\n";
cardSim->printMemory();

// Print specific sector
std::cout << "\n=== Sector 0 ===\n";
cardSim->printSector(0);

// Print all trailers
std::cout << "\n=== All Trailers ===\n";
cardSim->printAllTrailers();

// Print reader state
std::cout << "\n=== Reader State ===\n";
reader.printState();

// Get UID
BYTEV uid = cardSim->getUID();
std::cout << "UID: ";
for (BYTE b : uid) {
    std::cout << std::hex << static_cast<int>(b) << " ";
}
std::cout << std::dec << "\n";
```

---

## Example 9: 4K Card Simulation

```cpp
// Create 4K card (40 sectors)
auto cardSim = std::make_shared<MifareCardSimulator>(true);  // true = 4K
SimulatedReader reader(cardSim);
MifareCardCore card(reader, true);

std::cout << "Card size: " << cardSim->memorySize() << " bytes\n";
std::cout << "Sectors: " << cardSim->sectorCount() << "\n";

// Sectors 0-31 have 4 blocks each
// Sectors 32-39 have 16 blocks each
```

---

## Example 10: Linear Read/Write

```cpp
// Write large data across multiple blocks
std::string largeData = "This is a long text that spans multiple blocks...";
card.writeLinear(1, BYTEV(largeData.begin(), largeData.end()));

// Read it back
BYTEV readData = card.readLinear(1, largeData.size());
std::cout << "Read " << readData.size() << " bytes\n";
```

---

## API Reference

### MifareCardSimulator

```cpp
// Creation
explicit MifareCardSimulator(bool is4K = false);

// Layout info
bool is4K() const;
size_t sectorCount() const;
size_t memorySize() const;

// Key management
void setKey(KeyType kt, const KEYBYTES& key, BYTE slot);
const KEYBYTES& getKey(KeyType kt) const;
bool hasKey(KeyType kt) const;

// Read/write
BLOCK readBlock(int block) const;
void writeBlock(int block, const BYTE data[16]);
BLOCK readTrailer(int sector) const;
void writeTrailer(int sector, const BYTE trailer[16]);

// Backup
std::vector<BYTE> dumpMemory() const;
void restoreMemory(const std::vector<BYTE>& dump);

// Authentication
void authenticate(int sector, KeyType kt, BYTE keySlot);
bool isAuthenticated(int sector, KeyType kt, BYTE keySlot) const;
void clearAuthentication();

// Card properties
BYTEV getUID() const;
void setUID(const BYTEV& uid);

// Debug
void printMemory() const;
void printSector(int sector) const;
void printAllTrailers() const;
```

### SimulatedReader

```cpp
explicit SimulatedReader(std::shared_ptr<MifareCardSimulator> card);

void loadKey(const BYTE* key, KeyStructure ks, BYTE slot);
void setKeyType(KeyType kt);
void setKeyNumber(BYTE slot);
void setAuthRequested(bool requested);
void performAuth(BYTE block);
BYTEV readPage(BYTE block);
void writePage(BYTE block, const BYTE* data);
bool isConnected() const;

// Test methods
std::shared_ptr<MifareCardSimulator> card();
void reset();
void printState() const;
```

---

## Best Practices

✅ **Always create shared_ptr** - SimulatedReader needs shared ownership  
✅ **Use SectorConfigBuilder** - For readable, maintainable config setup  
✅ **Test authentication** - Verify your key logic works  
✅ **Backup before modifications** - Use dumpMemory() for safety  
✅ **Check block types** - isManufacturerBlock(), isTrailerBlock()  
✅ **Use printSector()** - For debugging unexpected behavior  

---

## Benefits

🚀 **Development**: Test without hardware  
🔒 **Security**: Verify auth logic before deployment  
⚡ **Performance**: Fast in-memory simulation  
🐛 **Debugging**: Full memory introspection  
📊 **Testing**: Comprehensive edge case coverage  
🔄 **Backup/Restore**: Experiment safely  

