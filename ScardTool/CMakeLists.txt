# ══════════════════════════════════════════════════════════════════════════════
# ScardTool — CMakeLists.txt
#
# Bu dosya scardtool projesinin tüm derleme modlarını tek bir CMakeLists.txt
# altında yönetir. Her seçenek bağımsız çalışabilir; bazıları bir arada da
# kullanılabilir.
#
# ── Neden çok modlu? ──────────────────────────────────────────────────────────
#
# scardtool üç farklı bağlamda kullanılır:
#
#   1. Sahada (embedded board / masaüstü): tek başına çalışan CLI aracı.
#      Kurulum gerektirmez, binary'yi kopyalayıp çalıştırmak yeterlidir.
#
#   2. Kütüphane olarak: başka bir C++ projesine gömülür (örn. kiosk yazılımı,
#      toplu taşıma bileti sistemi). Projenin kendi main() fonksiyonu vardır;
#      scardtool sadece bir bileşendir.
#
#   3. Script engine olarak: sistem Lua veya Tcl interpreter'ında çalışır.
#      Kullanıcı kendi script'ini yazar; scardtool PC/SC işlemlerini sağlar.
#      Özellikle test otomasyonu ve CI/CD pipeline'ları için kullanışlıdır.
#
# ── Boyut 1: Çıktı türü ───────────────────────────────────────────────────────
#
#   BUILD_STANDALONE (varsayılan: ON)
#     → scardtool binary'si üretilir.
#     → Embedded Linux'ta tek çözüm budur: `scp scardtool board:/usr/bin/`
#     → EMBED_LUA veya EMBED_TCL ile birleştirilebilir.
#
#   BUILD_STATIC_LIB (varsayılan: OFF)
#     → libscardtool.a üretilir.
#     → Başka bir C++ projesine static link edilir.
#     → Deployment basittir: harici .so bağımlılığı yoktur.
#     → Kullanım: target_link_libraries(myapp PRIVATE scardtool_static)
#     → Header: LibApi/ScardToolLib.h (C ve C++ API)
#
#   BUILD_SHARED_LIB (varsayılan: OFF)
#     → libscardtool.so/.dll üretilir.
#     → Birden fazla uygulama aynı kütüphaneyi kullanıyorsa
#       veya runtime'da yüklenecekse tercih edilir.
#     → SOVERSION ile API versiyonlaması desteklenir.
#     → Kullanım: dlopen() veya target_link_libraries
#
#   BUILD_LUA_EXTENSION (varsayılan: OFF)
#     → scardtool.so üretilir (Lua C extension standardı).
#     → Lua tarafında: local sc = require("scardtool")
#     → Sistem Lua zaten kuruluysa bu seçenek idealdir.
#     → Lua binary'si scardtool.so'yu yükler; PC/SC işlemleri Lua'dan yapılır.
#     → Lua'nın binary format / string.pack ile byte-level APDU inşası mümkün.
#
#   BUILD_TCL_EXTENSION (varsayılan: OFF)
#     → Scardtool.so üretilir (Tcl extension standardı: büyük harf).
#     → pkgIndex.tcl ile birlikte: package require scardtool
#     → OpenSC ve PCSC araçlarıyla aynı ekoloji.
#     → Tcl'in `binary scan/format` komutu APDU byte işlemi için mükemmeldir.
#     → Sistem Tcl zaten kuruluysa bu seçenek idealdir.
#
# ── Boyut 2: Script engine gömme (sadece standalone için) ────────────────────
#
#   EMBED_LUA (varsayılan: OFF)
#     → Lua 5.4 statik olarak scardtool binary'sine gömülür.
#     → Sistem Lua kurulu olmasa da çalışır: embedded board için ideal.
#     → Lua bulunamazsa FetchContent ile kaynak koddan derlenir.
#     → .lua dosyaları: scardtool -f script.lua
#     → string.pack/unpack ile byte-level APDU inşası.
#     → EMBED_TCL ile birlikte kullanılamaz.
#
#   EMBED_TCL (varsayılan: OFF)
#     → Tcl 8.6 statik olarak scardtool binary'sine gömülür.
#     → Sistem Tcl kurulu olmayabilir; binary her şeyi içerir.
#     → NOT: Tcl kaynak koddan derleme desteklenmez; sistem Tcl gerekir.
#       Eğer bulunamazsa bu seçenek otomatik devre dışı kalır.
#     → .tcl dosyaları: scardtool -f script.tcl
#     → binary format/scan ile byte-level APDU inşası.
#     → EMBED_LUA ile birlikte kullanılamaz.
#
# ── Seçenek kombinasyonları ───────────────────────────────────────────────────
#
#   Senaryo A — Embedded Linux, çok kısıtlı ortam, sadece binary:
#     cmake -DBUILD_STANDALONE=ON ..
#
#   Senaryo B — Embedded Linux + Lua script desteği, sıfır bağımlılık:
#     cmake -DBUILD_STANDALONE=ON -DEMBED_LUA=ON ..
#
#   Senaryo C — Masaüstü, Lua zaten kurulu, script desteği isteniyor:
#     cmake -DBUILD_LUA_EXTENSION=ON ..
#     # Ardından: lua5.4 -e "local sc=require('scardtool'); sc.set_reader(0)"
#
#   Senaryo D — Başka C++ projesine statik entegrasyon:
#     cmake -DBUILD_STATIC_LIB=ON ..
#     # Ardından: target_link_libraries(myapp PRIVATE scardtool)
#
#   Senaryo E — Birden fazla uygulama, ortak shared library:
#     cmake -DBUILD_SHARED_LIB=ON ..
#
#   Senaryo F — Geliştirici ortamı, hem binary hem Lua extension:
#     cmake -DBUILD_STANDALONE=ON -DBUILD_LUA_EXTENSION=ON ..
#
#   Senaryo G — Tcl test otomasyonu (OpenSC uyumlu):
#     cmake -DBUILD_TCL_EXTENSION=ON ..
#
#   Senaryo H — Cross-compile, ARM embedded Linux:
#     cmake -DCMAKE_TOOLCHAIN_FILE=arm-gnueabihf.cmake \
#           -DBUILD_STANDALONE=ON \
#           -DEMBED_LUA=ON \
#           -DSCARDTOOL_USE_LINENOISE=OFF ..
#     # linenoise interactive özellik istemiyorsak kapatılabilir
#
# ── Cross-compile notları ─────────────────────────────────────────────────────
#
#   Lua 5.4 musl libc ve uclibc ile uyumludur.
#   LUA_USE_LINUX flag'i ile cross-compile sorunsuz çalışır.
#
#   Tcl cross-compile daha zordur; Tcl'i hedef sysroot'a kurmak gerekir.
#   EMBED_TCL cross-compile için önerilmez — EMBED_LUA kullanın.
#
#   linenoise-ng POSIX readline gerektirir; embed board'da kapatılabilir:
#     -DSCARDTOOL_USE_LINENOISE=OFF
#
# ── C API (LibApi/ScardToolLib.h) ─────────────────────────────────────────────
#
#   Tüm library modları (static/shared/extension) ortak C API'yi kullanır.
#   Bu sayede Lua, Tcl, Python (ctypes) veya herhangi bir FFI C API'yi
#   kullanabilir. C++ RAII wrapper da aynı header'da tanımlanmıştır.
#
# ══════════════════════════════════════════════════════════════════════════════

cmake_minimum_required(VERSION 3.16)
project(scardtool LANGUAGES CXX C)

# ── Seçenekler ────────────────────────────────────────────────────────────────

# Çıktı türleri — birden fazlası aynı anda aktif olabilir
option(BUILD_STANDALONE
    "Standalone scardtool binary üret. Embedded board ve masaüstü için."
    ON)

option(BUILD_STATIC_LIB
    "libscardtool.a üret. Başka C++ projesine static link için. \
Harici bağımlılık olmaz, deployment basittir."
    OFF)

option(BUILD_SHARED_LIB
    "libscardtool.so/.dll üret. Birden fazla uygulama paylaşacaksa \
veya runtime yüklemesi (dlopen) gerekiyorsa kullanılır."
    OFF)

option(BUILD_LUA_EXTENSION
    "scardtool.so üret — Lua 5.4 C extension. \
Sistem Lua zaten kuruluysa require('scardtool') ile kullanılır. \
Lua'nın string.pack/unpack ile APDU byte inşası mümkün olur."
    OFF)

option(BUILD_TCL_EXTENSION
    "Scardtool.so üret — Tcl 8.6 extension. \
package require scardtool ile kullanılır. \
Tcl binary scan/format APDU byte işlemi için mükemmeldir. \
OpenSC/PCSC araçlarıyla aynı ekoloji."
    OFF)

# Script engine gömme (sadece BUILD_STANDALONE ile anlamlıdır)
option(EMBED_LUA
    "Lua 5.4'ü standalone binary'e statik göm. \
Sistem Lua kurulu olmasa da çalışır; embedded Linux için idealdir. \
Lua bulunamazsa otomatik olarak kaynak koddan derlenir (FetchContent). \
EMBED_TCL ile aynı anda kullanılamaz."
    OFF)

option(EMBED_TCL
    "Tcl 8.6'yı standalone binary'e statik göm. \
NOT: Tcl kaynak koddan cross-compile desteklenmez; sistem Tcl gereklidir. \
EMBED_LUA ile aynı anda kullanılamaz. Cross-compile için EMBED_LUA tercih edin."
    OFF)

option(SCARDTOOL_USE_LINENOISE
    "linenoise-ng ile readline benzeri interactive input. \
Embedded board'da istemiyorsanız -DSCARDTOOL_USE_LINENOISE=OFF ile kapatın. \
Kapalıyken getline() kullanılır, daha az bağımlılık."
    ON)

# ── Çakışma kontrolü ─────────────────────────────────────────────────────────

if(EMBED_LUA AND EMBED_TCL)
    message(FATAL_ERROR
        "EMBED_LUA ve EMBED_TCL aynı anda aktif edilemez.\n"
        "Lua: embedded Linux için önerilir (musl/uclibc uyumlu, cross-compile kolay)\n"
        "Tcl: sistem Tcl kuruluysa ve binary scan/format gerekiyorsa")
endif()

if(NOT BUILD_STANDALONE AND (EMBED_LUA OR EMBED_TCL))
    message(WARNING
        "EMBED_LUA/EMBED_TCL yalnızca BUILD_STANDALONE=ON ile anlamlıdır.\n"
        "Extension veya library modlarında Lua/Tcl sistem kütüphaneleri kullanılır.")
endif()

# ── Bağımlılıklar ─────────────────────────────────────────────────────────────

include(FetchContent)

# nlohmann/json — JSON çıktı ve MCP protokolü için
# Header-only, derleme süresini artırır ama link bağımlılığı yoktur
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(nlohmann_json)

# linenoise-ng — interactive shell için readline benzeri input
# Kapatılabilir: -DSCARDTOOL_USE_LINENOISE=OFF
# Embedded board'da gereksiz olabilir (getline() yeterlidir)
if(SCARDTOOL_USE_LINENOISE)
    FetchContent_Declare(linenoise_ng
        GIT_REPOSITORY https://github.com/arangodb/linenoise-ng.git
        GIT_TAG master
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(linenoise_ng)
    # Ensure third-party linenoise respects the global MSVC runtime selection
    if(MSVC)
        if(TARGET linenoise)
            message(STATUS "Patching linenoise target to use MSVC runtime: ${CMAKE_MSVC_RUNTIME_LIBRARY}")
            set_target_properties(linenoise PROPERTIES
                MSVC_RUNTIME_LIBRARY ${CMAKE_MSVC_RUNTIME_LIBRARY}
            )
            # Exclude vendor example executable from top-level build (avoids link/runtime mismatch)
            if(TARGET example)
                message(STATUS "Excluding linenoise example target from top-level build")
                set_target_properties(example PROPERTIES EXCLUDE_FROM_ALL TRUE EXCLUDE_FROM_DEFAULT_BUILD TRUE)
            endif()
        else()
            message(WARNING "linenoise target not found after FetchContent; runtime cannot be enforced")
        endif()
    endif()
endif()

# ── Lua 5.4 ───────────────────────────────────────────────────────────────────
#
# EMBED_LUA veya BUILD_LUA_EXTENSION aktifse Lua aranır.
#
# Arama sırası:
#   1. Sistem Lua (find_package) — zaten kuruluysa kullanılır
#   2. FetchContent — sistem'de yoksa kaynak kod indirilip derlenir
#
# Cross-compile için:
#   Lua 5.4 musl ve uclibc ile uyumludur.
#   CMAKE_TOOLCHAIN_FILE ile ARM/MIPS/RISC-V için cross-compile çalışır.
#   LUA_USE_LINUX tanımı ile POSIX API'leri etkinleştirilir.
#
if(EMBED_LUA OR BUILD_LUA_EXTENSION)
    find_package(Lua 5.4 QUIET)

    if(NOT LUA_FOUND)
        message(STATUS
            "Sistem Lua 5.4 bulunamadı. Kaynak kod indiriliyor (FetchContent)...")
        FetchContent_Declare(lua_src
            URL https://www.lua.org/ftp/lua-5.4.7.tar.gz
            # SHA256 hash — değişirse güncelle:
            # URL_HASH SHA256=...
        )
        FetchContent_MakeAvailable(lua_src)

        # Lua core dosyaları (lua ve luac CLI'ları hariç)
        file(GLOB LUA_SOURCES "${lua_src_SOURCE_DIR}/src/*.c")
        list(FILTER LUA_SOURCES EXCLUDE REGEX "(lua|luac)\\.c$")

        add_library(lua_static STATIC ${LUA_SOURCES})
        target_include_directories(lua_static PUBLIC "${lua_src_SOURCE_DIR}/src")

        # Platform tanımları:
        #   LUA_USE_LINUX  → POSIX dlopen, readline, longjmp
        #   LUA_USE_WINDOWS → Windows API
        #   (musl ve uclibc de LUA_USE_LINUX ile uyumludur)
        target_compile_definitions(lua_static PRIVATE
            $<$<BOOL:${UNIX}>:LUA_USE_LINUX>
            $<$<BOOL:${WIN32}>:LUA_USE_WINDOWS>)

        set(LUA_LIBRARIES lua_static)
        set(LUA_INCLUDE_DIR "${lua_src_SOURCE_DIR}/src")
        set(LUA_FOUND TRUE)
        message(STATUS "Lua 5.4 kaynak koddan derleniyor.")
    else()
        message(STATUS "Sistem Lua ${LUA_VERSION_STRING} kullanılıyor.")
    endif()
endif()

# ── Tcl 8.6 ───────────────────────────────────────────────────────────────────
#
# Tcl sadece sistem kurulumundan aranır.
# Kaynak koddan cross-compile desteklenmez (autoconf karmaşıklığı).
#
# Debian/Ubuntu: apt install tcl-dev
# Alpine: apk add tcl-dev
# macOS: brew install tcl-tk
#
# Cross-compile için:
#   Tcl yerine Lua tercih edilmesi önerilir.
#   Zorunluysa hedef sysroot'a Tcl kurulmalıdır.
#
if(EMBED_TCL OR BUILD_TCL_EXTENSION)
    find_package(TCL QUIET)
    if(NOT TCL_FOUND)
        message(WARNING
            "Tcl bulunamadı.\n"
            "  Debian/Ubuntu: apt install tcl-dev\n"
            "  Alpine:        apk add tcl-dev\n"
            "  macOS:         brew install tcl-tk\n"
            "EMBED_TCL ve BUILD_TCL_EXTENSION devre dışı bırakılıyor.")
        set(EMBED_TCL           OFF CACHE BOOL "" FORCE)
        set(BUILD_TCL_EXTENSION OFF CACHE BOOL "" FORCE)
    else()
        message(STATUS "Tcl ${TCL_VERSION} bulundu.")
    endif()
endif()

# ── Ortak include ve link ayarları ────────────────────────────────────────────

# scardtool modül include yolları — tüm hedefler için ortak
set(SCARDTOOL_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}               # ScardTool/ kökü
    ${CMAKE_CURRENT_SOURCE_DIR}/../McpServer  # McpServer header'ları
    ${CMAKE_CURRENT_SOURCE_DIR}/../Card/Card
    ${CMAKE_CURRENT_SOURCE_DIR}/../Reader/Reader
    ${CMAKE_CURRENT_SOURCE_DIR}/../Cipher/Cipher)

# ── Hedef konfigürasyon fonksiyonu ───────────────────────────────────────────
#
# Her hedef için ortak kurulum:
#   - Include yolları
#   - Temel kütüphaneler
#   - C++17 standardı
#   - Platform tanımları
#   - linenoise bağlantısı (STANDALONE modda)
#   - Lua/Tcl gömme (STANDALONE modda isteniyorsa)
#   - Shared/extension için visibility ayarları
#
# MODE parametresi davranışı kontrol eder:
#   STANDALONE  → binary; Lua/Tcl gömme aktifleştirilebilir
#   STATIC      → .a; gömme yapılmaz; linker caller'a bırakılır
#   SHARED      → .so/.dll; visibility=hidden; export sadece SCARDTOOL_API
#   LUA_EXT     → .so; Lua extension; Lua'ya link edilmez (runtime resolve)
#   TCL_EXT     → .so; Tcl extension; Tcl'e link edilir
#
function(scardtool_setup TARGET MODE)
    target_include_directories(${TARGET} PRIVATE ${SCARDTOOL_INCLUDE_DIRS})

    # Temel bağımlılıklar: tüm modlar için zorunlu
    target_link_libraries(${TARGET} PRIVATE
        utils
        reader
        nlohmann_json::nlohmann_json)

    target_compile_features(${TARGET} PRIVATE cxx_std_17)

    # Windows: Unicode API'leri etkinleştir
    if(WIN32)
        target_compile_definitions(${TARGET} PRIVATE UNICODE _UNICODE)
    endif()

    # linenoise: sadece STANDALONE modda; embedded'da kapatılabilir
    if(SCARDTOOL_USE_LINENOISE AND MODE STREQUAL "STANDALONE")
        target_compile_definitions(${TARGET} PRIVATE SCARDTOOL_USE_LINENOISE)
        target_link_libraries(${TARGET} PRIVATE linenoise)
        target_include_directories(${TARGET} PRIVATE
            ${linenoise_ng_SOURCE_DIR}/include)
    endif()

    # ── Lua gömme ─────────────────────────────────────────────────────────────
    # Sadece STANDALONE modda ve EMBED_LUA=ON ise:
    #   Lua statik binary içine dahil edilir.
    #   .lua uzantılı dosyalar scardtool -f script.lua ile çalışır.
    #   Sistem Lua kurulu olmasa da çalışır.
    if(EMBED_LUA AND LUA_FOUND AND MODE STREQUAL "STANDALONE")
        target_compile_definitions(${TARGET} PRIVATE
            SCARDTOOL_EMBED_LUA    # main.cpp: .lua uzantısını Lua engine'e yönlendir
            SCARDTOOL_BUILD_LUA_EXT) # lua_scardtool.cpp: SCARDTOOL_API export
        target_link_libraries(${TARGET} PRIVATE ${LUA_LIBRARIES})
        target_include_directories(${TARGET} PRIVATE ${LUA_INCLUDE_DIR})
        target_sources(${TARGET} PRIVATE LibApi/lua_scardtool.cpp)
        message(STATUS "[${TARGET}] Lua 5.4 statik olarak gömüldü.")
    endif()

    # ── Tcl gömme ─────────────────────────────────────────────────────────────
    # Sadece STANDALONE modda ve EMBED_TCL=ON ise:
    #   Tcl statik binary içine dahil edilir.
    #   .tcl uzantılı dosyalar scardtool -f script.tcl ile çalışır.
    if(EMBED_TCL AND TCL_FOUND AND MODE STREQUAL "STANDALONE")
        target_compile_definitions(${TARGET} PRIVATE
            SCARDTOOL_EMBED_TCL
            SCARDTOOL_BUILD_TCL_EXT)
        target_link_libraries(${TARGET} PRIVATE ${TCL_LIBRARY})
        target_include_directories(${TARGET} PRIVATE ${TCL_INCLUDE_PATH})
        target_sources(${TARGET} PRIVATE LibApi/tcl_scardtool.cpp)
        message(STATUS "[${TARGET}] Tcl 8.6 statik olarak gömüldü.")
    endif()

    # ── Shared/Extension visibility ───────────────────────────────────────────
    # .so hedeflerinde sadece SCARDTOOL_API işaretli semboller dışarı açılır.
    # Bu sayede internal semboller başka kütüphanelerle çakışmaz.
    if(MODE STREQUAL "SHARED" OR MODE STREQUAL "LUA_EXT" OR MODE STREQUAL "TCL_EXT")
        target_compile_definitions(${TARGET} PRIVATE SCARDTOOL_BUILD_SHARED)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            # Tüm semboller varsayılan hidden; sadece __attribute__((visibility("default")))
            # olanlar dışarıya açılır
            target_compile_options(${TARGET} PRIVATE -fvisibility=hidden)
        endif()
    endif()
endfunction()

# ══════════════════════════════════════════════════════════════════════════════
# HEDEF 1: Standalone Binary — scardtool
#
# Ne zaman: Embedded board, masaüstü, CI/CD pipeline, doğrudan kullanım.
#
# Özellikler:
#   - main.cpp + LibApi/ScardToolLib.cpp dahildir
#   - EMBED_LUA=ON ile Lua 5.4 içine gömülür
#   - EMBED_TCL=ON ile Tcl 8.6 içine gömülür
#   - SCARDTOOL_USE_LINENOISE=OFF ile sadece getline() (minimal)
#
# Cross-compile örneği:
#   cmake -DCMAKE_TOOLCHAIN_FILE=arm-gnueabihf.cmake \
#         -DBUILD_STANDALONE=ON \
#         -DEMBED_LUA=ON \
#         -DSCARDTOOL_USE_LINENOISE=OFF \
#         -DCMAKE_BUILD_TYPE=MinSizeRel ..
#
# ══════════════════════════════════════════════════════════════════════════════
if(BUILD_STANDALONE)
    add_executable(scardtool
        main.cpp
        LibApi/ScardToolLib.cpp)
    scardtool_setup(scardtool STANDALONE)
    install(TARGETS scardtool RUNTIME DESTINATION bin)
    message(STATUS "Hedef eklendi: scardtool (standalone binary)")
endif()

# ══════════════════════════════════════════════════════════════════════════════
# HEDEF 2: Statik Kütüphane — libscardtool.a
#
# Ne zaman: scardtool'u başka bir C++ projesine dahil etmek istiyorsunuz
#           ve harici bağımlılık istemiyorsunuz.
#
# Kullanım (proje CMakeLists.txt):
#   find_library(SCARDTOOL_LIB scardtool HINTS /usr/local/lib)
#   target_link_libraries(myapp PRIVATE ${SCARDTOOL_LIB})
#   target_include_directories(myapp PRIVATE /usr/local/include/scardtool)
#
# Veya FetchContent/add_subdirectory ile:
#   add_subdirectory(ScardTool)
#   target_link_libraries(myapp PRIVATE scardtool_static)
#
# API: LibApi/ScardToolLib.h — C ve C++ RAII wrapper
#
# Not: main() fonksiyonu dahil edilmez; sadece kütüphane kodu derlenir.
#      main.cpp da dahil edilir ama main() sembolu caller tarafından sağlanır.
#      Daha temiz bir statik lib için ayrı core kaynak seti oluşturulabilir.
#
# ══════════════════════════════════════════════════════════════════════════════
if(BUILD_STATIC_LIB)
    add_library(scardtool_static STATIC
        main.cpp
        LibApi/ScardToolLib.cpp)
    scardtool_setup(scardtool_static STATIC)

    # OUTPUT_NAME: libscardtool.a (prefix lib otomatik eklenir)
    set_target_properties(scardtool_static PROPERTIES OUTPUT_NAME scardtool)

    # Public include: yüklemeden sonra ScardToolLib.h erişilebilir olsun
    target_include_directories(scardtool_static PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/LibApi>
        $<INSTALL_INTERFACE:include/scardtool>)

    install(TARGETS scardtool_static ARCHIVE DESTINATION lib)
    install(FILES LibApi/ScardToolLib.h DESTINATION include/scardtool)
    message(STATUS "Hedef eklendi: libscardtool.a (statik kütüphane)")
endif()

# ══════════════════════════════════════════════════════════════════════════════
# HEDEF 3: Dinamik Kütüphane — libscardtool.so / scardtool.dll
#
# Ne zaman:
#   - Birden fazla uygulama aynı scardtool kütüphanesini kullanıyor.
#   - Güncelleme yapılacak: uygulamaları yeniden derlemeden .so güncellenebilir.
#   - dlopen() ile runtime yükleme gerekiyor.
#   - Python (ctypes/cffi), Ruby veya diğer diller C API kullanacak.
#
# Özellikler:
#   - SOVERSION: API versiyonlaması (libscardtool.so.1 → libscardtool.so.1.0.0)
#   - visibility=hidden: internal semboller gizlenir, sadece SCARDTOOL_API açılır
#   - Windows: SCARDTOOL_BUILD_SHARED tanımı ile __declspec(dllexport) etkin
#
# Python örneği (ctypes):
#   import ctypes
#   sc = ctypes.CDLL("libscardtool.so")
#   ctx = sc.scardtool_create()
#   sc.scardtool_set_reader(ctx, 0)
#
# ══════════════════════════════════════════════════════════════════════════════
if(BUILD_SHARED_LIB)
    add_library(scardtool_shared SHARED
        main.cpp
        LibApi/ScardToolLib.cpp)
    scardtool_setup(scardtool_shared SHARED)

    set_target_properties(scardtool_shared PROPERTIES
        OUTPUT_NAME scardtool  # → libscardtool.so
        VERSION     1.0.0      # tam versiyon
        SOVERSION   1)         # uyumluluk versiyonu (SONAME)

    target_include_directories(scardtool_shared PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/LibApi>
        $<INSTALL_INTERFACE:include/scardtool>)

    install(TARGETS scardtool_shared LIBRARY DESTINATION lib)
    install(FILES LibApi/ScardToolLib.h DESTINATION include/scardtool)
    message(STATUS "Hedef eklendi: libscardtool.so (dinamik kütüphane)")
endif()

# ══════════════════════════════════════════════════════════════════════════════
# HEDEF 4: Lua 5.4 Extension — scardtool.so
#
# Ne zaman: Sistem Lua zaten kurulu; scardtool'u Lua script'lerinden
#           require() ile kullanmak istiyorsunuz.
#
# Neden Lua?
#   - Lua 5.4 string.pack/unpack: byte-level APDU inşası için idealdir.
#   - Minimal bağımlılık: ~250KB, musl uyumlu, cross-compile trivial.
#   - Gömülü sistemlerde (OpenWrt, router, kiosk) Lua yaygın.
#
# Lua'da kullanım:
#   local sc = require("scardtool")
#   sc.set_reader(0)
#   local resp = sc.send_apdu("FF CA 00 00 00")
#   -- APDU inşası:
#   local aid = string.pack("c8", "\xD2\x76\x00\x00\x85\x01\x00\x00")
#   local apdu = string.pack(">BBBBB", 0x00, 0xA4, 0x04, 0x00, #aid) .. aid
#
# Teknik notlar:
#   - PREFIX "" ve SUFFIX ".so": Lua convention (libscardtool.so değil scardtool.so)
#   - Lua'ya LINK edilmez: semboller Lua interpreter'dan runtime resolve edilir.
#     Bu sayede farklı Lua sürümleriyle uyumlu olur.
#   - macOS: "-undefined dynamic_lookup" ile Lua semboller lazy resolve edilir.
#   - luaopen_scardtool: extension entry point (Lua standardı: luaopen_<name>)
#
# Kurulum sonrası:
#   scardtool.so → /usr/lib/lua/5.4/ (veya LUA_CPATH'teki bir dizin)
#
# ══════════════════════════════════════════════════════════════════════════════
if(BUILD_LUA_EXTENSION)
    if(NOT LUA_FOUND)
        message(WARNING
            "BUILD_LUA_EXTENSION=ON ama Lua 5.4 bulunamadı — hedef atlanıyor.\n"
            "  Debian/Ubuntu: apt install liblua5.4-dev\n"
            "  Alpine:        apk add lua5.4-dev\n"
            "  macOS:         brew install lua")
    else()
        add_library(scardtool_lua MODULE
            LibApi/ScardToolLib.cpp
            LibApi/lua_scardtool.cpp)
        scardtool_setup(scardtool_lua LUA_EXT)

        # Lua convention: "scardtool.so" (lib prefix ve farklı suffix yok)
        set_target_properties(scardtool_lua PROPERTIES
            PREFIX      ""
            OUTPUT_NAME "scardtool"
            SUFFIX      ".so")

        target_compile_definitions(scardtool_lua PRIVATE SCARDTOOL_BUILD_LUA_EXT)
        target_include_directories(scardtool_lua PRIVATE ${LUA_INCLUDE_DIR})

        # Lua'ya LINK ETME — semboller runtime'da interpreter'dan gelir.
        # Bu yaklaşım farklı Lua sürümleriyle uyumu sağlar.
        if(APPLE)
            # macOS'ta "undefined dynamic_lookup" ile lazy resolution
            set_target_properties(scardtool_lua PROPERTIES
                LINK_FLAGS "-undefined dynamic_lookup")
        endif()
        # Linux'ta ek flag gerekmez; dlopen RTLD_GLOBAL ile Lua sembolleri açar.

        install(TARGETS scardtool_lua LIBRARY DESTINATION lib/lua/5.4)
        message(STATUS "Hedef eklendi: scardtool.so (Lua 5.4 extension)")
    endif()
endif()

# ══════════════════════════════════════════════════════════════════════════════
# HEDEF 5: Tcl 8.6 Extension — Scardtool.so
#
# Ne zaman: Sistem Tcl kurulu; scardtool'u Tcl script'lerinden
#           "package require scardtool" ile kullanmak istiyorsunuz.
#
# Neden Tcl?
#   - Tcl'in `binary scan/format` komutu APDU byte işlemi için mükemmeldir:
#       binary format "H2H2H2H2" $cla $ins $p1 $p2 → APDU header
#       binary scan $response "H2H2" sw1 sw2       → SW1/SW2 parse
#   - OpenSC, PCSC-lite test araçları Tcl kullanıyor; aynı ekoloji.
#   - Tkinter UI ile kart uygulaması GUI'si hızlıca yazılabilir.
#
# Tcl'de kullanım:
#   package require scardtool
#   scardtool::set_reader 0
#   set resp [scardtool::send_apdu "FF CA 00 00 00"]
#   # APDU inşası:
#   set data [binary format "H*" "01020304"]
#   binary scan $data H* hex_data
#   set apdu [binary format "H2H2H2H2ca*" 00 A4 04 00 [string length $data] $data]
#
# Teknik notlar:
#   - OUTPUT_NAME "Scardtool": Tcl convention büyük harf (Scardtool.so)
#   - Tcl_InitStubs: runtime Tcl sürümüne bağlanır (8.6+)
#   - pkgIndex.tcl: "package require scardtool" için gereklidir
#   - Scardtool_Init / Scardtool_SafeInit: Tcl extension entry point standardı
#   - Tcl'e LINK EDİLİR (Lua'dan farklı): Tcl semboller link zamanında çözülür
#
# Kurulum sonrası:
#   Scardtool.so + pkgIndex.tcl → /usr/lib/tcl/scardtool1.0/
#   tclsh8.6: package require scardtool
#
# ══════════════════════════════════════════════════════════════════════════════
if(BUILD_TCL_EXTENSION)
    if(NOT TCL_FOUND)
        message(WARNING
            "BUILD_TCL_EXTENSION=ON ama Tcl bulunamadı — hedef atlanıyor.")
    else()
        add_library(scardtool_tcl MODULE
            LibApi/ScardToolLib.cpp
            LibApi/tcl_scardtool.cpp)
        scardtool_setup(scardtool_tcl TCL_EXT)

        # Tcl convention: büyük harf ile başlar
        set_target_properties(scardtool_tcl PROPERTIES
            PREFIX      ""
            OUTPUT_NAME "Scardtool"
            SUFFIX      ".so")

        target_compile_definitions(scardtool_tcl PRIVATE SCARDTOOL_BUILD_TCL_EXT)
        target_include_directories(scardtool_tcl PRIVATE ${TCL_INCLUDE_PATH})

        # Tcl'e link edilir (Lua extension'dan farklı):
        # Tcl sembolleri (Tcl_InitStubs, Tcl_CreateObjCommand, vb.) link zamanında çözülür
        target_link_libraries(scardtool_tcl PRIVATE ${TCL_LIBRARY})

        # pkgIndex.tcl — "package require scardtool" için gerekli
        # Tcl bu dosyayı okuyarak Scardtool.so'yu yükler
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl"
            "# scardtool Tcl package index\n"
            "# Otomatik üretilmiştir — elle düzenlemeyin\n"
            "package ifneeded scardtool 1.0 "
            "[list load [file join \$dir Scardtool.so] Scardtool]\n")

        set(TCL_PKG_INSTALL_DIR
            "${CMAKE_INSTALL_PREFIX}/lib/tcl/scardtool1.0"
            CACHE PATH "Tcl package kurulum dizini")

        install(TARGETS scardtool_tcl
                LIBRARY DESTINATION "${TCL_PKG_INSTALL_DIR}")
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl"
                DESTINATION "${TCL_PKG_INSTALL_DIR}")
        message(STATUS "Hedef eklendi: Scardtool.so (Tcl 8.6 extension)")
    endif()
endif()

# ══════════════════════════════════════════════════════════════════════════════
# Tests
#
# Testler yalnızca BUILD_TESTING=ON ile derlenir.
# Varsayılan: CTest'in default değeri olan ON.
# Kapatmak için: cmake -DBUILD_TESTING=OFF ..
#
# Testler gerçek PC/SC reader gerektirmez; --dry-run ve mock ile çalışır.
# ══════════════════════════════════════════════════════════════════════════════
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(Tests)
endif()

# ══════════════════════════════════════════════════════════════════════════════
# Kurulum özeti
# ══════════════════════════════════════════════════════════════════════════════
message(STATUS "")
message(STATUS "┌─ scardtool build konfigürasyonu ─────────────────────────────")
message(STATUS "│")
message(STATUS "│  Çıktı türleri:")
message(STATUS "│    Standalone binary     : ${BUILD_STANDALONE}")
message(STATUS "│    Statik kütüphane      : ${BUILD_STATIC_LIB}  (libscardtool.a)")
message(STATUS "│    Dinamik kütüphane     : ${BUILD_SHARED_LIB}  (libscardtool.so)")
message(STATUS "│    Lua extension         : ${BUILD_LUA_EXTENSION}  (scardtool.so)")
message(STATUS "│    Tcl extension         : ${BUILD_TCL_EXTENSION}  (Scardtool.so)")
message(STATUS "│")
message(STATUS "│  Script engine gömme (standalone):")
message(STATUS "│    Gömülü Lua 5.4        : ${EMBED_LUA}")
message(STATUS "│    Gömülü Tcl 8.6        : ${EMBED_TCL}")
message(STATUS "│")
message(STATUS "│  Diğer seçenekler:")
message(STATUS "│    linenoise-ng           : ${SCARDTOOL_USE_LINENOISE}")
message(STATUS "│    Testler                : ${BUILD_TESTING}")
message(STATUS "│")
message(STATUS "│  Hızlı senaryo örnekleri:")
message(STATUS "│    Embedded Linux         : cmake -DBUILD_STANDALONE=ON ..")
message(STATUS "│    Embedded + Lua         : cmake -DEMBED_LUA=ON ..")
message(STATUS "│    C++ entegrasyonu       : cmake -DBUILD_STATIC_LIB=ON ..")
message(STATUS "│    Lua extension          : cmake -DBUILD_LUA_EXTENSION=ON ..")
message(STATUS "│    Tcl extension          : cmake -DBUILD_TCL_EXTENSION=ON ..")
message(STATUS "└───────────────────────────────────────────────────────────────")
message(STATUS "")
