cmake_minimum_required(VERSION 3.14)
project(cipher LANGUAGES CXX)

# Platform-bağımsız cipher'lar her yerde derlenir.
# CNG (bcrypt) cipher'lar sadece Windows'ta derlenir.
set(CIPHER_COMMON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Cipher/XorCipher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Cipher/CaesarCipher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Cipher/AffineCipher.cpp
)

if(WIN32)
    set(CIPHER_PLATFORM_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/Cipher/CngAES.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cipher/Cng3DES.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cipher/CngAESGcm.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cipher/CngBlockCipher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cipher/CipherAadFallback.cpp
    )
else()
    set(CIPHER_PLATFORM_SOURCES "")
endif()

add_library(cipher STATIC ${CIPHER_COMMON_SOURCES} ${CIPHER_PLATFORM_SOURCES})

target_include_directories(cipher PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Cipher
)

if(WIN32)
    target_link_libraries(cipher PUBLIC bcrypt)
endif()
