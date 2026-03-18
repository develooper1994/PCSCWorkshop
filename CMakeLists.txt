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
# CMake 4.0 ile birlikte gelen yeni policy yönetimi:
if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING
        "Allow legacy subprojects to configure under newer CMake" FORCE)
endif()

project(PCSCWorkshop
        DESCRIPTION "PC/SC Smart Card Workshop Tool"
        LANGUAGES CXX C
)

# ─────────────────────────────────────────────────────────────────────────────
# Modül yolları
# ─────────────────────────────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ─────────────────────────────────────────────────────────────────────────────
# Merkezi compiler / linker ayarları
# ─────────────────────────────────────────────────────────────────────────────
include(CompilerOptions)   # cmake/CompilerOptions.cmake

# ─────────────────────────────────────────────────────────────────────────────
# Derleme bilgisi header'ı üret  (build_info.h)
# ─────────────────────────────────────────────────────────────────────────────
include(BuildInfo)         # cmake/BuildInfo.cmake

# ─────────────────────────────────────────────────────────────────────────────
# Bağımlılıklar (linenoise vb.) — izole edilmiş modül
# ─────────────────────────────────────────────────────────────────────────────
include(Dependencies)      # cmake/Dependencies.cmake

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
