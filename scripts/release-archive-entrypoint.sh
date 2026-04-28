#!/usr/bin/env bash
# AP M5-1.6 Tranche B: entrypoint for the release-archive Docker stage.
# Copies the produced archive and its SHA-256 sidecar into the directory
# given as the first argument (default /output, which the user mounts via
# `docker run -v "$PWD/release-assets:/output"`). Ensures the target
# exists and surfaces a helpful hint if it does not.
set -euo pipefail

target_dir="${1:-/output}"
source_dir="/workspace/release-out"

if ! [ -d "$source_dir" ]; then
    echo "error: $source_dir is missing; the release-archive stage did not produce any artifact" >&2
    exit 1
fi

if ! [ -d "$target_dir" ]; then
    echo "error: target directory '$target_dir' does not exist" >&2
    echo "hint: mount a host directory into /output with" >&2
    echo "  docker run --rm -v \"\$PWD/release-assets:/output\" <image>" >&2
    exit 2
fi

cp "$source_dir"/cmake-xray_*.tar.gz "$target_dir/"
cp "$source_dir"/cmake-xray_*.tar.gz.sha256 "$target_dir/"

echo "wrote release archive to $target_dir"
ls -1 "$target_dir"/cmake-xray_*
