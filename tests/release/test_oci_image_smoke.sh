#!/usr/bin/env bash
# AP M5-1.6 Tranche C: smoke tests for the OCI runtime image and the
# scripts/oci-image-publish.sh idempotency contracts.
#
# 1. Build the runtime image locally via oci-image-publish.sh build.
# 2. Run the container with --help and --version, pin the --version
#    output against the build's app-version argument so a Versionsdrift
#    between Tag, CMake-Cache-Var, embedded binary string and OCI label
#    surfaces here.
# 3. Verify the OCI label org.opencontainers.image.version matches the
#    --version output exactly.
# 4. Refuse-to-update-`latest` for prerelease versions (containing '-').
#
# Runs the smoke locally only when `docker` is available; otherwise
# prints SKIP. Other release-* tests follow the same pattern.
set -euo pipefail

if ! command -v docker >/dev/null 2>&1; then
    echo "SKIP: oci image smoke (docker not available)"
    exit 0
fi

case "$(uname -s)" in
    Linux) ;;
    *)
        echo "SKIP: oci image smoke (Linux-only, got $(uname -s))"
        exit 0
        ;;
esac

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
publish_script="$repo_root/scripts/oci-image-publish.sh"

if ! [ -x "$publish_script" ]; then
    echo "error: $publish_script missing or not executable" >&2
    exit 2
fi

failures=0

assert_equal() {
    local description="$1" expected="$2" actual="$3"
    if [ "$expected" = "$actual" ]; then
        echo "PASS: $description"
    else
        echo "FAIL: $description -- expected '$expected', got '$actual'" >&2
        failures=$((failures + 1))
    fi
}

assert_command_succeeds() {
    local description="$1"
    shift
    if "$@" >/dev/null 2>&1; then
        echo "PASS: $description"
    else
        echo "FAIL: $description -- command failed: $*" >&2
        failures=$((failures + 1))
    fi
}

assert_command_fails() {
    local description="$1"
    shift
    if "$@" >/dev/null 2>&1; then
        echo "FAIL: $description -- command unexpectedly succeeded: $*" >&2
        failures=$((failures + 1))
    else
        echo "PASS: $description"
    fi
}

stable_version="0.0.0-oci-smoke"
prerelease_version="0.0.0-oci-smoke-rc.1"
local_repo="cmake-xray-oci-smoke"
stable_tag="${local_repo}:${stable_version}"
prerelease_tag="${local_repo}:${prerelease_version}"

cleanup() {
    docker image rm -f "$stable_tag" "$prerelease_tag" >/dev/null 2>&1 || true
}
trap cleanup EXIT

# --- Build (no push) ---
bash "$publish_script" "$local_repo" "$stable_version" build >/dev/null

# --- Container smokes ---
assert_command_succeeds "container --help exits 0" \
    docker run --rm "$stable_tag" --help

extracted_version=$(docker run --rm "$stable_tag" --version)
assert_equal "container --version matches build-arg" "$stable_version" "$extracted_version"

# --- OCI label consistency ---
label_version=$(docker inspect \
    --format='{{ index .Config.Labels "org.opencontainers.image.version" }}' \
    "$stable_tag")
assert_equal "OCI label image.version matches --version" "$stable_version" "$label_version"

# --- latest refusal for prerelease ---
# `cmd_latest` (scripts/oci-image-publish.sh) checks the prerelease
# pattern *-* in the version string before it ever touches a remote or
# local image. A `docker tag` of the existing stable image is enough to
# stage a prerelease artifact; building the prerelease image from
# scratch (cmake + docker build) would burn ~120 s for nothing.
docker tag "$stable_tag" "$prerelease_tag"
assert_command_fails "publish-script refuses 'latest' for prerelease version" \
    bash "$publish_script" "$local_repo" "$prerelease_version" latest

# --- usage-error paths ---
assert_command_fails "publish-script rejects empty image repo" \
    bash "$publish_script" "" "$stable_version" build
assert_command_fails "publish-script rejects empty version" \
    bash "$publish_script" "$local_repo" "" build
assert_command_fails "publish-script rejects unknown command" \
    bash "$publish_script" "$local_repo" "$stable_version" frobnicate

echo ""
if [ "$failures" -gt 0 ]; then
    echo "$failures OCI image smoke check(s) FAILED" >&2
    exit 1
fi
echo "All OCI image smoke checks passed"
