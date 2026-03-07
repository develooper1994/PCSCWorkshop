# Copilot Instructions

## Dil & Standart
- C++17 kullan (`/std:c++17`). Zorunlu olmadıkça C++20/23/26'e geçme.
- Mevcut projede C++14 seviyesinde yazılmış kod var; yeni kod C++17 olabilir ama mevcut koda zorunda olmadıkça standartdı yükseltmeden C++20/23/26 özelliği ekleme.

## Derleyici & Platform
- **Windows:** MSVC (Visual Studio 2022, v17.x) — birincil geliştirme ortamı.
- **Linux:** GCC 11+ / Clang 14+ — derlenmeli, pcsclite bağımlılığı.
- Cross-platform: `Platform.h` üzerinden `#ifdef _WIN32` guard'ları kullan. Doğrudan `<windows.h>` dahil etme.

## Build Sistemi
- **Visual Studio** `.sln` / `.vcxproj` ve **CMake** yan yana desteklenmeli.
- C++17 standardı `Directory.Build.props` (VS) ve root `CMakeLists.txt` (CMake) ile merkezi ayarlanır.
- Yeni `.cpp` dosyası eklendiğinde hem `.vcxproj`'a hem ilgili `CMakeLists.txt`'ye ekle.
- Yeni `.h` dosyası eklendiğinde `.vcxproj`'a ekle (CMake GLOB ile otomatik bulur).
- `CMakeLists.txt` dosyalarında gereksiz `set(CMAKE_CXX_STANDARD)` tekrar etme — root'ta tanımlı.

## Kütüphaneler & Bağımlılıklar
Projede kullanılan kütüphaneler:
| Kütüphane | Platform | Kullanım |
|-----------|----------|----------|
| winscard (PC/SC) | Windows: `winscard.lib` / Linux: `libpcsclite` | Akıllı kart iletişimi |
| OpenSSL (libcrypto) | Tüm platformlar (vcpkg / apt / brew) | AES, 3DES, GCM, CMAC, SHA, HMAC, KDF, Random |

> ✅ **CNG → OpenSSL migration tamamlandı.**
> Detaylar: `Cipher/CRYPTO_MIGRATION_PLAN.md`

- Yeni kütüphane eklenirse bu listeyi güncelle.
- Kütüphane kaldırılırsa listeden çıkar ve ilgili CMake/vcxproj bağımlılığını temizle.

## Proje Mimarisi
```
Utils (static lib)     — Platform.h, PCSC, PcscUtils, Log, Result, StatusWord
Cipher (static lib)    — crypto:: facade, AesCbcCipher, AesCtrCipher, AesGcmCipher,
                         TripleDesCbcCipher, BlockCipher, Hash, Hmac, Kdf, Random
                         XorCipher, CaesarCipher, AffineCipher (OpenSSL backend)
Reader (static lib)    — Reader (base), ACR1281UReader, PcscCommands
Card (static lib)      — CardIO, CardInterface, DesfireCommands, DesfireAuth
Workshop1 (exe)        — Ana uygulama
Tests (exe)            — Test runner
```
Bağımlılık yönü: `Utils ← Cipher ← Reader ← Card ← Workshop1/Tests`

## Kod Kuralları
- **`std::function` KULLANMA.** Template, function pointer veya lambda kullan.
- Hata yönetimi: exception-free yollar için `Result<T>` pattern'ini tercih et; throwing versiyonlar `tryXxx().unwrap()` ile implemente edilir.
- PCSC sınıfı tek giriş noktası — eski `CardConnection` sınıfı kaldırıldı; doğrudan `PCSC&` referansı kullan.
- Reader alt sınıfları (ACR1281UReader vb.) constructor'da `PCSC&` alır.
- Pointer/referans: `BYTE* data`, `const PCSC& pcsc` (sol hizalı).
- Header guard: `#ifndef PCSC_XXX_H` / `#define` / `#endif` (pragma once değil).

## Format & Stil
- `.clang-format` dosyası var — `BasedOnStyle: Microsoft`, `BreakBeforeBraces: Allman`.
- `.editorconfig` dosyası var — tab indent, UTF-8, LF.
- Gereksiz yorum ekleme. Mevcut dosyadaki yorum stilini takip et.
- Açıklama: istemedikçe uzun açıklama yapma, doğrudan kodu yaz.

## Copilot Davranışı
- Hafızayı efektif kullan — tekrarlayan bilgiyi sormak yerine instructions'dan oku.
- Gereksiz açıklama yapma. Kod değişikliği istendiyse doğrudan değiştir.
- Değişiklik yapınca `run_build` ile doğrula.
- CMake build'i de etkileniyorsa her iki sistemi de güncelle.
- Düzenli ve temiz çalış: yarım iş bırakma, bir değişikliğin tüm etkilerini (include, link, test) takip et.
- **İndeks kullan:** `.project-index.md` ilk kontrol noktası — proje mimarisi, dependencies, key files hızlı referans. Her major değişiklikten sonra ve taramalarında uyumsuzluk olursa güncelle.

## Git & Repo Yönetimi

### Komit Stratejisi
- **Stabil değişiklikler:** Ana branch'e doğrudan push — küçük bug fix'ler, dokumentasyon, linter ayarları.
- **Feature/Deneysel:** Her zaman feature branch'te — yeni Reader, yeni Cipher, mimari değişiklikler, CNG alternatifi (Linux).
- **Commit öncesi:** Yapılmamış değişiklikler var mı kontrol et (`git status`). Varsa:
  - Değişiklik istenmişse: `git add` + `git commit -m "..."`
  - İstenmediyse: `git stash` veya `git restore`

### Feature Branch Akışı
```
1. git status                           # Yapılmamış değişiklik var mı?
2. git checkout -b feature/xxx-desc     # Yeni branch aç
3. Geliştir → test → build doğrula
4. git add .  &&  git commit -m "feat: xxx"
5. git push origin feature/xxx-desc
6. (kod review gerekirse: PR açıl)
7. git checkout main && git pull
8. git merge feature/xxx-desc           # Fast-forward veya squash
9. git push origin main
10. git branch -d feature/xxx-desc      # Local sil
11. git push origin --delete feature/xxx-desc  # Remote sil
```

### Özet Commit
- İş bittikten sonra (birden fazla commit varsa) özet commit yap:
  ```
  git commit --allow-empty -m "chore: cleanup — finalize feature X"
  ```
- Tek satırdan uzunsa: başlık + boş satır + detay.
- Geçmiş commitler doğruysa force-push yapmadan bırak.

### Branch İsimlendirmesi
- `feature/reader-acr1281u-extended` — yeni Reader
- `fix/pcsc-linux-compat` — bug fix
- `refactor/cardio-architecture` — mimari değişiklik
- `docs/build-setup` — dokümentasyon
- `chore/deps-update` — bağımlılık güncellemesi
