#!/usr/bin/env bash
# AP M5-1.6 Tranche B: Linux release archive reproducibility smoke. Builds
# the archive twice from the same commit with the same SOURCE_DATE_EPOCH
# and compares archive checksum, member listing (with file modes/owners),
# *.sha256 file and the embedded binary checksum byte-for-byte.
#
# Linux-only because scripts/build-release-archive.sh uses GNU-tar flags
# (--sort=name, --owner=0, --numeric-owner) that BSD tar does not accept;
# macOS hosts get a SKIP per plan-M5-1-6.md Tranche-B Sub-Risiken. macOS
# maintainers running this smoke locally use the linux-amd64 toolchain
# container instead.
#
# To keep the test gate fast, both build runs share a single FetchContent
# cache (XRAY_FETCHCONTENT_BASE_DIR) so CMake does not download CLI11 /
# nlohmann_json / doctest twice. The first build still has to fetch them
# unless the env points at an existing cache (e.g. the CTest invocation's
# own build/_deps). The repo-relative build dir is a sensible default.
set -euo pipefail

case "$(uname -s)" in
    Linux) ;;
    *)
        echo "SKIP: release archive reproducibility (Linux-only, got $(uname -s))"
        exit 0
        ;;
esac

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
build_script="$repo_root/scripts/build-release-archive.sh"

if ! [ -x "$build_script" ]; then
    echo "error: $build_script missing or not executable" >&2
    exit 2
fi

version="0.0.0-repro-smoke"

# Share a FetchContent cache between the two runs to avoid duplicate
# downloads of CLI11/nlohmann_json/doctest. Default to the test's own
# build/_deps directory if the caller has not pointed at another cache.
if [ -z "${XRAY_FETCHCONTENT_BASE_DIR:-}" ] && [ -d "$repo_root/build/_deps" ]; then
    export XRAY_FETCHCONTENT_BASE_DIR="$repo_root/build/_deps"
fi

# Ensure both builds use the same SOURCE_DATE_EPOCH; the script defaults
# to 1 if unset, but we set it explicitly for clarity in test output.
export SOURCE_DATE_EPOCH=1

dir1="$(mktemp -d -t cmake-xray-repro-a.XXXXXX)"
dir2="$(mktemp -d -t cmake-xray-repro-b.XXXXXX)"
extract1="$(mktemp -d -t cmake-xray-extract-a.XXXXXX)"
extract2="$(mktemp -d -t cmake-xray-extract-b.XXXXXX)"
trap 'rm -rf "$dir1" "$dir2" "$extract1" "$extract2"' EXIT

bash "$build_script" "$version" "$dir1" >/dev/null
bash "$build_script" "$version" "$dir2" >/dev/null

archive_basename="cmake-xray_${version}_linux_x86_64.tar.gz"
failures=0

assert_files_byte_equal() {
    local description="$1" file_a="$2" file_b="$3"
    if cmp -s "$file_a" "$file_b"; then
        echo "PASS: $description"
    else
        echo "FAIL: $description -- $file_a differs from $file_b" >&2
        diff -u "$file_a" "$file_b" | head -20 >&2 || true
        failures=$((failures + 1))
    fi
}

# 1. The compressed tarball must be byte-identical.
assert_files_byte_equal "archive byte-equal" \
    "$dir1/$archive_basename" "$dir2/$archive_basename"

# 2. The *.sha256 sidecar must be byte-identical (its content embeds the
# archive checksum so this is a stronger statement than just byte-equal).
assert_files_byte_equal "sha256 sidecar byte-equal" \
    "$dir1/$archive_basename.sha256" "$dir2/$archive_basename.sha256"

# 3. The tar member listing (names, modes, owners, sizes, mtimes) must
# match. tar -tvf prints all metadata; comparing the listings catches a
# regression where a future change accidentally drops a reproducibility
# flag (e.g. forgetting --numeric-owner) that does not show up in the
# raw byte-compare because the new flag still produces identical bytes
# on this host but would diverge on a host with different /etc/passwd.
tar -tvf "$dir1/$archive_basename" > "$dir1/members.txt"
tar -tvf "$dir2/$archive_basename" > "$dir2/members.txt"
assert_files_byte_equal "tar member listing equal" \
    "$dir1/members.txt" "$dir2/members.txt"

# 4. The embedded cmake-xray binary itself must be byte-identical. This
# is the strongest check: any non-determinism in the compiler output
# (build IDs, timestamps embedded in DWARF, randomised symbol tables)
# would surface here even if the surrounding tar metadata still hashed
# the same.
tar -xf "$dir1/$archive_basename" -C "$extract1"
tar -xf "$dir2/$archive_basename" -C "$extract2"
( cd "$extract1" && sha256sum cmake-xray > "binary.sha256" )
( cd "$extract2" && sha256sum cmake-xray > "binary.sha256" )
assert_files_byte_equal "embedded binary checksum equal" \
    "$extract1/binary.sha256" "$extract2/binary.sha256"

echo ""
if [ "$failures" -gt 0 ]; then
    echo "$failures release archive reproducibility check(s) FAILED" >&2
    exit 1
fi
echo "All release archive reproducibility checks passed"
