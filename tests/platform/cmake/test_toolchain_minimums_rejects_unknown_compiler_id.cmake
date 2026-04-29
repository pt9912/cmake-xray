# AP M5-1.7 Tranche A follow-up: negative smoke for the toolchain-minimums
# fail-fast gate. Fakes a CMAKE_CXX_COMPILER_ID that is not present in
# tests/platform/toolchain-minimums.json (e.g. "Foo") and includes the
# production module in script mode. The module's string(JSON ... GET ...)
# call MUST surface a JSON-key-missing error and FATAL_ERROR; CTest
# inverts the result via WILL_FAIL.

cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_COMPILER_ID "Foo")
set(CMAKE_CXX_COMPILER_VERSION "1.0")

include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/ToolchainMinimums.cmake")
