cmake_minimum_required(VERSION 3.14)
project(PCSC_workshop1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# Application executable – only main.cpp lives here now.
# Cipher and Reader are built as separate static libraries via the top-level
# CMakeLists.txt (add_subdirectory) and linked below.
# -----------------------------------------------------------------------------

add_executable(PCSC_workshop1 main.cpp SmartCardApp.cpp)

target_include_directories(PCSC_workshop1 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

# Link the static libraries provided by sibling projects
target_link_libraries(PCSC_workshop1 PRIVATE reader cipher)

# Link platform libs on Windows
if (WIN32)
    target_link_libraries(PCSC_workshop1 PRIVATE winscard bcrypt)
endif()
