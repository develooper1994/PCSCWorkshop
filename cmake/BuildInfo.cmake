# cmake/BuildInfo.cmake
# ─────────────────────────────────────────────────────────────────────────────
# Derleme zamanında  include/build_info.h  üretir.
# İçerik:  sürüm, git hash, derleme tarihi, aktif feature flag'ları.
# ─────────────────────────────────────────────────────────────────────────────

if(_BUILD_INFO_INCLUDED)
    return()
endif()
set(_BUILD_INFO_INCLUDED TRUE)

# ── Git hash al (opsiyonel; git yoksa "unknown") ──────────────────────────────
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE   _GIT_HASH
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --dirty --always
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE   _GIT_DESCRIBE
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    set(_GIT_HASH    "unknown")
    set(_GIT_DESCRIBE "unknown")
endif()

# ── Aktif / pasif feature listesi ─────────────────────────────────────────────
# ENABLE_CIPHER durumuna göre string oluştur
# ──────────────────────────────────────────────────────────────────────────────
if(ENABLE_CIPHER)
    set(_CIPHER_STATUS "ON")
    set(_CIPHER_DEFINE  "1")
else()
    set(_CIPHER_STATUS "OFF")
    set(_CIPHER_DEFINE  "0")
endif()

if(SCARDTOOL_USE_LINENOISE)
	set(_LINENOISE_STATUS "ON")
	set(_LINENOISE_DEFINE  "1")
else()
	set(_LINENOISE_STATUS "OFF")
	set(_LINENOISE_DEFINE  "0")
endif()

if(EMBED_LUA OR BUILD_LUA_EXTENSION)
	set(_LUA_STATUS "ON")
	set(_LUA_DEFINE  "1")
else()
	set(_LUA_STATUS "OFF")
	set(_LUA_DEFINE  "0")
endif()

if(EMBED_TCL OR BUILD_TCL_EXTENSION)
	set(_TCL_STATUS "ON")
	set(_TCL_DEFINE  "1")
else()
	set(_TCL_STATUS "OFF")
	set(_TCL_DEFINE  "0")
endif()
# ──────────────────────────────────────────────────────────────────────────────



if(SCARDTOOL_RUNTIME STREQUAL "static")
    set(_RUNTIME_STATUS "STATIC (/MT)")
elseif(SCARDTOOL_RUNTIME STREQUAL "dynamic")
    set(_RUNTIME_STATUS "DYNAMIC (/MD)")
else()
    set(_RUNTIME_STATUS "UNKNOWN")
endif()

if(NOT DEFINED PROJECT_VERSION)
	set(PROJECT_VERSION "0.0.0")
else()
	set(PROJECT_VERSION "${PROJECT_VERSION}")
endif()

# ── Üretilen dosyanın hedef dizini ────────────────────────────────────────────
set(BUILD_INFO_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated/include")
file(MAKE_DIRECTORY "${BUILD_INFO_INCLUDE_DIR}")

# ── Header içeriği ─────────────────────────────────────────────────────────
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/build_info.h.in"
    "${BUILD_INFO_INCLUDE_DIR}/build_info.h"
    @ONLY
)

message(STATUS "[BuildInfo] build_info.h -> ${BUILD_INFO_INCLUDE_DIR}/build_info.h")
message(STATUS "[BuildInfo] Git: ${_GIT_DESCRIBE} (${_GIT_HASH})")
message(STATUS "[BuildInfo] ENABLE_CIPHER = ${_CIPHER_STATUS}")
message(STATUS "[BuildInfo] libnoise      = ${_LIBNOISE_STATUS}")
message(STATUS "[BuildInfo] Lua           = ${_LUA_STATUS}")
message(STATUS "[BuildInfo] Tcl           = ${_TCL_STATUS}")
message(STATUS "[BuildInfo] Runtime       = ${_RUNTIME_STATUS}")

# ── Kolayca include edebilmek için INTERFACE kütüphane ────────────────────────
add_library(build_info_headers INTERFACE)
target_include_directories(build_info_headers INTERFACE "${BUILD_INFO_INCLUDE_DIR}")
