#!/usr/bin/env bash
# AP M5-1.5 Tranche D Folge-Refactor: split run_e2e.sh into focused suites
# so `ctest --parallel` schedules them concurrently. This script focuses on
# M5 artifact goldens: JSON (with schema-validity reuse), DOT (with python
# stdlib syntax validation) and HTML for analyze and impact, plus the
# per-platform absolute-changed-file goldens. Sources run_e2e_lib.sh for
# shared helpers.
script_dir="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=run_e2e_lib.sh
. "$script_dir/run_e2e_lib.sh"

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

# M5 DOT goldens (analyze) — byte-stable diff against golden AND syntax
# validation of the produced bytes per docs/plan-M5-1-3.md.
assert_exit "M5 dot analyze compile-db-only exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/multi_tu_compile_db/compile_commands.json --format dot --top 2
assert_stdout_equals_file "M5 dot analyze compile-db-only matches golden" tests/e2e/testdata/m5/dot-reports/analyze_compile_db_only.dot \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/multi_tu_compile_db/compile_commands.json --format dot --top 2
assert_dot_stdout_validates "M5 dot analyze compile-db-only validates against syntax gate" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/multi_tu_compile_db/compile_commands.json --format dot --top 2

assert_exit "M5 dot analyze file-api-only exits 0" 0 "$BINARY" analyze --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --format dot --top 2
assert_stdout_equals_file "M5 dot analyze file-api-only matches golden" tests/e2e/testdata/m5/dot-reports/analyze_file_api_only.dot \
    "$BINARY" analyze --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --format dot --top 2
assert_dot_stdout_validates "M5 dot analyze file-api-only validates against syntax gate" \
    "$BINARY" analyze --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --format dot --top 2

assert_exit "M5 dot analyze mixed-input exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m4/partial_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/partial_targets/build --format dot --top 2
assert_stdout_equals_file "M5 dot analyze mixed-input matches golden" tests/e2e/testdata/m5/dot-reports/analyze_mixed_input.dot \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m4/partial_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/partial_targets/build --format dot --top 2
assert_dot_stdout_validates "M5 dot analyze mixed-input validates against syntax gate" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m4/partial_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/partial_targets/build --format dot --top 2

assert_exit "M5 dot analyze default top exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/multi_tu_compile_db/compile_commands.json --format dot
assert_stdout_equals_file "M5 dot analyze default top matches golden" tests/e2e/testdata/m5/dot-reports/analyze_default_top.dot \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/multi_tu_compile_db/compile_commands.json --format dot
assert_dot_stdout_validates "M5 dot analyze default top validates against syntax gate" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/multi_tu_compile_db/compile_commands.json --format dot

# truncating_compile_db wires up real on-disk source files so the include
# resolver actually produces hotspots; that lets these cases exercise
# context_limit truncation end-to-end. Unique_keys embed the resolved
# working-directory path and are normalized to "HOSTPATH" via
# assert_dot_stdout_equals_file. Simultaneous binding of node_limit and
# edge_limit is covered by tests/adapters/test_dot_report_adapter.cpp; the
# e2e goldens here label both budgets but only context_limit is binding.
assert_exit "M5 dot analyze top-truncated exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/truncating_compile_db/compile_commands.json --format dot --top 1
assert_dot_stdout_equals_file "M5 dot analyze top-truncated matches golden" tests/e2e/testdata/m5/dot-reports/analyze_top_truncated.dot \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/truncating_compile_db/compile_commands.json --format dot --top 1
assert_dot_stdout_validates "M5 dot analyze top-truncated validates against syntax gate" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/truncating_compile_db/compile_commands.json --format dot --top 1

assert_exit "M5 dot analyze budget-truncated exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/truncating_compile_db/compile_commands.json --format dot --top 6
assert_dot_stdout_equals_file "M5 dot analyze budget-truncated matches golden" tests/e2e/testdata/m5/dot-reports/analyze_budget_truncated.dot \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/truncating_compile_db/compile_commands.json --format dot --top 6
assert_dot_stdout_validates "M5 dot analyze budget-truncated validates against syntax gate" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/truncating_compile_db/compile_commands.json --format dot --top 6

# Backslash-as-path-separator behaviour is platform-dependent: on POSIX, a
# literal backslash is part of the source filename and ends up DOT-escaped as
# `back\\slash.cpp`; on Windows the path resolver normalizes \ → / before
# reaching the DOT adapter, so the rendered path becomes `back/slash.cpp`.
# Per-platform golden keeps both branches host-stable; backslash escaping is
# additionally exercised in tests/adapters/test_dot_report_adapter.cpp via a
# unit-test-controlled string.
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        analyze_escaping_golden="tests/e2e/testdata/m5/dot-reports/analyze_escaping_windows.dot"
        ;;
    *)
        analyze_escaping_golden="tests/e2e/testdata/m5/dot-reports/analyze_escaping.dot"
        ;;
esac
assert_exit "M5 dot analyze escaping exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/escape_paths/compile_commands.json --format dot --top 5
assert_stdout_equals_file "M5 dot analyze escaping matches golden" "$analyze_escaping_golden" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/escape_paths/compile_commands.json --format dot --top 5
assert_dot_stdout_validates "M5 dot analyze escaping validates against syntax gate" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/escape_paths/compile_commands.json --format dot --top 5

# M5 DOT goldens (impact)
assert_exit "M5 dot impact compile-db direct exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m5/dot-fixtures/multi_tu_compile_db/compile_commands.json --changed-file /project/src/app.cpp --format dot
assert_stdout_equals_file "M5 dot impact compile-db direct matches golden" tests/e2e/testdata/m5/dot-reports/impact_compile_db_direct.dot \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m5/dot-fixtures/multi_tu_compile_db/compile_commands.json --changed-file /project/src/app.cpp --format dot
assert_dot_stdout_validates "M5 dot impact compile-db direct validates against syntax gate" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m5/dot-fixtures/multi_tu_compile_db/compile_commands.json --changed-file /project/src/app.cpp --format dot

# The file API-only impact case differs by platform because /project/...
# is absolute on POSIX (kept verbatim) but treated as relative to the file
# API source root on Windows (where it resolves to "src/main.cpp"). Two
# goldens keep both branches byte-stable.
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        impact_file_api_only_golden="tests/e2e/testdata/m5/dot-reports/impact_file_api_only_windows.dot"
        ;;
    *)
        impact_file_api_only_golden="tests/e2e/testdata/m5/dot-reports/impact_file_api_only.dot"
        ;;
esac
assert_exit "M5 dot impact file-api-only exits 0" 0 "$BINARY" impact --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --changed-file /project/src/main.cpp --format dot
assert_stdout_equals_file "M5 dot impact file-api-only matches golden" "$impact_file_api_only_golden" \
    "$BINARY" impact --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --changed-file /project/src/main.cpp --format dot
assert_dot_stdout_validates "M5 dot impact file-api-only validates against syntax gate" \
    "$BINARY" impact --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --changed-file /project/src/main.cpp --format dot

assert_exit "M5 dot impact mixed-input exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m4/partial_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/partial_targets/build --changed-file /project/src/main.cpp --format dot
assert_stdout_equals_file "M5 dot impact mixed-input matches golden" tests/e2e/testdata/m5/dot-reports/impact_mixed_input.dot \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m4/partial_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/partial_targets/build --changed-file /project/src/main.cpp --format dot
assert_dot_stdout_validates "M5 dot impact mixed-input validates against syntax gate" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m4/partial_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/partial_targets/build --changed-file /project/src/main.cpp --format dot

assert_exit "M5 dot impact budget untruncated exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m5/dot-fixtures/many_tu_compile_db/compile_commands.json --changed-file /project/src/file_00.cpp --format dot
assert_stdout_equals_file "M5 dot impact budget untruncated matches golden" tests/e2e/testdata/m5/dot-reports/impact_budget_untruncated.dot \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m5/dot-fixtures/many_tu_compile_db/compile_commands.json --changed-file /project/src/file_00.cpp --format dot
assert_dot_stdout_validates "M5 dot impact budget untruncated validates against syntax gate" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m5/dot-fixtures/many_tu_compile_db/compile_commands.json --changed-file /project/src/file_00.cpp --format dot

# Heuristic-edge style coverage: m2/basic_project transitively includes
# common/shared.h via common/config.h, so impact reports it as 3 heuristic
# TUs and the changed_file→TU edges carry style="dashed".
assert_exit "M5 dot impact heuristic edges exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --format dot
assert_dot_stdout_equals_file "M5 dot impact heuristic edges matches golden" tests/e2e/testdata/m5/dot-reports/impact_heuristic_edges.dot \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --format dot
assert_dot_stdout_validates "M5 dot impact heuristic edges validates against syntax gate" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --format dot

# Heuristic-edge variant with File API targets exercises both
# changed_file→heuristic_tu and heuristic_tu→target dashed edges plus the
# target node impact="heuristic" attribute.
assert_exit "M5 dot impact heuristic targets exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m4/with_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/with_targets/build --changed-file include/common/shared.h --format dot
assert_dot_stdout_equals_file "M5 dot impact heuristic targets matches golden" tests/e2e/testdata/m5/dot-reports/impact_heuristic_targets.dot \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m4/with_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/with_targets/build --changed-file include/common/shared.h --format dot
assert_dot_stdout_validates "M5 dot impact heuristic targets validates against syntax gate" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m4/with_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/with_targets/build --changed-file include/common/shared.h --format dot

# Real impact node_limit truncation: 110 source files all #include common.h,
# so impact --changed-file include/common.h yields 110 heuristic TUs and
# changed_file + 99 TUs hit node_limit=100 (1 changed_file + 100 TU caps the
# graph, the 100th onwards drop and graph_truncated=true).
assert_exit "M5 dot impact truncated exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m5/dot-fixtures/impact_truncating_compile_db/compile_commands.json --changed-file include/common.h --format dot
assert_dot_stdout_equals_file "M5 dot impact truncated matches golden" tests/e2e/testdata/m5/dot-reports/impact_truncated.dot \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m5/dot-fixtures/impact_truncating_compile_db/compile_commands.json --changed-file include/common.h --format dot
assert_dot_stdout_validates "M5 dot impact truncated validates against syntax gate" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m5/dot-fixtures/impact_truncating_compile_db/compile_commands.json --changed-file include/common.h --format dot

# cli_absolute provenance for --changed-file --format dot: m2/basic_project
# does not match the absolute path, so the graph contains only the
# changed_file node. Mirrors the JSON impact_absolute / impact_absolute_windows
# split — Linux passes /project/..., Windows requires C:/project/... .
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        dot_impact_absolute_arg="C:/project/src/app/main.cpp"
        dot_impact_absolute_golden="tests/e2e/testdata/m5/dot-reports/impact_absolute_windows.dot"
        ;;
    *)
        dot_impact_absolute_arg="/project/src/app/main.cpp"
        dot_impact_absolute_golden="tests/e2e/testdata/m5/dot-reports/impact_absolute.dot"
        ;;
esac
assert_exit "M5 dot impact absolute changed-file exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file "$dot_impact_absolute_arg" --format dot
assert_stdout_equals_file "M5 dot impact absolute changed-file matches golden" "$dot_impact_absolute_golden" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file "$dot_impact_absolute_arg" --format dot
assert_dot_stdout_validates "M5 dot impact absolute changed-file validates against syntax gate" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file "$dot_impact_absolute_arg" --format dot

# M5 HTML goldens (analyze) — byte-stable diff against golden per
# docs/plan-M5-1-4.md Tranche C.1. AP M5-1.4 ships no separate HTML structure
# validator script (the contract is locked in via tests/adapters/test_html_report_adapter.cpp
# and the byte-stable goldens here); a future Tranche D may add an optional
# tests/validate_html_reports.py if a structure smoke is needed beyond pure
# golden diffs.
assert_exit "M5 html analyze compile-db-only exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --format html --top 2
assert_stdout_equals_file "M5 html analyze compile-db-only matches golden" tests/e2e/testdata/m5/html-reports/analyze_compile_db_only.html \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --format html --top 2

assert_exit "M5 html analyze file-api-only exits 0" 0 "$BINARY" analyze --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --format html --top 2
assert_stdout_equals_file "M5 html analyze file-api-only matches golden" tests/e2e/testdata/m5/html-reports/analyze_file_api_only.html \
    "$BINARY" analyze --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --format html --top 2

assert_exit "M5 html analyze mixed-input exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m4/with_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/with_targets/build --format html --top 2
assert_stdout_equals_file "M5 html analyze mixed-input matches golden" tests/e2e/testdata/m5/html-reports/analyze_mixed_input.html \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m4/with_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/with_targets/build --format html --top 2

# Default --top (no flag) exercises the CLI's effective top value (10) in the
# HTML pipeline so the golden locks in the default-top contract.
assert_exit "M5 html analyze default top exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/multi_tu_compile_db/compile_commands.json --format html
assert_stdout_equals_file "M5 html analyze default top matches golden" tests/e2e/testdata/m5/html-reports/analyze_default_top.html \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/multi_tu_compile_db/compile_commands.json --format html

assert_exit "M5 html analyze top untruncated exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --format html --top 10
assert_stdout_equals_file "M5 html analyze top untruncated matches golden" tests/e2e/testdata/m5/html-reports/analyze_top_untruncated.html \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --format html --top 10

# truncating_compile_db has 30 TUs and 6 hotspots with 6 affected TUs each.
# --top 1 truncates ranking (1 of 30), hotspot list (1 of 6) and per-hotspot
# context (1 of 6); --top 3 keeps ranking and hotspot truncation but emphasises
# per-hotspot context truncation (3 of 6 per hotspot).
assert_exit "M5 html analyze top truncated exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/truncating_compile_db/compile_commands.json --format html --top 1
assert_stdout_equals_file "M5 html analyze top truncated matches golden" tests/e2e/testdata/m5/html-reports/analyze_top_truncated.html \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/truncating_compile_db/compile_commands.json --format html --top 1

assert_exit "M5 html analyze hotspot context truncated exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/truncating_compile_db/compile_commands.json --format html --top 3
assert_stdout_equals_file "M5 html analyze hotspot context truncated matches golden" tests/e2e/testdata/m5/html-reports/analyze_hotspot_context_truncated.html \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/truncating_compile_db/compile_commands.json --format html --top 3

# directory_contexts exercises the contract requirement that translation units
# with identical source_path stay distinguishable through the Directory
# column, plus the "No include hotspots to report." leersatz. Together with
# analyze_default_top (3 leersaetze on multi_tu_compile_db) this discharges
# plan list item "leeres Ergebnis mit dokumentierten Leersaetzen" — see the
# manifest header for the full mapping.
assert_exit "M5 html analyze directory contexts exits 0" 0 "$BINARY" analyze --cmake-file-api tests/e2e/testdata/m4/directory_contexts/build --format html --top 5
assert_stdout_equals_file "M5 html analyze directory contexts matches golden" tests/e2e/testdata/m5/html-reports/analyze_directory_contexts.html \
    "$BINARY" analyze --cmake-file-api tests/e2e/testdata/m4/directory_contexts/build --format html --top 5

# m5/html-fixtures/escape_paths is HTML-specific; it embeds <, >, &, ", ',
# backslash, newline and <script>-style strings so the golden locks in the
# documented escape rules from docs/report-html.md. Backslash-as-path-separator
# behaviour is platform-dependent: on POSIX, a literal backslash is part of
# the source filename and renders as `back\slash.cpp`; on Windows the path
# resolver normalises \ -> / before reaching the HTML adapter, so the rendered
# path becomes `back/slash.cpp`. The split mirrors the DOT escaping golden
# pair (analyze_escaping[_windows].dot) - any divergence beyond the path
# separator points at an HTML-adapter escape bug, not a fixture issue.
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        html_escaping_golden="tests/e2e/testdata/m5/html-reports/analyze_escaping_windows.html"
        ;;
    *)
        html_escaping_golden="tests/e2e/testdata/m5/html-reports/analyze_escaping.html"
        ;;
esac
assert_exit "M5 html analyze escaping exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/html-fixtures/escape_paths/compile_commands.json --format html --top 5
assert_stdout_equals_file "M5 html analyze escaping matches golden" "$html_escaping_golden" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/html-fixtures/escape_paths/compile_commands.json --format html --top 5

# M5 HTML goldens (impact)
assert_exit "M5 html impact compile-db relative exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --format html
assert_stdout_equals_file "M5 html impact compile-db relative matches golden" tests/e2e/testdata/m5/html-reports/impact_compile_db_relative.html \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --format html

assert_exit "M5 html impact file-api relative exits 0" 0 "$BINARY" impact --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --changed-file src/main.cpp --format html
assert_stdout_equals_file "M5 html impact file-api relative matches golden" tests/e2e/testdata/m5/html-reports/impact_file_api_relative.html \
    "$BINARY" impact --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --changed-file src/main.cpp --format html

assert_exit "M5 html impact mixed-input exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m4/with_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/with_targets/build --changed-file include/common/shared.h --format html
assert_stdout_equals_file "M5 html impact mixed-input matches golden" tests/e2e/testdata/m5/html-reports/impact_mixed_input.html \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m4/with_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/with_targets/build --changed-file include/common/shared.h --format html

# Direct TUs + direct targets path; this case has no diagnostics so it covers
# the "No diagnostics." leersatz alongside the direct kind sections.
assert_exit "M5 html impact direct targets exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m4/with_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/with_targets/build --changed-file src/app/main.cpp --format html
assert_stdout_equals_file "M5 html impact direct targets matches golden" tests/e2e/testdata/m5/html-reports/impact_direct_targets.html \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m4/with_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/with_targets/build --changed-file src/app/main.cpp --format html

# No matching translation units → all four affected sections render their
# leersaetze (no directly/heuristically affected TUs/targets).
assert_exit "M5 html impact no affected targets exits 0" 0 "$BINARY" impact --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --changed-file include/common/shared.h --format html
assert_stdout_equals_file "M5 html impact no affected targets matches golden" tests/e2e/testdata/m5/html-reports/impact_no_affected_targets.html \
    "$BINARY" impact --cmake-file-api tests/e2e/testdata/m4/file_api_only/build --changed-file include/common/shared.h --format html

# cli_absolute provenance for HTML mirrors the JSON/DOT split: POSIX accepts
# /project/... as absolute, Windows requires C:/project/.... Same fixture,
# per-platform argument and per-platform golden so both hosts exercise the
# cli_absolute branch byte-stably; the goldens differ only in changed_file.
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        html_impact_absolute_arg="C:/project/src/app/main.cpp"
        html_impact_absolute_golden="tests/e2e/testdata/m5/html-reports/impact_absolute_windows.html"
        ;;
    *)
        html_impact_absolute_arg="/project/src/app/main.cpp"
        html_impact_absolute_golden="tests/e2e/testdata/m5/html-reports/impact_absolute.html"
        ;;
esac
assert_exit "M5 html impact absolute changed-file exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file "$html_impact_absolute_arg" --format html
assert_stdout_equals_file "M5 html impact absolute changed-file matches golden" "$html_impact_absolute_golden" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file "$html_impact_absolute_arg" --format html

# AP M5-1.7 Tranche C.1: --output-Pflicht-Smokes. Plan-Pflichtkommandoliste
# verlangt fuer json/dot/html × analyze/impact, dass die echte Binary mit
# `--output <path>` eine Datei schreibt, leeres stdout liefert und die Datei
# das jeweilige Formatgate oder Golden erfuellt. Eine Smoke pro
# (Subcommand, Format)-Kombination reicht; tiefergehende Variantenabdeckung
# bleibt bei den stdout-Smokes oben. Pfade gehen ueber native_path, damit
# MSYS-Bash auf Windows den Native-Pfad an die Binary uebergibt.
output_smoke_dir="$(mktemp -d)"

# JSON --output (analyze + impact)
json_analyze_target="$output_smoke_dir/analyze_json.out"
assert_output_file_writes_empty_stdout "M5 --output json analyze writes file with empty stdout" \
    "$json_analyze_target" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --format json --output "$(native_path "$json_analyze_target")"
assert_file_equals "M5 --output json analyze file matches golden" \
    "$json_analyze_target" tests/e2e/testdata/m5/json-reports/analyze_compile_db_only.json
assert_schema_validates "M5 --output json analyze file validates against schema" \
    "$json_analyze_target"

json_impact_target="$output_smoke_dir/impact_json.out"
assert_output_file_writes_empty_stdout "M5 --output json impact writes file with empty stdout" \
    "$json_impact_target" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --format json --output "$(native_path "$json_impact_target")"
assert_file_equals "M5 --output json impact file matches golden" \
    "$json_impact_target" tests/e2e/testdata/m5/json-reports/impact_compile_db_relative.json
assert_schema_validates "M5 --output json impact file validates against schema" \
    "$json_impact_target"

# DOT --output (analyze + impact). The multi_tu_compile_db fixture is
# synthetic (/project/... paths), so the DOT goldens are byte-stable across
# hosts and assert_file_equals is exact. Real-host-path DOT smokes (with
# unique_key drift) would use assert_dot_file_equals_file from
# run_e2e_lib.sh, kept as a helper for future Tranche-D additions.
dot_analyze_target="$output_smoke_dir/analyze_dot.out"
assert_output_file_writes_empty_stdout "M5 --output dot analyze writes file with empty stdout" \
    "$dot_analyze_target" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/multi_tu_compile_db/compile_commands.json --format dot --top 2 --output "$(native_path "$dot_analyze_target")"
assert_file_equals "M5 --output dot analyze file matches golden" \
    "$dot_analyze_target" tests/e2e/testdata/m5/dot-reports/analyze_compile_db_only.dot
assert_dot_syntax_validates "M5 --output dot analyze file validates against DOT contract" \
    "$dot_analyze_target"

dot_impact_target="$output_smoke_dir/impact_dot.out"
assert_output_file_writes_empty_stdout "M5 --output dot impact writes file with empty stdout" \
    "$dot_impact_target" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m5/dot-fixtures/multi_tu_compile_db/compile_commands.json --changed-file /project/src/app.cpp --format dot --output "$(native_path "$dot_impact_target")"
assert_file_equals "M5 --output dot impact file matches golden" \
    "$dot_impact_target" tests/e2e/testdata/m5/dot-reports/impact_compile_db_direct.dot
assert_dot_syntax_validates "M5 --output dot impact file validates against DOT contract" \
    "$dot_impact_target"

# HTML --output (analyze + impact)
html_analyze_target="$output_smoke_dir/analyze_html.out"
assert_output_file_writes_empty_stdout "M5 --output html analyze writes file with empty stdout" \
    "$html_analyze_target" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --format html --top 2 --output "$(native_path "$html_analyze_target")"
assert_file_equals "M5 --output html analyze file matches golden" \
    "$html_analyze_target" tests/e2e/testdata/m5/html-reports/analyze_compile_db_only.html
assert_html_file_validates "M5 --output html analyze file validates against HTML contract" \
    "$html_analyze_target"

html_impact_target="$output_smoke_dir/impact_html.out"
assert_output_file_writes_empty_stdout "M5 --output html impact writes file with empty stdout" \
    "$html_impact_target" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --format html --output "$(native_path "$html_impact_target")"
assert_file_equals "M5 --output html impact file matches golden" \
    "$html_impact_target" tests/e2e/testdata/m5/html-reports/impact_compile_db_relative.html
assert_html_file_validates "M5 --output html impact file validates against HTML contract" \
    "$html_impact_target"

rm -rf "$output_smoke_dir"

echo ""
if [ "$failures" -gt 0 ]; then
    echo "$failures E2E test(s) FAILED" >&2
    exit 1
else
    echo "All E2E tests passed"
fi
