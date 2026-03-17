# ══════════════════════════════════════════════════════════════════════════════
# PCSCWorkshop — Root CMakeLists.txt
#
# Bu dosya tüm alt projeleri tek bir CMake build'inde birleştirir.
#
# ── Proje Yapısı ─────────────────────────────────────────────────────────────
#
#   Utils/      → Ortak yardımcı yapılar (PCSC.h, Result.h, BasicError.h)
#                 Diğer tüm projeler Utils'e bağımlıdır.
#
#   Cipher/     → Kriptografi katmanı (AES-CBC/CTR/GCM, 3DES-CBC, XOR, PBKDF2)
#                 Bağımsız kütüphane; scardtool şifreleme için kullanır.
#
#   Reader/     → PC/SC reader soyutlama katmanı
#                 Windows WinSCard.dll ve Linux libpcsclite üzerinde çalışır.
#
#   Card/       → Kart tipi soyutlamaları (Mifare Classic, Ultralight, DESFire)
#                 Gelecekte cardsim (sanal kart simülasyonu) buraya eklenecek.
#
#   McpServer/  → MCP (Model Context Protocol) server referans implementasyonu
#                 AI asistanların PC/SC araçlarını kullanmasını sağlar.
#
#   ScardTool/  → Ana hedef: CLI + MCP + Interactive + Library
#                 Detaylı derleme seçenekleri için ScardTool/CMakeLists.txt
#
# ── Legacy Projeler (derleme dışı) ───────────────────────────────────────────
#
#   Tests/      → Eski gerçek kart testleri (RealCardReaderTest.cpp)
#                 Gerçek PC/SC reader gerektirir; CI'da çalışmaz.
#                 Yeni birim testler ScardTool/Tests/'ta (GTest, dry-run).
#
#   Workshop1/  → Manuel deneme kodları; production'a dahil edilmez.
#
# ── Platform Notları ─────────────────────────────────────────────────────────
#
#   Windows: WinSCard.dll sistem kütüphanesi kullanılır (extra kurulum yok).
#            OpenSSL ile /MT vs /MD CRT çakışması var — preset kullanın.
#            Geliştirme: cmake --preset win-debug   (/MD, hızlı derleme)
#            Dağıtım:    cmake --preset win-release  (/MT + LTCG, static CRT)
#
#   Linux:   libpcsclite gereklidir.
#            Debian/Ubuntu: apt install libpcsclite-dev
#            Alpine:        apk add pcsc-lite-dev
#
#   Embedded: Cross-compile için CMAKE_TOOLCHAIN_FILE kullanın.
#            Ayrıntılar için ScardTool/BUILD.md dosyasına bakın.
#
# ── Hızlı Başlangıç ──────────────────────────────────────────────────────────
#
#   Linux varsayılan:
#     cmake --preset linux-debug
#     cmake --build --preset linux-debug --target scardtool
#
#   Windows geliştirme:
#     cmake --preset win-debug
#     cmake --build --preset win-debug --target scardtool
#
#   Windows dağıtım (/MT + LTCG):
#     cmake --preset win-release
#     cmake --build --preset win-release --target scardtool
#
#   Cross-compile ARM:
#     cmake -DCMAKE_TOOLCHAIN_FILE=arm-gnueabihf.cmake \
#           -DBUILD_STANDALONE=ON -DEMBED_LUA=ON \
#           -DSCARDTOOL_USE_LINENOISE=OFF \
#           -DCMAKE_BUILD_TYPE=MinSizeRel ..
#
# ══════════════════════════════════════════════════════════════════════════════

cmake_minimum_required(VERSION 3.16)
project(PCSCWorkshop VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# IDE için compile_commands.json üret (clangd, clang-tidy, VS Code vb.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── MSVC Runtime + LTCG Konfigürasyonu ───────────────────────────────────────
#
# Sorun: Windows'ta OpenSSL pre-built binary'leri /MT (static CRT) ile
#        derlenmiştir. MSVC varsayılan /MD (dynamic CRT) ile çakışır ve
#        "LNK2038: mismatch detected for 'RuntimeLibrary'" hatasına yol açar.
#
# Çözüm: İki preset grubu (CMakePresets.json):
#
#   win-debug / win-relwithdebinfo:
#     → /MD veya /MDd: dynamic CRT, küçük binary, hızlı derleme.
#     → Geliştirme ve test için uygundur.
#     → OpenSSL'in kendi /MD build'i gerekir (varsa).
#
#   win-release:
#     → /MT: static CRT, harici runtime bağımlılığı yok.
#     → LTCG (/GL + /LTCG): link-time optimization, ~%5-15 daha küçük/hızlı binary.
#     → OpenSSL /MT build ile uyumlu.
#     → Son kullanıcıya dağıtım için ideal: tek binary, VCRT kurulumu gerekmez.
#
# Preset olmadan manuel derleme yapılacaksa varsayılan /MD kullanılır.
# CMAKE_MSVC_RUNTIME_LIBRARY preset'ten gelir; override edilmemelidir.
#
if(MSVC)
    # CMP0091: CMAKE_MSVC_RUNTIME_LIBRARY ile CRT seçimini merkezi yönet.
    # Bu policy olmadan her target kendi CRT'sini seçer → uyumsuzluk.
    cmake_policy(SET CMP0091 NEW)

    # LTCG (Link Time Code Generation):
    #   /GL compile flag + /LTCG linker flag.
    #   Derleyici tüm translation unit'leri birlikte optimize eder.
    #   Sadece dağıtım build'inde kullanılır (derleme süresi artar).
    option(SCARDTOOL_ENABLE_LTCG
        "Link Time Code Generation etkinleştir (/GL + /LTCG). Sadece dağıtım için."
        OFF)

    if(SCARDTOOL_ENABLE_LTCG)
        set(CMAKE_CXX_FLAGS_RELEASE         "${CMAKE_CXX_FLAGS_RELEASE} /GL")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE  "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
        set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} /LTCG")
        message(STATUS "LTCG etkin: /GL + /LTCG")
    endif()

    if(NOT CMAKE_MSVC_RUNTIME_LIBRARY)
        # Preset kullanılmadan derleme: güvenli varsayılan /MD.
        # Dağıtım için: cmake --preset win-release
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
        message(STATUS "CMAKE_MSVC_RUNTIME_LIBRARY ayarlanmamış → /MD (geliştirme)")
        message(STATUS "Dağıtım için: cmake --preset win-release")
    else()
        message(STATUS "MSVC Runtime: ${CMAKE_MSVC_RUNTIME_LIBRARY}")
    endif()

    # Uyarı seviyesi ve encoding:
    #   /W3    → standart uyarılar
    #   /WX-   → uyarıları hata olarak işleme (third-party uyumluluğu)
    #   /utf-8 → kaynak kod UTF-8 (Türkçe karakterler için)
    add_compile_options(/W3 /WX- /utf-8)

    # Suppress edilmiş uyarılar:
    #   /wd4996 → deprecated POSIX fonksiyon isimleri (strdup, strcpy vb.)
    #   /wd4244 → dönüşüm uyarıları (size_t → int gibi)
    add_compile_options(/wd4996 /wd4244)
endif()

# ── Alt Projeler: Kütüphaneler ────────────────────────────────────────────────
#
# Bağımlılık zinciri:
#   Utils ← Cipher ← ScardTool
#   Utils ← Reader ← ScardTool
#   Utils ← Card   (ScardTool tarafından dolaylı kullanılır)
#

# Utils: temel tipler, PCSC soyutlaması, hata yönetimi
# Tüm projeler bu kütüphaneye bağımlıdır
add_subdirectory(Utils)

# Cipher: AES-CBC/CTR/GCM, 3DES-CBC, XOR, PBKDF2, HKDF
# Kart verisi şifreleme/çözme için kullanılır
add_subdirectory(Cipher)

# Reader: PC/SC reader soyutlama, ACR1281U desteği
# WinSCard.dll (Windows) / libpcsclite (Linux/macOS) üzerinde çalışır
add_subdirectory(Reader)

# Card: Kart tipi soyutlamaları (Mifare Classic/Ultralight, DESFire EV1/2/3)
# Gelecek plan: cardsim sanal kart simülasyonu buraya eklenecek
add_subdirectory(Card)

# ── Alt Projeler: Çalıştırılabilirler ────────────────────────────────────────

# McpServer: MCP (Model Context Protocol) referans server
# AI asistanların (Claude, GPT vb.) PC/SC kartlarla çalışmasını sağlar
add_subdirectory(McpServer)

# ScardTool: Ana hedef — CLI + MCP + Interactive + Library + Extension
# Derleme seçenekleri için: ScardTool/CMakeLists.txt
add_subdirectory(ScardTool)

# ── Legacy Projeler ───────────────────────────────────────────────────────────
#
# Aşağıdaki projeler aktif geliştirme dışındadır.
# Gereksiz derleme süresinden kaçınmak için varsayılan olarak derlenmez.
#
# Tests/ (eski gerçek kart testleri):
#   RealCardReaderTest.cpp gerçek PC/SC reader bağlı olmasını gerektirir.
#   CI/CD ortamında çalışmaz. Yeni birim testler ScardTool/Tests/'ta (GTest).
#   Aktifleştirmek için yorumu kaldır:
# add_subdirectory(Tests)
#
# Workshop1/ (manuel denemeler):
#   PC/SC reader ile interaktif test kodları. Production'a dahil değildir.
#   Aktifleştirmek için yorumu kaldır:
# add_subdirectory(Workshop1)
