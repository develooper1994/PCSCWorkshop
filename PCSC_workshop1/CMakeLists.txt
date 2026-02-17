cmake_minimum_required(VERSION 3.8)
project(PCSC_workshop1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources
file(GLOB_RECURSE SRC_FILES "*.cpp")
file(GLOB_RECURSE HDR_FILES "*.h")

add_executable(PCSC_workshop1 ${SRC_FILES} ${HDR_FILES})

# Link winscard on Windows
if (WIN32)
    target_link_libraries(PCSC_workshop1 PRIVATE Ws2_32)
    target_link_libraries(PCSC_workshop1 PRIVATE winscard)
endif()

# Ensure include directories
target_include_directories(PCSC_workshop1 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
