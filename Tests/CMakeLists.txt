cmake_minimum_required(VERSION 3.14)
project(tests LANGUAGES CXX)

file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# Utils içindeki test helper'ları da dahil et
file(GLOB_RECURSE UTILS_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../Utils/Utils/Tests/*.cpp")
list(APPEND TEST_SOURCES ${UTILS_TEST_SOURCES})

add_executable(PCSC_tests ${TEST_SOURCES})

target_include_directories(PCSC_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../Utils/Utils/Tests/ACR1281U
    ${CMAKE_CURRENT_SOURCE_DIR}/../Reader/Reader/ACR1281U
)

target_link_libraries(PCSC_tests PRIVATE card reader cipher utils)
