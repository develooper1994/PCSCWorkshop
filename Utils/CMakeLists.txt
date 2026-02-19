cmake_minimum_required(VERSION 3.14)
project(utils LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Collect sources (Utils.cpp is the only .cpp; the rest are header-only)
file(GLOB UTILS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Utils/*.cpp")
add_library(utils STATIC ${UTILS_SOURCES})

# The utils headers use wide-string Win32 APIs (SCardConnect, SCardListReadersW …)
# so UNICODE / _UNICODE must be defined.
target_compile_definitions(utils PUBLIC UNICODE _UNICODE)

# Public include directories so consumers can #include "PcscUtils.h" etc.
target_include_directories(utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Utils
)

# On Windows the utils headers use winscard
if(WIN32)
    target_link_libraries(utils PUBLIC winscard)
endif()
