# AP M5-1.7 Tranche A: fail-fast toolchain version gate.
#
# Reads tests/platform/toolchain-minimums.json (the single source of truth
# for CMake and per-compiler minimum versions) and aborts CMake configure
# with FATAL_ERROR when the host's CMake or C++ compiler is below the
# documented minimum. ClangCL is intentionally not a separate key: it
# reports as either "Clang" or "MSVC" via CMAKE_CXX_COMPILER_ID, and the
# corresponding compiler_minimums entry applies — see plan-M5-1-7.md
# "CMake-/Compiler-Kompatibilitaet".

set(_xray_toolchain_minimums_json
    "${CMAKE_SOURCE_DIR}/tests/platform/toolchain-minimums.json")

if(NOT EXISTS "${_xray_toolchain_minimums_json}")
    message(FATAL_ERROR
        "AP M5-1.7 toolchain minimums file is missing: "
        "${_xray_toolchain_minimums_json}. The native platform contract "
        "requires this file as the single source of truth.")
endif()

file(READ "${_xray_toolchain_minimums_json}" _xray_toolchain_minimums_text)

string(JSON _xray_required_cmake_version
    GET "${_xray_toolchain_minimums_text}" "cmake" "minimum_version")

# Cross-check against cmake_minimum_required(VERSION ...): the project's
# documented minimum and the JSON minimum must agree, otherwise one of
# the two has drifted and the documentation chain (README, guide.md,
# CMakeLists.txt) will silently mismatch.
if(NOT CMAKE_MINIMUM_REQUIRED_VERSION VERSION_EQUAL "${_xray_required_cmake_version}")
    message(FATAL_ERROR
        "cmake_minimum_required(VERSION ${CMAKE_MINIMUM_REQUIRED_VERSION}) "
        "disagrees with toolchain-minimums.json cmake.minimum_version="
        "${_xray_required_cmake_version}. Update both together so the "
        "documented minimum stays aligned across CMakeLists.txt, README.md, "
        "docs/guide.md and the JSON contract.")
endif()

if(CMAKE_VERSION VERSION_LESS "${_xray_required_cmake_version}")
    message(FATAL_ERROR
        "Host CMake ${CMAKE_VERSION} is below the AP M5-1.7 minimum "
        "${_xray_required_cmake_version}. Upgrade CMake before running "
        "configure on this platform.")
endif()

string(JSON _xray_compiler_minimum
    ERROR_VARIABLE _xray_compiler_minimum_error
    GET "${_xray_toolchain_minimums_text}"
        "compiler_minimums" "${CMAKE_CXX_COMPILER_ID}")

if(_xray_compiler_minimum_error)
    message(FATAL_ERROR
        "No compiler_minimums entry for CMAKE_CXX_COMPILER_ID="
        "${CMAKE_CXX_COMPILER_ID} in ${_xray_toolchain_minimums_json}. "
        "Add the entry (matching CMake's COMPILER_ID exactly) or use a "
        "supported toolchain. JSON parser said: ${_xray_compiler_minimum_error}")
endif()

if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "${_xray_compiler_minimum}")
    message(FATAL_ERROR
        "Host ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} is "
        "below the AP M5-1.7 minimum ${_xray_compiler_minimum}. Upgrade "
        "the C++ compiler or pin a newer toolchain before configuring.")
endif()

message(STATUS
    "AP M5-1.7 toolchain check: cmake ${CMAKE_VERSION} (>= "
    "${_xray_required_cmake_version}), ${CMAKE_CXX_COMPILER_ID} "
    "${CMAKE_CXX_COMPILER_VERSION} (>= ${_xray_compiler_minimum})")

unset(_xray_toolchain_minimums_json)
unset(_xray_toolchain_minimums_text)
unset(_xray_required_cmake_version)
unset(_xray_compiler_minimum)
unset(_xray_compiler_minimum_error)
