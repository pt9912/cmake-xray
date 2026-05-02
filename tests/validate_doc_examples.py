#!/usr/bin/env python3
"""Doc-examples drift gate (AP M5-1.8 A.2 + A.5 fixup).

Validates that docs/examples/ stays in sync with docs/examples/manifest.txt
and -- when invoked with --binary -- that the committed example bytes match
the cmake-xray binary's current output for the args recorded in
docs/examples/generation-spec.json:

  1. manifest entries match directory contents (no extras, no missing);
  2. each example's SHA-256 matches the manifest hash byte-for-byte;
  3. per-format validation for *.json, *.dot and *.html examples by
     dispatching to tests/validate_json_schema.py, tests/validate_dot_reports.py
     and tests/validate_html_reports.py respectively;
  4. (with --binary) regenerate each example via the supplied cmake-xray
     binary using the generation-spec args and byte-diff the captured
     stdout against the committed example file. plan-M5-1-8.md
     "Validierung, dass docs/examples/ keine driftenden Kopien enthaelt"
     accepts either CI-vs-generator drift compare OR golden-byte compare;
     this implements the generator-output path so a binary change that
     drifts an example surfaces in the doc_examples_validation CTest entry.

Pure stdlib for the parity/hash check; the format-validation chain shells
out to the existing per-format validators so a docs/examples/ file with
the wrong schema, broken DOT syntax or a forbidden HTML element fails the
gate the same way the goldens-side gates do. plan-M5-1-8.md states that
format validation does not replace the drift compare; both run.

Usage:
    python3 tests/validate_doc_examples.py \\
        --repo-root <path-to-repo>          # optional, default CWD
        --manifest docs/examples/manifest.txt
        --examples-dir docs/examples
        [--binary <path-to-cmake-xray>]     # optional, enables drift step
        [--generation-spec docs/examples/generation-spec.json]

Exit codes:
    0  All checks passed.
    1  Drift detected (parity, hash, format, or binary-vs-file diff).
    2  Environment error (missing manifest, unreadable file, missing
       per-format validator script, missing generation-spec).
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

EXIT_OK = 0
EXIT_VALIDATION_FAILED = 1
EXIT_ENVIRONMENT_ERROR = 2

MANIFEST_BASENAME = "manifest.txt"
GENERATION_SPEC_BASENAME = "generation-spec.json"
INFRA_BASENAMES = frozenset((MANIFEST_BASENAME, GENERATION_SPEC_BASENAME))

# DOT reports embed the absolute source/build directory in TU `unique_key`
# attributes (`unique_key="<src>|<build>"`). On hosts where those absolute
# paths differ -- developer machine, GHA runner, Docker bind-mount -- the
# bytes drift even though the report content is otherwise stable. The
# drift step normalises both sides to the same HOSTPATH placeholder before
# diff, mirroring tests/e2e/run_e2e_lib.sh's `normalize_dot_unique_keys`
# helper. SHA-256 hashing in check_hashes() still pins the committed file
# byte-for-byte, so an in-place edit is still caught.
DOT_UNIQUE_KEY_PATTERN = re.compile(rb'unique_key="[^"]*\|[^"]*"')


def normalize_dot_unique_keys(content: bytes) -> bytes:
    return DOT_UNIQUE_KEY_PATTERN.sub(b'unique_key="HOSTPATH"', content)


def normalize_line_endings(content: bytes) -> bytes:
    # Windows binaries emit CRLF; committed examples ship with LF. The drift
    # check needs to compare report content, not host text-mode quirks. Mirrors
    # tests/e2e/run_e2e_lib.sh's normalize_line_endings helper that the m5
    # JSON/DOT/HTML goldens already rely on for cross-platform stability.
    return content.replace(b"\r\n", b"\n").replace(b"\r", b"")


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
    parser.add_argument("--binary", type=Path, default=None,
                        help="Path to the cmake-xray binary. When set, the "
                             "validator regenerates each example via the "
                             "generation-spec.json args and byte-diffs the "
                             "captured stdout against the committed file.")
    parser.add_argument("--generation-spec", type=Path,
                        default=Path("docs/examples/generation-spec.json"),
                        help="Generation-spec path relative to --repo-root. "
                             "Only consulted when --binary is set.")
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
        if entry.name in INFRA_BASENAMES:
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
    json_schema = repo_root / "spec" / "report-json.schema.json"

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


def check_binary_drift(spec_path: Path, examples_dir: Path,
                       binary_path: Path, repo_root: Path) -> int:
    if not binary_path.is_file():
        sys.stderr.write(f"error: --binary path does not exist: {binary_path}\n")
        return EXIT_ENVIRONMENT_ERROR
    if not spec_path.is_file():
        sys.stderr.write(f"error: generation spec not found: {spec_path}\n")
        return EXIT_ENVIRONMENT_ERROR
    try:
        spec = json.loads(spec_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        sys.stderr.write(f"error: cannot parse generation spec: {exc}\n")
        return EXIT_ENVIRONMENT_ERROR
    entries = spec.get("examples")
    if not isinstance(entries, list):
        sys.stderr.write("error: generation spec missing 'examples' array\n")
        return EXIT_ENVIRONMENT_ERROR

    failed = 0
    seen: set[str] = set()
    for entry in entries:
        name = entry.get("file")
        args_list = entry.get("args")
        if not isinstance(name, str) or not isinstance(args_list, list):
            sys.stderr.write(f"error: malformed spec entry: {entry!r}\n")
            failed += 1
            continue
        seen.add(name)
        target_path = examples_dir / name
        if not target_path.is_file():
            sys.stderr.write(
                f"error: spec references missing example {name}\n")
            failed += 1
            continue
        try:
            completed = subprocess.run(
                [str(binary_path)] + [str(a) for a in args_list],
                capture_output=True, cwd=repo_root, check=False)
        except FileNotFoundError as exc:
            sys.stderr.write(
                f"error: cannot run {binary_path} for {name}: {exc}\n")
            return EXIT_ENVIRONMENT_ERROR
        if completed.returncode != 0:
            sys.stderr.write(
                f"error: regen failed for {name}: exit {completed.returncode}\n")
            sys.stderr.write(
                completed.stderr.decode("utf-8", errors="replace"))
            failed += 1
            continue
        expected = target_path.read_bytes()
        actual_for_diff = normalize_line_endings(completed.stdout)
        expected_for_diff = normalize_line_endings(expected)
        if target_path.suffix.lower() == ".dot":
            actual_for_diff = normalize_dot_unique_keys(actual_for_diff)
            expected_for_diff = normalize_dot_unique_keys(expected_for_diff)
        if actual_for_diff != expected_for_diff:
            sys.stderr.write(
                f"error: drift for {name} -- binary stdout "
                f"({len(completed.stdout)} B) differs from committed file "
                f"({len(expected)} B)\n")
            sys.stderr.write(
                "  rerun docs/examples/ regen via:\n"
                f"    {binary_path} {' '.join(str(a) for a in args_list)} "
                f"> docs/examples/{name}\n"
                "  then update docs/examples/manifest.txt SHA-256 entry.\n")
            failed += 1

    # Cross-check: every example file must have a spec entry, otherwise it
    # cannot be drift-detected. This pairs with the manifest parity check
    # above and prevents a regression where a file is added to the manifest
    # but forgotten in the spec.
    missing_in_spec: list[str] = []
    for path in sorted(examples_dir.iterdir()):
        if not path.is_file():
            continue
        if path.name in INFRA_BASENAMES:
            continue
        if path.name not in seen:
            missing_in_spec.append(path.name)
    if missing_in_spec:
        sys.stderr.write(
            "error: examples missing from docs/examples/generation-spec.json:\n")
        for name in missing_in_spec:
            sys.stderr.write(f"  - {name}\n")
        failed += len(missing_in_spec)

    return EXIT_VALIDATION_FAILED if failed else EXIT_OK


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()
    manifest_path = (repo_root / args.manifest).resolve()
    examples_dir = (repo_root / args.examples_dir).resolve()
    spec_path = (repo_root / args.generation_spec).resolve()

    manifest = parse_manifest(manifest_path)
    directory = list_directory(examples_dir)

    parity_status = check_parity(manifest, directory)
    if parity_status != EXIT_OK:
        return parity_status

    hash_status = check_hashes(manifest, examples_dir)
    if hash_status != EXIT_OK:
        return hash_status

    format_status = check_formats(manifest, examples_dir, repo_root)
    if format_status != EXIT_OK:
        return format_status

    if args.binary is None:
        return EXIT_OK
    binary_path = args.binary.resolve()
    # Windows binaries embed host paths with backslashes and host-specific
    # absolute prefixes (e.g. resolving "/project/src/main.cpp" against the
    # current drive). The committed examples are Linux-generated with
    # forward slashes and synthetic /project/... prefixes; per-platform
    # variants would double the doc-maintenance load. The drift step
    # instead skips on Windows binaries; Linux native and the
    # docs-examples-portability workflow job give the drift coverage.
    # Parity, SHA-256 and format validators above still run on every
    # platform.
    if sys.platform.startswith("win") or binary_path.suffix.lower() == ".exe":
        sys.stderr.write(
            "note: skipping doc-examples drift step on Windows; "
            "the binary's path-display semantics diverge from the "
            "Linux-generated example bytes. Drift coverage runs on "
            "Linux native plus the docs-examples-portability job.\n")
        return EXIT_OK
    return check_binary_drift(spec_path, examples_dir, binary_path, repo_root)


if __name__ == "__main__":
    sys.exit(main())
