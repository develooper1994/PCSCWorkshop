// LOG SISTEMI - HIZLI BAÞLANGIÇ KILAVUZU
// ============================================================

## TEMEL KULLANIM (5 DAKÝKA)

### 1. Include Ekle
```cpp
#include "Log/Log.h"
```

### 2. main() Ýçinde Baþlat
```cpp
int main() {
    // Debug modunda baþla (recommended)
    pcsc::Log::getInstance().setLogLevel(pcsc::LogLevel::Debug);
    pcsc::Log::getInstance().enableAllLogTypes();
    pcsc::Log::getInstance().enableAllCategories();
    
    // ...
    return 0;
}
```

### 3. Kodunuzda Kullanýn
```cpp
LOG_DEBUG("Bu bir debug mesajý");
LOG_INFO("Bu bir info mesajý");
LOG_WARNING("Bu bir uyarý");
LOG_ERROR("Bu bir hata");
```

---

## KATEGORÝ ile KULLANIM

Her dosyadaki logu kategoriyle belirleyin:

```cpp
// PCSC (Smart Card iþlemleri)
LOG_PCSC_DEBUG("APDU send");
LOG_PCSC_ERROR("Card error");

// Reader (Okuyucu iþlemleri)
LOG_READER_DEBUG("Reader info");
LOG_READER_ERROR("Reader error");

// Connection (Baðlantý iþlemleri)
LOG_CONN_DEBUG("Connection established");
LOG_CONN_ERROR("Connection failed");

// Card (Kart iþlemleri)
LOG_CARD_DEBUG("Card selected");
LOG_CARD_ERROR("Card error");

// Cipher (Þifreleme iþlemleri)
LOG_CIPHER_DEBUG("Encryption started");
LOG_CIPHER_ERROR("Encryption failed");
```

---

## RUNTIME'DA KONTROL

### Log Seviyesini Deðiþtir
```cpp
// Sadece hatalar göster
pcsc::Log::getInstance().setLogLevel(pcsc::LogLevel::Error);

// Info ve daha yukarýsý
pcsc::Log::getInstance().setLogLevel(pcsc::LogLevel::Info);

// Tüm loglar (debug)
pcsc::Log::getInstance().setLogLevel(pcsc::LogLevel::Debug);

// Hiçbir þey gösterme
pcsc::Log::getInstance().setLogLevel(pcsc::LogLevel::Off);
```

### Belirli Türleri Kapat
```cpp
// Debug mesajlarýný gizle
pcsc::Log::getInstance().disableLogType(pcsc::LogType::Debug);

// Tüm debug'ý kapat
pcsc::Log::getInstance().disableAllLogTypes();

// Debug'ý aç
pcsc::Log::getInstance().enableLogType(pcsc::LogType::Debug);
```

### Kategorileri Kontrol Et
```cpp
// PCSC kategorisini gizle (low-level ayrýntýlar)
pcsc::Log::getInstance().disableCategory(pcsc::LogCategory::PCSC);

// Reader kategorisini aç
pcsc::Log::getInstance().enableCategory(pcsc::LogCategory::Reader);

// Tüm kategorileri kapat
pcsc::Log::getInstance().disableAllCategories();

// Tüm kategorileri aç
pcsc::Log::getInstance().enableAllCategories();
```

---

## AYARLARI YAZDIRMA

Mevcut ayarlarý konsola yazdýrýn:

```cpp
pcsc::Log::getInstance().printSettings();
```

---

## KOMBÝNE KULLANIM

### Örnek 1: Development
```cpp
// Tüm açýk ama PCSC detaylarýný gizle (kalabalýk olduðu için)
pcsc::Log::getInstance().setLogLevel(pcsc::LogLevel::Debug);
pcsc::Log::getInstance().enableAllLogTypes();
pcsc::Log::getInstance().enableAllCategories();
pcsc::Log::getInstance().disableCategory(pcsc::LogCategory::PCSC);
```

### Örnek 2: Debugging Baðlantý Problemi
```cpp
// Sadece Connection kategori, Debug level
pcsc::Log::getInstance().setLogLevel(pcsc::LogLevel::Debug);
pcsc::Log::getInstance().disableAllCategories();
pcsc::Log::getInstance().enableCategory(pcsc::LogCategory::Connection);
```

### Örnek 3: Production
```cpp
// Sadece hatalar
pcsc::Log::getInstance().setLogLevel(pcsc::LogLevel::Error);
```

### Örnek 4: Test Modu
```cpp
// Uyarý ve hata
pcsc::Log::getInstance().setLogLevel(pcsc::LogLevel::Warning);
pcsc::Log::getInstance().disableLogType(pcsc::LogType::Debug);
```

---

## MAKRO REFERANSI

| Makro | Açýklama |
|-------|----------|
| `LOG_DEBUG(msg)` | Debug mesajý (General) |
| `LOG_INFO(msg)` | Info mesajý (General) |
| `LOG_WARNING(msg)` | Uyarý mesajý (General) |
| `LOG_ERROR(msg)` | Hata mesajý (General) |
| `LOG_PCSC_*` | PCSC kategorisi |
| `LOG_READER_*` | Reader kategorisi |
| `LOG_CONN_*` | Connection kategorisi |
| `LOG_CARD_*` | Card kategorisi |
| `LOG_CIPHER_*` | Cipher kategorisi |

---

## DOSYA BAÞINDA KATEGORISINI AYARLA

Her dosyada doðru makrolarý kullanarak kategorisini belirle:

```cpp
// CardConnection.cpp
#include "Log/Log.h"

// CONNECTION kategorisini kullan
LOG_CONN_DEBUG("Starting connection");
// APDU iþlemleri için PCSC kategorisi
LOG_PCSC_DEBUG("Sending APDU");
```

```cpp
// Reader.cpp
#include "Log/Log.h"

// Reader iþlemleri
LOG_READER_DEBUG("Reading card");
```

```cpp
// MifareClassic.cpp
#include "Log/Log.h"

// Kart iþlemleri
LOG_CARD_DEBUG("Authenticating block");
```

---

## ÝLERÝ AYARLAMALAR

### Tüm Debug Türlerini Kapat Ama PCSC Hatalarý Göster
```cpp
pcsc::Log::getInstance().setLogLevel(pcsc::LogLevel::Error);
pcsc::Log::getInstance().disableAllCategories();
pcsc::Log::getInstance().enableCategory(pcsc::LogCategory::PCSC);
```

### Varsayýlanlara Sýfýrla
```cpp
pcsc::Log::getInstance().resetToDefaults();
```

---

**Detaylý dokümantasyon: Log/ dizinindeki diðer dosyalara bakýn.**
