# CardMemoryLayout - Complete Zero-Copy Architecture

## 🎯 Konsept

**Tüm kartın belleğini single union ile temsil etmek** - aynı bellek, farklı view'lar:

```
┌─────────────────────────────────────────────────────────────┐
│              CardMemoryLayout (is4K=false)                  │
├─────────────────────────────────────────────────────────────┤
│  Card1KMemoryLayout                                         │
│  ┌─────────────────────────────────────────────────────────┐│
│  │ union {                                                  ││
│  │   BYTE raw[1024]              ◄─ Raw bytes view        ││
│  │   MifareBlock blocks[64]       ◄─ Block view (16B each)││
│  │   sectors[16][4]              ◄─ Sector view          ││
│  │   detailed.sector[16]         ◄─ Named field view     ││
│  │ }                                                        ││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

---

## 📊 Memory Views (1K Card Example)

### View 1: Raw Bytes
```cpp
cardMem.data.card1K.raw[0...1023]
// Direct byte access
BYTE uid = cardMem.data.card1K.raw[0];
BYTE firstKeyA = cardMem.data.card1K.raw[6];  // Sector 0 trailer, KeyA start
```

### View 2: Blocks (64 × 16 bytes)
```cpp
cardMem.data.card1K.blocks[0]     // Block 0 (manufacturer)
cardMem.data.card1K.blocks[1]     // Block 1 (data)
cardMem.data.card1K.blocks[3]     // Block 3 (sector 0 trailer)
cardMem.data.card1K.blocks[63]    // Block 63 (sector 15 trailer)

// Access via union
MifareBlock b = cardMem.data.card1K.blocks[3];
auto keyA = b.trailer.keyA;  // Direct access to trailer's KeyA
```

### View 3: Sectors (16 × 4 blocks)
```cpp
cardMem.data.card1K.sectors[0].block[0]  // Sector 0, Block 0
cardMem.data.card1K.sectors[0].block[3]  // Sector 0, Block 3 (trailer)
cardMem.data.card1K.sectors[5].block[2]  // Sector 5, Block 2
```

### View 4: Detailed (Named Fields)
```cpp
// Sector 0
cardMem.data.card1K.detailed.sector[0].dataBlock0
cardMem.data.card1K.detailed.sector[0].dataBlock1
cardMem.data.card1K.detailed.sector[0].dataBlock2
cardMem.data.card1K.detailed.sector[0].trailerBlock

// Access trailer keys directly
auto trailer = cardMem.data.card1K.detailed.sector[5].trailerBlock;
auto keyA = trailer.trailer.keyA;
auto accessBits = trailer.trailer.accessBits;
```

---

## 📊 Memory Views (4K Card Example)

### View 1: Raw Bytes
```cpp
cardMem.data.card4K.raw[0...4095]
```

### View 2: Blocks (256 × 16 bytes)
```cpp
cardMem.data.card4K.blocks[0]      // Block 0 (manufacturer)
cardMem.data.card4K.blocks[63]     // Block 63 (last block of sector 15)
cardMem.data.card4K.blocks[127]    // Block 127 (last normal sector trailer)
cardMem.data.card4K.blocks[128]    // Block 128 (first extended sector block)
cardMem.data.card4K.blocks[255]    // Block 255 (last block, sector 39 trailer)
```

### View 3: Mixed Sectors
```cpp
// Normal sectors (0-31): 4 blocks each
cardMem.data.card4K.sectors.normal[0].block[0]
cardMem.data.card4K.sectors.normal[31].block[3]  // Last normal sector trailer

// Extended sectors (32-39): 16 blocks each
cardMem.data.card4K.sectors.extended[0].block[0]  // Sector 32, Block 0
cardMem.data.card4K.sectors.extended[7].block[15] // Sector 39, Block 15 (trailer)
```

### View 4: Detailed Normal (Sectors 0-31)
```cpp
cardMem.data.card4K.detailedNormal.sector[0].dataBlock0
cardMem.data.card4K.detailedNormal.sector[31].trailerBlock
```

### View 5: Detailed Extended (Sectors 32-39)
```cpp
cardMem.data.card4K.detailedExtended.sector[0].dataBlocks[14]  // Sector 32
cardMem.data.card4K.detailedExtended.sector[7].trailerBlock    // Sector 39 trailer
```

---

## ⚡ Performance Benefits

✅ **Zero-Copy**: Aynı bellek, farklı yorumlamalar  
✅ **Cache-Friendly**: Contiguous memory layout  
✅ **Fast Access**: Direct pointer arithmetic  
✅ **Type-Safe**: Union ensures valid access patterns  
✅ **No Indirection**: No pointers, direct struct access  

---

## 🎯 Usage Patterns

### Pattern 1: Raw Memory Operations (Hardware)
```cpp
CardMemoryLayout cardMem(false);  // 1K
// Simulate hardware dump
BYTE hwDump[1024] = {...};
std::memcpy(cardMem.data.card1K.raw, hwDump, 1024);

// Now access via any view
MifareBlock block3 = cardMem.data.card1K.blocks[3];
```

### Pattern 2: Block-Level Operations
```cpp
// Read block 5
MifareBlock blk = cardMem.getBlock(5);

// Interpret as data
auto data = blk.data.data;

// Or as something else if trailer
auto keys = blk.trailer.keyA;  // If it's a trailer
```

### Pattern 3: Sector Operations
```cpp
// Access sector 7, block 2
int sectorIdx = 7;
int blockIdx = 2;
MifareBlock block = cardMem.data.card1K.sectors[sectorIdx].block[blockIdx];

// Or with detailed view
auto trailer = cardMem.data.card1K.detailed.sector[7].trailerBlock;
```

### Pattern 4: Full Sector Backup
```cpp
// Backup entire sector
struct SectorBackup {
    MifareBlock blocks[4];
};

SectorBackup backup;
std::memcpy(&backup, &cardMem.data.card1K.sectors[3], sizeof(SectorBackup));
```

---

## 🔧 Type Safety

Union constexpr info for compile-time knowledge:

```cpp
// For 1K:
cardMem.data.card1K.blocks.size() == 64
cardMem.data.card1K.sectors.size() == 16

// For 4K:
cardMem.data.card4K.blocks.size() == 256
cardMem.data.card4K.sectors.normal.size() == 32
cardMem.data.card4K.sectors.extended.size() == 8
```

---

## 🚀 Integration with CardTopology

```cpp
CardMemoryLayout cardMem(false);  // 1K
CardTopology topology(false);     // 1K

// Layout tells you which blocks are trailers
int sectorIdx = 5;
int trailerBlock = topology.trailerBlockOfSector(sectorIdx);

// Memory gives you the data
MifareBlock trailer = cardMem.getBlock(trailerBlock);
auto keys = trailer.trailer.keyA;  // Zero-copy access
```

---

## 📝 Summary

**CardMemoryLayout** = Complete physical memory representation
- Single buffer
- Multiple zero-copy views
- Type-safe access
- Hardware-friendly
- Perfect for drivers/simulators

Şimdi tüm kartın layout'ı union-based, zero-copy! 🎉

