cmake_minimum_required(VERSION 3.14)
project(PCSCWorkshop VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build Cipher static library first
add_subdirectory(Cipher)

# Build Reader static library (depends on Cipher)
add_subdirectory(Reader)

# Build the main application (depends on Cipher and Reader)
add_subdirectory(PCSC_workshop1)
