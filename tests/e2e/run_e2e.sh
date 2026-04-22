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
assert_stdout_contains "--help shows impact subcommand" "impact" "$BINARY" --help

# analyze --help
assert_exit "analyze --help exits 0" 0 "$BINARY" analyze --help
assert_stdout_contains "analyze --help shows --compile-commands" "compile-commands" "$BINARY" analyze --help
assert_stdout_contains "analyze --help shows --top" "top" "$BINARY" analyze --help

# impact --help
assert_exit "impact --help exits 0" 0 "$BINARY" impact --help
assert_stdout_contains "impact --help shows --changed-file" "changed-file" "$BINARY" impact --help

# Analyze success path
assert_exit "M2 analyze exits 0" 0 "$BINARY" analyze --compile-commands "$TESTDATA/m2/basic_project/compile_commands.json" --top 2
assert_stdout_contains "M2 analyze shows ranking" "translation unit ranking" "$BINARY" analyze --compile-commands "$TESTDATA/m2/basic_project/compile_commands.json" --top 2
assert_stdout_contains "M2 analyze shows top limit" "top 2 of 3 translation units" "$BINARY" analyze --compile-commands "$TESTDATA/m2/basic_project/compile_commands.json" --top 2
assert_stdout_contains "M2 analyze shows hotspots" "include hotspots" "$BINARY" analyze --compile-commands "$TESTDATA/m2/basic_project/compile_commands.json" --top 2

# Impact success path
assert_exit "impact source exits 0" 0 "$BINARY" impact --compile-commands "$TESTDATA/m2/basic_project/compile_commands.json" --changed-file src/app/main.cpp
assert_stdout_contains "impact source shows one affected TU" "affected translation units: 1" "$BINARY" impact --compile-commands "$TESTDATA/m2/basic_project/compile_commands.json" --changed-file src/app/main.cpp
assert_exit "impact transitive header exits 0" 0 "$BINARY" impact --compile-commands "$TESTDATA/m2/basic_project/compile_commands.json" --changed-file include/common/shared.h
assert_stdout_contains "impact transitive header is heuristic" "impact analysis for include/common/shared.h \[heuristic\]" "$BINARY" impact --compile-commands "$TESTDATA/m2/basic_project/compile_commands.json" --changed-file include/common/shared.h
assert_stdout_contains "impact transitive header shows three affected TUs" "affected translation units: 3" "$BINARY" impact --compile-commands "$TESTDATA/m2/basic_project/compile_commands.json" --changed-file include/common/shared.h
assert_stdout_contains "impact transitive header surfaces include diagnostics" "warning: could not resolve include" "$BINARY" impact --compile-commands "$TESTDATA/m2/basic_project/compile_commands.json" --changed-file include/common/shared.h
assert_exit "impact path semantics exits 0" 0 "$BINARY" impact --compile-commands "$TESTDATA/m2/basic_project/compile_commands.json" --changed-file ./include/common/../common/config.h
assert_stdout_contains "impact path semantics normalizes lexical path" "impact analysis for include/common/config.h \[heuristic\]" "$BINARY" impact --compile-commands "$TESTDATA/m2/basic_project/compile_commands.json" --changed-file ./include/common/../common/config.h
assert_exit "impact missing file exits 0" 0 "$BINARY" impact --compile-commands "$TESTDATA/m2/basic_project/compile_commands.json" --changed-file include/generated/version.h
assert_stdout_contains "impact missing file shows zero affected TUs" "affected translation units: 0" "$BINARY" impact --compile-commands "$TESTDATA/m2/basic_project/compile_commands.json" --changed-file include/generated/version.h

# Reproducibility and duplicate TU observations
assert_stdout_contains "duplicate analyze shows debug observation" "src/app/main.cpp \[directory: build/debug\]" "$BINARY" analyze --compile-commands "$TESTDATA/m2/duplicate_tu_entries/compile_commands.json" --top 3
assert_stdout_contains "duplicate analyze shows release observation" "src/app/main.cpp \[directory: build/release\]" "$BINARY" analyze --compile-commands "$TESTDATA/m2/duplicate_tu_entries/compile_commands.json" --top 3
assert_exit "duplicate impact exits 0" 0 "$BINARY" impact --compile-commands "$TESTDATA/m2/duplicate_tu_entries/compile_commands.json" --changed-file src/app/main.cpp
assert_stdout_contains "duplicate impact shows two affected observations" "affected translation units: 2" "$BINARY" impact --compile-commands "$TESTDATA/m2/duplicate_tu_entries/compile_commands.json" --changed-file src/app/main.cpp
assert_stdout_contains "duplicate impact shows debug observation" "src/app/main.cpp \[directory: build/debug\] \[direct\]" "$BINARY" impact --compile-commands "$TESTDATA/m2/duplicate_tu_entries/compile_commands.json" --changed-file src/app/main.cpp
assert_stdout_contains "duplicate impact shows release observation" "src/app/main.cpp \[directory: build/release\] \[direct\]" "$BINARY" impact --compile-commands "$TESTDATA/m2/duplicate_tu_entries/compile_commands.json" --changed-file src/app/main.cpp

basic_output=$("$BINARY" analyze --compile-commands "$TESTDATA/m2/basic_project/compile_commands.json" --top 3 2>/dev/null) || true
permuted_output=$("$BINARY" analyze --compile-commands "$TESTDATA/m2/permuted_compile_commands/compile_commands.json" --top 3 2>/dev/null) || true
if [ "$basic_output" = "$permuted_output" ]; then
    echo "PASS: permuted compile_commands produce identical analyze output"
else
    echo "FAIL: permuted compile_commands produce identical analyze output — stdout differed" >&2
    failures=$((failures + 1))
fi

# M1 compatibility
assert_exit "legacy valid input exits 0" 0 "$BINARY" analyze --compile-commands "$TESTDATA/valid/compile_commands.json"
assert_exit "only_command exits 0" 0 "$BINARY" analyze --compile-commands "$TESTDATA/only_command/compile_commands.json"
assert_exit "command_and_arguments exits 0" 0 "$BINARY" analyze --compile-commands "$TESTDATA/command_and_arguments/compile_commands.json"
assert_exit "empty_arguments_with_command exits 0" 0 "$BINARY" analyze --compile-commands "$TESTDATA/empty_arguments_with_command/compile_commands.json"

# CLI usage error
assert_exit "missing --compile-commands exits 2" 2 "$BINARY" analyze
assert_exit "impact missing --changed-file exits 2" 2 "$BINARY" impact --compile-commands "$TESTDATA/m2/basic_project/compile_commands.json"

# Input file errors
assert_exit "nonexistent file exits 3" 3 "$BINARY" analyze --compile-commands /nonexistent/compile_commands.json
assert_stderr_contains "nonexistent file error message" "cannot open" "$BINARY" analyze --compile-commands /nonexistent/compile_commands.json

# Invalid input data
assert_exit "invalid JSON exits 4" 4 "$BINARY" analyze --compile-commands "$TESTDATA/invalid_syntax/compile_commands.json"
assert_exit "impact invalid JSON exits 4" 4 "$BINARY" impact --compile-commands "$TESTDATA/invalid_syntax/compile_commands.json" --changed-file src/main.cpp
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
