cmake_minimum_required(VERSION 3.14)
project(scardtool LANGUAGES CXX)

# ── nlohmann/json (McpServer ile paylaşılır, FetchContent zaten çalıştı) ─────
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# ── linenoise-ng (opsiyonel) ──────────────────────────────────────────────────
option(SCARDTOOL_USE_LINENOISE "Use linenoise-ng for interactive input" ON)

if(SCARDTOOL_USE_LINENOISE)
    FetchContent_Declare(
        linenoise_ng
        GIT_REPOSITORY https://github.com/arangodb/linenoise-ng.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(linenoise_ng)
endif()

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(scardtool
    main.cpp
)

# Mcp/ dizini için McpServer'dan kopyalanmış header'lar
# (JsonRpc.h, ToolRegistry.h, McpServer.h, HexUtils.h)
# Bunlar McpServer subproject'ten sembolik veya include ile paylaşılır.
target_include_directories(scardtool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../McpServer
    ${CMAKE_CURRENT_SOURCE_DIR}/../Card/Card
    ${CMAKE_CURRENT_SOURCE_DIR}/../Reader/Reader
    ${CMAKE_CURRENT_SOURCE_DIR}/../Cipher/Cipher
)

target_link_libraries(scardtool PRIVATE
    utils
    reader
    nlohmann_json::nlohmann_json
)

target_compile_features(scardtool PRIVATE cxx_std_17)

if(WIN32)
    target_compile_definitions(scardtool PRIVATE UNICODE _UNICODE)
endif()

if(SCARDTOOL_USE_LINENOISE)
    target_compile_definitions(scardtool PRIVATE SCARDTOOL_USE_LINENOISE)
    target_link_libraries(scardtool PRIVATE linenoise)
    target_include_directories(scardtool PRIVATE
        ${linenoise_ng_SOURCE_DIR}/include
    )
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(Tests)
endif()

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS scardtool RUNTIME DESTINATION bin)

# Bash completion script üret (opsiyonel)
# install(FILES completion/scardtool.bash
#         DESTINATION /etc/bash_completion.d)

# ── Shell completion hedefi ───────────────────────────────────────────────────
# Kullanım: cmake --build build --target scardtool-completion
# add_custom_target(scardtool-completion
#     COMMAND scardtool --completion bash > scardtool.bash
#     DEPENDS scardtool
# )
