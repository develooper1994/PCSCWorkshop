cmake_minimum_required(VERSION 3.14)
project(PCSCWorkshop VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# IDE için compile_commands.json üret (clang-tidy, LSP vb.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── compile_commands.json → proje kökünde symlink (clangd / cpptools için) ──
if(UNIX)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_BINARY_DIR}/compile_commands.json
            ${CMAKE_SOURCE_DIR}/compile_commands.json
    )
endif()

# ── Static kütüphaneler ──────────────────────────────────────────────────────
add_subdirectory(Utils)
add_subdirectory(Cipher)
add_subdirectory(Reader)
add_subdirectory(Card)

# ── Çalıştırılabilir hedefler ────────────────────────────────────────────────
add_subdirectory(Tests)
add_subdirectory(Workshop1)
