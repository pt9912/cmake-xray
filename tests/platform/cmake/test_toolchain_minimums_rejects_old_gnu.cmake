# AP M5-1.7 Tranche A follow-up: negative smoke for the toolchain-minimums
# fail-fast gate. Fakes a too-old GNU 9 toolchain against the real
# tests/platform/toolchain-minimums.json (GNU minimum 10) and includes the
# production module in script mode. The module MUST FATAL_ERROR; CTest
# inverts the result via WILL_FAIL so a clean exit would surface as a
# regression.

cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_COMPILER_ID "GNU")
set(CMAKE_CXX_COMPILER_VERSION "9")

include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/ToolchainMinimums.cmake")
