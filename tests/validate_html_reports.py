#!/usr/bin/env python3
"""HTML structure validator for cmake-xray reports (AP M5-1.4 Tranche D).

This is a stdlib-only structural smoke for the HTML report contract from
spec/report-html.md. It does not parse HTML in the strict spec sense;
instead it walks the document with a small forward parser and checks the
contract-level invariants the goldens are expected to keep:

  - the document begins with `<!doctype html>` (case-insensitive)
  - exactly one `<html lang="en">` element
  - exactly one `<head>` and one `<body>` element
  - exactly one `<main>` element with class `report`
  - exactly one `<h1>` element
  - no `<script>` element in any form (open or close tag, src or inline)
  - no `<iframe>`, `<link>`, `<embed>`, `<object>` elements
  - no HTML comments (`<!--` or `-->` outside of CSS strings)
  - no external resource references: `http://`, `https://`, `data:`,
    `@import`, `url(...)` patterns
  - no inline event handlers (`on*=` attributes)

The script mirrors validate_dot_reports.py: stdlib-only, manifest+directory
parity check, batch and single-file modes, no network access.

Usage:
    python3 tests/validate_html_reports.py \\
        --reports-dir tests/e2e/testdata/m5/html-reports \\
        --manifest tests/e2e/testdata/m5/html-reports/manifest.txt

    python3 tests/validate_html_reports.py --validate-file path/to/output.html

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

# Tags whose mere presence violates the HTML report contract because they
# either pull in code, embed remote resources, or expand the attack surface
# beyond the static report scope locked down in spec/report-html.md.
FORBIDDEN_TAG_PATTERNS = (
    re.compile(r"<\s*script\b", re.IGNORECASE),
    re.compile(r"</\s*script\s*>", re.IGNORECASE),
    re.compile(r"<\s*iframe\b", re.IGNORECASE),
    re.compile(r"<\s*link\b", re.IGNORECASE),
    re.compile(r"<\s*embed\b", re.IGNORECASE),
    re.compile(r"<\s*object\b", re.IGNORECASE),
)

# External resource references the contract forbids. data: URIs are treated
# the same way because the static-document scope rules them out alongside
# remote URLs.
FORBIDDEN_RESOURCE_PATTERNS = (
    re.compile(r"https?://", re.IGNORECASE),
    re.compile(r"@import\b", re.IGNORECASE),
    re.compile(r"\burl\s*\(", re.IGNORECASE),
    re.compile(r"\bdata:", re.IGNORECASE),
)

# Inline event handlers (onclick=, onload=, ...) are equivalent to inline
# script and must not appear in the static report.
INLINE_HANDLER_RE = re.compile(r"\son[a-z]+\s*=", re.IGNORECASE)

DOCTYPE_RE = re.compile(r"^\s*<!doctype\s+html\s*>", re.IGNORECASE)
HTML_TAG_RE = re.compile(r"<\s*html\b[^>]*>", re.IGNORECASE)
HTML_LANG_RE = re.compile(r"<\s*html\b[^>]*\blang\s*=\s*\"en\"", re.IGNORECASE)
HEAD_OPEN_RE = re.compile(r"<\s*head\b", re.IGNORECASE)
BODY_OPEN_RE = re.compile(r"<\s*body\b", re.IGNORECASE)
MAIN_OPEN_RE = re.compile(
    r"<\s*main\b[^>]*\bclass\s*=\s*\"[^\"]*\breport\b[^\"]*\"", re.IGNORECASE)
H1_OPEN_RE = re.compile(r"<\s*h1\b", re.IGNORECASE)
COMMENT_RE = re.compile(r"<!--")


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


def list_html_files(reports_dir: Path) -> list[str]:
    if not reports_dir.is_dir():
        sys.stderr.write(f"error: reports directory not found: {reports_dir}\n")
        sys.exit(EXIT_ENVIRONMENT_ERROR)
    return sorted(p.name for p in reports_dir.iterdir()
                  if p.is_file() and p.suffix == ".html")


def diff_manifest_directory(manifest_entries: list[str],
                             html_files: list[str]) -> int:
    manifest_set = set(manifest_entries)
    files_set = set(html_files)
    missing_on_disk = sorted(manifest_set - files_set)
    missing_in_manifest = sorted(files_set - manifest_set)

    if not missing_on_disk and not missing_in_manifest:
        return EXIT_OK
    if missing_on_disk:
        sys.stderr.write("error: manifest entries without matching HTML file:\n")
        for name in missing_on_disk:
            sys.stderr.write(f"  - {name}\n")
    if missing_in_manifest:
        sys.stderr.write("error: HTML files not listed in manifest:\n")
        for name in missing_in_manifest:
            sys.stderr.write(f"  - {name}\n")
    return EXIT_VALIDATION_FAILED


def validate_doctype(text: str) -> tuple[int, str | None]:
    if DOCTYPE_RE.match(text) is None:
        return EXIT_VALIDATION_FAILED, "missing leading '<!doctype html>'"
    return EXIT_OK, None


def count_pattern(pattern: re.Pattern[str], text: str) -> int:
    return len(pattern.findall(text))


def validate_singletons(text: str) -> tuple[int, str | None]:
    # The contract pins exactly-one for html, head, body, main.report and h1.
    # Multiple opening tags would either leak structure or duplicate landmarks
    # screen readers depend on.
    for name, pattern in (
        ("<html>", HTML_TAG_RE),
        ("<head>", HEAD_OPEN_RE),
        ("<body>", BODY_OPEN_RE),
        ("<main class=\"report\">", MAIN_OPEN_RE),
        ("<h1>", H1_OPEN_RE),
    ):
        count = count_pattern(pattern, text)
        if count != 1:
            return EXIT_VALIDATION_FAILED, (
                f"expected exactly one {name}, found {count}")
    return EXIT_OK, None


def validate_html_lang(text: str) -> tuple[int, str | None]:
    if HTML_LANG_RE.search(text) is None:
        return EXIT_VALIDATION_FAILED, "missing or wrong <html lang=\"en\">"
    return EXIT_OK, None


def validate_no_forbidden_tags(text: str) -> tuple[int, str | None]:
    for pattern in FORBIDDEN_TAG_PATTERNS:
        match = pattern.search(text)
        if match is not None:
            return EXIT_VALIDATION_FAILED, (
                f"forbidden tag found: {match.group(0)!r}")
    return EXIT_OK, None


def validate_no_external_resources(text: str) -> tuple[int, str | None]:
    for pattern in FORBIDDEN_RESOURCE_PATTERNS:
        match = pattern.search(text)
        if match is not None:
            return EXIT_VALIDATION_FAILED, (
                f"forbidden external-resource reference: {match.group(0)!r}")
    return EXIT_OK, None


def validate_no_inline_handlers(text: str) -> tuple[int, str | None]:
    match = INLINE_HANDLER_RE.search(text)
    if match is not None:
        return EXIT_VALIDATION_FAILED, (
            f"forbidden inline event handler: {match.group(0)!r}")
    return EXIT_OK, None


def validate_no_html_comments(text: str) -> tuple[int, str | None]:
    if COMMENT_RE.search(text) is not None:
        return EXIT_VALIDATION_FAILED, "HTML comment '<!--' is not permitted"
    return EXIT_OK, None


CHECKS = (
    validate_doctype,
    validate_html_lang,
    validate_singletons,
    validate_no_forbidden_tags,
    validate_no_external_resources,
    validate_no_inline_handlers,
    validate_no_html_comments,
)


def validate_html_file(path: Path) -> int:
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
        description="Validate cmake-xray HTML report structure and contract.")
    parser.add_argument("--reports-dir", type=Path,
                        help="Directory of HTML report goldens.")
    parser.add_argument("--manifest", type=Path,
                        help="Manifest file listing each golden by name.")
    parser.add_argument("--validate-file", type=Path, action="append",
                        default=[],
                        help="Validate a single HTML file. Repeatable. Used "
                             "by run_e2e.sh to structurally check the binary's "
                             "actually produced HTML output.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if args.validate_file:
        final_status = EXIT_OK
        for path in args.validate_file:
            status = validate_html_file(path)
            if status != EXIT_OK:
                final_status = status
        return final_status

    if args.reports_dir is None or args.manifest is None:
        sys.stderr.write(
            "error: --reports-dir and --manifest are required unless "
            "--validate-file is set\n")
        return EXIT_ENVIRONMENT_ERROR

    manifest_entries = parse_manifest(args.manifest)
    html_files = list_html_files(args.reports_dir)
    parity_status = diff_manifest_directory(manifest_entries, html_files)
    if parity_status != EXIT_OK:
        return parity_status

    if not manifest_entries:
        return EXIT_OK

    final_status = EXIT_OK
    for entry in manifest_entries:
        path = args.reports_dir / entry
        status = validate_html_file(path)
        if status != EXIT_OK:
            final_status = status
    return final_status


if __name__ == "__main__":
    sys.exit(main())
