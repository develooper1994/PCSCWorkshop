# cmake/CompilerOptions.cmake
# ─────────────────────────────────────────────────────────────────────────────
# Merkezi derleyici ayarları:
#   - MSVC runtime seçimi (MD/MT) — USE_STATIC_RUNTIME option'a göre
#   - Yüksek uyarı seviyesi + uyarı düzeltmeleri
#   - Unicode (UTF-8) zorunlu kılma
#   - C++17 standardı
# ─────────────────────────────────────────────────────────────────────────────

# Bu modül yalnızca bir kez çalışsın
if(_COMPILER_OPTIONS_INCLUDED)
    return()
endif()
set(_COMPILER_OPTIONS_INCLUDED TRUE)

# ─────────────────────────────────────────────────────────────────────────────
# C++ standardı — tüm hedeflere uygulanacak varsayılan
# ─────────────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# IDE için compile_commands.json üret (clangd, clang-tidy, VS Code vb.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─────────────────────────────────────────────────────────────────────────────
# MSVC için ek yardımcı fonksiyonlar
# - scardtool_msvc_runtime(): CRT runtime seçimi için preset'lerle uyumlu fonksiyon
# - scardtool_msvc_flags(): LTCG, uyarılar, Unicode tanımlarımları vb. ekler
# ─────────────────────────────────────────────────────────────────────────────
function(scardtool_msvc_runtime)
    if(NOT MSVC)
        return()
    endif()

    set(SCARDTOOL_RUNTIME "auto" CACHE STRING "auto|dynamic|static")
    set_property(CACHE SCARDTOOL_RUNTIME PROPERTY STRINGS auto dynamic static)

    # CMake 3.15+ ile CMAKE_MSVC_RUNTIME_LIBRARY kullanarak runtime seçimi
    cmake_policy(SET CMP0091 NEW)

    if(SCARDTOOL_RUNTIME STREQUAL "dynamic")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        message(STATUS "CRT: /MD")
    elseif(SCARDTOOL_RUNTIME STREQUAL "static")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        message(STATUS "CRT: /MT")
    else()
        message(STATUS "CRT: auto")
    endif()
endfunction()
function(scardtool_msvc_flags)
    if(NOT MSVC)
        return()
    endif()

    option(SCARDTOOL_ENABLE_LTCG "Link Time Code Generation. Enable LTCG (/GL + /LTCG)" OFF)

    # ── Paralel derleme (/MP) ─────────────────────────────────────────────────
    cmake_host_system_information(RESULT _cpu_count QUERY NUMBER_OF_LOGICAL_CORES)
    add_compile_options(/MP${_cpu_count})
    message(STATUS "[CompilerOptions] Paralel derleme: /MP${_cpu_count}")

    add_compile_options(
        /W4             # Yüksek uyarı seviyesi
        /WX-            # Uyarıları hata yapMA (henüz; düzelttikçe WX açılabilir)
        /permissive-    # Standarda sıkı uyum
        /utf-8          # Kaynak + çalışma zamanı karakter seti UTF-8
        /wd4100         # unreferenced formal parameter — kütüphaneler çok üretiyor
        /wd4505         # unreferenced local function — header-only kütüphaneler
        /wd4244         # narrowing conversion — agresif; gerekirse geri aç
        /wd4267         # size_t -> int dönüşümü — kütüphanelerden geliyor
        /wd4996         # deprecated CRT (_sprintf vb.) — kütüphane uyumluluğu
        /wd4127         # constant conditional expression — eski C kodları
        /wd4251         # dll-interface uyarısı — 3. parti kütüphaneler
        /bigobj         # Çok sayıda template kodu derlerken "fatal error C1128: number of sections exceeded" hatasını önler
    )

    add_compile_definitions(
        UNICODE _UNICODE
        _CRT_SECURE_NO_WARNINGS
        _SCL_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )

    if(SCARDTOOL_ENABLE_LTCG)
        add_compile_options($<$<CONFIG:Release>:/GL>)
        add_link_options($<$<CONFIG:Release>:/LTCG>)
        message(STATUS "LTCG: ON")
    endif()
endfunction()

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
    # kullanım
    scardtool_msvc_runtime()
    scardtool_msvc_flags()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Linux/macOS: -Wall -Wextra uyarıları, ancak hata yapma
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter  # Kütüphanelerde çok var, temizlemek zor
        -Wno-unused-function   # Header-only kütüphanelerde çok var
        -Wno-deprecated-declarations  # Eski API'ler bazı platformlarda kaçınılmaz
        )
    # UTF-8 kaynak dosyası
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Kütüphanelerin kendi compile flag'larını temizleyen yardımcı makro
# Kullanım: clean_target_warnings(linenoise)
# ─────────────────────────────────────────────────────────────────────────────
macro(clean_target_warnings _target)
    if(TARGET ${_target})
        if(MSVC)
            target_compile_options(${_target} PRIVATE
                /W0         # Bu hedef için tüm uyarıları kapat
                /wd4996
            )
        else()
            target_compile_options(${_target} PRIVATE
                -w          # Tüm uyarıları kapat
            )
        endif()
    endif()
endmacro()
