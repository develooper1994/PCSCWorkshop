cmake_minimum_required(VERSION 3.14)
project(reader_impl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB READER_IMPL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# If top-level reader target exists, include these sources
if(TARGET reader)
    target_sources(reader PRIVATE ${READER_IMPL_SOURCES})
else()
    add_library(reader STATIC ${READER_IMPL_SOURCES})
    target_include_directories(reader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(reader PUBLIC utils cipher)
endif()
