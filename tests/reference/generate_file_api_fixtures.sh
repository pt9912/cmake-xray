#!/usr/bin/env bash
# AP M5-1.8 A.3: regenerate the versioned CMake-File-API-Reply fixtures
# for tests/reference/scale_*.
#
# The fixtures back the M5 File-API performance baseline in
# docs/user/performance.md. Configure-only -- the reply directory is the only
# artefact we keep, so regen is fast and the build/CMakeCache.txt etc.
# stay out of source control.
#
# Output:
#   - tests/reference/scale_<N>/build/.cmake/api/v1/reply/ populated with
#     the codemodel-v2 reply matching the .cmake/api/v1/query/ stamp.
#   - tests/reference/file-api-performance-manifest.json with the
#     fixture list, scale, CMake version, generator, source/build/reply
#     paths, regeneration command, timestamp policy and AP M6 baseline
#     measurement metadata.
#
# Usage:
#   bash tests/reference/generate_file_api_fixtures.sh
#
# Requires `cmake` on PATH; no compiler invocation, configure-only.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SCALES=(250 500 1000)

if ! command -v cmake >/dev/null 2>&1; then
    echo "error: cmake is required" >&2
    exit 2
fi

cmake_version_full="$(cmake --version | head -1)"
cmake_version="${cmake_version_full##* }"

# JSON-escape helper for the manifest writer below.
json_escape() {
    python3 -c 'import json,sys; print(json.dumps(sys.argv[1]))' "$1"
}

manifest_path="$SCRIPT_DIR/file-api-performance-manifest.json"
{
    cat <<EOF
{
  "\$comment": "AP M5-1.8 A.3 and AP M6-1.7 A.3 File-API performance baseline manifest. Fixtures are versioned reply directories under tests/reference/scale_*/build/.cmake/api/v1/reply/. Regenerate via tests/reference/generate_file_api_fixtures.sh; review the diff to absorb host-path or CMake-version drift.",
  "format_version": 1,
  "regeneration_command": "tests/reference/generate_file_api_fixtures.sh",
  "regenerated_at_policy": "host-local at regeneration time; not asserted in CI",
  "cmake_version": $(json_escape "$cmake_version"),
  "fixtures": [
EOF
} > "$manifest_path"

first="true"
for scale in "${SCALES[@]}"; do
    src="$SCRIPT_DIR/scale_${scale}"
    build="$src/build"

    if [ ! -d "$src" ]; then
        echo "error: source dir missing for scale_${scale}: $src" >&2
        echo "       run tests/reference/generate_reference_projects.py first" >&2
        exit 2
    fi
    if [ ! -f "$src/CMakeLists.txt" ]; then
        echo "error: CMakeLists.txt missing in $src; regenerate via" >&2
        echo "       tests/reference/generate_reference_projects.py" >&2
        exit 2
    fi

    rm -rf "$build"
    mkdir -p "$build/.cmake/api/v1/query"

    # Place a build-tree stateless query so cmake emits the codemodel-v2
    # reply during configure. The query lives under the build dir
    # (cmake-file-api(7) "Query Files" section); we (re)create it on
    # every regeneration so the source tree stays free of file-API
    # plumbing.
    : > "$build/.cmake/api/v1/query/codemodel-v2"

    # Configure only -- no compile step needed for the reply.
    cmake -S "$src" -B "$build" >/dev/null

    reply="$build/.cmake/api/v1/reply"
    if [ ! -d "$reply" ]; then
        echo "error: cmake did not produce file-API reply for scale_${scale}" >&2
        exit 1
    fi

    # Detect the generator CMake actually picked (Unix Makefiles, Ninja,
    # ...). Useful for the manifest so reviewers can tell whether a
    # regeneration switched generators.
    generator="$(grep -E '^CMAKE_GENERATOR:' "$build/CMakeCache.txt" \
        | head -1 | cut -d= -f2)"

    # Trim everything except the reply directory.
    tmp_reply_parent="$(mktemp -d)"
    mv "$reply" "$tmp_reply_parent/reply"
    rm -rf "$build"
    mkdir -p "$build/.cmake/api/v1"
    mv "$tmp_reply_parent/reply" "$reply"
    rmdir "$tmp_reply_parent"

    reply_count="$(find "$reply" -maxdepth 1 -type f | wc -l)"

    src_rel="${src#"$REPO_ROOT/"}"
    build_rel="${build#"$REPO_ROOT/"}"
    reply_rel="${reply#"$REPO_ROOT/"}"

    if [ "$first" = "true" ]; then
        first="false"
    else
        printf ',\n' >> "$manifest_path"
    fi
    {
        printf '    {\n'
        printf '      "fixture": "scale_%s",\n' "$scale"
        printf '      "scale": %s,\n' "$scale"
        printf '      "source_dir": %s,\n' "$(json_escape "$src_rel")"
        printf '      "build_dir": %s,\n' "$(json_escape "$build_rel")"
        printf '      "reply_dir": %s,\n' "$(json_escape "$reply_rel")"
        printf '      "generator": %s,\n' "$(json_escape "$generator")"
        printf '      "reply_file_count": %s\n' "$reply_count"
        printf '    }'
    } >> "$manifest_path"

    echo "scale_${scale}: $reply_count reply files at $reply_rel"
done

cat <<'EOF' >> "$manifest_path"

  ],
  "m6_performance_baselines": {
    "measured_at": "2026-05-13",
    "tool_version": "1.3.0",
    "measurement_note": "Measured in the cmake-xray:test Docker image with /usr/bin/time; wall times below 0.01 s appear as 0.00 s.",
    "datasets": [
      {
        "kind": "file_api_extraction",
        "fixture": "file_api_loaded",
        "build_dir": "tests/e2e/testdata/m6/file_api_loaded/build",
        "target_count": 11,
        "edge_count": 10,
        "target_graph_status": "loaded",
        "wall_time_s": 0.00,
        "max_rss_kb": 5760
      },
      {
        "kind": "file_api_extraction",
        "fixture": "file_api_partial",
        "build_dir": "tests/e2e/testdata/m6/file_api_partial/build",
        "target_count": 2,
        "edge_count": 1,
        "target_graph_status": "partial",
        "wall_time_s": 0.00,
        "max_rss_kb": 5760
      },
      {
        "kind": "file_api_extraction",
        "fixture": "scale_500",
        "build_dir": "tests/reference/scale_500/build",
        "target_count": 1,
        "edge_count": 0,
        "target_graph_status": "loaded",
        "wall_time_s": 0.39,
        "max_rss_kb": 6880
      },
      {
        "kind": "file_api_extraction",
        "fixture": "scale_1000",
        "build_dir": "tests/reference/scale_1000/build",
        "target_count": 1,
        "edge_count": 0,
        "target_graph_status": "loaded",
        "wall_time_s": 0.71,
        "max_rss_kb": 8496
      },
      {
        "kind": "reverse_bfs",
        "fixture": "file_api_loaded",
        "changed_file": "src/hub.cpp",
        "impact_target_depth": 0,
        "target_count": 11,
        "prioritised_target_count": 1,
        "effective_depth": 0,
        "wall_time_s": 0.00,
        "max_rss_kb": 5760
      },
      {
        "kind": "reverse_bfs",
        "fixture": "file_api_loaded",
        "changed_file": "src/hub.cpp",
        "impact_target_depth": 2,
        "target_count": 11,
        "prioritised_target_count": 11,
        "effective_depth": 1,
        "wall_time_s": 0.00,
        "max_rss_kb": 5600
      },
      {
        "kind": "compare",
        "json_size_class": "small",
        "scenario": "loaded_vs_partial",
        "baseline": "tests/e2e/testdata/m6/json-reports/analyze-file-api-loaded.json",
        "current": "tests/e2e/testdata/m6/json-reports/analyze-file-api-partial.json",
        "tu_count": 5,
        "hotspot_count": 7,
        "target_count": 11,
        "compare_ms": 0,
        "max_rss_kb": 4960
      },
      {
        "kind": "compare",
        "json_size_class": "small",
        "scenario": "include_all_vs_project",
        "baseline": "tests/e2e/testdata/m6/json-reports/analyze-include-origin-mix.json",
        "current": "tests/e2e/testdata/m6/json-reports/analyze-include-origin-mix-scope-project.json",
        "tu_count": 5,
        "hotspot_count": 7,
        "target_count": 0,
        "compare_ms": 0,
        "max_rss_kb": 4800
      },
      {
        "kind": "compare",
        "json_size_class": "small",
        "scenario": "compile_db_identity_drift",
        "baseline": "tests/e2e/testdata/m6/json-reports/analyze-compile-db-only.json",
        "current": "tests/e2e/testdata/m6/json-reports/analyze-include-origin-mix.json",
        "tu_count": 5,
        "hotspot_count": 7,
        "target_count": 0,
        "compare_ms": 0,
        "max_rss_kb": 4960
      }
    ]
  }
}
EOF

echo "manifest written at ${manifest_path#"$REPO_ROOT/"}"
