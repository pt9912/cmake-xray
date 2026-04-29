#!/usr/bin/env bash
# AP M5-1.8 A.3: regenerate the versioned CMake-File-API-Reply fixtures
# for tests/reference/scale_*.
#
# The fixtures back the M5 File-API performance baseline in
# docs/performance.md. Configure-only -- the reply directory is the only
# artefact we keep, so regen is fast and the build/CMakeCache.txt etc.
# stay out of source control.
#
# Output:
#   - tests/reference/scale_<N>/build/.cmake/api/v1/reply/ populated with
#     the codemodel-v2 reply matching the .cmake/api/v1/query/ stamp.
#   - tests/reference/file-api-performance-manifest.json with the
#     fixture list, scale, CMake version, generator, source/build/reply
#     paths, regeneration command and ISO-8601 timestamp policy.
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
  "\$comment": "AP M5-1.8 A.3 File-API performance baseline manifest. Fixtures are versioned reply directories under tests/reference/scale_*/build/.cmake/api/v1/reply/. Regenerate via tests/reference/generate_file_api_fixtures.sh; review the diff to absorb host-path or CMake-version drift.",
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

  ]
}
EOF

echo "manifest written at ${manifest_path#"$REPO_ROOT/"}"
