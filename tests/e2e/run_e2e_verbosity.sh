#!/usr/bin/env bash
# AP M5-1.5 Tranche D Folge-Refactor: split run_e2e.sh into focused suites
# so `ctest --parallel` schedules them concurrently. This script focuses on
# the Tranche C.2 verbosity smokes: Console quiet/verbose goldens plus
# artifact-mode byte-stability and the exact 7-/8-line verbose stderr
# sequences for analyze and impact across markdown/json/dot/html. Sources
# run_e2e_lib.sh for shared helpers and the C.2 stderr/file assertions.
script_dir="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=run_e2e_lib.sh
. "$script_dir/run_e2e_lib.sh"

# Console quiet binary smokes
assert_exit "M5 console analyze --quiet exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json --quiet --top 2
assert_stdout_equals_file "M5 console analyze --quiet matches console-analyze-quiet golden" tests/e2e/testdata/m5/verbosity/console-analyze-quiet.txt \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json --quiet --top 2
assert_stderr_empty "M5 console analyze --quiet keeps stderr empty" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json --quiet --top 2
assert_stdout_ends_with_newline "M5 console analyze --quiet stdout ends with newline" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json --quiet --top 2

assert_exit "M5 console impact --quiet exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json --changed-file include/common/config.h --quiet
assert_stdout_equals_file "M5 console impact --quiet matches console-impact-quiet golden" tests/e2e/testdata/m5/verbosity/console-impact-quiet.txt \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json --changed-file include/common/config.h --quiet
assert_stderr_empty "M5 console impact --quiet keeps stderr empty" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json --changed-file include/common/config.h --quiet
assert_stdout_ends_with_newline "M5 console impact --quiet stdout ends with newline" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json --changed-file include/common/config.h --quiet

# Console verbose binary smokes — verbose-stderr fuer Console wird nur beim
# Fehler aktiv; im Erfolgsfall bleibt stderr leer.
assert_exit "M5 console analyze --verbose exits 0" 0 "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json --verbose --top 2
assert_stdout_equals_file "M5 console analyze --verbose matches console-analyze-verbose golden" tests/e2e/testdata/m5/verbosity/console-analyze-verbose.txt \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json --verbose --top 2
assert_stderr_empty "M5 console analyze --verbose keeps stderr empty on success" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json --verbose --top 2
assert_stdout_ends_with_newline "M5 console analyze --verbose stdout ends with newline" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json --verbose --top 2

assert_exit "M5 console impact --verbose exits 0" 0 "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json --changed-file include/common/config.h --verbose
assert_stdout_equals_file "M5 console impact --verbose matches console-impact-verbose golden" tests/e2e/testdata/m5/verbosity/console-impact-verbose.txt \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json --changed-file include/common/config.h --verbose
assert_stderr_empty "M5 console impact --verbose keeps stderr empty on success" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json --changed-file include/common/config.h --verbose
assert_stdout_ends_with_newline "M5 console impact --verbose stdout ends with newline" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json --changed-file include/common/config.h --verbose

# ---- Quiet artifact byte-stability stdout (analyze + impact x 4 formats) ----

assert_stdout_equals_file "M5 markdown analyze --quiet stdout byte-stable to normal-mode markdown" tests/e2e/testdata/m3/report_project/analyze-markdown.md \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json --quiet --format markdown --top 2
assert_stderr_empty "M5 markdown analyze --quiet keeps stderr empty" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json --quiet --format markdown --top 2

assert_stdout_equals_file "M5 json analyze --quiet stdout byte-stable to compile-db-only golden" tests/e2e/testdata/m5/json-reports/analyze_compile_db_only.json \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --quiet --format json
assert_stderr_empty "M5 json analyze --quiet keeps stderr empty" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --quiet --format json

assert_stdout_equals_file "M5 dot analyze --quiet stdout byte-stable to compile-db-only golden" tests/e2e/testdata/m5/dot-reports/analyze_compile_db_only.dot \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/multi_tu_compile_db/compile_commands.json --quiet --format dot --top 2
assert_stderr_empty "M5 dot analyze --quiet keeps stderr empty" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/multi_tu_compile_db/compile_commands.json --quiet --format dot --top 2

assert_stdout_equals_file "M5 html analyze --quiet stdout byte-stable to compile-db-only golden" tests/e2e/testdata/m5/html-reports/analyze_compile_db_only.html \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --quiet --format html --top 2
assert_stderr_empty "M5 html analyze --quiet keeps stderr empty" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --quiet --format html --top 2

assert_stdout_equals_file "M5 markdown impact --quiet stdout byte-stable to normal-mode markdown" tests/e2e/testdata/m3/report_impact_header/impact-markdown.md \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json --changed-file include/common/config.h --quiet --format markdown
assert_stderr_empty "M5 markdown impact --quiet keeps stderr empty" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json --changed-file include/common/config.h --quiet --format markdown

assert_stdout_equals_file "M5 json impact --quiet stdout byte-stable to compile-db-relative golden" tests/e2e/testdata/m5/json-reports/impact_compile_db_relative.json \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --quiet --format json
assert_stderr_empty "M5 json impact --quiet keeps stderr empty" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --quiet --format json

assert_dot_stdout_equals_file "M5 dot impact --quiet stdout byte-stable to heuristic-edges golden" tests/e2e/testdata/m5/dot-reports/impact_heuristic_edges.dot \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --quiet --format dot
assert_stderr_empty "M5 dot impact --quiet keeps stderr empty" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --quiet --format dot

assert_stdout_equals_file "M5 html impact --quiet stdout byte-stable to compile-db-relative golden" tests/e2e/testdata/m5/html-reports/impact_compile_db_relative.html \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --quiet --format html
assert_stderr_empty "M5 html impact --quiet keeps stderr empty" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --quiet --format html

# ---- Verbose artifact byte-stability stdout (analyze + impact x 4 formats) ----
# Verbose stdout muss zum Normalmodus byte-identisch sein; stderr enthaelt
# den dokumentierten 7-/8-Zeilen-Block. Die exakten stderr-Sequenzen werden
# unten ueber assert_stderr_equals_file gegen ein heredoc-File gepinnt.

assert_stdout_equals_file "M5 markdown analyze --verbose stdout byte-stable to normal-mode markdown" tests/e2e/testdata/m3/report_project/analyze-markdown.md \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json --verbose --format markdown --top 2

assert_stdout_equals_file "M5 json analyze --verbose stdout byte-stable to compile-db-only golden" tests/e2e/testdata/m5/json-reports/analyze_compile_db_only.json \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --verbose --format json
assert_json_stdout_validates "M5 json analyze --verbose stdout validates against schema" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --verbose --format json

assert_stdout_equals_file "M5 dot analyze --verbose stdout byte-stable to compile-db-only golden" tests/e2e/testdata/m5/dot-reports/analyze_compile_db_only.dot \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/multi_tu_compile_db/compile_commands.json --verbose --format dot --top 2
assert_dot_stdout_validates "M5 dot analyze --verbose stdout validates against syntax gate" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/multi_tu_compile_db/compile_commands.json --verbose --format dot --top 2

assert_stdout_equals_file "M5 html analyze --verbose stdout byte-stable to compile-db-only golden" tests/e2e/testdata/m5/html-reports/analyze_compile_db_only.html \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --verbose --format html --top 2
assert_html_stdout_validates "M5 html analyze --verbose stdout validates against structure gate" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --verbose --format html --top 2

assert_stdout_equals_file "M5 markdown impact --verbose stdout byte-stable to normal-mode markdown" tests/e2e/testdata/m3/report_impact_header/impact-markdown.md \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json --changed-file include/common/config.h --verbose --format markdown

assert_stdout_equals_file "M5 json impact --verbose stdout byte-stable to compile-db-relative golden" tests/e2e/testdata/m5/json-reports/impact_compile_db_relative.json \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --verbose --format json
assert_json_stdout_validates "M5 json impact --verbose stdout validates against schema" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --verbose --format json

assert_dot_stdout_equals_file "M5 dot impact --verbose stdout byte-stable to heuristic-edges golden" tests/e2e/testdata/m5/dot-reports/impact_heuristic_edges.dot \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --verbose --format dot
assert_dot_stdout_validates "M5 dot impact --verbose stdout validates against syntax gate" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --verbose --format dot

assert_stdout_equals_file "M5 html impact --verbose stdout byte-stable to compile-db-relative golden" tests/e2e/testdata/m5/html-reports/impact_compile_db_relative.html \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --verbose --format html
assert_html_stdout_validates "M5 html impact --verbose stdout validates against structure gate" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --verbose --format html

# ---- Verbose artifact stderr exact-line pinning (8 sequences) ----
# Plan ~749 mandates a diff-based helper; the heredoc files below pin the
# 7-line analyze and 8-line impact sequences for all four artifact formats.
verbose_stderr_dir="$(mktemp -d)"

cat > "$verbose_stderr_dir/analyze-markdown.txt" <<'EOF'
verbose: report_type=analyze
verbose: format=markdown
verbose: output=stdout
verbose: compile_database_source=cli
verbose: cmake_file_api_source=not_provided
verbose: observation_source=exact
verbose: target_metadata=not_loaded
EOF
assert_stderr_equals_file "M5 markdown analyze --verbose stderr matches 7-line block" "$verbose_stderr_dir/analyze-markdown.txt" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json --verbose --format markdown --top 2

cat > "$verbose_stderr_dir/analyze-json.txt" <<'EOF'
verbose: report_type=analyze
verbose: format=json
verbose: output=stdout
verbose: compile_database_source=cli
verbose: cmake_file_api_source=not_provided
verbose: observation_source=exact
verbose: target_metadata=not_loaded
EOF
assert_stderr_equals_file "M5 json analyze --verbose stderr matches 7-line block" "$verbose_stderr_dir/analyze-json.txt" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --verbose --format json

cat > "$verbose_stderr_dir/analyze-dot.txt" <<'EOF'
verbose: report_type=analyze
verbose: format=dot
verbose: output=stdout
verbose: compile_database_source=cli
verbose: cmake_file_api_source=not_provided
verbose: observation_source=exact
verbose: target_metadata=not_loaded
EOF
assert_stderr_equals_file "M5 dot analyze --verbose stderr matches 7-line block" "$verbose_stderr_dir/analyze-dot.txt" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m5/dot-fixtures/multi_tu_compile_db/compile_commands.json --verbose --format dot --top 2

cat > "$verbose_stderr_dir/analyze-html.txt" <<'EOF'
verbose: report_type=analyze
verbose: format=html
verbose: output=stdout
verbose: compile_database_source=cli
verbose: cmake_file_api_source=not_provided
verbose: observation_source=exact
verbose: target_metadata=not_loaded
EOF
assert_stderr_equals_file "M5 html analyze --verbose stderr matches 7-line block" "$verbose_stderr_dir/analyze-html.txt" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --verbose --format html --top 2

cat > "$verbose_stderr_dir/impact-markdown.txt" <<'EOF'
verbose: report_type=impact
verbose: format=markdown
verbose: output=stdout
verbose: compile_database_source=cli
verbose: cmake_file_api_source=not_provided
verbose: observation_source=exact
verbose: target_metadata=not_loaded
verbose: changed_file_source=compile_database_directory
EOF
assert_stderr_equals_file "M5 markdown impact --verbose stderr matches 8-line block" "$verbose_stderr_dir/impact-markdown.txt" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json --changed-file include/common/config.h --verbose --format markdown

cat > "$verbose_stderr_dir/impact-json.txt" <<'EOF'
verbose: report_type=impact
verbose: format=json
verbose: output=stdout
verbose: compile_database_source=cli
verbose: cmake_file_api_source=not_provided
verbose: observation_source=exact
verbose: target_metadata=not_loaded
verbose: changed_file_source=compile_database_directory
EOF
assert_stderr_equals_file "M5 json impact --verbose stderr matches 8-line block" "$verbose_stderr_dir/impact-json.txt" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --verbose --format json

cat > "$verbose_stderr_dir/impact-dot.txt" <<'EOF'
verbose: report_type=impact
verbose: format=dot
verbose: output=stdout
verbose: compile_database_source=cli
verbose: cmake_file_api_source=not_provided
verbose: observation_source=exact
verbose: target_metadata=not_loaded
verbose: changed_file_source=compile_database_directory
EOF
assert_stderr_equals_file "M5 dot impact --verbose stderr matches 8-line block" "$verbose_stderr_dir/impact-dot.txt" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --verbose --format dot

cat > "$verbose_stderr_dir/impact-html.txt" <<'EOF'
verbose: report_type=impact
verbose: format=html
verbose: output=stdout
verbose: compile_database_source=cli
verbose: cmake_file_api_source=not_provided
verbose: observation_source=exact
verbose: target_metadata=not_loaded
verbose: changed_file_source=compile_database_directory
EOF
assert_stderr_equals_file "M5 html impact --verbose stderr matches 8-line block" "$verbose_stderr_dir/impact-html.txt" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --verbose --format html

# top_limit darf NICHT in der Verbose-Artefakt-stderr auftauchen (plan ~754).
assert_stderr_does_not_contain "M5 verbose artifact stderr does not include top_limit" "top_limit" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --verbose --format json --top 2
assert_stderr_does_not_contain "M5 verbose impact stderr does not include top_limit" "top_limit" \
    "$BINARY" impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h --verbose --format json

# ---- Quiet/Verbose --output file byte-stability ----
output_dir="$(native_path "$(mktemp -d)")"

# Quiet --output writes byte-stable file with empty stdout/stderr.
quiet_json_target="$output_dir/analyze-quiet.json"
assert_exit "M5 json analyze --quiet --output exits 0" 0 \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --quiet --format json --output "$quiet_json_target"
assert_file_equals "M5 json analyze --quiet --output writes byte-stable file" "$quiet_json_target" tests/e2e/testdata/m5/json-reports/analyze_compile_db_only.json

# Verbose --output writes byte-stable file with empty stdout and the documented
# stderr block including output=file.
verbose_json_target="$output_dir/analyze-verbose.json"
assert_exit "M5 json analyze --verbose --output exits 0" 0 \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --verbose --format json --output "$verbose_json_target"
assert_file_equals "M5 json analyze --verbose --output writes byte-stable file" "$verbose_json_target" tests/e2e/testdata/m5/json-reports/analyze_compile_db_only.json

cat > "$verbose_stderr_dir/analyze-json-output-file.txt" <<'EOF'
verbose: report_type=analyze
verbose: format=json
verbose: output=file
verbose: compile_database_source=cli
verbose: cmake_file_api_source=not_provided
verbose: observation_source=exact
verbose: target_metadata=not_loaded
EOF
assert_stderr_equals_file "M5 json analyze --verbose --output stderr emits output=file block" \
    "$verbose_stderr_dir/analyze-json-output-file.txt" \
    "$BINARY" analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --verbose --format json --output "$output_dir/analyze-verbose-stderr.json"

rm -rf "$verbose_stderr_dir" "$output_dir"


echo ""
if [ "$failures" -gt 0 ]; then
    echo "$failures E2E test(s) FAILED" >&2
    exit 1
else
    echo "All E2E tests passed"
fi
