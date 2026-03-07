cmake_minimum_required(VERSION 3.14)
project(reader LANGUAGES CXX)

file(GLOB_RECURSE READER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Reader/*.cpp"
)

add_library(reader STATIC ${READER_SOURCES})

target_include_directories(reader PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Reader
    ${CMAKE_CURRENT_SOURCE_DIR}/../Card/Card
)

target_link_libraries(reader PUBLIC utils cipher)
