# ?? LOG SÝSTEMÝ DÝZÝNÝ

Geliþtirilmiþ, 3 katmanlý kontrol mekanizmasýyla enterprise-grade log sistemi.

## ?? DOSYA YAPISI

```
Log/
??? Log.h              ? Header, enums, makrolar
??? Log.cpp            ? Implementation, Singleton
??? README.md          ? Sistem genel açýklamasý
??? QUICKSTART.md      ? Hýzlý baþlangýç (5 dakika) ?
??? STRATEGY.md        ? Detaylý kontrol mekanizmalarý
??? INDEX.md           ? Bu dosya
```

## ?? DOSYA AÇIKLAMALARI

### `Log.h` (200+ satýr)
- **Amaç:** Header dosyasý
- **Ýçerik:**
  - `LogLevel` enum (5 seviye)
  - `LogType` enum (4 tür)
  - `LogCategory` enum (6 kategori)
  - `Log` sýnýfý tanýmý
  - Makro tanýmlarý (15+ makro seti)
- **Include:** `#include "Log/Log.h"`

### `Log.cpp` (300+ satýr)
- **Amaç:** Implementation
- **Ýçerik:**
  - Singleton pattern
  - Log seviyesi kontrolü
  - Log type kontrolü
  - Kategori kontrolü
  - Thread-safe iþlemler
- **Compile:** Otomatik (project build'e dahil)

### `README.md`
- **Amaç:** Sistem genel açýklamasý
- **Ýçerik:**
  - Özellikler özeti
  - Hýzlý baþlama
  - Ana makrolar
  - Runtime kontrol
  - Dosya yapýsý

### `QUICKSTART.md` ?
- **Amaç:** Hýzlý baþlangýç rehberi
- **Hedef:** Acele eden geliþtiriciler
- **Süre:** 5 dakika
- **Ýçerik:**
  - Temel kullaným (3 adým)
  - Kategori makrolarý
  - Runtime kontrol örnekleri
  - Komb ayarlamalar
  - Makro referansi

### `STRATEGY.md`
- **Amaç:** Detaylý kontrol mekanizmalarý
- **Hedef:** Sistemi anlamak isteyen geliþtiriciler
- **Ýçerik:**
  - 3 katmanlý mimarileri
  - Kontrol fonksiyonlarý
  - Senaryo örnekleri
  - Kontrol akýþý
  - Best practices

### `INDEX.md`
- **Amaç:** Bu dosya - navigasyon ve dosya açýklamasý

## ?? HIZLI BAÞLAMA

### 1. Oku
?? **QUICKSTART.md** (5 dakika)

### 2. Include et
```cpp
#include "Log/Log.h"
```

### 3. Baþlat (main())
```cpp
pcsc::Log::getInstance().setLogLevel(pcsc::LogLevel::Debug);
pcsc::Log::getInstance().enableAllLogTypes();
pcsc::Log::getInstance().enableAllCategories();
```

### 4. Kullan
```cpp
LOG_DEBUG("message");
LOG_PCSC_DEBUG("APDU send");
LOG_READER_ERROR("error");
```

## ?? ANA MAKROLAR

| Kategori | Makrolar |
|----------|----------|
| **Genel** | `LOG_DEBUG`, `LOG_INFO`, `LOG_WARNING`, `LOG_ERROR` |
| **PCSC** | `LOG_PCSC_DEBUG`, `LOG_PCSC_INFO`, ... |
| **Reader** | `LOG_READER_DEBUG`, `LOG_READER_INFO`, ... |
| **Cipher** | `LOG_CIPHER_DEBUG`, `LOG_CIPHER_INFO`, ... |
| **Connection** | `LOG_CONN_DEBUG`, `LOG_CONN_INFO`, ... |
| **Card** | `LOG_CARD_DEBUG`, `LOG_CARD_INFO`, ... |

## ?? KONTROL FONKSÝYONLARI

### Global Level
```cpp
setLogLevel(level)
getLogLevel()
```

### Log Types
```cpp
enableLogType(type)
disableLogType(type)
toggleLogType(type)
enableAllLogTypes()
disableAllLogTypes()
```

### Categories
```cpp
enableCategory(category)
disableCategory(category)
enableAllCategories()
disableAllCategories()
```

### Kontrol
```cpp
isDebugEnabled()
isInfoEnabled()
isWarningEnabled()
isErrorEnabled()

isDebugEnabledForCategory(category)
isInfoEnabledForCategory(category)
isWarningEnabledForCategory(category)
isErrorEnabledForCategory(category)
```

### Diðer
```cpp
resetToDefaults()
printSettings()
```

## ?? DOSYALARDA KULLANIM

Her dosyada uygun kategori makrosunu kullan:

```cpp
// CardConnection.cpp
#include "Log/Log.h"

LOG_CONN_DEBUG("Connection iþlemi");
LOG_PCSC_DEBUG("APDU iþlemi");
```

```cpp
// Reader.cpp
#include "Log/Log.h"

LOG_READER_DEBUG("Reader iþlemi");
```

```cpp
// MifareClassic.cpp
#include "Log/Log.h"

LOG_CARD_DEBUG("Kart iþlemi");
```

## ?? SENARYO ÖRNEKLERÝ

### Development
```cpp
setLogLevel(Debug);
enableAllLogTypes();
enableAllCategories();
disableCategory(PCSC);  // Kalabalýk olduðu için
```

### Production
```cpp
setLogLevel(Error);
```

### Debugging Spesifik Sorun
```cpp
setLogLevel(Debug);
disableAllCategories();
enableCategory(Connection);
enableCategory(Card);
```

### Testing
```cpp
setLogLevel(Warning);
disableLogType(Debug);
disableLogType(Info);
```

## ? AVANTAJLAR

? **3 seviyeli kontrol** - Maksimum esneklik
? **Granüler** - Tam istediðinizi yapýn
? **Runtime** - Uygulamayý durdurmadan deðiþtir
? **Thread-Safe** - Çok thread'li ortamlarda güvenli
? **Production Ready** - Minimal overhead
? **Easy to Use** - Basit makrolar

## ?? NAVIGASYON

| Ýhtiyaç | Dosya |
|---------|-------|
| Hýzlý baþla | **QUICKSTART.md** ? |
| Detaylý anla | **STRATEGY.md** |
| Sistem taný | **README.md** |
| Navigasyon | **INDEX.md** (bu) |

---

**Baþla: QUICKSTART.md'yi oku!** ??
