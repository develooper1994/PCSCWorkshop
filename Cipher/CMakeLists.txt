cmake_minimum_required(VERSION 3.8)
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

# If the PCSC_workshop1 utils headers are available adjacent to this project (when
# built as a subdirectory), expose them so legacy relative includes resolve.
set(PCSC_WORKSHOP_UTIL_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/../PCSC_workshop1/utils/PcscUtils.h")
if(EXISTS ${PCSC_WORKSHOP_UTIL_HEADER})
    target_include_directories(cipher PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../PCSC_workshop1
        ${CMAKE_CURRENT_SOURCE_DIR}/../PCSC_workshop1/utils
    )
endif()

# On Windows the cipher implementation uses bcrypt APIs for some algorithms
if (WIN32)
    target_link_libraries(cipher PUBLIC bcrypt)
endif()
