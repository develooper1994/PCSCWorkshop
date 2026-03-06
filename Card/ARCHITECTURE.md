# Card Subsystem Architecture

Mifare Classic 1K/4K kartlarını PCSC protokolü ile yönetmek için modern, temiz mimari.

## Dosya Yapısı

```
Card/Card/
├── CardDataTypes.h                 ← Ortak tip tanımları (BYTE, KEYBYTES, vb)
├── CardInterface.h/cpp             ← In-memory kart modeli
├── CardIO.h/cpp                    ← PCSC + CardInterface köprüsü
│
├── CardModel/
│   ├── BlockDefinition.h           ← MifareBlock union (raw 16-byte block)
│   ├── CardMemoryLayout.h          ← Tüm kart bellegi (1K/4K union)
│   ├── CardDataTypes.h             ← Permission enum, KeyInfo struct
│   ├── CardTopology.h/cpp          ← Blok/sektör hesaplamaları
│   └── TrailerConfig.h             ← Access bits codec (tam Mifare spec)
│
└── CardProtocol/
    ├── AccessControl.h/cpp         ← Permission matrisi decode
    ├── KeyManagement.h/cpp         ← Key kayıt/sorgulama
    └── AuthenticationState.h/cpp   ← Auth oturum takibi
```

## Temel Konseptler

### 1. CardMemoryLayout — Zero-Copy Physical Model

```cpp
CardMemoryLayout layout(false);  // 1K
// Union view:
layout.data.card1K.blocks[0].manufacturer.uid
layout.data.card1K.blocks[3].trailer.keyA
layout.data.card1K.detailed.sector[2].trailerBlock
```

**Özellik**: Raw bellek hiç kopyalanmaz — union overlaying.

### 2. CardInterface — In-Memory Logic Model

```cpp
CardInterface card(false);       // 1K
card.loadMemory(rawBytes, 1024);

// Topology
int sector = card.getSectorForBlock(8);
int trailer = card.getTrailerBlockOfSector(0);
bool isMfg = card.isManufacturerBlock(0);

// Keys
card.registerKey(KeyType::A, key, KeyStructure::NonVolatile, 0x01);
KEYBYTES cardKey = card.getCardKey(0, KeyType::A);  // trailer'dan oku

// Auth simulation
card.authenticate(5, KeyType::A);
bool ok = card.isAuthenticated(5);

// Access check
bool r = card.canRead(8, KeyType::A);
bool w = card.canWrite(8, KeyType::B);
```

**Özellik**: I/O yapı yapmaz — saf bellek modeli.

### 3. CardIO — PCSC Bridge

```cpp
ACR1281UReader reader(conn, 16, false, ...);
CardIO io(reader, false);        // 1K

io.setDefaultKey(key, ...);
int okBlocks = io.readCard();    // PCSC → memory

BYTEV data = io.readBlock(8);    // PCSC oku + memory güncelle
io.writeBlock(8, payload);       // PCSC yaz + memory güncelle

// Trailer yönetimi
TrailerConfig tc = io.readTrailer(2);
SectorAccessConfig cfg = io.getAccessConfig(2);
io.setSectorMode(5, SectorMode::READ_AB_WRITE_B);  // karta yaz
io.writeTrailer(5, newConfig);

// Bulk backup
auto backup = io.saveAllTrailers();
io.restoreAllTrailers(backup);

// Model erişimi
CardInterface& card = io.card();
Reader& rdr = io.reader();
```

**Özellik**: Her read/write hem PCSC'ye hem memory'ye yansır.

### 4. TrailerConfig — Access Bits Codec (Mifare Spec)

```cpp
// Factory default decode
ACCESSBYTES ab = {0xFF, 0x07, 0x80, 0x69};
SectorAccessConfig cfg = AccessBitsCodec::decode(ab);

// Data block permission matrix (C1C2C3)
DataBlockPermission dp = cfg.dataPermission(0);
bool read = dp.readA;      // true
bool write = dp.writeB;    // true

// Trailer permission matrix
TrailerPermission tp = cfg.trailerPermission();
bool keyBWrite = tp.keyBWriteB;  // false (001 modunda)

// Hazır modlar
SectorAccessConfig readOnly = sectorModeToConfig(SectorMode::READ_ONLY_AB);
ACCESSBYTES encoded = AccessBitsCodec::encode(readOnly);
bool valid = AccessBitsCodec::verify(encoded);  // true

// Trailer seri/deseriyalize
TrailerConfig tc = TrailerConfig::factoryDefault();
MifareBlock blk = tc.toBlock();
TrailerConfig parsed = TrailerConfig::fromBlock(blk);
```

**Özellik**: Tam Mifare Classic spec — 8 C1C2C3 kombinasyon permission tablosu.

## Workflow Örneği

### Gerçek Kart (PCSC)

```cpp
ACR1281UReader reader(conn, ...);
CardIO io(reader, false);

// 1. Kartı oku
int ok = io.readCard();
KEYBYTES uid = io.card().getUID();

// 2. Trailer incele
SectorAccessConfig cfg = io.getAccessConfig(2);
DataBlockPermission dp = cfg.dataPermission(0);

// 3. Veri yaz
io.writeBlock(8, "Hello World!");

// 4. Access restrictive hale getir
io.setSectorMode(5, SectorMode::READ_AB_WRITE_B);

// 5. Yedekle
auto backup = io.saveAllTrailers();
```

### Simülasyon (PCSC olmadan)

```cpp
CardInterface card(false);
card.loadMemory(rawBytes, 1024);

// In-memory işler
int sector = card.getSectorForBlock(8);
bool canWrite = card.canWrite(8, KeyType::B);
card.authenticate(2, KeyType::A);
```

## Test Suite

**Unit Tests** (simülasyon, PCSC yok):
- `Card Memory Layout (1K/4K)` — Memory union yapısı
- `Card Topology` — Blok/sektör hesaplamaları
- `Key Management` — Key kayıt/sorgulama
- `Access Control` — Permission tablo decode
- `Authentication State` — Auth session
- `Card Interface` — Tüm bileşen entegrasyonu
- `AccessBits Codec` — Access bits encode/decode/verify
- `Trailer Config` — Trailer serialize/deserialize
- **`Card Simulation`** — Tam kart scenario (10 adım)

**Integration Test** (PCSC + CardIO):
- `RealCardReaderTest.cpp` — Gerçek kart ile full okuma/yazma/trailer

## Mifare Classic Access Bits (Spec)

Trailer block (16 byte):
```
[KeyA: 6] [AccessBits: 4] [KeyB: 6]
           ↓
         Byte 6-8: C1, C2, C3 (inverted copies ile)
         Byte 9: GPB (General Purpose Byte)
```

**8 Kombinasyon C1C2C3**:
- `000` → Data: A|B read+write (transport)
- `001` → Data: A|B read+write, Trailer: A only
- `010` → Data: A|B read-only
- `011` → Data: B only
- `100` → Data: A|B read, B write
- `101` → Data: B read
- `110` → Data: A|B read, B inc, A|B dec (value block)
- `111` → Frozen (no access)

## Temizlik Notları

### Silinenler (eski çorba):
- ❌ `MifareClassic/` — MifareCardCore, MifareCardSimulator, SimulatedReader
- ❌ `Topology/` — Eski SectorConfig, Topology, SectorConfigBuilder
- ❌ `CardModel/AccessBits.h/cpp` — TrailerConfig::AccessBitsCodec tarafından değiştirildi
- ❌ `Card.h/cpp` — Eski wrapper
- ❌ `Desfire/` — Mifare Classic değil

### Tutulananlar (modern):
- ✅ `CardInterface` + `CardIO` — Sağlam, dokümante, test edilmiş
- ✅ `CardMemoryLayout` + `CardTopology` — Zero-copy, spesifik
- ✅ `TrailerConfig` — Tam Mifare spec codec
- ✅ `AccessControl` — Gerçek permission tablo
- ✅ Tüm tests geçiyor (10/10)
