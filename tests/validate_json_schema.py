#!/usr/bin/env python3
"""JSON schema validator for cmake-xray reports (AP M5-1.2 Tranche A).

Validates docs/report-json.schema.json against the JSON Schema Draft 2020-12
meta-schema, then validates every JSON report golden listed in the manifest
against the schema. The manifest and the report directory must agree.

The script is offline: it never makes network requests. Missing dependencies
yield a clear stderr message and a nonzero exit, never a silent skip.

Usage:
    python3 tests/validate_json_schema.py \
        --schema docs/report-json.schema.json \
        --reports-dir tests/e2e/testdata/m5/json-reports \
        --manifest tests/e2e/testdata/m5/json-reports/manifest.txt

    python3 tests/validate_json_schema.py \
        --schema docs/report-json.schema.json \
        --syntax-only

Exit codes:
    0  All checks passed.
    1  Validation failed (schema syntax, manifest mismatch, or report invalid).
    2  Missing dependency or unreadable input.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

EXIT_OK = 0
EXIT_VALIDATION_FAILED = 1
EXIT_ENVIRONMENT_ERROR = 2

MISSING_DEPENDENCY_MESSAGE = (
    "error: the 'jsonschema' Python package is required to validate cmake-xray\n"
    "       JSON reports but is not installed in the current environment.\n"
    "\n"
    "       Install it with one of:\n"
    "         python3 -m pip install -r tests/requirements-json-schema.txt\n"
    "         apt-get install -y python3-jsonschema   # Debian/Ubuntu\n"
    "\n"
    "       This validator does not access the network and will not silently\n"
    "       skip if the dependency is missing.\n"
)


def import_jsonschema():
    try:
        import jsonschema  # noqa: F401
        from jsonschema import Draft202012Validator
        from jsonschema.exceptions import SchemaError, ValidationError
    except ImportError:
        sys.stderr.write(MISSING_DEPENDENCY_MESSAGE)
        sys.exit(EXIT_ENVIRONMENT_ERROR)
    return Draft202012Validator, SchemaError, ValidationError


def read_json(path: Path):
    try:
        with path.open("r", encoding="utf-8") as fh:
            return json.load(fh)
    except FileNotFoundError:
        sys.stderr.write(f"error: file not found: {path}\n")
        sys.exit(EXIT_ENVIRONMENT_ERROR)
    except OSError as exc:
        sys.stderr.write(f"error: cannot read {path}: {exc}\n")
        sys.exit(EXIT_ENVIRONMENT_ERROR)
    except json.JSONDecodeError as exc:
        sys.stderr.write(f"error: invalid JSON in {path}: {exc}\n")
        sys.exit(EXIT_VALIDATION_FAILED)


def validate_schema_syntax(schema, draft_validator_cls, schema_error_cls) -> int:
    try:
        draft_validator_cls.check_schema(schema)
    except schema_error_cls as exc:
        sys.stderr.write(f"error: schema is not a valid JSON Schema Draft 2020-12 document\n")
        sys.stderr.write(f"       {exc.message}\n")
        return EXIT_VALIDATION_FAILED
    return EXIT_OK


def parse_manifest(manifest_path: Path) -> list[str]:
    # Each line is treated as a whole: blank or whitespace-only lines are
    # skipped, lines starting with '#' are comments, everything else is taken
    # verbatim as a filename. Inline trailing comments are not supported.
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


def list_report_files(reports_dir: Path) -> list[str]:
    if not reports_dir.is_dir():
        sys.stderr.write(f"error: reports directory not found: {reports_dir}\n")
        sys.exit(EXIT_ENVIRONMENT_ERROR)
    return sorted(p.name for p in reports_dir.iterdir() if p.is_file()
                  and p.suffix == ".json")


def diff_manifest_directory(manifest_entries: list[str],
                             report_files: list[str]) -> int:
    manifest_set = set(manifest_entries)
    files_set = set(report_files)

    missing_on_disk = sorted(manifest_set - files_set)
    missing_in_manifest = sorted(files_set - manifest_set)

    if not missing_on_disk and not missing_in_manifest:
        return EXIT_OK

    if missing_on_disk:
        sys.stderr.write("error: manifest entries without matching report file:\n")
        for name in missing_on_disk:
            sys.stderr.write(f"  - {name}\n")
    if missing_in_manifest:
        sys.stderr.write("error: report files not listed in manifest:\n")
        for name in missing_in_manifest:
            sys.stderr.write(f"  - {name}\n")
    return EXIT_VALIDATION_FAILED


def validate_report(validator, report_path: Path,
                    validation_error_cls) -> int:
    document = read_json(report_path)
    errors = sorted(validator.iter_errors(document), key=lambda e: list(e.absolute_path))
    if not errors:
        return EXIT_OK
    sys.stderr.write(f"error: {report_path} does not satisfy the report schema\n")
    for error in errors:
        location = "/".join(str(part) for part in error.absolute_path) or "(root)"
        sys.stderr.write(f"  - {location}: {error.message}\n")
    return EXIT_VALIDATION_FAILED


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate cmake-xray JSON report goldens against the schema.")
    parser.add_argument("--schema", required=True, type=Path,
                        help="Path to docs/report-json.schema.json.")
    parser.add_argument("--reports-dir", type=Path,
                        help="Directory of JSON report goldens.")
    parser.add_argument("--manifest", type=Path,
                        help="Manifest file listing each golden by name.")
    parser.add_argument("--syntax-only", action="store_true",
                        help="Validate the schema only; skip goldens and manifest.")
    parser.add_argument("--validate-file", type=Path, action="append",
                        default=[],
                        help="Validate a single JSON file against the schema "
                             "and exit. Repeatable. Used by run_e2e.sh to "
                             "schema-check the binary's actually produced "
                             "JSON output.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    draft_validator_cls, schema_error_cls, validation_error_cls = import_jsonschema()

    schema = read_json(args.schema)
    syntax_status = validate_schema_syntax(schema, draft_validator_cls, schema_error_cls)
    if syntax_status != EXIT_OK:
        return syntax_status

    if args.syntax_only:
        return EXIT_OK

    if args.validate_file:
        validator = draft_validator_cls(schema)
        final_status = EXIT_OK
        for path in args.validate_file:
            status = validate_report(validator, path, validation_error_cls)
            if status != EXIT_OK:
                final_status = status
        return final_status

    if args.reports_dir is None or args.manifest is None:
        sys.stderr.write("error: --reports-dir and --manifest are required unless "
                         "--syntax-only or --validate-file is set\n")
        return EXIT_ENVIRONMENT_ERROR

    manifest_entries = parse_manifest(args.manifest)
    report_files = list_report_files(args.reports_dir)
    parity_status = diff_manifest_directory(manifest_entries, report_files)
    if parity_status != EXIT_OK:
        return parity_status

    if not manifest_entries:
        return EXIT_OK

    validator = draft_validator_cls(schema)
    final_status = EXIT_OK
    for entry in manifest_entries:
        report_path = args.reports_dir / entry
        status = validate_report(validator, report_path, validation_error_cls)
        if status != EXIT_OK:
            final_status = status
    return final_status


if __name__ == "__main__":
    sys.exit(main())
