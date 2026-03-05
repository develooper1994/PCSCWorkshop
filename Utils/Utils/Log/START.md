# ?? LOG SÝSTEMÝ - BAÞLAMA REHBERÝ

## ? 30 Saniye Özet

```cpp
#include "Log/Log.h"

// main()'de
pcsc::Log::getInstance().setLogLevel(pcsc::LogLevel::Debug);
pcsc::Log::getInstance().enableAllLogTypes();
pcsc::Log::getInstance().enableAllCategories();

// Kodda
LOG_DEBUG("msg");
LOG_PCSC_DEBUG("APDU");
LOG_READER_ERROR("error");
```

## ?? Dokümantasyon

| Dosya | Süre | Amaç |
|-------|------|------|
| **QUICKSTART.md** | 5 dk | Hýzlý baþlangýç ? |
| **STRATEGY.md** | 10 dk | Kontrol mekanizmalarý |
| **README.md** | 5 dk | Sistem genel açýklamasý |
| **INDEX.md** | 5 dk | Dosya yapýsý ve navigasyon |
| **ORGANIZATION.txt** | 3 dk | Yapýlan deðiþiklikleri |

## ?? Baþla

1. **QUICKSTART.md** oku (5 dakika)
2. **#include "Log/Log.h"** ekle
3. **LOG_*** makrolarý kullan

## ?? Include Path

```cpp
// ? Doðru
#include "Log/Log.h"

// ? Yanlýþ
#include "Log.h"
```

## ?? Dosya Baþýnda

```cpp
// Dosya türüne uygun makro
#include "Log/Log.h"

// PCSC iþlemleri
LOG_PCSC_DEBUG("msg");

// Reader iþlemleri
LOG_READER_ERROR("msg");

// Connection iþlemleri
LOG_CONN_INFO("msg");

// Card iþlemleri
LOG_CARD_DEBUG("msg");

// Cipher iþlemleri
LOG_CIPHER_ERROR("msg");
```

## ? Temel Makrolar

```cpp
LOG_DEBUG(msg)      // Debug
LOG_INFO(msg)       // Info
LOG_WARNING(msg)    // Warning
LOG_ERROR(msg)      // Hata
```

## ?? Runtime Kontrol

```cpp
// Seviye
Log::getInstance().setLogLevel(LogLevel::Error);

// Tür
Log::getInstance().disableLogType(LogType::Debug);

// Kategori
Log::getInstance().disableCategory(LogCategory::PCSC);

// Kontrol
if (Log::getInstance().isDebugEnabled()) { ... }
```

---

**?? Devam: QUICKSTART.md'yi oku!**
