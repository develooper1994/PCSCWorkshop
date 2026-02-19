cmake_minimum_required(VERSION 3.14)
project(reader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Collect reader implementation sources
file(GLOB READER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Reader/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Reader/ACR1281U/*.cpp"
)
add_library(reader STATIC ${READER_SOURCES})

# Public include directories so consumers can include Reader headers
target_include_directories(reader PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Reader
    ${CMAKE_CURRENT_SOURCE_DIR}/Reader/ACR1281U
)

# Reader depends on Cipher (for ICipher, Ciphers.h etc.)
target_link_libraries(reader PUBLIC cipher)

# Reader depends on Utils (for PcscUtils.h, CardConnection.h)
target_link_libraries(reader PUBLIC utils)
