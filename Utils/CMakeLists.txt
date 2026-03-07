cmake_minimum_required(VERSION 3.14)
project(utils LANGUAGES CXX)

file(GLOB_RECURSE UTILS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Utils/*.cpp")
# Tests/ içindeki dosyalar Reader'a bağımlı — döngüsel bağımlılık kaçınmak için
# Utils kütüphanesinden hariç tutulur; Tests target'ına dahildir.
list(FILTER UTILS_SOURCES EXCLUDE REGEX ".*/Tests/.*")
add_library(utils STATIC ${UTILS_SOURCES})

target_compile_definitions(utils PUBLIC UNICODE _UNICODE)

target_include_directories(utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Utils
)

if(WIN32)
    target_link_libraries(utils PUBLIC winscard)
else()
    # Linux / macOS: pcsclite
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PCSCLITE QUIET libpcsclite)
    endif()
    if(PCSCLITE_FOUND)
        target_include_directories(utils PUBLIC ${PCSCLITE_INCLUDE_DIRS})
        target_link_libraries(utils PUBLIC ${PCSCLITE_LIBRARIES})
    else()
        # Fallback: elle ara
        find_path(PCSCLITE_INCLUDE NAMES PCSC/winscard.h
                  PATHS /usr/include /usr/local/include)
        find_library(PCSCLITE_LIB NAMES pcsclite
                     PATHS /usr/lib /usr/local/lib)
        if(PCSCLITE_INCLUDE AND PCSCLITE_LIB)
            target_include_directories(utils PUBLIC ${PCSCLITE_INCLUDE})
            target_link_libraries(utils PUBLIC ${PCSCLITE_LIB})
        else()
            message(WARNING "pcsclite not found — PCSC functions will not link")
        endif()
    endif()
endif()
