cmake_minimum_required(VERSION 3.14)

# Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)
# Windows: static CRT
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

# Test binary
add_executable(scardtool_tests
    test_arg_parser.cpp
    test_card_math.cpp
    test_trailer_format.cpp
    test_access_bits.cpp
    test_hex_utils.cpp
    test_session_file.cpp
    test_script_runner.cpp
    test_commands_dryrun.cpp
    test_script_engine.cpp
    test_catalog.cpp
    test_crypto.cpp
)

target_include_directories(scardtool_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../McpServer
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../Card/Card
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../Reader/Reader
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../Cipher/Cipher
)

target_link_libraries(scardtool_tests PRIVATE
    GTest::gtest_main
    GTest::gmock
    utils
    reader
    nlohmann_json::nlohmann_json
)

if(SCARDTOOL_USE_LINENOISE)
    target_compile_definitions(scardtool_tests PRIVATE SCARDTOOL_USE_LINENOISE)
    target_link_libraries(scardtool_tests PRIVATE linenoise)
    target_include_directories(scardtool_tests PRIVATE
        ${linenoise_ng_SOURCE_DIR}/include
    )
endif()

target_compile_features(scardtool_tests PRIVATE cxx_std_17)

if(WIN32)
    target_compile_definitions(scardtool_tests PRIVATE UNICODE _UNICODE)
endif()

gtest_discover_tests(scardtool_tests)
