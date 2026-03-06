# SectorConfigBuilder Usage Examples

## Overview
`SectorConfigBuilder` provides a fluent API for constructing `SectorConfig` objects with readable, chainable method calls.

---

## Basic Examples

### Example 1: Simple Configuration with Keys
```cpp
#include "Topology/SectorConfigBuilder.h"

auto config = SectorConfigBuilder()
    .keyA({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
    .keyB({0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
    .accessMode(SectorConfigBuilder::AccessMode::MODE_0)  // KeyA: R, KeyB: RW
    .build();
```

### Example 2: Data Block Access Control
```cpp
// KeyA can read, KeyB can read and write
auto config = SectorConfigBuilder()
    .keyA({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
    .keyB({0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
    .dataBlockAccess(true, false, true, true)  // aRead, aWrite, bRead, bWrite
    .build();
```

### Example 3: Per-Key Configuration
```cpp
auto config = SectorConfigBuilder()
    .keyA({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
    .keyAAccess(true, false)  // KeyA readable, not writable
    .keyB({0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
    .keyBAccess(true, true)   // KeyB readable and writable
    .build();
```

---

## Advanced Examples

### Example 4: Key Properties with Names
```cpp
auto config = SectorConfigBuilder()
    .keyA({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
    .keyAProperties(KeyStructure::NonVolatile, 0x01, "MasterKeyA")
    .keyB({0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
    .keyBProperties(KeyStructure::Volatile, 0x02, "ReadOnlyKeyB")
    .accessMode(SectorConfigBuilder::AccessMode::MODE_1)  // Read-only mode
    .build();
```

### Example 5: Batch Configuration (All Sectors)
```cpp
MifareCardCore card(reader, false);  // 1K card

// Create standard config for all sectors
auto standardConfig = SectorConfigBuilder()
    .keyA({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
    .keyB({0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
    .accessMode(SectorConfigBuilder::AccessMode::MODE_0)
    .build();

// Apply to all sectors
card.setAllSectorsConfig(standardConfig);
card.applyAllSectorsConfig();
```

### Example 6: Different Access Modes
```cpp
// MODE_0: KeyA readable, KeyB readable+writable (default)
auto mode0 = SectorConfigBuilder()
    .keyA({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
    .keyB({0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
    .accessMode(SectorConfigBuilder::AccessMode::MODE_0)
    .build();

// MODE_1: Both keys readable only
auto mode1 = SectorConfigBuilder()
    .keyA({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
    .keyB({0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
    .accessMode(SectorConfigBuilder::AccessMode::MODE_1)
    .build();

// MODE_2: Both keys readable+writable
auto mode2 = SectorConfigBuilder()
    .keyA({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
    .keyB({0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
    .accessMode(SectorConfigBuilder::AccessMode::MODE_2)
    .build();

// MODE_3: KeyA readable+writable, KeyB readable only
auto mode3 = SectorConfigBuilder()
    .keyA({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
    .keyB({0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
    .accessMode(SectorConfigBuilder::AccessMode::MODE_3)
    .build();
```

### Example 7: Reusing Builder (Reset)
```cpp
SectorConfigBuilder builder;

// First config
auto config1 = builder
    .keyA({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
    .keyB({0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
    .accessMode(SectorConfigBuilder::AccessMode::MODE_0)
    .build();

// Reset and build second config
builder.reset();
auto config2 = builder
    .keyA({0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF})
    .keyB({0x11, 0x22, 0x33, 0x44, 0x55, 0x66})
    .accessMode(SectorConfigBuilder::AccessMode::MODE_3)
    .build();
```

---

## Real-World Scenario

### Scenario: Configure Card with Different Sector Permissions
```cpp
MifareCardCore card(reader, false);  // 1K card (16 sectors)

// Sector 0: Master key (KeyA RW, KeyB R)
auto sectorZero = SectorConfigBuilder()
    .keyA({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
    .keyB({0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
    .accessMode(SectorConfigBuilder::AccessMode::MODE_3)
    .keyAProperties(KeyStructure::NonVolatile, 0x01, "MasterKey")
    .build();

// Sectors 1-15: Standard config (KeyA R, KeyB RW)
auto standardSector = SectorConfigBuilder()
    .keyA({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
    .keyB({0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
    .accessMode(SectorConfigBuilder::AccessMode::MODE_0)
    .build();

// Apply to sector 0
card.setSectorConfig(0, sectorZero);
card.applySectorConfigToCard(0);

// Apply to all other sectors
for (int i = 1; i < 16; ++i) {
    card.setSectorConfig(i, standardSector);
    card.applySectorConfigToCard(i);
}
```

---

## Access Modes Reference

| Mode | KeyA | KeyB | Use Case |
|------|------|------|----------|
| **MODE_0** | Read | Read+Write | Default, KeyB is admin key |
| **MODE_1** | Read | Read | Locked read-only (both keys) |
| **MODE_2** | Read+Write | Read+Write | Full access for both keys |
| **MODE_3** | Read+Write | Read | KeyA is admin, KeyB is reader |

---

## Method Chaining Reference

All methods return `*this`, enabling fluent chaining:

```cpp
auto config = builder
    .keyA(...)                        // ← returns SectorConfigBuilder&
    .keyB(...)                        // ← returns SectorConfigBuilder&
    .dataBlockAccess(...)             // ← returns SectorConfigBuilder&
    .keyAProperties(...)              // ← returns SectorConfigBuilder&
    .keyBProperties(...)              // ← returns SectorConfigBuilder&
    .accessMode(...)                  // ← returns SectorConfigBuilder&
    .build();                         // ← returns SectorConfig
```

---

## Error Handling

```cpp
try {
    auto config = SectorConfigBuilder()
        .keyA({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
        .keyB({0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
        .accessMode(SectorConfigBuilder::AccessMode::MODE_0)
        .build();
} catch (const std::exception& e) {
    std::cerr << "Failed to build config: " << e.what() << "\n";
}
```

---

## Benefits

✅ **Readable**: Self-documenting, clear intent  
✅ **Flexible**: Easy to modify individual properties  
✅ **Type-Safe**: Compile-time checking with enums  
✅ **Composable**: Mix and match methods as needed  
✅ **Reusable**: Reset and build multiple configs  
