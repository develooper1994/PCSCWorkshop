cmake_minimum_required(VERSION 3.14)
project(tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Collect all test sources
file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(PCSC_tests ${TEST_SOURCES})

target_include_directories(PCSC_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Tests depend on Reader, Cipher and Utils.
# Include dirs and platform libs propagate transitively.
target_link_libraries(PCSC_tests PRIVATE reader cipher utils)
