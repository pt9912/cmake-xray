#!/usr/bin/env bash
# AP M5-1.8 A.5 follow-up: host-portability gate for the
# docs/examples drift check.
#
# The drift step in tests/validate_doc_examples.py normalises
# DOT unique_key host paths so that a binary running on a runner
# with /home/runner/... can still match a committed example
# generated on a developer host with /workspace/... or
# /Development/cmake-xray/... -- see the AP-1.8 review thread
# in plan-M5-1-8.md Liefer-Stand-Block. Without an explicit
# host-portability check, "lokal gruen" only verifies the path
# layout that the developer happens to be using; the GHA Build
# run #25123025175 surfaced the gap exactly because the linux-
# x86_64 native job has a different absolute prefix.
#
# This smoke runs validate_doc_examples.py from inside the
# cmake-xray:test image, but bind-mounts the host repo at
# /alt-mount instead of the image's default /workspace path.
# The image's pre-built binary at /workspace/build/cmake-xray
# resolves the spec args' fixture paths against /alt-mount/...,
# so DOT unique_key strings carry the /alt-mount prefix --
# different from the committed file's bytes (which were
# generated under /workspace). If validate_doc_examples'
# normalisation is correct, the check passes despite the
# divergence; if a DOT normaliser is missing, it fails the same
# way the GHA native job did.
#
# Usage:
#   scripts/verify-doc-examples-portability.sh
#
# Exit codes:
#   0  passed (or cleanly skipped when docker/Linux not available)
#   1  drift detected
#   2  environment error
set -euo pipefail

if ! command -v docker >/dev/null 2>&1; then
    echo "SKIP: doc-examples portability (docker not available)"
    exit 0
fi

case "$(uname -s)" in
    Linux) ;;
    *)
        echo "SKIP: doc-examples portability (Linux-only, got $(uname -s))"
        exit 0
        ;;
esac

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
image="cmake-xray:test"

# Reuse a cached image if present; otherwise build the test stage so
# /workspace/build/cmake-xray inside the container is available. The
# build pulls in the docs/examples/, generation-spec and validator
# from the host's source tree, so it always reflects the current
# repo state.
if ! docker image inspect "$image" >/dev/null 2>&1; then
    docker build --target test -t "$image" "$repo_root" >/dev/null
fi

alt_mount="/alt-mount"

# Run the validator against the alt-mount bind. --repo-root makes
# validate_doc_examples read manifest, generation-spec and example
# bytes from the host's checkout (under /alt-mount), while --binary
# points at the in-image cmake-xray. The binary's stdout for each
# generation-spec entry will carry /alt-mount/... in DOT
# unique_keys; the committed file bytes carry /workspace/... (or
# whatever the developer's last regen path was). If the normaliser
# strips both to HOSTPATH, the gate passes; otherwise it errors
# with the same message the GHA native job emitted.
docker run --rm \
    -v "$repo_root:$alt_mount" \
    -w "$alt_mount" \
    "$image" \
    python3 "$alt_mount/tests/validate_doc_examples.py" \
        --repo-root "$alt_mount" \
        --binary /workspace/build/cmake-xray

echo "PASS: docs/examples drift gate is host-portable"
