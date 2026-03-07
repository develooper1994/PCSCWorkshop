# DESFire API — Kullanım Kılavuzu

Bu belge, `CardIO` üzerinden DESFire kartlarla çalışmanın temel akışlarını gösterir.

---

## Ön Koşullar

```cpp
#include "CardIO.h"
#include "CardModel/DesfireMemoryLayout.h"
#include "CardProtocol/DesfireCommands.h"
#include "CardProtocol/DesfireAuth.h"
```

`Reader` ve `CardConnection` nesneleri hazır olmalıdır.

---

## 1. Temel Akış: Keşif → Seç → Auth → Oku/Yaz

```cpp
// CardIO oluştur (DESFire modu)
CardIO io(reader, CardType::MifareDesfire);

// 1. Kart keşfi — GetVersion → model'e yükle
DesfireVersionInfo vi = io.discoverCard();
// vi.totalCapacity(), vi.guessVariant() kullanılabilir

// 2. Application seç
DesfireAID myApp = DesfireAID::fromUint(0x010203);
io.selectApplication(myApp);

// 3. Auth (AES-128, key 0)
BYTEV key(16, 0x00);  // factory default
io.authenticateDesfire(key, 0, DesfireKeyType::AES128);

// 4. Dosya oku
BYTEV data = io.readFileData(/*fileNo=*/0, /*offset=*/0, /*length=*/128);

// 5. Dosya yaz
BYTEV payload = { 'H','e','l','l','o' };
io.writeFileData(0, 0, payload);
```

---

## 2. Application ve Dosya Yönetimi

### Application Oluştur/Sil

```cpp
// PICC master key ile auth gerekli
io.selectApplication(DesfireAID::picc());
io.authenticateDesfire(piccMasterKey, 0, DesfireKeyType::AES128);

// Application oluştur: AID=0x010203, keySettings=0x0F, 3 AES key
io.createApplication(
    DesfireAID::fromUint(0x010203),
    0x0F,   // keySettings: tüm izinler açık
    3,      // maxKeys
    DesfireKeyType::AES128
);

// Application sil
io.deleteApplication(DesfireAID::fromUint(0x010203));
```

### Dosya Oluştur/Sil

```cpp
// Önce application seç + auth
io.selectApplication(DesfireAID::fromUint(0x010203));
io.authenticateDesfire(appKey, 0, DesfireKeyType::AES128);

// Standard Data File (256 byte, Plain comm, tüm key 0)
DesfireAccessRights ar;
ar.readKey = 0; ar.writeKey = 0;
ar.readWriteKey = 0; ar.changeKey = 0;

io.createStdDataFile(0, DesfireCommMode::Plain, ar, 256);

// Value File
io.createValueFile(1, DesfireCommMode::MAC, ar,
    /*lower=*/0, /*upper=*/10000, /*initial=*/500);

// Linear Record File
io.createLinearRecordFile(2, DesfireCommMode::Full, ar,
    /*recordSize=*/32, /*maxRecords=*/100);

// Dosya sil
io.deleteFile(2);
```

---

## 3. Value İşlemleri (Transaction)

```cpp
// Auth sonrası:
io.creditValue(/*fileNo=*/1, /*amount=*/100);
io.commitTransaction();  // kalıcı yap

io.debitValue(1, 50);
io.commitTransaction();

// İptal etmek isterseniz:
io.creditValue(1, 999);
io.abortTransaction();  // geri al
```

---

## 4. Keşif API'ları

```cpp
// Karttaki uygulamalar
auto aids = io.getApplicationIDs();
for (auto& aid : aids) {
    cout << "AID: " << hex << aid.toUint() << endl;
}

// Seçili uygulamadaki dosyalar
io.selectApplication(aids[0]);
auto files = io.getFileIDs();

// Dosya ayarları
for (BYTE f : files) {
    auto settings = io.getFileSettings(f);
    // settings.fileType, settings.standard.fileSize, vs.
}

// Kalan bellek
size_t free = io.getFreeMemory();

// Key versiyonu
BYTE ver = io.getKeyVersion(0);
```

---

## 5. Kart Tipi Kontrolü

```cpp
CardIO io(reader, CardType::MifareDesfire);

if (io.card().isDesfire()) {
    // DESFire API kullan
}

if (io.card().isClassic()) {
    // Classic API kullan (readBlock, writeBlock, etc.)
}

// DESFire kartında Classic API çağrılırsa → std::logic_error
```

---

## 6. Hata Yönetimi

```cpp
try {
    io.selectApplication(DesfireAID::fromUint(0xFFFFFF));
}
catch (const std::runtime_error& e) {
    // DESFire error: 91A0 (application not found)
    cerr << e.what() << endl;
}

try {
    io.readFileData(0, 0, 128);  // auth olmadan
}
catch (const std::runtime_error& e) {
    // DESFire error: 91AE (authentication error)
}

try {
    // Classic API → DESFire kartında
    io.readBlock(5);
}
catch (const std::logic_error& e) {
    // "readBlock: requires Classic/Ultralight card"
}
```

---

## Mimari Özet

```
CardIO (DESFire)
│
├── discoverCard()          → GetVersion (3-part)
├── selectApplication()     → SelectApp APDU
├── authenticateDesfire()    → DesfireAuth (3-pass mutual)
│     ├── DesfireCrypto     → nonce, rotate, session key
│     └── CngBlockCipher    → raw AES/3DES CBC (Cipher proj.)
│
├── readFileData()          → ReadData + multi-frame
├── writeFileData()         → WriteData
├── createApplication()     → CreateApp
├── createStdDataFile()     → CreateStdDataFile
├── creditValue()           → Credit + CommitTransaction
│
└── DesfireSecureMessaging  → CMAC wrap/unwrap (MAC mode)
```

---

## Proje Dağılımı

| Sınıf | Proje | Sorumluluk |
|---|---|---|
| `CngBlockCipher` | Cipher | Raw AES/3DES CBC (no padding) + CMAC |
| `DesfireCrypto` | Card | Nonce, rotate, session key derivation |
| `DesfireAuth` | Card | 3-pass mutual auth state machine |
| `DesfireSession` | Card | Session state (key, IV, counter) |
| `DesfireCommands` | Card | APDU construction + response parsing |
| `DesfireSecureMessaging` | Card | CMAC wrap/unwrap |
| `DesfireMemoryLayout` | Card | In-memory model (app/file hierarchy) |
| `CardIO` | Card | Facade — tüm API'ları birleştirir |
