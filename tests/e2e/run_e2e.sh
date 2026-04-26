#!/usr/bin/env bash
# End-to-end test runner for cmake-xray binary.
# Usage: run_e2e.sh <binary> <testdata_dir>
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

# --help
assert_exit "--help exits 0" 0 "$BINARY" --help
assert_stdout_contains "--help shows cmake-xray" "cmake-xray" "$BINARY" --help
assert_stdout_contains "--help shows analyze subcommand" "analyze" "$BINARY" --help
assert_stdout_contains "--help shows impact subcommand" "impact" "$BINARY" --help

# analyze --help
assert_exit "analyze --help exits 0" 0 "$BINARY" analyze --help
assert_stdout_contains "analyze --help shows --compile-commands" "compile-commands" "$BINARY" analyze --help
assert_stdout_contains "analyze --help shows --top" "top" "$BINARY" analyze --help
assert_stdout_contains "analyze --help shows --format" "format" "$BINARY" analyze --help
assert_stdout_contains "analyze --help shows --output" "output" "$BINARY" analyze --help

# impact --help
assert_exit "impact --help exits 0" 0 "$BINARY" impact --help
assert_stdout_contains "impact --help shows --changed-file" "changed-file" "$BINARY" impact --help
assert_stdout_contains "impact --help shows --format" "format" "$BINARY" impact --help
assert_stdout_contains "impact --help shows --output" "output" "$BINARY" impact --help

# Golden outputs
assert_exit "M3 analyze console exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json --top 2
assert_stdout_equals_file "M3 analyze console matches golden" tests/e2e/testdata/m3/report_project/analyze-console.txt \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json --top 2
assert_exit "M3 analyze markdown exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json --format markdown --top 2
assert_stdout_equals_file "M3 analyze markdown matches golden" tests/e2e/testdata/m3/report_project/analyze-markdown.md \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json --format markdown --top 2

assert_exit "M3 impact console exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json --changed-file include/common/config.h
assert_stdout_equals_file "M3 impact console matches golden" tests/e2e/testdata/m3/report_impact_header/impact-console.txt \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json --changed-file include/common/config.h
assert_exit "M3 impact markdown exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json --changed-file include/common/config.h --format markdown
assert_stdout_equals_file "M3 impact markdown matches golden" tests/e2e/testdata/m3/report_impact_header/impact-markdown.md \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json --changed-file include/common/config.h --format markdown

assert_exit "M3 direct impact console exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_impact_source/compile_commands.json --changed-file src/app/main.cpp
assert_stdout_equals_file "M3 direct impact console matches golden" tests/e2e/testdata/m3/report_impact_source/impact-console.txt \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_impact_source/compile_commands.json --changed-file src/app/main.cpp
assert_exit "M3 direct impact markdown exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_impact_source/compile_commands.json --changed-file src/app/main.cpp --format markdown
assert_stdout_equals_file "M3 direct impact markdown matches golden" tests/e2e/testdata/m3/report_impact_source/impact-markdown.md \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_impact_source/compile_commands.json --changed-file src/app/main.cpp --format markdown

assert_exit "M3 empty impact console exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_empty_impact/compile_commands.json --changed-file include/generated/version.h
assert_stdout_equals_file "M3 empty impact console matches golden" tests/e2e/testdata/m3/report_empty_impact/impact-console.txt \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_empty_impact/compile_commands.json --changed-file include/generated/version.h
assert_exit "M3 empty impact markdown exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_empty_impact/compile_commands.json --changed-file include/generated/version.h --format markdown
assert_stdout_equals_file "M3 empty impact markdown matches golden" tests/e2e/testdata/m3/report_empty_impact/impact-markdown.md \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_empty_impact/compile_commands.json --changed-file include/generated/version.h --format markdown

assert_exit "M3 diagnostics analyze console exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_diagnostics/compile_commands.json --top 3
assert_stdout_equals_file "M3 diagnostics analyze console matches golden" tests/e2e/testdata/m3/report_diagnostics/analyze-console.txt \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_diagnostics/compile_commands.json --top 3
assert_exit "M3 diagnostics analyze markdown exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_diagnostics/compile_commands.json --format markdown --top 3
assert_stdout_equals_file "M3 diagnostics analyze markdown matches golden" tests/e2e/testdata/m3/report_diagnostics/analyze-markdown.md \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_diagnostics/compile_commands.json --format markdown --top 3

assert_exit "M3 top-limit analyze console exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_top_limit/compile_commands.json --top 1
assert_stdout_equals_file "M3 top-limit analyze console matches golden" tests/e2e/testdata/m3/report_top_limit/analyze-console.txt \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_top_limit/compile_commands.json --top 1
assert_exit "M3 top-limit analyze markdown exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_top_limit/compile_commands.json --format markdown --top 1
assert_stdout_equals_file "M3 top-limit analyze markdown matches golden" tests/e2e/testdata/m3/report_top_limit/analyze-markdown.md \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_top_limit/compile_commands.json --format markdown --top 1

# M4 golden outputs: file-api-only analyze
assert_exit "M4 file-api-only analyze console exits 0" 0 "$BINARY" analyze --cmake-file-api tests/e2e/testdata/m4/file_api_only/build
assert_stdout_equals_file "M4 file-api-only analyze console matches golden" tests/e2e/testdata/m4/file_api_only/analyze-console.txt \
    "$BINARY" analyze --cmake-file-api tests/e2e/testdata/m4/file_api_only/build
assert_exit "M4 file-api-only analyze markdown exits 0" 0 "$BINARY" analyze --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --format markdown
assert_stdout_equals_file "M4 file-api-only analyze markdown matches golden" tests/e2e/testdata/m4/file_api_only/analyze-markdown.md \
    "$BINARY" analyze --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --format markdown

# M4 golden outputs: file-api-only impact
assert_exit "M4 file-api-only impact console exits 0" 0 "$BINARY" impact --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --changed-file include/common/config.h
assert_stdout_equals_file "M4 file-api-only impact console matches golden" tests/e2e/testdata/m4/file_api_only/impact-console.txt \
    "$BINARY" impact --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --changed-file include/common/config.h
assert_exit "M4 file-api-only impact markdown exits 0" 0 "$BINARY" impact --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --changed-file include/common/config.h --format markdown
assert_stdout_equals_file "M4 file-api-only impact markdown matches golden" tests/e2e/testdata/m4/file_api_only/impact-markdown.md \
    "$BINARY" impact --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --changed-file include/common/config.h --format markdown

# M4 golden outputs: multi-target analyze
assert_exit "M4 multi-target analyze console exits 0" 0 "$BINARY" analyze --cmake-file-api tests/e2e/testdata/m4/multi_target/build
assert_stdout_equals_file "M4 multi-target analyze console matches golden" tests/e2e/testdata/m4/multi_target/analyze-console.txt \
    "$BINARY" analyze --cmake-file-api tests/e2e/testdata/m4/multi_target/build
assert_exit "M4 multi-target analyze markdown exits 0" 0 "$BINARY" analyze --cmake-file-api tests/e2e/testdata/m4/multi_target/build --format markdown
assert_stdout_equals_file "M4 multi-target analyze markdown matches golden" tests/e2e/testdata/m4/multi_target/analyze-markdown.md \
    "$BINARY" analyze --cmake-file-api tests/e2e/testdata/m4/multi_target/build --format markdown

# M4 golden outputs: mixed-path analyze with partial targets
assert_exit "M4 mixed-path analyze console exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m4/partial_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/partial_targets/build
assert_stdout_equals_file "M4 mixed-path analyze console matches golden" tests/e2e/testdata/m4/partial_targets/analyze-console.txt \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m4/partial_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/partial_targets/build
assert_exit "M4 mixed-path analyze markdown exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m4/partial_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/partial_targets/build --format markdown
assert_stdout_equals_file "M4 mixed-path analyze markdown matches golden" tests/e2e/testdata/m4/partial_targets/analyze-markdown.md \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m4/partial_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/partial_targets/build --format markdown

# M4 golden outputs: mixed-path impact with partial targets (relative path, no hit)
assert_exit "M4 mixed-path impact console exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m4/partial_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/partial_targets/build --changed-file src/main.cpp
assert_stdout_equals_file "M4 mixed-path impact console matches golden" tests/e2e/testdata/m4/partial_targets/impact-console.txt \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m4/partial_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/partial_targets/build --changed-file src/main.cpp
assert_exit "M4 mixed-path impact markdown exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m4/partial_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/partial_targets/build --changed-file src/main.cpp --format markdown
assert_stdout_equals_file "M4 mixed-path impact markdown matches golden" tests/e2e/testdata/m4/partial_targets/impact-markdown.md \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m4/partial_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/partial_targets/build --changed-file src/main.cpp --format markdown

# M4 golden outputs: mixed-path impact with actual target hit (absolute path)
assert_exit "M4 mixed-path direct impact console exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m4/partial_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/partial_targets/build --changed-file /project/src/main.cpp
assert_stdout_equals_file "M4 mixed-path direct impact console matches golden" tests/e2e/testdata/m4/partial_targets/impact-direct-console.txt \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m4/partial_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/partial_targets/build --changed-file /project/src/main.cpp
assert_exit "M4 mixed-path direct impact markdown exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m4/partial_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/partial_targets/build --changed-file /project/src/main.cpp --format markdown
assert_stdout_equals_file "M4 mixed-path direct impact markdown matches golden" tests/e2e/testdata/m4/partial_targets/impact-direct-markdown.md \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m4/partial_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/partial_targets/build --changed-file /project/src/main.cpp --format markdown

# M5 JSON goldens (analyze) — byte-stable diff against golden AND schema
# validation of the actually produced bytes per docs/plan-M5-1-2.md:367.
assert_exit "M5 json analyze compile-db-only exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --format json
assert_stdout_equals_file "M5 json analyze compile-db-only matches golden" tests/e2e/testdata/m5/json-reports/analyze_compile_db_only.json \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --format json
assert_json_stdout_validates "M5 json analyze compile-db-only validates against schema" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --format json

assert_exit "M5 json analyze file-api build-dir exits 0" 0 "$BINARY" analyze --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --format json
assert_stdout_equals_file "M5 json analyze file-api build-dir matches golden" tests/e2e/testdata/m5/json-reports/analyze_file_api_build_dir.json \
    "$BINARY" analyze --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --format json
assert_json_stdout_validates "M5 json analyze file-api build-dir validates against schema" \
    "$BINARY" analyze --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --format json

assert_exit "M5 json analyze file-api reply-dir exits 0" 0 "$BINARY" analyze --cmake-file-api tests/e2e/testdata/m4/file_api_only/build/.cmake/api/v1/reply --format json
assert_stdout_equals_file "M5 json analyze file-api reply-dir matches golden" tests/e2e/testdata/m5/json-reports/analyze_file_api_reply_dir.json \
    "$BINARY" analyze --cmake-file-api tests/e2e/testdata/m4/file_api_only/build/.cmake/api/v1/reply --format json
assert_json_stdout_validates "M5 json analyze file-api reply-dir validates against schema" \
    "$BINARY" analyze --cmake-file-api tests/e2e/testdata/m4/file_api_only/build/.cmake/api/v1/reply --format json

assert_exit "M5 json analyze mixed-input exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m4/with_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/with_targets/build --format json
assert_stdout_equals_file "M5 json analyze mixed-input matches golden" tests/e2e/testdata/m5/json-reports/analyze_mixed_input.json \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m4/with_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/with_targets/build --format json
assert_json_stdout_validates "M5 json analyze mixed-input validates against schema" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m4/with_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/with_targets/build --format json

assert_exit "M5 json analyze ranking truncated exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --format json --top 2
assert_stdout_equals_file "M5 json analyze ranking truncated matches golden" tests/e2e/testdata/m5/json-reports/analyze_ranking_truncated.json \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --format json --top 2
assert_json_stdout_validates "M5 json analyze ranking truncated validates against schema" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --format json --top 2

assert_exit "M5 json analyze hotspot truncated exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --format json --top 1
assert_stdout_equals_file "M5 json analyze hotspot truncated matches golden" tests/e2e/testdata/m5/json-reports/analyze_hotspot_truncated.json \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --format json --top 1
assert_json_stdout_validates "M5 json analyze hotspot truncated validates against schema" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --format json --top 1

# M5 JSON goldens (impact)
assert_exit "M5 json impact compile-db relative exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --format json
assert_stdout_equals_file "M5 json impact compile-db relative matches golden" tests/e2e/testdata/m5/json-reports/impact_compile_db_relative.json \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --format json
assert_json_stdout_validates "M5 json impact compile-db relative validates against schema" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --format json

assert_exit "M5 json impact file-api relative exits 0" 0 "$BINARY" impact --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --changed-file src/main.cpp --format json
assert_stdout_equals_file "M5 json impact file-api relative matches golden" tests/e2e/testdata/m5/json-reports/impact_file_api_relative.json \
    "$BINARY" impact --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --changed-file src/main.cpp --format json
assert_json_stdout_validates "M5 json impact file-api relative validates against schema" \
    "$BINARY" impact --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --changed-file src/main.cpp --format json

assert_exit "M5 json impact mixed-input relative exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m4/with_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/with_targets/build --changed-file include/common/shared.h --format json
assert_stdout_equals_file "M5 json impact mixed-input relative matches golden" tests/e2e/testdata/m5/json-reports/impact_mixed_input_relative.json \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m4/with_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/with_targets/build --changed-file include/common/shared.h --format json
assert_json_stdout_validates "M5 json impact mixed-input relative validates against schema" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m4/with_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/with_targets/build --changed-file include/common/shared.h --format json

# cli_absolute provenance requires --changed-file to be absolute. POSIX
# accepts /project/... as absolute; Windows requires a drive letter
# (C:/project/...). Use a per-platform argument and a per-platform golden so
# both hosts exercise the cli_absolute branch byte-stably; the goldens differ
# only in the changed_file string under inputs.
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        impact_absolute_arg="C:/project/src/app/main.cpp"
        impact_absolute_golden="tests/e2e/testdata/m5/json-reports/impact_absolute_windows.json"
        ;;
    *)
        impact_absolute_arg="/project/src/app/main.cpp"
        impact_absolute_golden="tests/e2e/testdata/m5/json-reports/impact_absolute.json"
        ;;
esac
assert_exit "M5 json impact absolute changed-file exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file "$impact_absolute_arg" --format json
assert_stdout_equals_file "M5 json impact absolute changed-file matches golden" "$impact_absolute_golden" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file "$impact_absolute_arg" --format json
assert_json_stdout_validates "M5 json impact absolute changed-file validates against schema" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file "$impact_absolute_arg" --format json

# Markdown file output
report_dir="$(native_path "$(mktemp -d)")"
report_file="$report_dir/analyze.md"
assert_exit "markdown output file exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json --format markdown --output "$report_file" --top 2
if [ -f "$report_file" ]; then
    assert_file_equals "markdown output file matches golden" "$report_file" tests/e2e/testdata/m3/report_project/analyze-markdown.md
else
    echo "FAIL: markdown output file was not created" >&2
    failures=$((failures + 1))
fi
if find "$report_dir" -maxdepth 1 -name '.cmake-xray-*' | grep -q .; then
    echo "FAIL: markdown output left temporary files behind" >&2
    failures=$((failures + 1))
else
    echo "PASS: markdown output leaves no temporary files behind"
fi
rm -rf "$report_dir"

# Impact and path semantics
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

permute_dir="$(native_path "$(mktemp -d)")"
cp -R tests/e2e/testdata/m2/basic_project/. "$permute_dir/"
baseline_markdown=$("$BINARY" analyze --compile-commands "$permute_dir/compile_commands.json" --format markdown --top 2 2>/dev/null) || true
cp tests/e2e/testdata/m2/permuted_compile_commands/compile_commands.json "$permute_dir/compile_commands.json"
permuted_markdown=$("$BINARY" analyze --compile-commands "$permute_dir/compile_commands.json" --format markdown --top 2 2>/dev/null) || true
if [ "$baseline_markdown" = "$permuted_markdown" ]; then
    echo "PASS: permuted compile_commands produce identical markdown analyze output"
else
    echo "FAIL: permuted compile_commands produce identical markdown analyze output — stdout differed" >&2
    failures=$((failures + 1))
fi

cp -R tests/e2e/testdata/m2/basic_project/. "$permute_dir/"
baseline_impact_markdown=$("$BINARY" impact --compile-commands "$permute_dir/compile_commands.json" --changed-file include/common/shared.h --format markdown 2>/dev/null) || true
cp tests/e2e/testdata/m2/permuted_compile_commands/compile_commands.json "$permute_dir/compile_commands.json"
permuted_impact_markdown=$("$BINARY" impact --compile-commands "$permute_dir/compile_commands.json" --changed-file include/common/shared.h --format markdown 2>/dev/null) || true
if [ "$baseline_impact_markdown" = "$permuted_impact_markdown" ]; then
    echo "PASS: permuted compile_commands produce identical markdown impact output"
else
    echo "FAIL: permuted compile_commands produce identical markdown impact output — stdout differed" >&2
    failures=$((failures + 1))
fi
rm -rf "$permute_dir"

# M1 compatibility
assert_exit "legacy valid input exits 0" 0 "$BINARY" analyze --compile-commands "$TESTDATA/valid/compile_commands.json"
assert_exit "only_command exits 0" 0 "$BINARY" analyze --compile-commands "$TESTDATA/only_command/compile_commands.json"
assert_exit "command_and_arguments exits 0" 0 "$BINARY" analyze --compile-commands "$TESTDATA/command_and_arguments/compile_commands.json"
assert_exit "empty_arguments_with_command exits 0" 0 "$BINARY" analyze --compile-commands "$TESTDATA/empty_arguments_with_command/compile_commands.json"

# CLI usage error
assert_exit "missing --compile-commands exits 2" 2 "$BINARY" analyze
assert_exit "impact missing --changed-file exits 2" 2 "$BINARY" impact --compile-commands "$TESTDATA/m2/basic_project/compile_commands.json"
assert_exit "output without markdown exits 2" 2 "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json --output report.md

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
