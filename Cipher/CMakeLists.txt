cmake_minimum_required(VERSION 3.14)
project(cipher LANGUAGES CXX)

# ── OpenSSL ──────────────────────────────────────────────────────────────────
find_package(OpenSSL REQUIRED)

# ── Kaynaklar (GLOB — dosya ekle/çıkar'da CMake güncellemesi gerekmez) ──────
file(GLOB CIPHER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Cipher/*.cpp")

add_library(cipher STATIC ${CIPHER_SOURCES})

target_include_directories(cipher PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Cipher
)

target_link_libraries(cipher PUBLIC OpenSSL::Crypto utils)
