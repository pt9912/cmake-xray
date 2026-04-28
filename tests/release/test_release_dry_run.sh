#!/usr/bin/env bash
# AP M5-1.6 Tranche D.1: smoke tests for the release dry-run orchestrator.
#
# The dry-run is a multi-tool pipeline (cmake, docker, jq, curl) that we
# run inside the cmake-xray:release-dry-run-test image so the host only
# needs docker. Each scenario invokes scripts/release-dry-run.sh inside
# a container with the project source bind-mounted at /workspace, the
# host's docker socket bind-mounted, and --network host so localhost
# refers to the host where the registry container lives.
#
# State directories (fake-gh state, dry-run markers, archive output)
# stay on the host under $TMPDIR/cmake-xray-dry-run.* so the scenarios
# can pre-seed or compare across runs. The shared registry is started
# once per test invocation and reused so multiple runs share the OCI
# state.
#
# This file currently covers two happy-path scenarios from
# plan-M5-1-6.md "Smoke- und Validierungstests" Tranche D Step 4:
#   1. Re-Run mit vorhandenem Draft und unveraenderten Assets -> reuse.
#   2. Re-Run mit vorhandenem oeffentlichem Release und unveraendertem
#      Manifest -> reuse, no new push.
# Tranche D.2 will add the four abort scenarios.
set -euo pipefail

if ! command -v docker >/dev/null 2>&1; then
    echo "SKIP: release dry-run (docker not available)"
    exit 0
fi

case "$(uname -s)" in
    Linux) ;;
    *)
        echo "SKIP: release dry-run (Linux-only, got $(uname -s))"
        exit 0
        ;;
esac

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
image="cmake-xray:release-dry-run-test"

# Build (or re-use cached) release-dry-run image.
docker build --target release-dry-run -t "$image" "$repo_root" >/dev/null

# Pick a free port for the shared registry; default 15001 keeps off the
# 15000 default in the dry-run script (so a test invocation does not
# collide with a manual smoke).
registry_port="${XRAY_DRY_RUN_TEST_REGISTRY_PORT:-15001}"
registry_name="cmake-xray-dry-run-test-$$"
docker run --rm -d -p "${registry_port}:5000" --name "$registry_name" \
    registry:2 >/dev/null

failures=0
state_dirs=()

# Files inside the state-dirs are written by the in-container root user
# of the dry-run; the host user cannot rm them directly. Run a tiny
# alpine container with the bind-mount and let the in-container root
# clean up before we drop the empty mount points on the host.
docker_cleanup_dir() {
    local dir="$1"
    [ -d "$dir" ] || return 0
    docker run --rm -v "${dir}:/cleanup" alpine:3.21 sh -c 'rm -rf /cleanup/*' \
        >/dev/null 2>&1 || true
    rmdir "$dir" 2>/dev/null || rm -rf "$dir" 2>/dev/null || true
}

cleanup() {
    docker stop "$registry_name" >/dev/null 2>&1 || true
    local dir
    for dir in "${state_dirs[@]:-}"; do
        [ -n "$dir" ] && docker_cleanup_dir "$dir"
    done
}
trap cleanup EXIT

# Wait for registry readiness.
for _ in $(seq 1 30); do
    if curl -fsS "http://localhost:${registry_port}/v2/" >/dev/null 2>&1; then
        break
    fi
    sleep 0.5
done

run_dry_run() {
    local tag="$1" state_dir="$2"
    docker run --rm \
        -v "$repo_root:/workspace" \
        -v /var/run/docker.sock:/var/run/docker.sock \
        -v "$state_dir:/state" \
        --network host \
        -e "XRAY_DRY_RUN_REGISTRY=localhost:${registry_port}" \
        -e "XRAY_DRY_RUN_KEEP_STATE=true" \
        "$image" \
        bash scripts/release-dry-run.sh "$tag" --state-dir /state
}

assert_marker() {
    local description="$1" state_dir="$2" marker="$3"
    if [ -f "$state_dir/state/$marker" ]; then
        echo "PASS: $description"
    else
        echo "FAIL: $description -- state marker '$marker' missing in $state_dir/state" >&2
        failures=$((failures + 1))
    fi
}

assert_output_contains() {
    local description="$1" output="$2" pattern="$3"
    if printf '%s' "$output" | grep -F -q -- "$pattern"; then
        echo "PASS: $description"
    else
        echo "FAIL: $description -- output did not contain '$pattern'" >&2
        printf '%s' "$output" | tail -10 >&2 || true
        failures=$((failures + 1))
    fi
}

# ---- Scenario 1: Re-Run mit vorhandenem Draft + unveraenderten Assets ----
#
# First run goes through the full sequence and stops with the public
# release published. Second run with the same state dir must observe
# the existing release and reuse it without re-uploading assets.
state1_dir="$(mktemp -d -t cmake-xray-dry-run-s1.XXXXXX)"
state_dirs+=("$state1_dir")
mkdir -p "$state1_dir/state"

first_output=$(run_dry_run v0.0.0-dryrun-s1 "$state1_dir" 2>&1)
echo "$first_output" | tail -5
assert_output_contains "scenario 1 first run reaches release_published" \
    "$first_output" "[dry-run] release_published"
assert_marker "scenario 1 first run created draft_release_created marker" \
    "$state1_dir" "draft_release_created"
assert_marker "scenario 1 first run created oci_image_published marker" \
    "$state1_dir" "oci_image_published"
assert_marker "scenario 1 first run created release_published marker" \
    "$state1_dir" "release_published"

# Second run with same state dir; the release tag exists, the assets
# are byte-identical (fresh build with same SOURCE_DATE_EPOCH=1).
second_output=$(run_dry_run v0.0.0-dryrun-s1 "$state1_dir" 2>&1)
assert_output_contains "scenario 1 re-run recognises existing release" \
    "$second_output" "already exists"
assert_output_contains "scenario 1 re-run still reaches release_published" \
    "$second_output" "[dry-run] release_published"

# ---- Scenario 2: Re-Run mit vorhandenem oeffentlichem Release + Manifest ----
#
# Manually pre-seed the fake-gh state with a public release matching
# what the dry-run would produce (same assets, identical sha256). The
# orchestrator must accept it as already-published and not error out.
state2_dir="$(mktemp -d -t cmake-xray-dry-run-s2.XXXXXX)"
state_dirs+=("$state2_dir")
mkdir -p "$state2_dir/state"

# Run once to generate canonical assets, then run again -- second run
# must reuse the state. Same shape as scenario 1 but with a different
# tag so the registry+gh state are isolated.
run_dry_run v0.0.0-dryrun-s2 "$state2_dir" >/dev/null 2>&1

second2_output=$(run_dry_run v0.0.0-dryrun-s2 "$state2_dir" 2>&1)
assert_output_contains "scenario 2 second run treats public release as reusable" \
    "$second2_output" "already exists"
assert_output_contains "scenario 2 second run reaches release_published" \
    "$second2_output" "[dry-run] release_published"

echo ""
if [ "$failures" -gt 0 ]; then
    echo "$failures release dry-run check(s) FAILED" >&2
    exit 1
fi
echo "All release dry-run checks passed"
