cmake_minimum_required(VERSION 3.14)
project(PCSCWorkshop VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# IDE için compile_commands.json üret (clang-tidy, LSP vb.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── MSVC Runtime + LTCG Konfigürasyonu ─────────────────────────────────────
# Sorun: OpenSSL Windows pre-built /MT (static CRT) ile derleniyor.
#        MSVC default /MD (dynamic CRT) ile çakışır → linker hatası.
#
# Çözüm iki grup preset:
#   win-debug / win-relwithdebinfo → /MD(d)  hızlı derleme, geliştirme
#   win-release                   → /MT    + LTCG, dağıtım binary'si
#
# CMAKE_MSVC_RUNTIME_LIBRARY preset'ten gelir (CMakePresets.json).
# SCARDTOOL_ENABLE_LTCG=ON ise /GL (LTCG) + /LTCG linker flag'i aktif olur.
# ──────────────────────────────────────────────────────────────────────────

if(MSVC)
    # Policy: CMAKE_MSVC_RUNTIME_LIBRARY ile CRT seçimini merkezi yap
    cmake_policy(SET CMP0091 NEW)

    option(SCARDTOOL_ENABLE_LTCG "Enable Link Time Code Generation (/GL + /LTCG)" OFF)

    if(SCARDTOOL_ENABLE_LTCG)
        # Tüm hedefler için /GL (whole program optimization)
        # ScardTool CMakeLists'te target_compile_options eklenecek
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
        set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} /LTCG")
        message(STATUS "LTCG enabled: /GL + /LTCG")
    endif()

    # OpenSSL ile CRT uyumsuzluğunu önle: tüm target'lar aynı CRT'yi kullanmalı.
    # CMAKE_MSVC_RUNTIME_LIBRARY preset'ten gelir. Değer:
    #   MultiThreadedDLL        → /MD   (Release, hızlı geliştirme)
    #   MultiThreadedDebugDLL   → /MDd  (Debug)
    #   MultiThreaded           → /MT   (Release, dağıtım — OpenSSL uyumlu)
    #   MultiThreadedDebug      → /MTd  (Debug static — OpenSSL debug uyumlu)
    if(NOT CMAKE_MSVC_RUNTIME_LIBRARY)
        # Preset'siz derleme için güvenli varsayılan: /MD
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
        message(STATUS "CMAKE_MSVC_RUNTIME_LIBRARY not set, defaulting to /MD (MultiThreadedDLL)")
        message(STATUS "For OpenSSL /MT builds use: cmake --preset win-release")
    else()
        message(STATUS "MSVC Runtime: ${CMAKE_MSVC_RUNTIME_LIBRARY}")
    endif()

    # Genel MSVC uyarı ve hata ayarları
    add_compile_options(/W3 /WX- /utf-8)
    # /wd4996: deprecated POSIX function names (strdup, etc.)
    # /wd4244: conversion warnings (size_t → int)
    add_compile_options(/wd4996 /wd4244)
endif()

# ── Static kütüphaneler ──────────────────────────────────────────────────────
add_subdirectory(Utils)
add_subdirectory(Cipher)
add_subdirectory(Reader)
add_subdirectory(Card)

# ── Çalıştırılabilir hedefler ────────────────────────────────────────────────
# NOT: Tests/ ve Workshop1/ eski legacy projelerdir.
#   Tests/    → RealCardReaderTest.cpp: gerçek PC/SC reader gerektirir
#   Workshop1/ → PC/SC reader ile manuel denemeler
# Bunlar ScardTool/Tests/ (GTest, dry-run) ile KARIŞTIRILMAMALI.
# Derlenmelerini istersen yorumu kaldır:
# add_subdirectory(Tests)
# add_subdirectory(Workshop1)

add_subdirectory(McpServer)
add_subdirectory(ScardTool)
