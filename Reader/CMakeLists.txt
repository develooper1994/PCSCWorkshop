cmake_minimum_required(VERSION 3.14)
project(reader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Collect reader implementation sources
file(GLOB_READER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Reader/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/Reader/*/*.cpp")

# Fallback: if READER_SOURCES found, create static lib
if(READER_SOURCES)
    add_library(reader STATIC ${READER_SOURCES})
    target_include_directories(reader PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Reader
    )
    target_link_libraries(reader PUBLIC utils cipher)
else()
    # Try more specific glob
    file(GLOB READER_IMPL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Reader/Reader/*.cpp")
    add_library(reader STATIC ${READER_IMPL_SOURCES})
    target_include_directories(reader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Reader)
    target_link_libraries(reader PUBLIC utils cipher)
endif()
