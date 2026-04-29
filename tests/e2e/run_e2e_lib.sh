#!/usr/bin/env bash
# Shared library for the cmake-xray e2e binary smokes (AP M5-1.5 Tranche D
# Folge-Refactor). Sourced by run_e2e_smoke.sh, run_e2e_artifacts.sh and
# run_e2e_verbosity.sh; each sub-script exposes BINARY/TESTDATA via $1/$2,
# inherits the assert_* and validator helpers from this file and runs its
# own focused subset of tests. Splitting into three CTest entries lets
# `ctest --parallel` schedule them concurrently.
set -euo pipefail

# Prevent MSYS2/Git Bash from converting Unix-style paths in CLI arguments
# (e.g. /project/src/main.cpp → C:/Program Files/Git/project/src/main.cpp).
export MSYS_NO_PATHCONV=1
export MSYS2_ARG_CONV_EXCL="*"

# On MSYS2/Git Bash, convert a POSIX path to a native Windows path.
# On other platforms, return the path unchanged.
native_path() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -m "$1"
    else
        printf '%s' "$1"
    fi
}

BINARY="$1"
TESTDATA="$2"
failures=0
REPO_ROOT="$(cd "$TESTDATA/../../.." && pwd)"

normalize_line_endings() {
    tr -d '\r' < "$1"
}

# Replace TU-style unique_key values (which embed absolute on-disk paths and
# therefore vary by host/checkout layout) with the literal "HOSTPATH" so DOT
# goldens for fixtures with on-disk source files stay byte-stable across
# Linux, macOS, Windows and Docker matrix runners. Target unique_keys
# ("name::TYPE", no `|`) are left intact so display-name and type regressions
# stay visible.
normalize_dot_unique_keys() {
    sed -E 's@unique_key="[^"]*\|[^"]*"@unique_key="HOSTPATH"@g' "$1"
}

assert_stdout_equals_file() {
    local description="$1" expected_file="$2"
    shift 2
    local actual_file actual_clean expected_clean
    actual_file="$(mktemp)"
    actual_clean="$(mktemp)"
    expected_clean="$(mktemp)"
    "$@" >"$actual_file" 2>/dev/null || true
    normalize_line_endings "$actual_file" > "$actual_clean"
    normalize_line_endings "$expected_file" > "$expected_clean"
    if cmp -s "$actual_clean" "$expected_clean"; then
        echo "PASS: $description"
    else
        echo "FAIL: $description — stdout differed from $expected_file" >&2
        diff -u "$expected_clean" "$actual_clean" >&2 || true
        failures=$((failures + 1))
    fi
    rm -f "$actual_file" "$actual_clean" "$expected_clean"
}

# AP M5-1.7 Tranche C.1: --output-Pflicht-Smoke-Helfer. Run a cmake-xray
# invocation that uses --output <target_path> and assert (1) exit 0, (2)
# empty stdout, (3) the target file was created. Caller is responsible for
# the format-specific golden compare and gate validation; the file at
# <target_path> is left in place for those follow-up assertions.
assert_output_file_writes_empty_stdout() {
    local description="$1" target_path="$2"
    shift 2
    local stdout_file stderr_file actual_exit=0
    stdout_file="$(mktemp)"
    stderr_file="$(mktemp)"
    "$@" >"$stdout_file" 2>"$stderr_file" || actual_exit=$?
    if [ "$actual_exit" -ne 0 ]; then
        echo "FAIL: $description — exit was $actual_exit, expected 0" >&2
        cat "$stderr_file" >&2
        failures=$((failures + 1))
    elif [ -s "$stdout_file" ]; then
        echo "FAIL: $description — stdout was not empty" >&2
        head -c 200 "$stdout_file" >&2
        failures=$((failures + 1))
    elif ! [ -f "$target_path" ]; then
        echo "FAIL: $description — output file $target_path was not created" >&2
        failures=$((failures + 1))
    else
        echo "PASS: $description"
    fi
    rm -f "$stdout_file" "$stderr_file"
}

# DOT-aware variant of assert_stdout_equals_file that strips host-dependent
# absolute paths from TU-style unique_key attributes before the byte compare.
# Use only for goldens whose backing fixture relies on on-disk source files
# and therefore produces unique_keys containing the resolved working
# directory; goldens for synthetic /project/... fixtures keep using the
# stricter assert_stdout_equals_file.
assert_dot_stdout_equals_file() {
    local description="$1" expected_file="$2"
    shift 2
    local actual_file actual_clean expected_clean
    actual_file="$(mktemp)"
    actual_clean="$(mktemp)"
    expected_clean="$(mktemp)"
    "$@" >"$actual_file" 2>/dev/null || true
    normalize_dot_unique_keys "$actual_file" | tr -d '\r' > "$actual_clean"
    normalize_line_endings "$expected_file" > "$expected_clean"
    if cmp -s "$actual_clean" "$expected_clean"; then
        echo "PASS: $description"
    else
        echo "FAIL: $description — stdout differed from $expected_file" >&2
        diff -u "$expected_clean" "$actual_clean" >&2 || true
        failures=$((failures + 1))
    fi
    rm -f "$actual_file" "$actual_clean" "$expected_clean"
}

# AP M5-1.7 Tranche C.1: DOT-aware file-vs-golden compare. Same unique_key
# normalisation rules as assert_dot_stdout_equals_file, but reads the actual
# DOT bytes from a file (used by --output smokes). Use only for goldens
# whose backing fixture relies on on-disk source files; synthetic
# /project/... fixtures keep using assert_file_equals.
assert_dot_file_equals_file() {
    local description="$1" actual_file="$2" expected_file="$3"
    local actual_clean expected_clean
    actual_clean="$(mktemp)"
    expected_clean="$(mktemp)"
    normalize_dot_unique_keys "$actual_file" | tr -d '\r' > "$actual_clean"
    normalize_line_endings "$expected_file" > "$expected_clean"
    if cmp -s "$actual_clean" "$expected_clean"; then
        echo "PASS: $description"
    else
        echo "FAIL: $description — file differed from $expected_file" >&2
        diff -u "$expected_clean" "$actual_clean" >&2 || true
        failures=$((failures + 1))
    fi
    rm -f "$actual_clean" "$expected_clean"
}

assert_file_equals() {
    local description="$1" actual_file="$2" expected_file="$3"
    local actual_clean expected_clean
    actual_clean="$(mktemp)"
    expected_clean="$(mktemp)"
    normalize_line_endings "$actual_file" > "$actual_clean"
    normalize_line_endings "$expected_file" > "$expected_clean"
    if cmp -s "$actual_clean" "$expected_clean"; then
        echo "PASS: $description"
    else
        echo "FAIL: $description — file differed from $expected_file" >&2
        diff -u "$expected_clean" "$actual_clean" >&2 || true
        failures=$((failures + 1))
    fi
    rm -f "$actual_clean" "$expected_clean"
}

assert_exit() {
    local description="$1" expected="$2"
    shift 2
    local actual=0
    "$@" >/dev/null 2>&1 || actual=$?
    if [ "$actual" -ne "$expected" ]; then
        echo "FAIL: $description — expected exit $expected, got $actual" >&2
        failures=$((failures + 1))
    else
        echo "PASS: $description"
    fi
}

assert_stdout_contains() {
    local description="$1" pattern="$2"
    shift 2
    local output
    output=$("$@" 2>/dev/null) || true
    if echo "$output" | grep -q "$pattern"; then
        echo "PASS: $description"
    else
        echo "FAIL: $description — stdout did not contain '$pattern'" >&2
        failures=$((failures + 1))
    fi
}

assert_stderr_contains() {
    local description="$1" pattern="$2"
    shift 2
    local errout
    errout=$("$@" 2>&1 1>/dev/null) || true
    if echo "$errout" | grep -q "$pattern"; then
        echo "PASS: $description"
    else
        echo "FAIL: $description — stderr did not contain '$pattern'" >&2
        failures=$((failures + 1))
    fi
}

cd "$REPO_ROOT"

# Validate a DOT file via tests/validate_dot_reports.py (pure stdlib) so the
# native CI matrix stays Graphviz-free. Docker stages add a parallel
# graphviz `dot -Tsvg` gate in CTest. native_path keeps the script and file
# paths Windows-MSYS-Bash-safe.
dot_validator_script="$(native_path "$REPO_ROOT/tests/validate_dot_reports.py")"
assert_dot_syntax_validates() {
    local description="$1" dot_file="$2"
    if ! command -v python3 >/dev/null 2>&1; then
        echo "SKIP: $description (python3 not available)"
        return
    fi
    if ! [ -f "$REPO_ROOT/tests/validate_dot_reports.py" ]; then
        echo "SKIP: $description (validator missing)"
        return
    fi
    local dot_file_native
    dot_file_native="$(native_path "$dot_file")"
    if python3 "$dot_validator_script" --validate-file "$dot_file_native" >/dev/null 2>&1; then
        echo "PASS: $description"
    else
        echo "FAIL: $description — produced DOT did not satisfy validate_dot_reports.py" >&2
        python3 "$dot_validator_script" --validate-file "$dot_file_native" >&2 || true
        failures=$((failures + 1))
    fi
}

assert_dot_stdout_validates() {
    local description="$1"
    shift
    local captured
    captured="$(mktemp)"
    "$@" >"$captured" 2>/dev/null || true
    assert_dot_syntax_validates "$description" "$captured"
    rm -f "$captured"
}

# Validate a JSON file against docs/report-json.schema.json. Skips silently
# if the validator script or python3 is unavailable so the bash smoke does
# not fail on minimal hosts; CTest itself already enforces the schema gate
# via the report_json_schema_validation entry.
#
# Paths go through native_path so MSYS Bash's POSIX-style /d/a/... is
# converted to D:\a\... before being handed to a non-MSYS Python on Windows.
validator_script="$(native_path "$REPO_ROOT/tests/validate_json_schema.py")"
schema_path="$(native_path "$REPO_ROOT/docs/report-json.schema.json")"
assert_schema_validates() {
    local description="$1" json_file="$2"
    if ! command -v python3 >/dev/null 2>&1; then
        echo "SKIP: $description (python3 not available)"
        return
    fi
    if ! [ -f "$REPO_ROOT/tests/validate_json_schema.py" ] || \
       ! [ -f "$REPO_ROOT/docs/report-json.schema.json" ]; then
        echo "SKIP: $description (validator or schema missing)"
        return
    fi
    local json_file_native
    json_file_native="$(native_path "$json_file")"
    if python3 "$validator_script" --schema "$schema_path" --validate-file "$json_file_native" >/dev/null 2>&1; then
        echo "PASS: $description"
    else
        echo "FAIL: $description — produced JSON did not satisfy report-json.schema.json" >&2
        python3 "$validator_script" --schema "$schema_path" --validate-file "$json_file_native" >&2 || true
        failures=$((failures + 1))
    fi
}

# Run a JSON-producing cmake-xray invocation and validate the captured
# stdout against the schema in addition to any byte-stable golden compare.
assert_json_stdout_validates() {
    local description="$1"
    shift
    local captured
    captured="$(mktemp)"
    "$@" >"$captured" 2>/dev/null || true
    assert_schema_validates "$description" "$captured"
    rm -f "$captured"
}

# ---- AP M5-1.5 Tranche C.2 helpers (shared by run_e2e_verbosity.sh) ----

assert_stderr_equals_file() {
    local description="$1" expected_file="$2"
    shift 2
    local actual_file actual_clean expected_clean
    actual_file="$(mktemp)"
    actual_clean="$(mktemp)"
    expected_clean="$(mktemp)"
    "$@" 2>"$actual_file" >/dev/null || true
    normalize_line_endings "$actual_file" > "$actual_clean"
    normalize_line_endings "$expected_file" > "$expected_clean"
    if cmp -s "$actual_clean" "$expected_clean"; then
        echo "PASS: $description"
    else
        echo "FAIL: $description — stderr differed from expected sequence" >&2
        diff -u "$expected_clean" "$actual_clean" >&2 || true
        failures=$((failures + 1))
    fi
    rm -f "$actual_file" "$actual_clean" "$expected_clean"
}

assert_stderr_empty() {
    local description="$1"
    shift
    local actual_file
    actual_file="$(mktemp)"
    "$@" 2>"$actual_file" >/dev/null || true
    if [ -s "$actual_file" ]; then
        echo "FAIL: $description — stderr was not empty" >&2
        cat "$actual_file" >&2
        failures=$((failures + 1))
    else
        echo "PASS: $description"
    fi
    rm -f "$actual_file"
}

assert_stdout_ends_with_newline() {
    local description="$1"
    shift
    local actual_file
    actual_file="$(mktemp)"
    "$@" >"$actual_file" 2>/dev/null || true
    if [ ! -s "$actual_file" ]; then
        echo "FAIL: $description — stdout was empty" >&2
        failures=$((failures + 1))
        rm -f "$actual_file"
        return
    fi
    local last_byte
    last_byte=$(tail -c 1 "$actual_file" | od -An -tx1 | tr -d ' \n')
    if [ "$last_byte" = "0a" ]; then
        echo "PASS: $description"
    else
        echo "FAIL: $description — stdout did not end with a single newline (last byte 0x$last_byte)" >&2
        failures=$((failures + 1))
    fi
    rm -f "$actual_file"
}

assert_stderr_does_not_contain() {
    local description="$1" pattern="$2"
    shift 2
    local errout
    errout=$("$@" 2>&1 1>/dev/null) || true
    # grep -F treats $pattern as a literal string so future patterns with `.`,
    # `*` or `[` cannot accidentally match more than intended; the leading
    # `--` shields against patterns starting with `-`.
    if printf '%s' "$errout" | grep -F -q -- "$pattern"; then
        echo "FAIL: $description — stderr contained forbidden pattern '$pattern'" >&2
        failures=$((failures + 1))
    else
        echo "PASS: $description"
    fi
}

# HTML structure validator analog to assert_dot_stdout_validates /
# assert_json_stdout_validates. Plan ~746: ein neuer
# assert_html_stdout_validates-Helfer wird in run_e2e.sh ergaenzt.
html_validator_script="$(native_path "$REPO_ROOT/tests/validate_html_reports.py")"

# AP M5-1.7 Tranche C.1: HTML file validator analog to
# assert_schema_validates / assert_dot_syntax_validates. Used by --output
# smokes that need to validate the written file (instead of stdout).
assert_html_file_validates() {
    local description="$1" html_file="$2"
    if ! command -v python3 >/dev/null 2>&1; then
        echo "SKIP: $description (python3 not available)"
        return
    fi
    if ! [ -f "$REPO_ROOT/tests/validate_html_reports.py" ]; then
        echo "SKIP: $description (validator missing)"
        return
    fi
    local html_file_native
    html_file_native="$(native_path "$html_file")"
    if python3 "$html_validator_script" --validate-file "$html_file_native" >/dev/null 2>&1; then
        echo "PASS: $description"
    else
        echo "FAIL: $description — file did not satisfy validate_html_reports.py" >&2
        python3 "$html_validator_script" --validate-file "$html_file_native" >&2 || true
        failures=$((failures + 1))
    fi
}

assert_html_stdout_validates() {
    local description="$1"
    shift
    if ! command -v python3 >/dev/null 2>&1; then
        echo "SKIP: $description (python3 not available)"
        return
    fi
    if ! [ -f "$REPO_ROOT/tests/validate_html_reports.py" ]; then
        echo "SKIP: $description (validator missing)"
        return
    fi
    local captured captured_native
    captured="$(mktemp)"
    "$@" >"$captured" 2>/dev/null || true
    captured_native="$(native_path "$captured")"
    if python3 "$html_validator_script" --validate-file "$captured_native" >/dev/null 2>&1; then
        echo "PASS: $description"
    else
        echo "FAIL: $description — produced HTML did not satisfy validate_html_reports.py" >&2
        python3 "$html_validator_script" --validate-file "$captured_native" >&2 || true
        failures=$((failures + 1))
    fi
    rm -f "$captured"
}
