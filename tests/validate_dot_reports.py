#!/usr/bin/env python3
"""DOT syntax validator for cmake-xray reports (AP M5-1.3 Tranche A).

This is the native-CI fallback for the Graphviz `dot -Tsvg` gate. It uses
only the Python standard library and validates Graphviz-DOT structure plus
the cmake-xray-specific contract (digraph name, mandatory graph attributes,
quoted-string escape rules, balanced braces, attribute list syntax).

The script is offline: no network requests, no external dependencies. It
mirrors the manifest/parity checks of tests/validate_json_schema.py so the
DOT and JSON gates feel uniform.

Usage:
    python3 tests/validate_dot_reports.py \\
        --reports-dir tests/e2e/testdata/m5/dot-reports \\
        --manifest tests/e2e/testdata/m5/dot-reports/manifest.txt

    python3 tests/validate_dot_reports.py --validate-file path/to/output.dot

Exit codes:
    0  All checks passed.
    1  Validation failed.
    2  Environment error (missing inputs, unreadable files).
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

EXIT_OK = 0
EXIT_VALIDATION_FAILED = 1
EXIT_ENVIRONMENT_ERROR = 2

# cmake-xray-specific attributes that every DOT report must carry. The
# Graphviz path proves syntactic validity via dot -Tsvg; the contract-specific
# attribute checks happen here so both gates enforce the same expectations.
REQUIRED_GRAPH_ATTRIBUTES = (
    "xray_report_type",
    "format_version",
    "graph_node_limit",
    "graph_edge_limit",
    "graph_truncated",
)

ALLOWED_GRAPH_NAMES = ("cmake_xray_analysis", "cmake_xray_impact")

DIGRAPH_HEADER_RE = re.compile(r"^\s*digraph\s+(\w+)\s*\{")


def read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except FileNotFoundError:
        sys.stderr.write(f"error: file not found: {path}\n")
        sys.exit(EXIT_ENVIRONMENT_ERROR)
    except OSError as exc:
        sys.stderr.write(f"error: cannot read {path}: {exc}\n")
        sys.exit(EXIT_ENVIRONMENT_ERROR)


def parse_manifest(manifest_path: Path) -> list[str]:
    # Whole-line semantics: blank or whitespace-only lines are skipped, lines
    # starting with '#' are comments, everything else is a bare filename.
    if not manifest_path.exists():
        sys.stderr.write(f"error: manifest not found: {manifest_path}\n")
        sys.exit(EXIT_ENVIRONMENT_ERROR)
    entries: list[str] = []
    for raw_line in manifest_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        entries.append(line)
    return entries


def list_dot_files(reports_dir: Path) -> list[str]:
    if not reports_dir.is_dir():
        sys.stderr.write(f"error: reports directory not found: {reports_dir}\n")
        sys.exit(EXIT_ENVIRONMENT_ERROR)
    return sorted(p.name for p in reports_dir.iterdir() if p.is_file()
                  and p.suffix == ".dot")


def diff_manifest_directory(manifest_entries: list[str],
                             dot_files: list[str]) -> int:
    manifest_set = set(manifest_entries)
    files_set = set(dot_files)
    missing_on_disk = sorted(manifest_set - files_set)
    missing_in_manifest = sorted(files_set - manifest_set)

    if not missing_on_disk and not missing_in_manifest:
        return EXIT_OK
    if missing_on_disk:
        sys.stderr.write("error: manifest entries without matching DOT file:\n")
        for name in missing_on_disk:
            sys.stderr.write(f"  - {name}\n")
    if missing_in_manifest:
        sys.stderr.write("error: DOT files not listed in manifest:\n")
        for name in missing_in_manifest:
            sys.stderr.write(f"  - {name}\n")
    return EXIT_VALIDATION_FAILED


def validate_graph_header(text: str) -> tuple[int, str | None]:
    match = DIGRAPH_HEADER_RE.search(text)
    if match is None:
        return EXIT_VALIDATION_FAILED, "missing 'digraph <name> {' header"
    name = match.group(1)
    if name not in ALLOWED_GRAPH_NAMES:
        allowed = ", ".join(ALLOWED_GRAPH_NAMES)
        return EXIT_VALIDATION_FAILED, f"unknown digraph name '{name}'; expected one of {allowed}"
    return EXIT_OK, None


def validate_balanced_braces(text: str) -> tuple[int, str | None]:
    depth = 0
    in_string = False
    in_line_comment = False
    in_block_comment = False
    prev_char = ""
    for ch in text:
        if in_line_comment:
            if ch == "\n":
                in_line_comment = False
            prev_char = ch
            continue
        if in_block_comment:
            if prev_char == "*" and ch == "/":
                in_block_comment = False
            prev_char = ch
            continue
        if in_string:
            if ch == '"' and prev_char != "\\":
                in_string = False
            prev_char = ch
            continue
        if ch == '"':
            in_string = True
            prev_char = ch
            continue
        if ch == "/" and prev_char == "/":
            in_line_comment = True
            prev_char = ""
            continue
        if ch == "*" and prev_char == "/":
            in_block_comment = True
            prev_char = ""
            continue
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth < 0:
                return EXIT_VALIDATION_FAILED, "unbalanced '}' before matching '{'"
        prev_char = ch
    if in_string:
        return EXIT_VALIDATION_FAILED, "unterminated quoted string"
    if depth != 0:
        return EXIT_VALIDATION_FAILED, f"unbalanced braces (final depth {depth})"
    return EXIT_OK, None


def validate_string_escapes(text: str) -> tuple[int, str | None]:
    # Inside quoted strings, every literal control character must be a
    # documented escape (\\, \", \n, \r, \t, \xHH). Raw control characters are
    # contract violations.
    in_string = False
    prev_char = ""
    for index, ch in enumerate(text):
        if in_string:
            if ch == '"' and prev_char != "\\":
                in_string = False
            elif ord(ch) < 0x20:
                return EXIT_VALIDATION_FAILED, (
                    f"raw control character (0x{ord(ch):02X}) inside quoted string at offset {index}")
        else:
            if ch == '"':
                in_string = True
        prev_char = ch
    return EXIT_OK, None


def validate_required_graph_attributes(text: str) -> tuple[int, str | None]:
    # Every required attribute must appear at least once outside a string.
    # We use a simple state machine that tracks whether we are inside a quoted
    # string and accumulates non-string-context text for substring matches.
    out: list[str] = []
    in_string = False
    prev_char = ""
    for ch in text:
        if in_string:
            if ch == '"' and prev_char != "\\":
                in_string = False
        else:
            if ch == '"':
                in_string = True
            else:
                out.append(ch)
        prev_char = ch
    body = "".join(out)
    missing = [name for name in REQUIRED_GRAPH_ATTRIBUTES if name not in body]
    if missing:
        return EXIT_VALIDATION_FAILED, (
            "missing required graph attribute(s): " + ", ".join(missing))
    return EXIT_OK, None


def validate_dot_file(path: Path) -> int:
    text = read_text(path)
    final_status = EXIT_OK
    for check in (validate_graph_header, validate_balanced_braces,
                  validate_string_escapes, validate_required_graph_attributes):
        status, message = check(text)
        if status != EXIT_OK:
            sys.stderr.write(f"error: {path}: {message}\n")
            final_status = status
    return final_status


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate cmake-xray DOT report syntax and contract.")
    parser.add_argument("--reports-dir", type=Path,
                        help="Directory of DOT report goldens.")
    parser.add_argument("--manifest", type=Path,
                        help="Manifest file listing each golden by name.")
    parser.add_argument("--validate-file", type=Path, action="append",
                        default=[],
                        help="Validate a single DOT file. Repeatable. Used by "
                             "run_e2e.sh to syntax-check the binary's actually "
                             "produced DOT output.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if args.validate_file:
        final_status = EXIT_OK
        for path in args.validate_file:
            status = validate_dot_file(path)
            if status != EXIT_OK:
                final_status = status
        return final_status

    if args.reports_dir is None or args.manifest is None:
        sys.stderr.write(
            "error: --reports-dir and --manifest are required unless "
            "--validate-file is set\n")
        return EXIT_ENVIRONMENT_ERROR

    manifest_entries = parse_manifest(args.manifest)
    dot_files = list_dot_files(args.reports_dir)
    parity_status = diff_manifest_directory(manifest_entries, dot_files)
    if parity_status != EXIT_OK:
        return parity_status

    if not manifest_entries:
        return EXIT_OK

    final_status = EXIT_OK
    for entry in manifest_entries:
        path = args.reports_dir / entry
        status = validate_dot_file(path)
        if status != EXIT_OK:
            final_status = status
    return final_status


if __name__ == "__main__":
    sys.exit(main())
