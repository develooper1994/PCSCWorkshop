# cmake/Dependencies.cmake
# ─────────────────────────────────────────────────────────────────────────────
# Tüm 3. parti bağımlılıklar burada yönetilir.
#
# TEMEL FELSEFE:
#   • Kütüphaneler kendi "çöplüklerinde" ne yaparsa yapsın — biz dokunmayız.
#   • Bağımlılıklar FetchContent ile indirilir; sadece bir kez derlenir.
#   • Kütüphanelerin uyarıları INTERFACE hedefleri ile bastırılır.
#   • Runtime uyuşmazlığı: kütüphanelere CMAKE_MSVC_RUNTIME_LIBRARY enjekte
#     edilir (CMP0091 mevcutsa) — böylece uygulama ile aynı runtime kullanır.
#
# KULLANIM:
#   target_link_libraries(my_app PRIVATE dep::linenoise)
# ─────────────────────────────────────────────────────────────────────────────

if(_DEPENDENCIES_INCLUDED)
    return()
endif()
set(_DEPENDENCIES_INCLUDED TRUE)

include(FetchContent)

# ─────────────────────────────────────────────────────────────────────────────
# Yardımcı: Alt projenin MSVC runtime'ını ana projeyle eşitle
# Kütüphane CMAKE_MSVC_RUNTIME_LIBRARY'yi CACHE'e yazsa da bizi etkiLEMEZ;
# FetchContent populate'den SONRA hedef üzerinde override uygularız.
# ─────────────────────────────────────────────────────────────────────────────
function(_fix_dep_runtime _target)
    if(NOT MSVC OR NOT TARGET ${_target})
        return()
    endif()
    if(POLICY CMP0091)
        cmake_policy(SET CMP0091 NEW)
        if(SCARDTOOL_RUNTIME STREQUAL "static")
            set_target_properties(${_target} PROPERTIES
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        elseif(SCARDTOOL_RUNTIME STREQUAL "dynamic")
            set_target_properties(${_target} PROPERTIES
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        else()
            message(STATUS "[Dependencies] Runtime eşitleme: auto (proje ayarlarına göre)")
        endif()
    endif()
endfunction()

# ─────────────────────────────────────────────────────────────────────────────
# Yardımcı: Kütüphane hedefine uyarı-bastır + runtime-eşitle uygula
# ─────────────────────────────────────────────────────────────────────────────
function(_sanitize_dep _target)
    if(NOT TARGET ${_target})
        message(WARNING "[Dependencies] Hedef bulunamadı: ${_target}")
        return()
    endif()
    # Uyarıları kapat (kütüphanenin kendi kodundaki uyarılar bizi ilgilendirmez)
    if(MSVC)
        target_compile_options(${_target} PRIVATE /W0 /wd4996 /utf-8)
    else()
        target_compile_options(${_target} PRIVATE -w)
    endif()
    # Runtime eşitle
    _fix_dep_runtime(${_target})
endfunction()

# Susspress developer warnings from FetchContent (e.g. about policies)
set(FETCHCONTENT_QUIET ON)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE BOOL "" FORCE)

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
        GIT_SHALLOW TRUE
        # ── PATCH: linenoise'un CMakeLists.txt'indeki eski policy satırını
        # modern minimum sürümle değiştir — kaynak dizine YAZILMAZ,
        # build/_deps/linenoise-src/ içinde uygulanır.
        PATCH_COMMAND
            ${CMAKE_COMMAND} -E echo "Patching linenoise CMakeLists.txt..."
            COMMAND
            ${CMAKE_COMMAND}
                -DFILE=<SOURCE_DIR>/CMakeLists.txt
                -DOLD_STR=cmake_minimum_required(VERSION 2.6)
                -DNEW_STR=cmake_minimum_required(VERSION 3.16)
                -P "${CMAKE_CURRENT_LIST_DIR}/PatchFile.cmake"
        UPDATE_DISCONNECTED TRUE   # Bir kez indir, tekrar güncelleme
    )
    FetchContent_MakeAvailable(linenoise_ng)
    # Ensure third-party linenoise respects the global MSVC runtime selection
    if(MSVC)
        if(TARGET linenoise)
            message(STATUS "Patching linenoise target to use MSVC runtime: ${CMAKE_MSVC_RUNTIME_LIBRARY}")
            if(DEFINED CMAKE_MSVC_RUNTIME_LIBRARY AND NOT "${CMAKE_MSVC_RUNTIME_LIBRARY}" STREQUAL "")
                set_target_properties(linenoise PROPERTIES
                    MSVC_RUNTIME_LIBRARY ${CMAKE_MSVC_RUNTIME_LIBRARY}
                )
            else()
				message(WARNING "CMAKE_MSVC_RUNTIME_LIBRARY is not defined; skipping MSVC runtime patch for linenoise")
            endif()
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
	set(_LUA_STATUS "OFF")
	set(_LUA_DEFINE  "0")

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
		# is lua copiled? if so, set ON but i can't check it at this point, so I set it ON by default
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
