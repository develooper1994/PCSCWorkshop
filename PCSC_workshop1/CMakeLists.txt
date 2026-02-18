cmake_minimum_required(VERSION 3.8)
project(PCSC_workshop1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# cipher library
# -----------------------------------------------------------------------------
file(GLOB CIPHER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Cipher/*.cpp")
add_library(cipher STATIC ${CIPHER_SOURCES})
# Public include dirs so users of the library can include headers
target_include_directories(cipher PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Cipher
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

# -----------------------------------------------------------------------------
# application executable (sources excluding Cipher/ and tests/)
# -----------------------------------------------------------------------------
file(GLOB_RECURSE ALL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
set(APP_SOURCES "")
foreach(f ${ALL_SRC})
    if (NOT f MATCHES "/tests/" AND NOT f MATCHES "/Cipher/")
        list(APPEND APP_SOURCES ${f})
    endif()
endforeach()

add_executable(PCSC_workshop1 ${APP_SOURCES})
# Link the cipher library
target_link_libraries(PCSC_workshop1 PRIVATE cipher)

# Link platform libs on Windows
if (WIN32)
    target_link_libraries(PCSC_workshop1 PRIVATE Ws2_32)
    target_link_libraries(PCSC_workshop1 PRIVATE winscard)
    target_link_libraries(PCSC_workshop1 PRIVATE bcrypt)
endif()

# Ensure include directories for the app
target_include_directories(PCSC_workshop1 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# -----------------------------------------------------------------------------
# tests executable
# -----------------------------------------------------------------------------
file(GLOB TEST_SRC "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
if (TEST_SRC)
    add_executable(cipher_tests ${TEST_SRC})
    target_include_directories(cipher_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    # link cipher library
    target_link_libraries(cipher_tests PRIVATE cipher)
    # define test main guard so tests file provides main when built as test binary
    target_compile_definitions(cipher_tests PRIVATE BUILD_CIPHER_TEST_MAIN=1)
    if (WIN32)
        target_link_libraries(cipher_tests PRIVATE bcrypt)
    endif()
    enable_testing()
    add_test(NAME cipher_tests COMMAND cipher_tests)
endif()
