cmake_minimum_required(VERSION 3.20)

project(cmake-xray VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

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
