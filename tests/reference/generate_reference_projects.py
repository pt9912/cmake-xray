#!/usr/bin/env python3

from __future__ import annotations

import json
import shutil
from pathlib import Path


ROOT = Path(__file__).resolve().parent
SCALES = (250, 500, 1000)


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def source_contents(index: int) -> str:
    return (
        '#include "common/config.h"\n\n'
        f"int unit_{index:04d}() {{ return config_value() + {index}; }}\n"
    )


def compile_entry(index: int) -> dict[str, object]:
    stem = f"unit_{index:04d}"
    return {
        "directory": f"build/{stem}",
        "arguments": [
            "clang++",
            "-I../../include",
            "-c",
            f"../../src/{stem}.cpp",
            "-o",
            f"{stem}.o",
        ],
        "file": f"../../src/{stem}.cpp",
    }


def cmake_lists_contents(scale: int) -> str:
    # AP M5-1.8 A.3: each scale fixture also serves as a CMake source tree
    # so generate_file_api_fixtures.sh can run `cmake -B build` and produce
    # versioned File-API reply data without touching the host toolchain
    # beyond CMake itself. The library target wires every unit_NNNN.cpp
    # under src/ so the resulting codemodel reply contains the same TU
    # set as the hand-built compile_commands.json above. Compile is not
    # invoked; the configure step is the only mandatory operation.
    return (
        "cmake_minimum_required(VERSION 3.20)\n"
        f"project(scale_{scale} CXX)\n"
        "\n"
        "set(CMAKE_CXX_STANDARD 20)\n"
        "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
        "set(CMAKE_CXX_EXTENSIONS OFF)\n"
        "\n"
        "file(GLOB_RECURSE SCALE_SOURCES CONFIGURE_DEPENDS src/*.cpp)\n"
        f"add_library(scale_{scale} STATIC ${{SCALE_SOURCES}})\n"
        f"target_include_directories(scale_{scale} PRIVATE include)\n"
    )


def generate_scale(scale: int) -> None:
    target = ROOT / f"scale_{scale}"
    if target.exists():
        shutil.rmtree(target)

    write_text(target / "include/common/shared.h", "#pragma once\n\ninline int shared_value() { return 1; }\n")
    write_text(
        target / "include/common/config.h",
        '#pragma once\n\n#include "common/shared.h"\n\ninline int config_value() { return shared_value(); }\n',
    )

    for index in range(1, scale + 1):
        write_text(target / "src" / f"unit_{index:04d}.cpp", source_contents(index))

    compile_commands = [compile_entry(index) for index in range(1, scale + 1)]
    write_text(target / "compile_commands.json", json.dumps(compile_commands, indent=2) + "\n")

    # AP M5-1.8 A.3: CMakeLists for the File-API generation pipeline. The
    # codemodel-v2 query is intentionally not committed under
    # .cmake/api/v1/query/; tests/reference/generate_file_api_fixtures.sh
    # places the build-tree stateless query before each configure run so
    # the source tree stays clean and the regen path is the single source
    # of truth for query semantics.
    write_text(target / "CMakeLists.txt", cmake_lists_contents(scale))


def main() -> None:
    for scale in SCALES:
        generate_scale(scale)


if __name__ == "__main__":
    main()
