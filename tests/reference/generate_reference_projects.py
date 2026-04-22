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


def main() -> None:
    for scale in SCALES:
        generate_scale(scale)


if __name__ == "__main__":
    main()
