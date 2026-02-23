Saved deferred tasks

I saved the remaining (not-chosen) cleanup/optimization items here so you can continue later.

Applied already (per your choice):
1) Proje include/path ve filtre temizliði — in-progress: I updated many include paths after moving tests; verify project filters and .vcxproj entries if you want different layout.
3) ICipher API iyileþtirmeleri — changed `encryptInto`/`decryptInto` to return `size_t` and updated implementing classes (Affine, Caesar, etc.).
4) Hata/exception stratejisi — noted; I made small consistency fixes in some places, but a full strategy choice remains.
5) Test projesini CI / otomatik çalýþtýrma — moved tests into `Tests/` project; helper headers consolidated under `Tests/`.

Deferred items (saved here):
6) Küçük performans/özgünlük temizlemeleri — reduce copies, add move semantics where useful.
7) CipherUtil/egcd tipi ve constexpr düzenlemeleri — make egcd iterative/constexpr and type-safe unsigned arithmetic.
8) AES/3DES kaynak yönetimi ve güvenlik kontrolleri — IV/key validation, resource lifetime checks.
9) Kod stili/dokümantasyon — apply `#pragma once`, license headers, API docs.
10) Fazla/boþ dosyalarýn temizlenmesi — remove unused .cpp/.h files.
11) Paketleme / NuGet veya benzeri — package libraries.
12) Güvenlik incelemesi ve test senaryolarý geniþletme — AES-GCM AAD/fuzz tests, edge cases.

Notes:
- I placed this file at the repository root: `DEFERRED_TASKS.md`.
- Current workspace has had many header/function signatures changed (ICipher). I recommend running a full build in your IDE. I ran builds while making changes; some edits required iterative fixes. If you want, I can now continue to step 1 (finalize include/path cleanup) or stop here until you give further instructions.

What should I do next? (short answer: e.g. "devam et 1" or "bekle")
