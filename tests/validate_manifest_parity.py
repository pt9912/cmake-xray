#!/usr/bin/env python3
"""Manifest+directory parity gate for cmake-xray text-only goldens
(AP M6-1.2 Tranche A.3).

Stdlib-only validator that mirrors the manifest+directory parity step of
tests/validate_html_reports.py without imposing format-specific structure
checks. Used by the M6 markdown-reports/ and console-reports/ goldens
directories, which carry committed reference samples but have no
dedicated structure validator (markdown and console are line-based text
without a formal contract beyond docs/planning/plan-M6-1-2.md).

The check guarantees:
  - every non-comment, non-blank line in the manifest names a file that
    exists in the reports directory;
  - every file with the requested suffix in the reports directory is
    listed in the manifest (no stray drafts, no forgotten cleanup);
  - the manifest itself is not listed as an entry.

The script does NOT inspect file contents; that is intentional. Markdown
and console goldens are validated for byte-stability through dedicated
e2e binary tests where applicable; this gate only stops a stray file or
an out-of-date manifest from passing review unnoticed.

Usage:
    python3 tests/validate_manifest_parity.py \\
        --reports-dir tests/e2e/testdata/m6/markdown-reports \\
        --manifest tests/e2e/testdata/m6/markdown-reports/manifest.txt \\
        --suffix .md

Exit codes:
    0  Manifest and directory agree.
    1  Mismatch detected (missing entry, missing file, or both).
    2  Environment error (missing inputs).
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

EXIT_OK = 0
EXIT_VALIDATION_FAILED = 1
EXIT_ENVIRONMENT_ERROR = 2

MANIFEST_BASENAME = "manifest.txt"


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


def list_files(reports_dir: Path, suffix: str) -> list[str]:
    if not reports_dir.is_dir():
        sys.stderr.write(f"error: reports directory not found: {reports_dir}\n")
        sys.exit(EXIT_ENVIRONMENT_ERROR)
    return sorted(p.name for p in reports_dir.iterdir()
                  if p.is_file() and p.suffix == suffix
                  and p.name != MANIFEST_BASENAME)


def diff_manifest_directory(manifest_entries: list[str],
                             files: list[str]) -> int:
    manifest_set = set(manifest_entries)
    files_set = set(files)
    missing_on_disk = sorted(manifest_set - files_set)
    missing_in_manifest = sorted(files_set - manifest_set)
    if not missing_on_disk and not missing_in_manifest:
        return EXIT_OK
    if missing_on_disk:
        sys.stderr.write("error: manifest entries without matching file:\n")
        for name in missing_on_disk:
            sys.stderr.write(f"  - {name}\n")
    if missing_in_manifest:
        sys.stderr.write("error: files not listed in manifest:\n")
        for name in missing_in_manifest:
            sys.stderr.write(f"  - {name}\n")
    return EXIT_VALIDATION_FAILED


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate manifest/directory parity for text-only goldens.")
    parser.add_argument("--reports-dir", type=Path, required=True,
                        help="Directory containing the goldens.")
    parser.add_argument("--manifest", type=Path, required=True,
                        help="Manifest path (one filename per non-comment line).")
    parser.add_argument("--suffix", type=str, required=True,
                        help="File extension to enumerate (e.g. '.md').")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    entries = parse_manifest(args.manifest)
    files = list_files(args.reports_dir, args.suffix)
    return diff_manifest_directory(entries, files)


if __name__ == "__main__":
    sys.exit(main())
