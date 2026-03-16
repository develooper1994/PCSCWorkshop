cmake_minimum_required(VERSION 3.14)
project(pcsc_mcp LANGUAGES CXX)

# ── nlohmann/json — header-only, FetchContent ────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# ── Executable ───────────────────────────────────────────────────────────────
add_executable(pcsc_mcp
    main.cpp
)

target_include_directories(pcsc_mcp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pcsc_mcp PRIVATE
    utils
    reader
    nlohmann_json::nlohmann_json
)

# C++17 zorunlu (std::variant, if constexpr, std::optional)
target_compile_features(pcsc_mcp PRIVATE cxx_std_17)

# ── Platform flags ────────────────────────────────────────────────────────────
if(WIN32)
    target_compile_definitions(pcsc_mcp PRIVATE UNICODE _UNICODE)
    # winscard zaten utils üzerinden geliyor
endif()

# ── Install (opsiyonel) ───────────────────────────────────────────────────────
install(TARGETS pcsc_mcp RUNTIME DESTINATION bin)
