# MIFARE DESFire Integration Plan

Bu doküman, mevcut `Card` kütüphanesine `MIFARE DESFire` desteğini
Classic/Ultralight davranışını bozmadan eklemek için yol haritasını içerir.

> **Referans taslak kod:** `Card/Card/CardModel/DesfireMemoryLayout.h`
> Bu dosya şu an TASLAK/REFERANS niteliğindedir. Faz-1 entegrasyonunda
> ihtiyaca göre değişebilir.

---

## Hedef

- `CardType::MifareDesfire` desteğini eklemek.
- Dış API kullanımını mümkün olduğunca şeffaf tutmak.
- DESFire'a özgü **application/file** ve **3-pass mutual auth** modelini doğru uygulamak.
- Mevcut `Mifare Classic` ve `Ultralight` akışlarını kırmamak.

---

## DESFire Kart Varyantları ve Bellek Boyutları

DESFire kartları farklı nesil ve bellek kapasitelerinde üretilir:

| Varyant | Bellek Seçenekleri | Crypto | Not |
|---|---|---|---|
| DESFire EV1 | 2KB, 4KB, 8KB | DES, 2K3DES, 3K3DES, AES-128 | Yaygın |
| DESFire EV2 | 2KB, 4KB, 8KB, 16KB, 32KB | aynı + Transaction MAC | Secure Messaging v2 |
| DESFire EV3 | 2KB, 4KB, 8KB | aynı + SUN | NFC odaklı |
| DESFire Light | 640B | AES-128 / LRP | Basitleştirilmiş |

### Classic ile Karşılaştırma: Boyut Farkının Yapıya Etkisi

| | Classic 1K → 4K | DESFire 2K → 4K → 8K |
|---|---|---|
| **Yapısal fark** | Var: sektör 32-39 → 16 blok (extended) | Yok: aynı app/file hiyerarşisi |
| **Sonuç** | Ayrı `Card1KMemoryLayout` + `Card4KMemoryLayout` gerekli | Tek `DesfireMemoryLayout` yeterli |
| **Boyut ne zaman bilinir?** | Kart tipi (ATR) ile | Runtime'da `GetVersion` komutuyla |
| **Ne değişir?** | Sektör yapısı + blok sayısı | Sadece toplam/kalan kapasite |

Dolayısıyla DESFire için `MifareDesfire2K`, `MifareDesfire4K` gibi ayrı enum/layout
gerekmez. Tek `CardType::MifareDesfire` + runtime variant/kapasite bilgisi yeterlidir.

### Kart Kimliği: `GetVersion` Yanıtı

`GetVersion` komutu 3 parça × 7 byte döndürür. Bundan çıkarılacak bilgiler:

```
Hardware:  vendorID, type, subType, majorVer, minorVer, storageSize, protocol
Software:  vendorID, type, subType, majorVer, minorVer, storageSize, protocol
UID + Batch + Production: 7-byte UID, 5-byte batch, weekYear
```

`storageSize` byte'ı bellek kapasitesini kodlar:

| storageSize | Kapasite |
|---|---|
| 0x16 | 2 KB (2048 byte) |
| 0x18 | 4 KB (4096 byte) |
| 0x1A | 8 KB (8192 byte) |
| 0x1C | 16 KB (16384 byte) |
| 0x1E | 32 KB (32768 byte) |

Bu bilgi, `DesfireMemoryLayout.totalMemory` alanına yazılır ve
`freeMemory` ise `GetFreeMemory` komutuyla güncellenir.

---

## TODO

## 1) Model Katmanı

- [ ] `DesfireVariant` enum ekle (EV1, EV2, EV3, Light)
- [ ] `DesfireVersionInfo` struct ekle (`GetVersion` yanıtını parse eder,
      `storageSize` → kapasite çevirisi yapar)
- [ ] `Desfire` için yeni model tipleri ekle:
  - [ ] `DesfireApplication` (AID, key settings, key count)
  - [ ] `DesfireFile` (fileNo, fileType, comm mode, access rights, size)
  - [ ] `DesfireKeySettings` / `DesfireAccessRights`
- [ ] `DesfireMemoryLayout` — tek struct, boyuttan bağımsız:
  - [ ] `totalMemory` / `freeMemory` runtime'da `GetVersion` + `GetFreeMemory` ile doldurulur.
  - [ ] Variant bilgisi (EV1/EV2/EV3/Light) tutulur.
  - [ ] Tüm boyut varyantları (2K–32K) aynı hiyerarşi ile çalışır.
- [ ] `CardMemoryLayout` union'ına **gömülemez** (`std::vector` → heap).
      `CardInterface` içinde ayrı `std::unique_ptr<DesfireMemoryLayout>` olarak yaşar.
- [ ] DESFire bellek modelini sector/block yerine application/file perspektifinde temsil et.

## 2) Topology / Addressing

- [ ] `CardLayoutTopology` içinde DESFire için özel adresleme ekle:
  - [ ] `(AID, fileNo, offset, length)`
- [ ] Classic/Ultralight `sector/block` modelinden ayrıştırılmış yöntemler ekle.
- [ ] DESFire’da `isTrailerBlock` gibi kavramların `false/not-applicable` davranışını netleştir.

## 3) Authentication (Kritik)

- [ ] `DesfireAuthenticationState` ekle.
- [ ] 3-pass mutual authentication akışını implemente et:
  - [ ] PICC level auth
  - [ ] Application level auth
- [ ] Session key türetme ve session timeout/caching ekle.
- [ ] Yetkisiz erişimlerde net hata modeli tanımla.

## 4) Key Yönetimi

- [ ] DESFire key tipi desteği ekle:
  - [ ] DES / 2K3DES / 3K3DES / AES
- [ ] Key version ve key set metadata desteği ekle.
- [ ] Mevcut `KeyManagement` genişletilecekse backward-compatible tasarla;
      gerekirse `DesfireKeyManagement` ayrı sınıf aç.

## 5) I/O Katmanı

- [ ] `DesfireIO` (veya `CardIO` içinde type-dispatch) ekle:
  - [ ] `SelectApplication`
  - [ ] `Authenticate` (3-pass)
  - [ ] `ReadData` / `WriteData`
  - [ ] `GetFileSettings`
- [ ] Faz-2/opsiyonel komutlar:
  - [ ] `CreateApplication`
  - [ ] `CreateFile`
  - [ ] `DeleteApplication/File`
  - [ ] `ChangeKey`

## 6) Tek API / Şeffaf Kullanım

- [ ] Dış API’da kart tipine göre ayrı metod zorunluluğunu minimize et.
- [ ] DESFire için “virtual block” yaklaşımını değerlendir:
  - [ ] varsayılan `(AID, fileNo)` context içinde offset bazlı okuma/yazma.
- [ ] Gelişmiş DESFire fonksiyonları için explicit API bırak (app/file yönetimi).

## 7) Test ve Doğrulama

- [ ] Unit testler:
  - [ ] auth state + session lifecycle
  - [ ] access denied / success senaryoları
  - [ ] key type uyumluluk kontrolleri
- [ ] Integration testler:
  - [ ] app seçmeden read/write fail
  - [ ] app seç + auth sonrası read/write success
- [ ] Regression:
  - [ ] Classic testleri yeşil kalmalı
  - [ ] Ultralight testleri yeşil kalmalı

---

## Fazlı Plan

## Faz 1 — Altyapı (Kırmadan Ekle)

- `CardType::MifareDesfire` branch’lerini `CardMemoryLayout`, `CardLayoutTopology`,
  `CardInterface`, `CardIO` içinde aç.
- Mevcut Classic/Ultralight akışları aynen korunur.

**Çıktı:** Derlenen, type-dispatch hazır iskelet.

## Faz 2 — Auth ve Güvenlik Çekirdeği

- 3-pass mutual auth akışı + session key üretimi.
- Auth state + timeout/caching.

**Çıktı:** Auth olmadan korumalı komutlar çalışmaz, auth sonrası çalışır.

## Faz 3 — Veri Erişim Modeli

- AID + fileNo bazlı read/write akışı.
- Gerekirse virtual block adaptasyonu.

**Çıktı:** Dış API’dan temel read/write akışı stabil.

## Faz 4 — Gelişmiş DESFire Özellikleri

- App/File yaratma/silme, key yönetimi komutları.
- Hata yönetimi, transaction/retry davranışı.

**Çıktı:** Operasyonel DESFire feature set.

## Faz 5 — Stabilizasyon

- Gerçek kart testleri.
- Regression + performans/güvenlik kontrolleri.
- Dokümantasyon ve kullanım örnekleri.

**Çıktı:** Üretime hazır DESFire desteği.

---

## Mimari Notlar

### Neden Tek DesfireMemoryLayout?

Classic'te 1K ve 4K ayri struct gerektirir cunku sektor yapisi degisir.
DESFire'da 2K/4K/8K/16K/32K arasinda yapisal fark yoktur:

    Classic:    1K -> 16 sektor x 4 blok     (sabit)
                4K -> 32 sektor x 4 + 8 x 16 (farkli yapi!)

    DESFire:    2K -> app/file hiyerarsi, kapasite = 2048   (ayni yapi)
                4K -> app/file hiyerarsi, kapasite = 4096   (ayni yapi)
                8K -> app/file hiyerarsi, kapasite = 8192   (ayni yapi)

Fark yalnizca totalMemory / freeMemory degeridir.
Runtime'da GetVersion ve GetFreeMemory komutlariyla kesfedilir. Ayri enum/layout gerekmez.

### Memory Layout Karsilastirma

| | Classic | Ultralight | DESFire |
|---|---|---|---|
| Layout tipi | Statik union | Statik union | Dinamik hiyerarsi |
| Bellek birimi | Block (16B) | Page (4B) -> virtual block | File -> virtual block |
| Adresleme | block index | page index | (AID, fileNo, offset, len) |
| Union'a gomulur mu? | Evet (fixed-size) | Evet (fixed-size) | Hayir (std::vector -> heap) |
| Yapi ne zaman bilinir? | Derleme zamani | Derleme zamani | Calisma zamani kesif |
| Boyut yapiyi etkiler mi? | Evet (1K!=4K) | Hayir | Hayir (2K=4K=8K yapi) |
| getBlock(n) | blocks[n] | blocks[n] | activeFile->data[n*16] |

### Genel Ilkeler

- DESFire modeli, Classic'ten yapisal olarak farklidir; sector/trailer kavrami yoktur.
- Bu nedenle ic mimaride DESFire icin ayri strategy/adapter katmani tercih edilmelidir.
- Disariya tek tip API hissi verilirken, iceride card-type dispatch zorunludur.
- Guvenlik akislari (mutual auth, session key, secure messaging) opsiyon degil, cekirdek gerekliliktir.
- Boyut varyantlari (2K-32K) tek layout ile karsilanir; kapasite runtime'da belirlenir.
