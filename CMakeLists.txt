cmake_minimum_required(VERSION 3.20)

project(cmake-xray VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

add_compile_options(-Wall -Wextra -Wpedantic)

include(CTest)
include(cmake/Dependencies.cmake)

add_subdirectory(src)
add_subdirectory(tests)
