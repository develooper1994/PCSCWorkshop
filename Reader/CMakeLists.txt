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
# When built as subdirectory the 'cipher' target is already available.
if(NOT TARGET cipher)
    set(EXTERNAL_CIPHER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Cipher")
    add_subdirectory(${EXTERNAL_CIPHER_DIR} ${CMAKE_CURRENT_BINARY_DIR}/cipher_build)
endif()
target_link_libraries(reader PUBLIC cipher)

# Reader also needs PCSC_workshop1/utils headers (PcscUtils.h, CardConnection.h)
set(PCSC_UTILS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../PCSC_workshop1/utils")
if(EXISTS "${PCSC_UTILS_DIR}/PcscUtils.h")
    target_include_directories(reader PUBLIC ${PCSC_UTILS_DIR})
endif()

# On Windows the reader uses winscard and bcrypt (via cipher/utils)
if(WIN32)
    target_link_libraries(reader PUBLIC winscard bcrypt)
endif()
