cmake_minimum_required(VERSION 3.20)

project(cmake-xray VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

# AP M5-1.6 Tranche A: app version sources. The published app version is the
# canonical CLI/Release version that flows into application_info() and into
# `cmake-xray --version`. Two cache variables drive it:
#   - XRAY_APP_VERSION  is an explicit override; the CI release job derives
#     it from the validated tag without the leading 'v'
#     (see scripts/validate-release-tag.sh).
#   - XRAY_VERSION_SUFFIX is appended to PROJECT_VERSION when XRAY_APP_VERSION
#     is empty; useful for local non-release builds (e.g. "-dev").
# Setting both at the same time is fail-fast because two competing version
# sources are a bug magnet (see plan-M5-1-6.md "Projekt- und App-Version").
set(XRAY_APP_VERSION ""
    CACHE STRING
    "Override of the published app version (without leading 'v'). When empty, falls back to PROJECT_VERSION+XRAY_VERSION_SUFFIX.")
set(XRAY_VERSION_SUFFIX ""
    CACHE STRING
    "Suffix appended to PROJECT_VERSION when XRAY_APP_VERSION is empty (e.g. '-dev').")

if(XRAY_APP_VERSION AND XRAY_VERSION_SUFFIX)
    message(FATAL_ERROR
        "XRAY_APP_VERSION and XRAY_VERSION_SUFFIX must not both be set. "
        "Two competing version sources are a bug magnet: choose one. "
        "Set XRAY_APP_VERSION for tagged release builds (Tranche A); "
        "set XRAY_VERSION_SUFFIX for local non-release builds.")
endif()

if(XRAY_APP_VERSION)
    set(XRAY_RESOLVED_APP_VERSION "${XRAY_APP_VERSION}")
else()
    set(XRAY_RESOLVED_APP_VERSION "${PROJECT_VERSION}${XRAY_VERSION_SUFFIX}")
endif()

# The resolved app version flows into application_info() via a compile-time
# definition; using #define avoids any runtime file lookups and keeps the CLI
# --version output stable across the binary lifetime. add_compile_definitions
# applies the macro globally so every TU (src/, tests/) sees the same value.
add_compile_definitions(XRAY_APP_VERSION_STRING="${XRAY_RESOLVED_APP_VERSION}")

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(XRAY_ENABLE_COVERAGE "Enable coverage instrumentation for project targets" OFF)

function(xray_enable_coverage target_name)
    if(NOT XRAY_ENABLE_COVERAGE)
        return()
    endif()

    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "XRAY_ENABLE_COVERAGE requires a GNU- or Clang-compatible compiler")
    endif()

    target_compile_options(${target_name} PRIVATE -O0 -g --coverage)
    target_link_options(${target_name} PRIVATE --coverage)
endfunction()

include(CTest)
include(cmake/Dependencies.cmake)

add_subdirectory(src)
add_subdirectory(tests)
