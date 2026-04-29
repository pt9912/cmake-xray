cmake_minimum_required(VERSION 3.20)
project(scale_250 CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE SCALE_SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(scale_250 STATIC ${SCALE_SOURCES})
target_include_directories(scale_250 PRIVATE include)
