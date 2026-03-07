# Cipher Projesi — OpenSSL Migration & Crypto Facade Planı

> **Durum:** PLAN aşamasında — henüz implementasyon başlamadı.
> **Branch:** `feature/cipher-openssl-migration`
> **Öncelik:** Yüksek — Linux desteği için blocker.

---

## 1. Mevcut Durum Analizi

### 1.1 Cipher Projesi — Dosya Haritası

| Dosya | Platform | Sorumluluk | Durum |
|-------|----------|------------|-------|
| `Cipher.h` (ICipher) | Cross-platform ✅ | Temel cipher arayüzü | Korunacak |
| `ICipherAAD.h` | Cross-platform ✅ | AEAD arayüzü | Korunacak |
| `CipherAadFallback.cpp` | Cross-platform ✅ | AAD fallback | Korunacak |
| `CipherUtil.h` | Cross-platform ✅ | Modüler aritmetik (invMod256 vb.) | Korunacak |
| `XorCipher.h/.cpp` | Cross-platform ✅ | Basit XOR cipher | Korunacak |
| `CaesarCipher.h/.cpp` | Cross-platform ✅ | Caesar/Affine variant | Korunacak |
| `AffineCipher.h/.cpp` | Cross-platform ✅ | Affine cipher | Korunacak |
| **`CngAES.h/.cpp`** | **Windows-only ❌** | AES-CBC (padded) — CNG | → `AesCbcCipher` |
| **`Cng3DES.h/.cpp`** | **Windows-only ❌** | 3DES-CBC (padded) — CNG | → `TripleDesCbcCipher` |
| **`CngAESGcm.h/.cpp`** | **Windows-only ❌** | AES-GCM (AEAD) — CNG | → `AesGcmCipher` |
| **`CngBlockCipher.h/.cpp`** | **Windows-only ❌** | Raw CBC + CMAC — CNG | → `BlockCipher` |
| `CngAESGcmUtil.h` | CNG'ye bağlı | GCM nonce/tag split | → `AesGcmUtil.h` |
| `Cipher/Ciphers.h` (root) | **BOZUK** — guard yok | Convenience header | Silinecek |
| `Cipher/Cipher/Ciphers.h` | Guard'lı ✅ | Convenience header | Güncellenecek |

### 1.2 Utils Projesi — Temizlik Gereken Noktalar

| Dosya | Sorun | Aksiyon |
|-------|-------|---------|
| `PCSC_new.h` | Boş dosya | Sil |
| `Types.h` | `BYTE`/`BYTEV` çift tanım (`PcscUtils.h` ile) | Sil, tüm include'ları `PcscUtils.h`'ye yönlendir |
| `ArrayUtils.h` | `Utils/` root'unda, `Utils/Utils/` dışında | `Utils/Utils/` altına taşı |
| `Log/*.md`, `Log/ORGANIZATION.txt` | Kod değil, dağınık dokümantasyon | `docs/` klasörüne taşı veya sil |

### 1.3 Card Projesi — Etkilenen Dosyalar

| Dosya | Sorun | Aksiyon |
|-------|-------|---------|
| `DesfireCrypto.cpp` | `#include <windows.h>` + `<bcrypt.h>` + `BCryptGenRandom` | → OpenSSL `RAND_bytes()` |
| `DesfireCrypto.cpp` | `#include "CngBlockCipher.h"` | → `#include "BlockCipher.h"` |

---

## 2. Kütüphane Seçimi: OpenSSL

### 2.1 Neden OpenSSL?

| Kriter | OpenSSL | Mbed TLS | libsodium | Botan |
|--------|---------|----------|-----------|-------|
| AES-CBC | ✅ | ✅ | ❌ | ✅ |
| 3DES-CBC | ✅ | ✅ | ❌ | ✅ |
| AES-GCM | ✅ | ✅ | ✅ | ✅ |
| AES-CMAC | ✅ | ✅ | ❌ | ✅ |
| AES-CTR | ✅ | ✅ | ✅ | ✅ |
| SHA-256/384/512 | ✅ | ✅ | ✅ | ✅ |
| HMAC | ✅ | ✅ | ✅ | ✅ |
| PBKDF2 | ✅ | ✅ | ❌ | ✅ |
| HKDF | ✅ | ✅ | ✅ | ✅ |
| ECDH/ECDSA | ✅ | ✅ | ✅ | ✅ |
| Random | ✅ | ✅ | ✅ | ✅ |
| No-padding raw CBC | ✅ (`set_padding(0)`) | ✅ | ❌ | ✅ |
| Windows desteği | ✅ (vcpkg) | ✅ | ✅ | ✅ |
| Linux/macOS | ✅ (sistem paketi) | ✅ | ✅ | ✅ |
| Yaygınlık | ⭐⭐⭐ | ⭐⭐ | ⭐⭐ | ⭐ |
| C++17 uyum | ✅ (C API, wrapper yazılır) | ✅ | ✅ | ✅ |
| Boyut | Büyük (~3MB) | Küçük | Küçük | Büyük |

**Sonuç:** OpenSSL — en geniş algoritma desteği, en yaygın platform kapsamı,
DESFire'ın ihtiyaç duyduğu tüm no-padding raw CBC + CMAC senaryoları destekleniyor.

### 2.2 OpenSSL Kurulumu

**Windows (vcpkg):**
```bash
vcpkg install openssl:x64-windows
```

**Linux (apt):**
```bash
sudo apt install libssl-dev
```

**macOS (brew):**
```bash
brew install openssl
```

---

## 3. Yeni Crypto Mimarisi

### 3.1 Tek Giriş Noktası: `crypto` Namespace (Facade)

Tüm kriptografik işlemler `crypto` namespace'i altında toplanır. Kullanıcı sadece
`#include "Crypto.h"` yaparak her şeye erişir. Implementasyon detayları gizlidir.

```
Cipher/
├── Cipher/
│   │
│   │── ── Arayüzler (değişmez) ──────────────────────────
│   ├── Cipher.h                  ICipher arayüzü
│   ├── ICipherAAD.h              ICipherAAD arayüzü (AEAD)
│   ├── CipherAadFallback.cpp     AAD fallback implementasyonu
│   ├── CipherUtil.h              Modüler aritmetik
│   │
│   │── ── Tek Giriş Noktası (YENİ) ─────────────────────
│   ├── Crypto.h                  crypto:: facade header
│   ├── Crypto.cpp                OpenSSL implementasyonu
│   │
│   │── ── Block Cipher (YENİ — CngBlockCipher yerine) ──
│   ├── BlockCipher.h             Static API: encryptAES, decrypt2K3DES, cmacAES...
│   ├── BlockCipher.cpp           OpenSSL EVP backend
│   │
│   │── ── ICipher Implementasyonları (yeniden yazılacak) ─
│   ├── AesCbcCipher.h/.cpp       AES-CBC (padded) — ICipher
│   ├── AesCtrCipher.h/.cpp       AES-CTR — ICipher (YENİ)
│   ├── TripleDesCbcCipher.h/.cpp  3DES-CBC (padded) — ICipher
│   ├── AesGcmCipher.h/.cpp       AES-GCM — ICipherAAD
│   ├── AesGcmUtil.h              GCM nonce/tag split
│   │
│   │── ── Hash & MAC (YENİ) ────────────────────────────
│   ├── Hash.h/.cpp               SHA-256, SHA-384, SHA-512
│   ├── Hmac.h/.cpp               HMAC-SHA256, HMAC-SHA384, HMAC-SHA512
│   │
│   │── ── Key Derivation (YENİ) ────────────────────────
│   ├── Kdf.h/.cpp                PBKDF2, HKDF
│   │
│   │── ── Random (YENİ) ────────────────────────────────
│   ├── Random.h/.cpp             Kriptografik random byte üretimi
│   │
│   │── ── Basit Cipher'lar (değişmez) ──────────────────
│   ├── XorCipher.h/.cpp
│   ├── CaesarCipher.h/.cpp
│   ├── AffineCipher.h/.cpp
│   │
│   │── ── Convenience Header ───────────────────────────
│   └── Ciphers.h                 Tümünü include eder
│
├── Ciphers.h                     SİLİNECEK (guard'sız duplicate)
└── CMakeLists.txt                OpenSSL entegrasyonu
```

### 3.2 `Crypto.h` — API Tasarımı

```cpp
#ifndef PCSC_CRYPTO_H
#define PCSC_CRYPTO_H

#include "Cipher.h"
#include "ICipherAAD.h"
#include <vector>
#include <string>
#include <memory>
#include <cstddef>

// ════════════════════════════════════════════════════════════════════════════════
// crypto — Tek giriş noktası: tüm kriptografik işlemler
// ════════════════════════════════════════════════════════════════════════════════
//
//   #include "Crypto.h"
//
//   // Cipher oluştur
//   auto aes = crypto::aesCbc(key, iv);
//   auto enc = aes->encrypt(data);
//
//   // Hash
//   auto hash = crypto::sha256(data, len);
//
//   // HMAC
//   auto mac = crypto::hmacSha256(key, data, len);
//
//   // CMAC
//   auto cmac = crypto::cmacAes128(key, data, len);
//
//   // Random
//   auto nonce = crypto::randomBytes(16);
//
//   // Raw block cipher (no padding — DESFire auth)
//   auto enc = crypto::block::encryptAesCbc(key, iv, data, len);
//
//   // Key derivation
//   auto derived = crypto::pbkdf2Sha256("password", salt, 100000, 32);
//
// ════════════════════════════════════════════════════════════════════════════════

namespace crypto {

// ── Random ──────────────────────────────────────────────────────────────────
BYTEV randomBytes(size_t n);

// ── Hash ────────────────────────────────────────────────────────────────────
BYTEV sha256(const BYTE* data, size_t len);
BYTEV sha256(const BYTEV& data);

BYTEV sha384(const BYTE* data, size_t len);
BYTEV sha384(const BYTEV& data);

BYTEV sha512(const BYTE* data, size_t len);
BYTEV sha512(const BYTEV& data);

// ── HMAC ────────────────────────────────────────────────────────────────────
BYTEV hmacSha256(const BYTEV& key, const BYTE* data, size_t len);
BYTEV hmacSha256(const BYTEV& key, const BYTEV& data);

BYTEV hmacSha384(const BYTEV& key, const BYTE* data, size_t len);
BYTEV hmacSha384(const BYTEV& key, const BYTEV& data);

BYTEV hmacSha512(const BYTEV& key, const BYTE* data, size_t len);
BYTEV hmacSha512(const BYTEV& key, const BYTEV& data);

// ── CMAC (OMAC1) ────────────────────────────────────────────────────────────
BYTEV cmacAes128(const BYTEV& key, const BYTE* data, size_t len);
BYTEV cmacAes128(const BYTEV& key, const BYTEV& data);

// ── Key Derivation ──────────────────────────────────────────────────────────
BYTEV pbkdf2Sha256(const std::string& password, const BYTEV& salt,
                   int iterations, size_t keyLen);

BYTEV hkdf(const BYTEV& ikm, const BYTEV& salt, const BYTEV& info,
           size_t outLen);

// ── Block Cipher (raw CBC, no padding) — DESFire auth/secure messaging ─────
//    Input uzunluğu blok boyutunun katı olmalı.
//    IV her çağrıda bağımsız kopyalanır.
namespace block {
    // AES-128 CBC
    BYTEV encryptAesCbc(const BYTEV& key, const BYTEV& iv,
                        const BYTE* data, size_t len);
    BYTEV decryptAesCbc(const BYTEV& key, const BYTEV& iv,
                        const BYTE* data, size_t len);
    BYTEV encryptAesCbc(const BYTEV& key, const BYTEV& iv, const BYTEV& data);
    BYTEV decryptAesCbc(const BYTEV& key, const BYTEV& iv, const BYTEV& data);

    // 2K3DES CBC (16-byte key → expanded to 24)
    BYTEV encrypt2K3DesCbc(const BYTEV& key, const BYTEV& iv,
                           const BYTE* data, size_t len);
    BYTEV decrypt2K3DesCbc(const BYTEV& key, const BYTEV& iv,
                           const BYTE* data, size_t len);
    BYTEV encrypt2K3DesCbc(const BYTEV& key, const BYTEV& iv, const BYTEV& data);
    BYTEV decrypt2K3DesCbc(const BYTEV& key, const BYTEV& iv, const BYTEV& data);

    // 3K3DES CBC (24-byte key)
    BYTEV encrypt3K3DesCbc(const BYTEV& key, const BYTEV& iv,
                           const BYTE* data, size_t len);
    BYTEV decrypt3K3DesCbc(const BYTEV& key, const BYTEV& iv,
                           const BYTE* data, size_t len);
    BYTEV encrypt3K3DesCbc(const BYTEV& key, const BYTEV& iv, const BYTEV& data);
    BYTEV decrypt3K3DesCbc(const BYTEV& key, const BYTEV& iv, const BYTEV& data);

    constexpr size_t AES_BLOCK = 16;
    constexpr size_t DES_BLOCK = 8;
} // namespace block

// ── Cipher Factory — ICipher/ICipherAAD nesneleri oluştur ──────────────────
//    Polimorfik kullanım için (DesfireAuth, Reader, Test vb.)

std::unique_ptr<ICipher> aesCbc(const BYTEV& key, const BYTEV& iv);
std::unique_ptr<ICipher> aesCtr(const BYTEV& key, const BYTEV& nonce);
std::unique_ptr<ICipher> tripleDesCbc(const BYTEV& key, const BYTEV& iv);
std::unique_ptr<ICipherAAD> aesGcm(const BYTEV& key);

} // namespace crypto

#endif // PCSC_CRYPTO_H
```

### 3.3 Kullanım Örnekleri

**Eski kod (Windows-only):**
```cpp
#include "CngBlockCipher.h"
#include <windows.h>
#include <bcrypt.h>

// DESFire auth
BYTEV enc = CngBlockCipher::encryptAES(key, iv, plain);
BYTEV mac = CngBlockCipher::cmacAES(key, data);

// Random nonce
BCryptGenRandom(nullptr, rndA.data(), 16, BCRYPT_USE_SYSTEM_PREFERRED_RNG);

// ICipher kullanımı
CngAES aes(key, iv);
auto encrypted = aes.encrypt(data);
```

**Yeni kod (cross-platform):**
```cpp
#include "Crypto.h"

// DESFire auth
BYTEV enc = crypto::block::encryptAesCbc(key, iv, plain);
BYTEV mac = crypto::cmacAes128(key, data);

// Random nonce
BYTEV rndA = crypto::randomBytes(16);

// ICipher kullanımı
auto aes = crypto::aesCbc(key, iv);
auto encrypted = aes->encrypt(data);

// Yeni özellikler
auto hash = crypto::sha256(data);
auto hmac = crypto::hmacSha256(key, data);
auto derived = crypto::pbkdf2Sha256("password", salt, 100000, 32);
```

---

## 4. Eklenen Yeni Kriptografik Yöntemler

### 4.1 Hash Fonksiyonları
| Algoritma | Çıktı | Kullanım Alanı |
|-----------|-------|----------------|
| SHA-256 | 32 byte | Genel amaçlı hash, dijital imza, integrity |
| SHA-384 | 48 byte | TLS sertifika, yüksek güvenlik |
| SHA-512 | 64 byte | Yüksek güvenlik, büyük veri hash |

### 4.2 HMAC (Keyed Hash)
| Algoritma | Çıktı | Kullanım Alanı |
|-----------|-------|----------------|
| HMAC-SHA256 | 32 byte | Mesaj doğrulama, API imzalama |
| HMAC-SHA384 | 48 byte | TLS, yüksek güvenlik MAC |
| HMAC-SHA512 | 64 byte | Büyük veri bütünlük doğrulama |

### 4.3 AES-CTR (Counter Mode)
| Özellik | Detay |
|---------|-------|
| Padding | Gerekmez — stream cipher gibi çalışır |
| Paralellik | Encrypt/decrypt paralel yapılabilir |
| Kullanım | Streaming encryption, disk encryption |
| IV | Nonce + counter (16 byte) |

### 4.4 CMAC (Cipher-based MAC)
| Özellik | Detay |
|---------|-------|
| Algoritma | AES-128 CMAC (OMAC1, RFC 4493) |
| Çıktı | 16 byte |
| Kullanım | DESFire secure messaging, NFC doğrulama |

Zaten mevcut (CngBlockCipher::cmacAES) — OpenSSL'e taşınacak.

### 4.5 Key Derivation
| Algoritma | Kullanım |
|-----------|----------|
| PBKDF2-SHA256 | Parola → anahtar türetme |
| HKDF (RFC 5869) | IKM → çoklu anahtar türetme (TLS 1.3 style) |

### 4.6 Kriptografik Random
| Fonksiyon | Backend |
|-----------|---------|
| `crypto::randomBytes(n)` | OpenSSL `RAND_bytes()` |

Mevcut `BCryptGenRandom` (Windows-only) yerine geçer.

---

## 5. Implementasyon Fazları

### Faz 0 — Temizlik (branch: `chore/cipher-cleanup`)

**Tahmini süre: kısa**

- [ ] `Cipher/Ciphers.h` (root, guard'sız duplicate) sil
- [ ] `Utils/Utils/PCSC_new.h` (boş) sil
- [ ] `Utils/Utils/Types.h` → `PcscUtils.h`'den BYTE/BYTEV tanımını kullan, `Types.h`'yi `PcscUtils.h` redirect'ine çevir ya da sil
- [ ] `ArrayUtils.h` → `Utils/Utils/` altına taşı
- [ ] Build doğrula (CMake + VS)

### Faz 1 — OpenSSL Entegrasyonu (branch: `feature/cipher-openssl-migration`)

**Tahmini süre: orta**

- [ ] `Cipher/CMakeLists.txt`'ye OpenSSL bağımlılığı ekle
- [ ] `Random.h/.cpp` — `crypto::randomBytes()` implementasyonu
- [ ] `BlockCipher.h/.cpp` — `CngBlockCipher` → OpenSSL EVP ile yeniden yaz
  - `encryptAES`, `decryptAES` → `EVP_aes_128_cbc()` + `set_padding(0)`
  - `encrypt2K3DES`, `decrypt2K3DES` → `EVP_des_ede3_cbc()` + key expansion
  - `encrypt3K3DES`, `decrypt3K3DES` → `EVP_des_ede3_cbc()`
  - `cmacAES` → `EVP_MAC` (OpenSSL 3.x) veya `CMAC_CTX` (1.1.x)
- [ ] `AesCbcCipher.h/.cpp` — `CngAES` → OpenSSL
- [ ] `TripleDesCbcCipher.h/.cpp` — `Cng3DES` → OpenSSL
- [ ] `AesGcmCipher.h/.cpp` — `CngAESGcm` → OpenSSL
- [ ] `AesGcmUtil.h` — `CngAESGcmUtil.h` rename (içerik değişmez)
- [ ] Eski `Cng*.h/.cpp` dosyalarını sil
- [ ] `Ciphers.h` güncelle (yeni dosya isimleri)
- [ ] Build doğrula (CMake + VS)

### Faz 2 — Yeni Crypto Primitifler (branch: aynı veya `feature/cipher-new-primitives`)

**Tahmini süre: orta**

- [ ] `Hash.h/.cpp` — SHA-256, SHA-384, SHA-512
- [ ] `Hmac.h/.cpp` — HMAC-SHA256, HMAC-SHA384, HMAC-SHA512
- [ ] `AesCtrCipher.h/.cpp` — AES-CTR (ICipher implementasyonu)
- [ ] `Kdf.h/.cpp` — PBKDF2-SHA256, HKDF
- [ ] Build doğrula

### Faz 3 — Crypto Facade (branch: aynı)

**Tahmini süre: kısa**

- [ ] `Crypto.h/.cpp` — tüm `crypto::` namespace fonksiyonlarını tek header'da topla
- [ ] Factory fonksiyonları: `crypto::aesCbc()`, `crypto::aesGcm()`, `crypto::tripleDesCbc()`, `crypto::aesCtr()`
- [ ] Build doğrula

### Faz 4 — Tüketici Güncelleme (branch: aynı)

**Tahmini süre: orta**

- [ ] `Card/Card/CardProtocol/DesfireCrypto.cpp`:
  - `#include "CngBlockCipher.h"` → `#include "Crypto.h"`
  - `CngBlockCipher::encryptAES(...)` → `crypto::block::encryptAesCbc(...)`
  - `CngBlockCipher::decryptAES(...)` → `crypto::block::decryptAesCbc(...)`
  - `CngBlockCipher::encrypt2K3DES(...)` → `crypto::block::encrypt2K3DesCbc(...)`
  - `CngBlockCipher::decrypt2K3DES(...)` → `crypto::block::decrypt2K3DesCbc(...)`
  - `CngBlockCipher::encrypt3K3DES(...)` → `crypto::block::encrypt3K3DesCbc(...)`
  - `CngBlockCipher::decrypt3K3DES(...)` → `crypto::block::decrypt3K3DesCbc(...)`
  - `BCryptGenRandom(...)` → `crypto::randomBytes(...)`
  - `#include <windows.h>` + `<bcrypt.h>` kaldır
- [ ] `Card/Card/CardIO.cpp` — etkileniyorsa güncelle
- [ ] `Tests/cipher_test.cpp`:
  - `#ifdef _WIN32` guard'ını kaldır
  - `CngAESGcm(key)` → `crypto::aesGcm(key)`
  - `CngAES(key, iv)` → `crypto::aesCbc(key, iv)`
  - `Cng3DES(key3, iv3)` → `crypto::tripleDesCbc(key3, iv3)`
- [ ] `Tests/CardSystemTests.cpp`:
  - `CngBlockCipher::encrypt3K3DES(...)` → `crypto::block::encrypt3K3DesCbc(...)`
- [ ] Build doğrula (CMake + VS)
- [ ] Tüm testleri çalıştır

### Faz 5 — Backward Compatibility Alias (isteğe bağlı)

Eğer geçiş döneminde eski isimler gerekirse:

```cpp
// Compat.h — geçici, sonra silinecek
using CngBlockCipher = crypto::BlockCipherCompat;
using CngAES = AesCbcCipher;
using Cng3DES = TripleDesCbcCipher;
using CngAESGcm = AesGcmCipher;
```

Bu faz isteğe bağlı — eğer tüketici güncellemeleri tek seferde yapılırsa gerek yok.

---

## 6. CMake Değişiklikleri

### 6.1 `Cipher/CMakeLists.txt` (Yeni Hali)

```cmake
cmake_minimum_required(VERSION 3.14)
project(cipher LANGUAGES CXX)

# ── OpenSSL ──────────────────────────────────────────────────────────────────
find_package(OpenSSL REQUIRED)

# ── Platform-bağımsız kaynaklar ─────────────────────────────────────────────
set(CIPHER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Cipher/XorCipher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Cipher/CaesarCipher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Cipher/AffineCipher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Cipher/CipherAadFallback.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Cipher/Random.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Cipher/BlockCipher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Cipher/AesCbcCipher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Cipher/AesCtrCipher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Cipher/TripleDesCbcCipher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Cipher/AesGcmCipher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Cipher/Hash.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Cipher/Hmac.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Cipher/Kdf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Cipher/Crypto.cpp
)

add_library(cipher STATIC ${CIPHER_SOURCES})

target_include_directories(cipher PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Cipher
)

# OpenSSL — tek bağımlılık, her platformda
target_link_libraries(cipher PUBLIC OpenSSL::Crypto)

# Windows bcrypt artık GEREKMEZ
```

### 6.2 copilot-instructions.md Güncelleme

Kütüphaneler tablosuna OpenSSL eklenmeli:

```
| OpenSSL (libcrypto) | Tüm platformlar | AES, 3DES, GCM, CMAC, SHA, HMAC, KDF, Random |
```

`bcrypt (CNG)` satırı kaldırılmalı.

---

## 7. OpenSSL API Notları (Implementasyon Referansı)

### Raw CBC (no padding):
```cpp
EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr, key, iv);
EVP_CIPHER_CTX_set_padding(ctx, 0);  // NO PADDING
EVP_EncryptUpdate(ctx, out, &outLen, data, len);
EVP_EncryptFinal_ex(ctx, out + outLen, &finalLen);
EVP_CIPHER_CTX_free(ctx);
```

### 2K3DES (16→24 key expansion):
```cpp
// Aynı CNG mantığı: key16 → key24 = key16 + key16[0..7]
BYTEV key24 = key16;
key24.insert(key24.end(), key16.begin(), key16.begin() + 8);
EVP_EncryptInit_ex(ctx, EVP_des_ede3_cbc(), nullptr, key24.data(), iv);
EVP_CIPHER_CTX_set_padding(ctx, 0);
```

### AES-GCM:
```cpp
EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, nullptr, nullptr);
EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
EVP_EncryptInit_ex(ctx, nullptr, nullptr, key, nonce);
EVP_EncryptUpdate(ctx, nullptr, &len, aad, aad_len);  // AAD
EVP_EncryptUpdate(ctx, ct, &len, pt, pt_len);
EVP_EncryptFinal_ex(ctx, ct + len, &len);
EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag);
```

### CMAC (OpenSSL 3.x):
```cpp
EVP_MAC* mac = EVP_MAC_fetch(nullptr, "CMAC", nullptr);
EVP_MAC_CTX* ctx = EVP_MAC_CTX_new(mac);
OSSL_PARAM params[] = { OSSL_PARAM_construct_utf8_string("cipher", "AES-128-CBC", 0), OSSL_PARAM_END };
EVP_MAC_init(ctx, key, keyLen, params);
EVP_MAC_update(ctx, data, len);
EVP_MAC_final(ctx, out, &outLen, 16);
```

### Random:
```cpp
#include <openssl/rand.h>
RAND_bytes(buf, n);
```

### SHA-256:
```cpp
EVP_MD_CTX* ctx = EVP_MD_CTX_new();
EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
EVP_DigestUpdate(ctx, data, len);
EVP_DigestFinal_ex(ctx, out, &outLen);
EVP_MD_CTX_free(ctx);
```

---

## 8. Rename Tablosu (Eski → Yeni)

| Eski Sınıf/Dosya | Yeni Sınıf/Dosya |
|-------------------|-------------------|
| `CngBlockCipher` | `BlockCipher` (veya `crypto::block::` namespace) |
| `CngBlockCipher::encryptAES()` | `crypto::block::encryptAesCbc()` |
| `CngBlockCipher::decryptAES()` | `crypto::block::decryptAesCbc()` |
| `CngBlockCipher::encrypt2K3DES()` | `crypto::block::encrypt2K3DesCbc()` |
| `CngBlockCipher::decrypt2K3DES()` | `crypto::block::decrypt2K3DesCbc()` |
| `CngBlockCipher::encrypt3K3DES()` | `crypto::block::encrypt3K3DesCbc()` |
| `CngBlockCipher::decrypt3K3DES()` | `crypto::block::decrypt3K3DesCbc()` |
| `CngBlockCipher::cmacAES()` | `crypto::cmacAes128()` |
| `CngAES` | `AesCbcCipher` (+ `crypto::aesCbc()`) |
| `Cng3DES` | `TripleDesCbcCipher` (+ `crypto::tripleDesCbc()`) |
| `CngAESGcm` | `AesGcmCipher` (+ `crypto::aesGcm()`) |
| `CngAESGcmUtil.h` | `AesGcmUtil.h` |
| `BCryptGenRandom()` | `crypto::randomBytes()` |

---

## 9. Risk ve Dikkat Noktaları

| Risk | Mitigation |
|------|------------|
| OpenSSL 1.1.x vs 3.x API farkları | EVP API'si ikisinde de çalışır. CMAC için conditional compile gerekebilir. |
| vcpkg OpenSSL versiyonu uyumsuzluğu | `find_package(OpenSSL 1.1 REQUIRED)` ile minimum versiyon belirle |
| Test coverage | Mevcut cipher_test.cpp + CardSystemTests.cpp testleri yeni API'ye uyarlanacak |
| DESFire auth round-trip | CngBlockCipher ile bit-level aynı sonuç üretilmeli (AES/3DES standardı) |
| Link order | `cipher` → `OpenSSL::Crypto`, `card` → `cipher` (transitive) |
| Windows static linking | vcpkg `openssl:x64-windows-static` triplet kullanılabilir |

---

## 10. Başarı Kriterleri

- [ ] `cmake --build . --config Release` hem Windows hem Linux'ta başarılı
- [ ] `#include <windows.h>` ve `<bcrypt.h>` Cipher projesinde **sıfır** kez geçiyor
- [ ] `#include <windows.h>` ve `<bcrypt.h>` Card/DesfireCrypto.cpp'de **sıfır** kez geçiyor
- [ ] Mevcut tüm cipher testleri PASS
- [ ] Mevcut tüm DESFire auth testleri PASS (17/17)
- [ ] Yeni crypto primitif testleri (hash, hmac, kdf, random, aes-ctr) PASS
- [ ] `crypto::` namespace tek giriş noktası olarak çalışıyor
- [ ] copilot-instructions.md kütüphane tablosu güncel
