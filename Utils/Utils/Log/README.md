# LOG SISTEMI

Geliþtirilmiþ, 3 katmanlý kontrol mekanizmasýyla enterprise-grade log sistemi.

## ? ÖZELLÝKLER

- **Global Log Level** - 5 seçenek (Off, Error, Warning, Info, Debug)
- **Baðýmsýz Log Types** - Her tür ayrý kontrol edilebilir
- **Log Categories** - 6 kategori (PCSC, Reader, Cipher, Connection, Card, General)
- **Thread-Safe** - Çok thread'li ortamlarda güvenli
- **Runtime Ayarlama** - Uygulamayý durdurmadan kontrol et
- **Production Ready** - Minimal overhead

## ?? DOSYALAR

| Dosya | Açýklama |
|-------|----------|
| `Log.h` | Header ve makrolar |
| `Log.cpp` | Implementation |
| `QUICKSTART.md` | Hýzlý baþlangýç (5 dakika) |
| `STRATEGY.md` | Detaylý referans |
| `README.md` | Bu dosya |

## ?? HIZLI BAÞLAMA

### 1. Include et
```cpp
#include "Log/Log.h"
```

### 2. main()'de baþlat
```cpp
pcsc::Log::getInstance().setLogLevel(pcsc::LogLevel::Debug);
pcsc::Log::getInstance().enableAllLogTypes();
pcsc::Log::getInstance().enableAllCategories();
```

### 3. Kullan
```cpp
LOG_DEBUG("message");
LOG_PCSC_DEBUG("APDU send");
LOG_READER_ERROR("error");
LOG_CONN_INFO("info");
```

## ?? ANA MAKROLAR

### Temel
- `LOG_DEBUG(msg)` - Debug mesajý (General kategorisi)
- `LOG_INFO(msg)` - Info mesajý
- `LOG_WARNING(msg)` - Uyarý mesajý
- `LOG_ERROR(msg)` - Hata mesajý

### Kategori-Spesifik
- `LOG_PCSC_*()` - PCSC kategorisi
- `LOG_READER_*()` - Reader kategorisi
- `LOG_CIPHER_*()` - Cipher kategorisi
- `LOG_CONN_*()` - Connection kategorisi
- `LOG_CARD_*()` - Card kategorisi

## ?? RUNTIME KONTROLÜ

```cpp
// Seviyeyi deðiþtir
Log::getInstance().setLogLevel(LogLevel::Error);

// Türü kontrol et
Log::getInstance().disableLogType(LogType::Debug);

// Kategorisini kontrol et
Log::getInstance().disableCategory(LogCategory::PCSC);

// Ayarlarý yazdýr
Log::getInstance().printSettings();
```

## ?? DAHA FAZLA BÝLGÝ

- **QUICKSTART.md** - Hýzlý referans
- **STRATEGY.md** - Kontrol mekanizmalarý ve stratejileri

## ?? DOSYA BAÞINDA

Her dosyada uygun makrolarý kullan:

```cpp
// CardConnection.cpp
#include "Log/Log.h"
LOG_CONN_DEBUG("Baðlantý iliþkili");
LOG_PCSC_DEBUG("APDU iliþkili");

// Reader.cpp
#include "Log/Log.h"
LOG_READER_DEBUG("Reader iliþkili");

// MifareClassic.cpp
#include "Log/Log.h"
LOG_CARD_DEBUG("Kart iliþkili");
```

---

**Baþla: QUICKSTART.md veya STRATEGY.md'ye bak.**
