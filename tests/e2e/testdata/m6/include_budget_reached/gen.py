#!/usr/bin/env python3
"""Regenerate the include_budget_reached e2e fixture.

The fixture exists to exercise the
``include_node_budget_reached=true`` path end-to-end in the binary
output for all five report formats. The hardcoded budget per
docs/planning/done/plan-M6-1-4.md §75-77 is 10000 distinct header
traversal nodes per analyze run, so the fixture ships
``HEADER_COUNT = 10001`` flat headers (one above the budget) plus a
single translation unit that includes every one of them. Each header
has exactly one affected TU, which is below the
``affected_translation_units.size() >= 2`` hotspot threshold from
src/hexagon/services/analysis_support.cpp; the rendered reports
therefore show an empty hotspot list plus the mandatory budget note,
which keeps the goldens compact even though the fixture has 10000+
files.

Run from the repository root:

    python3 tests/e2e/testdata/m6/include_budget_reached/gen.py
"""

from __future__ import annotations

from pathlib import Path

HEADER_COUNT = 10001
FIXTURE_ROOT = Path(__file__).resolve().parent
INCLUDE_DIR = FIXTURE_ROOT / "include"
SRC_DIR = FIXTURE_ROOT / "src"


def write_headers() -> None:
    INCLUDE_DIR.mkdir(parents=True, exist_ok=True)
    for index in range(1, HEADER_COUNT + 1):
        path = INCLUDE_DIR / f"h{index:05d}.h"
        path.write_text("#pragma once\n", encoding="utf-8")


def write_source() -> None:
    SRC_DIR.mkdir(parents=True, exist_ok=True)
    lines = [f'#include "h{index:05d}.h"' for index in range(1, HEADER_COUNT + 1)]
    (SRC_DIR / "main.cpp").write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_compile_commands() -> None:
    payload = (
        "[\n"
        '  {\n'
        '    "directory": "build",\n'
        '    "command": "clang++ -I../include -c ../src/main.cpp -o main.o",\n'
        '    "file": "../src/main.cpp"\n'
        "  }\n"
        "]\n"
    )
    (FIXTURE_ROOT / "compile_commands.json").write_text(payload, encoding="utf-8")


def main() -> None:
    write_headers()
    write_source()
    write_compile_commands()


if __name__ == "__main__":
    main()
