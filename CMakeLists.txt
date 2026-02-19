cmake_minimum_required(VERSION 3.14)
project(PCSCWorkshop VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build Utils static library (PcscUtils, CardConnection, DESFire)
add_subdirectory(Utils)

# Build Cipher static library
add_subdirectory(Cipher)

# Build Reader static library (depends on Cipher and Utils)
add_subdirectory(Reader)

# Build the main application (depends on Reader, Cipher, Utils)
add_subdirectory(Workshop1)

# Build the test executable (depends on Reader, Cipher, Utils)
add_subdirectory(Tests)
