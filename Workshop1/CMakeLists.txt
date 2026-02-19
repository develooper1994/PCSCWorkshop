cmake_minimum_required(VERSION 3.14)
project(PCSC_workshop1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# Application executable
# Cipher, Reader and Utils are built as separate static libraries via the
# top-level CMakeLists.txt (add_subdirectory) and linked below.
# ----------------------------------------------------------------------------

add_executable(PCSC_workshop1 main.cpp SmartCardApp.cpp)

target_include_directories(PCSC_workshop1 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link the static libraries provided by sibling projects.
# Include directories and platform libs (winscard, bcrypt) propagate
# transitively through the PUBLIC dependencies of these targets.
target_link_libraries(PCSC_workshop1 PRIVATE reader cipher utils)
