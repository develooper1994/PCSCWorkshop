# MIFARE DESFire Integration Plan

Bu doküman, mevcut `Card` kütüphanesine `MIFARE DESFire` desteğini
Classic/Ultralight davranışını bozmadan eklemek için yol haritasını içerir.

---

## Hedef

- `CardType::MifareDesfire` desteğini eklemek.
- Dış API kullanımını mümkün olduğunca şeffaf tutmak.
- DESFire'a özgü **application/file** ve **3-pass mutual auth** modelini doğru uygulamak.
- Mevcut `Mifare Classic` ve `Ultralight` akışlarını kırmamak.

---

## TODO

## 1) Model Katmanı

- [ ] `Desfire` için yeni model tipleri ekle:
  - [ ] `DesfireApplication` (AID, key settings, key count)
  - [ ] `DesfireFile` (fileNo, fileType, comm mode, access rights, size)
  - [ ] `DesfireKeySettings` / `DesfireAccessRights`
- [ ] `CardMemoryLayout` içinde `CardType::MifareDesfire` dalı ekle.
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

- DESFire modeli, Classic’ten yapısal olarak farklıdır; sector/trailer kavramı yoktur.
- Bu nedenle iç mimaride DESFire için ayrı strategy/adapter katmanı tercih edilmelidir.
- Dışarıya tek tip API hissi verilirken, içeride card-type dispatch zorunludur.
- Güvenlik akışları (mutual auth, session key, secure messaging) opsiyon değil, çekirdek gerekliliktir.
