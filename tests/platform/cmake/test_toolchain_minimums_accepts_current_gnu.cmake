# AP M5-1.7 Tranche A follow-up: positive smoke for the toolchain-minimums
# fail-fast gate. Fakes a current GNU 13.3.0 toolchain against the real
# tests/platform/toolchain-minimums.json and includes the production module
# in script mode. Exits 0 when the gate accepts the (current) versions; any
# nonzero exit is an unexpected regression in the gate logic.

cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_COMPILER_ID "GNU")
set(CMAKE_CXX_COMPILER_VERSION "13.3.0")

include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/ToolchainMinimums.cmake")
