cmake_minimum_required(VERSION 3.14)
project(PCSC_workshop1 LANGUAGES CXX)

file(GLOB WORKSHOP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# Utils içindeki test helper'ları (testACR1281UReaderMifareClassicUnsecured vb.)
file(GLOB_RECURSE UTILS_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../Utils/Utils/Tests/*.cpp")
list(APPEND WORKSHOP_SOURCES ${UTILS_TEST_SOURCES})

add_executable(PCSC_workshop1 ${WORKSHOP_SOURCES})

target_include_directories(PCSC_workshop1 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../Utils/Utils/Tests/ACR1281U
    ${CMAKE_CURRENT_SOURCE_DIR}/../Reader/Reader/ACR1281U
)

target_link_libraries(PCSC_workshop1 PRIVATE card reader cipher utils)
