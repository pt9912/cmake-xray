#!/usr/bin/env python3
"""Verbosity manifest/directory parity validator (AP M5-1.5 Tranche B.1).

Stdlib-only validator that mirrors validate_dot_reports.py and
validate_html_reports.py for the tests/e2e/testdata/m5/verbosity/ goldens.
The validator enforces:

  - manifest entries and directory contents agree beidseitig (every entry
    on disk is listed and vice versa)
  - each golden begins with one of the documented Quiet/Verbose anchor
    lines (`analysis: ok`, `impact: ok`, `verbose analysis summary`,
    `verbose impact summary`)
  - each golden ends with exactly one trailing newline
  - no doubled newlines (`\\n\\n`) appear in the body, which would surface
    a renderer that accidentally appends `"\\n\\n"` instead of `"\\n"`

The script mirrors the AP-1.3/1.4 validator pattern: stdlib-only,
manifest+content checks, batch and single-file modes, no network.

Usage:
    python3 tests/validate_verbosity_reports.py \\
        --reports-dir tests/e2e/testdata/m5/verbosity \\
        --manifest tests/e2e/testdata/m5/verbosity/manifest.txt

    python3 tests/validate_verbosity_reports.py --validate-file path/to/output.txt

Exit codes:
    0  All checks passed.
    1  Validation failed.
    2  Environment error (missing inputs, unreadable files).
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

EXIT_OK = 0
EXIT_VALIDATION_FAILED = 1
EXIT_ENVIRONMENT_ERROR = 2

# Documented anchor lines a verbosity golden must start with. Quiet uses the
# `analysis: ok` / `impact: ok` summary line; Verbose (Tranche B.2) uses the
# section header `verbose analysis summary` / `verbose impact summary`.
ALLOWED_ANCHORS = (
    "analysis: ok",
    "impact: ok",
    "verbose analysis summary",
    "verbose impact summary",
)


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


def list_txt_files(reports_dir: Path) -> list[str]:
    # The manifest itself lives next to the goldens and uses .txt as well.
    # Excluding it keeps the parity check focused on golden files only.
    if not reports_dir.is_dir():
        sys.stderr.write(f"error: reports directory not found: {reports_dir}\n")
        sys.exit(EXIT_ENVIRONMENT_ERROR)
    return sorted(p.name for p in reports_dir.iterdir()
                  if p.is_file() and p.suffix == ".txt"
                  and p.name != "manifest.txt")


def diff_manifest_directory(manifest_entries: list[str],
                             txt_files: list[str]) -> int:
    manifest_set = set(manifest_entries)
    files_set = set(txt_files)
    missing_on_disk = sorted(manifest_set - files_set)
    missing_in_manifest = sorted(files_set - manifest_set)
    if not missing_on_disk and not missing_in_manifest:
        return EXIT_OK
    if missing_on_disk:
        sys.stderr.write("error: manifest entries without matching golden file:\n")
        for name in missing_on_disk:
            sys.stderr.write(f"  - {name}\n")
    if missing_in_manifest:
        sys.stderr.write("error: golden files not listed in manifest:\n")
        for name in missing_in_manifest:
            sys.stderr.write(f"  - {name}\n")
    return EXIT_VALIDATION_FAILED


def validate_anchor(text: str) -> tuple[int, str | None]:
    if not text:
        return EXIT_VALIDATION_FAILED, "empty file"
    first_line = text.splitlines()[0]
    if not any(first_line == anchor for anchor in ALLOWED_ANCHORS):
        return EXIT_VALIDATION_FAILED, (
            f"first line {first_line!r} is not a documented anchor "
            f"({', '.join(repr(a) for a in ALLOWED_ANCHORS)})")
    return EXIT_OK, None


def validate_trailing_newline(text: str) -> tuple[int, str | None]:
    if not text.endswith("\n"):
        return EXIT_VALIDATION_FAILED, "file does not end with a newline"
    return EXIT_OK, None


def validate_no_doubled_newlines_for_quiet(text: str) -> tuple[int, str | None]:
    # Quiet emitters produce exactly one newline per line, including the
    # last. Verbose emitters use blank lines between sections, so the rule
    # is intentionally Quiet-only. The anchor line decides the mode.
    if not text:
        return EXIT_OK, None
    first_line = text.splitlines()[0]
    if first_line not in ("analysis: ok", "impact: ok"):
        return EXIT_OK, None  # Verbose: blank-line separators allowed.
    if "\n\n" in text:
        return EXIT_VALIDATION_FAILED, (
            "doubled newline ('\\n\\n') found in a Quiet golden; Quiet "
            "renderers emit exactly one newline between every line")
    return EXIT_OK, None


def validate_verbose_section_separators(text: str) -> tuple[int, str | None]:
    # Verbose emitters separate sections with a single blank line. Triple
    # newlines (\n\n\n) would surface a renderer that accidentally emits
    # two blank lines, which would break the documented section layout.
    if not text:
        return EXIT_OK, None
    first_line = text.splitlines()[0]
    if first_line not in ("verbose analysis summary", "verbose impact summary"):
        return EXIT_OK, None  # Quiet: separator rule does not apply.
    if "\n\n\n" in text:
        return EXIT_VALIDATION_FAILED, (
            "triple newline ('\\n\\n\\n') found in a Verbose golden; "
            "Verbose sections are separated by a single blank line only")
    return EXIT_OK, None


CHECKS = (
    validate_anchor,
    validate_trailing_newline,
    validate_no_doubled_newlines_for_quiet,
    validate_verbose_section_separators,
)


def validate_txt_file(path: Path) -> int:
    text = read_text(path)
    final_status = EXIT_OK
    for check in CHECKS:
        status, message = check(text)
        if status != EXIT_OK:
            sys.stderr.write(f"error: {path}: {message}\n")
            final_status = status
    return final_status


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate cmake-xray Quiet/Verbose console goldens.")
    parser.add_argument("--reports-dir", type=Path,
                        help="Directory of verbosity goldens.")
    parser.add_argument("--manifest", type=Path,
                        help="Manifest file listing each golden by name.")
    parser.add_argument("--validate-file", type=Path, action="append",
                        default=[],
                        help="Validate a single golden file. Repeatable.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if args.validate_file:
        final_status = EXIT_OK
        for path in args.validate_file:
            status = validate_txt_file(path)
            if status != EXIT_OK:
                final_status = status
        return final_status

    if args.reports_dir is None or args.manifest is None:
        sys.stderr.write(
            "error: --reports-dir and --manifest are required unless "
            "--validate-file is set\n")
        return EXIT_ENVIRONMENT_ERROR

    manifest_entries = parse_manifest(args.manifest)
    txt_files = list_txt_files(args.reports_dir)
    parity_status = diff_manifest_directory(manifest_entries, txt_files)
    if parity_status != EXIT_OK:
        return parity_status

    if not manifest_entries:
        return EXIT_OK

    final_status = EXIT_OK
    for entry in manifest_entries:
        path = args.reports_dir / entry
        status = validate_txt_file(path)
        if status != EXIT_OK:
            final_status = status
    return final_status


if __name__ == "__main__":
    sys.exit(main())
