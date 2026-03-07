# MIFARE DESFire Integration Plan

Bu doküman, mevcut `Card` kütüphanesine `MIFARE DESFire` desteğini
Classic/Ultralight davranışını bozmadan eklemek için yol haritasını içerir.

> **⚠️ Durumu:** Faz 1-5 yazılım implentasyonu **TAMAMLANDI** (15/15 test PASS).
> Donanım doğrulaması (gerçek DESFire kartı) **FUTURE WORK** — kart henüz mevcut değil.
> 
> Detaylar için → `Card/FUTURE_WORK.md`

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

## 1) Model Katmanı ✅

- [x] DesfireVariant enum (EV1, EV2, EV3, Light)
- [x] DesfireVersionInfo struct (GetVersion parse, storageSize → kapasite)
- [x] DesfireApplication (AID, key settings, key count)
- [x] DesfireFile (fileNo, fileType, comm mode, access rights, size)
- [x] DesfireKeySettings / DesfireAccessRights
- [x] DesfireMemoryLayout — tek struct, boyuttan bağımsız
- [x] CardInterface içinde unique_ptr<DesfireMemoryLayout>
- [x] Bellek modeli app/file perspektifinde

## 2) Topology / Addressing ✅

- [x] CardLayoutTopology DESFire branch'leri (0 sector, 0 block)
- [x] isDesfire(), hasSectors() helper method'ları
- [x] isTrailerBlock = false, isManufacturerBlock düzeltildi

## 3) Authentication ✅

- [x] DesfireSession — session state (key, IV, counter, timeout)
- [x] DesfireAuth — 3-pass mutual auth state machine (TransmitFn callback)
- [x] DesfireCrypto — nonce, rotate, session key derivation (AES + 2K3DES)
- [x] CngBlockCipher — raw AES/3DES CBC (no padding) + AES-CMAC
- [x] Hata modeli: logic_error (yanlış kart tipi), runtime_error (auth fail)
- [x] Session timeout/caching (chrono tabanlı, setTimeoutMs)

## 4) Key Yönetimi ✅ (temel)

- [x] DesfireKeyType enum (DES, 2K3DES, 3K3DES, AES-128)
- [x] KeySettings bitfield (allowChangeMasterKey, freeDirectoryList, vb.)
- [x] Key version metadata (DesfireKeyConfig.keyVersion)
- [x] 3K3DES: CngBlockCipher encrypt/decrypt, auth, session key derivation
- [x] ChangeKey APDU (Faz 4)

## 5) I/O Katmanı ✅ (temel)

- [x] DesfireCommands — APDU construction + response parsing
- [x] Multi-frame receive (transceive + additionalFrame)
- [x] CardIO DESFire dispatch: selectApplication, authenticateDesfire
- [x] CardIO: readFileData, writeFileData, getFileSettings, getFileIDs
- [x] CardIO: discoverCard (GetVersion), getApplicationIDs, getFreeMemory
- [x] Classic API guard: trailer/access ops throw logic_error
- [x] Faz-4: CreateApplication, CreateFile, DeleteApp/File, ChangeKey, Secure Messaging

## 6) Tek API / Şeffaf Kullanım ✅ (temel)

- [x] CardIO facade: SelectApp → Auth → Read/Write
- [x] Virtual block: DesfireMemoryLayout::readVirtualBlock/writeVirtualBlock
- [x] Gelişmiş DESFire API: CreateApp/File, DeleteApp/File, Transaction, FormatPICC

## 7) Test ve Doğrulama ✅

- [x] DesfireMemoryLayout model testi (10 section, DF_CHECK makro)
- [x] DesfireAuth simulated 3-pass (9 section, DA_CHECK makro)
- [x] CngBlockCipher AES/3DES round-trip + CMAC smoke
- [x] Session key derivation (AES + 2K3DES + 3K3DES)
- [x] APDU construction + response parsing
- [x] Management APDU + Secure Messaging (11 section)
- [x] Integration test: session timeout, full auth lifecycle, command coverage, regression
- [x] 3K3DES: cipher round-trip, simulated 3-pass auth, session key derivation
- [x] Regression: **16/16 PASS** (10 Classic/UL + 6 DESFire)

---

## Fazlı Plan

## Faz 1 — Altyapı (Kırmadan Ekle) ✅ TAMAMLANDI

Commit: `feat: DESFire Faz 1`

- CardType::MifareDesfire branch'leri: CardTopology, CardMemoryLayout, CardInterface, CardIO
- 11/11 test geçti (10 mevcut + 1 yeni DESFire Memory Layout)

## Faz 2 — Auth ve Güvenlik Çekirdeği ✅ TAMAMLANDI

Commit: `feat: DESFire Faz 2`

- CngBlockCipher (Cipher projesi): Raw AES/3DES CBC (no padding) + AES-CMAC
- DesfireCrypto (Card projesi): nonce, rotate, session key derivation
- DesfireAuth (Card projesi): 3-pass mutual auth state machine
- DesfireSession (Card projesi): session state (key, IV, counter)
- 14/14 test geçti (simulated 3-pass auth dahil)

## Faz 3 — Veri Erişim Modeli ✅ TAMAMLANDI

Commit: `feat: DESFire Faz 3`

- DesfireCommands (Card projesi): APDU construction + response parsing + multi-frame
- CardIO DESFire API: selectApplication, authenticateDesfire, readFileData, writeFileData
- CardIO: discoverCard, getApplicationIDs, getFileIDs, getFileSettings, getFreeMemory
- 14/14 test geçti

## Faz 4 — Gelişmiş DESFire Özellikleri ✅ TAMAMLANDI

Commit: `feat: DESFire Faz 4`

- [x] CreateApplication / DeleteApplication — APDU + CardIO
- [x] CreateStdDataFile / CreateBackupDataFile / CreateValueFile / CreateLinearRecordFile / CreateCyclicRecordFile
- [x] DeleteFile
- [x] ChangeKey APDU (cryptogram hazırlığı DesfireCrypto'da)
- [x] GetKeySettings / ChangeKeySettings / GetKeyVersion
- [x] Secure Messaging: DesfireSecureMessaging (CMAC calc, truncate, wrap/unwrap)
- [x] Transaction: Credit / Debit / CommitTransaction / AbortTransaction
- [x] FormatPICC
- [x] 14/14 test geçti (Management APDU + Secure Messaging dahil)

## Faz 5 — Stabilizasyon ✅ TAMAMLANDI

Commit: `feat: DESFire Faz 5`

- [x] Session timeout/caching (`<chrono>` tabanlı, `setTimeoutMs()`, `isValid()/isExpired()`)
- [x] Integration test: simulated full lifecycle (auth→timeout→expire, AccessRights round-trip,
      full command INS coverage, Secure Messaging end-to-end, Classic regression)
- [x] Regression: **16/16 PASS** (10 Classic/UL + 6 DESFire)
- [x] Dokümantasyon
- [ ] Gerçek DESFire EV1/EV2 kart ile donanım doğrulaması (kart hazır olduğunda)

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
