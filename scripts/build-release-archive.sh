#!/usr/bin/env bash
# AP M5-1.6 Tranche B: reproducible Linux release-archive builder.
#
# Usage: scripts/build-release-archive.sh <version> <output-dir>
#
# Builds cmake-xray with the given app version, packages the binary alongside
# LICENSE and README.md as a deterministic tar.gz under <output-dir>, and
# writes a sha256 file. The reproducibility flags follow plan-M5-1-6.md
# Tranche B "Linux-Archiv": tar --sort=name --mtime=@SOURCE_DATE_EPOCH
# --owner=0 --group=0 --numeric-owner --format=ustar piped into `gzip -n`
# (no filename in header). The plan template lists `--pax-option=delete=
# ctime,delete=atime` which only applies to pax headers; ustar physically
# has no ctime/atime fields, so the flag is omitted as a no-op.
#
# SOURCE_DATE_EPOCH must be set in CI (derived from the tag's commit
# timestamp via `git log -1 --pretty=%ct $TAG`); local Repro-Smoke runs
# fall back to a fixed default so the smoke compares apples to apples.
# The --version-flag-Quelle XRAY_APP_VERSION is forwarded to cmake.
#
# Linux-only because GNU tar's reproducibility flags are not portable to
# BSD tar; macOS handling is plan-M5-1-6.md Tranche-E thema. Optional env:
# XRAY_FETCHCONTENT_BASE_DIR points cmake at a shared FetchContent cache
# (smoke runs share the test-stage's `build/_deps` so the second build
# does not redownload third-party sources).
set -euo pipefail

if [ $# -ne 2 ]; then
    echo "usage: $(basename "$0") <version> <output-dir>" >&2
    exit 2
fi

version="$1"
output_dir="$2"

case "$(uname -s)" in
    Linux) ;;
    *)
        echo "error: $(basename "$0") is Linux-only (got $(uname -s)). " \
             "macOS/Windows handling is plan-M5-1-6.md Tranche-E thema." >&2
        exit 1
        ;;
esac

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

: "${SOURCE_DATE_EPOCH:=1}"
export SOURCE_DATE_EPOCH

archive_basename="cmake-xray_${version}_linux_x86_64.tar.gz"
mkdir -p "$output_dir"

build_dir="$(mktemp -d -t cmake-xray-build.XXXXXX)"
staging_dir="$(mktemp -d -t cmake-xray-stage.XXXXXX)"
trap 'rm -rf "$build_dir" "$staging_dir"' EXIT

cmake_args=(-S "$repo_root" -B "$build_dir"
            -DCMAKE_BUILD_TYPE=Release
            -DXRAY_APP_VERSION="$version")
if [ -n "${XRAY_FETCHCONTENT_BASE_DIR:-}" ]; then
    cmake_args+=(-DFETCHCONTENT_BASE_DIR="$XRAY_FETCHCONTENT_BASE_DIR")
fi
cmake "${cmake_args[@]}" >/dev/null
cmake --build "$build_dir" --target cmake-xray --parallel >/dev/null

install -m 0755 "$build_dir/cmake-xray" "$staging_dir/cmake-xray"
install -m 0644 "$repo_root/LICENSE" "$staging_dir/LICENSE"
install -m 0644 "$repo_root/README.md" "$staging_dir/README.md"

# tar | gzip -n: --sort=name fixes member order, --mtime/--owner/--group
# pin metadata, --format=ustar uses the older POSIX format which only
# stores mtime per member (no ctime/atime, no pax extended headers), so
# the archive is reproducible without needing --pax-option to delete
# fields that ustar does not record in the first place. gzip -n keeps
# the filename out of the gzip header so the archive is byte-stable
# across builds in different working directories.
tar -C "$staging_dir" \
    --sort=name \
    --mtime="@${SOURCE_DATE_EPOCH}" \
    --owner=0 --group=0 --numeric-owner \
    --format=ustar \
    -cf - cmake-xray LICENSE README.md \
| gzip -n > "$output_dir/$archive_basename"

(
    cd "$output_dir"
    sha256sum "$archive_basename" > "${archive_basename}.sha256"
)

echo "wrote $output_dir/$archive_basename"
