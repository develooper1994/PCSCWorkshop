cmake_minimum_required(VERSION 3.8)
project(PCSC_workshop1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Collect sources excluding tests
file(GLOB_RECURSE ALL_SRC "*.cpp")
set(SRC_FILES "")
foreach(f ${ALL_SRC})
    if (NOT f MATCHES "/tests/")
        list(APPEND SRC_FILES ${f})
    endif()
endforeach()
file(GLOB_RECURSE HDR_FILES "*.h")

add_executable(PCSC_workshop1 ${SRC_FILES} ${HDR_FILES})

# Link winscard on Windows
if (WIN32)
    target_link_libraries(PCSC_workshop1 PRIVATE Ws2_32)
    target_link_libraries(PCSC_workshop1 PRIVATE winscard)
    target_link_libraries(PCSC_workshop1 PRIVATE bcrypt)
endif()

# Ensure include directories
target_include_directories(PCSC_workshop1 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Tests executable
file(GLOB_RECURSE TEST_SRC "tests/*.cpp")
if(TEST_SRC)
    add_executable(cipher_tests ${TEST_SRC})
    target_include_directories(cipher_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if (WIN32)
        target_link_libraries(cipher_tests PRIVATE bcrypt)
    endif()
    target_compile_definitions(cipher_tests PRIVATE BUILD_CIPHER_TEST_MAIN=1)
endif()
