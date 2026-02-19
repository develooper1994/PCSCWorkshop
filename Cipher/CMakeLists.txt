cmake_minimum_required(VERSION 3.14)
project(cipher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Collect cipher implementation sources
file(GLOB CIPHER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Cipher/*.cpp")
add_library(cipher STATIC ${CIPHER_SOURCES})

# Public include directories so users can include headers from the cipher project
target_include_directories(cipher PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Cipher
)

# On Windows the cipher implementation uses bcrypt APIs for some algorithms
if(WIN32)
    target_link_libraries(cipher PUBLIC bcrypt)
endif()
