cmake_minimum_required(VERSION 3.14)
project(card LANGUAGES CXX)

file(GLOB_RECURSE CARD_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Card/*.cpp"
)

add_library(card STATIC ${CARD_SOURCES})

target_include_directories(card PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Card
)

target_link_libraries(card PUBLIC reader cipher utils)
