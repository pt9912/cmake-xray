#!/usr/bin/env bash
# End-to-end test runner for cmake-xray binary.
# Usage: run_e2e.sh <binary> <testdata_dir>
set -euo pipefail

BINARY="$1"
TESTDATA="$2"
failures=0

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

# --help
assert_exit "--help exits 0" 0 "$BINARY" --help
assert_stdout_contains "--help shows cmake-xray" "cmake-xray" "$BINARY" --help
assert_stdout_contains "--help shows analyze subcommand" "analyze" "$BINARY" --help

# analyze --help
assert_exit "analyze --help exits 0" 0 "$BINARY" analyze --help
assert_stdout_contains "analyze --help shows --compile-commands" "compile-commands" "$BINARY" analyze --help

# Success path
assert_exit "valid input exits 0" 0 "$BINARY" analyze --compile-commands "$TESTDATA/valid/compile_commands.json"
assert_stdout_contains "valid input shows entry count" "1 entries" "$BINARY" analyze --compile-commands "$TESTDATA/valid/compile_commands.json"
assert_exit "only_command exits 0" 0 "$BINARY" analyze --compile-commands "$TESTDATA/only_command/compile_commands.json"
assert_exit "command_and_arguments exits 0" 0 "$BINARY" analyze --compile-commands "$TESTDATA/command_and_arguments/compile_commands.json"
assert_exit "empty_arguments_with_command exits 0" 0 "$BINARY" analyze --compile-commands "$TESTDATA/empty_arguments_with_command/compile_commands.json"

# CLI usage error
assert_exit "missing --compile-commands exits 2" 2 "$BINARY" analyze

# Input file errors
assert_exit "nonexistent file exits 3" 3 "$BINARY" analyze --compile-commands /nonexistent/compile_commands.json
assert_stderr_contains "nonexistent file error message" "cannot open" "$BINARY" analyze --compile-commands /nonexistent/compile_commands.json

# Invalid input data
assert_exit "invalid JSON exits 4" 4 "$BINARY" analyze --compile-commands "$TESTDATA/invalid_syntax/compile_commands.json"
assert_exit "not an array exits 4" 4 "$BINARY" analyze --compile-commands "$TESTDATA/not_an_array/compile_commands.json"
assert_exit "empty array exits 4" 4 "$BINARY" analyze --compile-commands "$TESTDATA/empty/compile_commands.json"
assert_exit "missing fields exits 4" 4 "$BINARY" analyze --compile-commands "$TESTDATA/missing_fields/compile_commands.json"
assert_exit "mixed valid/invalid exits 4" 4 "$BINARY" analyze --compile-commands "$TESTDATA/mixed_valid_invalid/compile_commands.json"

echo ""
if [ "$failures" -gt 0 ]; then
    echo "$failures E2E test(s) FAILED" >&2
    exit 1
else
    echo "All E2E tests passed"
fi
