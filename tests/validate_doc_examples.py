#!/usr/bin/env python3
"""Doc-examples drift gate (AP M5-1.8 A.2).

Validates that docs/examples/ stays in sync with docs/examples/manifest.txt:

  1. manifest entries match directory contents (no extras, no missing);
  2. each example's SHA-256 matches the manifest hash byte-for-byte;
  3. per-format validation for *.json, *.dot and *.html examples by
     dispatching to tests/validate_json_schema.py, tests/validate_dot_reports.py
     and tests/validate_html_reports.py respectively.

Pure stdlib for the parity/hash check; the format-validation chain shells
out to the existing per-format validators so a docs/examples/ file with
the wrong schema, broken DOT syntax or a forbidden HTML element fails the
gate the same way the goldens-side gates do. plan-M5-1-8.md
"Validierung, dass docs/examples/ keine driftenden Kopien enthaelt"
explicitly states that format validation does not replace the drift
compare; both run.

Usage:
    python3 tests/validate_doc_examples.py \\
        --repo-root <path-to-repo>          # optional, default CWD
        --manifest docs/examples/manifest.txt
        --examples-dir docs/examples

Exit codes:
    0  All checks passed.
    1  Drift detected (parity, hash, or format).
    2  Environment error (missing manifest, unreadable file, missing
       per-format validator script).
"""

from __future__ import annotations

import argparse
import hashlib
import subprocess
import sys
from pathlib import Path

EXIT_OK = 0
EXIT_VALIDATION_FAILED = 1
EXIT_ENVIRONMENT_ERROR = 2

MANIFEST_BASENAME = "manifest.txt"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate docs/examples/ against its manifest plus format gates.")
    parser.add_argument("--repo-root", type=Path, default=Path.cwd(),
                        help="Repository root (default: current working dir).")
    parser.add_argument("--manifest", type=Path,
                        default=Path("docs/examples/manifest.txt"),
                        help="Manifest path relative to --repo-root.")
    parser.add_argument("--examples-dir", type=Path,
                        default=Path("docs/examples"),
                        help="Examples directory relative to --repo-root.")
    return parser.parse_args()


def parse_manifest(manifest_path: Path) -> dict[str, str]:
    if not manifest_path.is_file():
        sys.stderr.write(f"error: manifest not found: {manifest_path}\n")
        sys.exit(EXIT_ENVIRONMENT_ERROR)
    entries: dict[str, str] = {}
    for raw in manifest_path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) != 2:
            sys.stderr.write(
                f"error: malformed manifest line (expected '<sha256> <name>'): {raw!r}\n")
            sys.exit(EXIT_VALIDATION_FAILED)
        sha, name = parts
        if len(sha) != 64 or any(c not in "0123456789abcdef" for c in sha):
            sys.stderr.write(
                f"error: not a lowercase SHA-256 hex digest: {sha!r}\n")
            sys.exit(EXIT_VALIDATION_FAILED)
        if name in entries:
            sys.stderr.write(f"error: duplicate manifest entry: {name}\n")
            sys.exit(EXIT_VALIDATION_FAILED)
        entries[name] = sha
    return entries


def list_directory(examples_dir: Path) -> set[str]:
    if not examples_dir.is_dir():
        sys.stderr.write(f"error: examples directory not found: {examples_dir}\n")
        sys.exit(EXIT_ENVIRONMENT_ERROR)
    files: set[str] = set()
    for entry in examples_dir.iterdir():
        if not entry.is_file():
            continue
        if entry.name == MANIFEST_BASENAME:
            continue
        files.add(entry.name)
    return files


def check_parity(manifest: dict[str, str], directory: set[str]) -> int:
    manifest_set = set(manifest.keys())
    missing_on_disk = sorted(manifest_set - directory)
    extra_on_disk = sorted(directory - manifest_set)
    if not missing_on_disk and not extra_on_disk:
        return EXIT_OK
    if missing_on_disk:
        sys.stderr.write("error: manifest entries without matching example file:\n")
        for name in missing_on_disk:
            sys.stderr.write(f"  - {name}\n")
    if extra_on_disk:
        sys.stderr.write("error: example files not listed in manifest:\n")
        for name in extra_on_disk:
            sys.stderr.write(f"  - {name}\n")
    return EXIT_VALIDATION_FAILED


def hash_file(path: Path) -> str:
    digest = hashlib.sha256()
    digest.update(path.read_bytes())
    return digest.hexdigest()


def check_hashes(manifest: dict[str, str], examples_dir: Path) -> int:
    failed = 0
    for name, expected in sorted(manifest.items()):
        actual = hash_file(examples_dir / name)
        if actual != expected:
            sys.stderr.write(f"error: SHA-256 drift for {name}\n")
            sys.stderr.write(f"  expected: {expected}\n")
            sys.stderr.write(f"  actual:   {actual}\n")
            failed += 1
    return EXIT_VALIDATION_FAILED if failed else EXIT_OK


def run_format_validator(label: str, args: list[str]) -> int:
    try:
        completed = subprocess.run(args, capture_output=True, text=True, check=False)
    except FileNotFoundError as exc:
        sys.stderr.write(f"error: {label} validator not runnable: {exc}\n")
        return EXIT_ENVIRONMENT_ERROR
    if completed.returncode == 0:
        return EXIT_OK
    sys.stderr.write(f"error: {label} validation failed (exit {completed.returncode})\n")
    if completed.stdout:
        sys.stderr.write(completed.stdout)
    if completed.stderr:
        sys.stderr.write(completed.stderr)
    return EXIT_VALIDATION_FAILED


def check_formats(manifest: dict[str, str], examples_dir: Path,
                  repo_root: Path) -> int:
    json_validator = repo_root / "tests" / "validate_json_schema.py"
    dot_validator = repo_root / "tests" / "validate_dot_reports.py"
    html_validator = repo_root / "tests" / "validate_html_reports.py"
    json_schema = repo_root / "docs" / "report-json.schema.json"

    final_status = EXIT_OK
    for name in sorted(manifest.keys()):
        path = examples_dir / name
        suffix = path.suffix.lower()
        if suffix == ".json":
            if not json_validator.is_file() or not json_schema.is_file():
                sys.stderr.write(
                    f"error: JSON validator or schema missing for {name}\n")
                final_status = EXIT_ENVIRONMENT_ERROR
                continue
            status = run_format_validator(
                f"JSON example {name}",
                [sys.executable, str(json_validator),
                 "--schema", str(json_schema),
                 "--validate-file", str(path)])
            if status != EXIT_OK:
                final_status = status
        elif suffix == ".dot":
            if not dot_validator.is_file():
                sys.stderr.write(
                    f"error: DOT validator missing for {name}\n")
                final_status = EXIT_ENVIRONMENT_ERROR
                continue
            status = run_format_validator(
                f"DOT example {name}",
                [sys.executable, str(dot_validator),
                 "--validate-file", str(path)])
            if status != EXIT_OK:
                final_status = status
        elif suffix == ".html":
            if not html_validator.is_file():
                sys.stderr.write(
                    f"error: HTML validator missing for {name}\n")
                final_status = EXIT_ENVIRONMENT_ERROR
                continue
            status = run_format_validator(
                f"HTML example {name}",
                [sys.executable, str(html_validator),
                 "--validate-file", str(path)])
            if status != EXIT_OK:
                final_status = status
        # .md, .txt, .* fall through: parity + hash already cover them.
    return final_status


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()
    manifest_path = (repo_root / args.manifest).resolve()
    examples_dir = (repo_root / args.examples_dir).resolve()

    manifest = parse_manifest(manifest_path)
    directory = list_directory(examples_dir)

    parity_status = check_parity(manifest, directory)
    if parity_status != EXIT_OK:
        return parity_status

    hash_status = check_hashes(manifest, examples_dir)
    if hash_status != EXIT_OK:
        return hash_status

    format_status = check_formats(manifest, examples_dir, repo_root)
    return format_status


if __name__ == "__main__":
    sys.exit(main())
