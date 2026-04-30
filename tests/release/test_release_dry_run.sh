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

# buildx 0.30.x ignores `--format '{{.Manifest.Digest}}'` and prints the
# default Name/MediaType/Digest block; buildx >= 0.33 honours the
# template. Mirror the awk filter in scripts/oci-image-publish.sh's
# read_remote_digest so cross-tag digest comparisons (e.g. scenario 10)
# survive the divergence on whichever buildx the host happens to ship.
inspect_digest() {
    local tag="$1"
    docker buildx imagetools inspect "$tag" --format '{{.Manifest.Digest}}' 2>/dev/null \
        | awk '
            /^sha256:/ { print; exit }
            /^Digest:[[:space:]]+sha256:/ { print $2; exit }
        '
}

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

# Helpers for tampering with state from inside an alpine container so we
# do not need root on the host. The fake-gh state files are root-owned
# (created by the in-container root user during the first run), and the
# scenarios below need to either rewrite a metadata.json or replace an
# asset's bytes before the second run.

run_in_alpine() {
    local dir="$1"
    shift
    docker run --rm -v "${dir}:/state" alpine:3.21 sh -c "$*"
}

assert_dry_run_aborts() {
    local description="$1" tag="$2" state_dir="$3" expected_marker_present="$4" \
          expected_marker_absent="$5"
    local out rc=0
    out=$(run_dry_run "$tag" "$state_dir" 2>&1) || rc=$?
    if [ "$rc" -eq 0 ]; then
        echo "FAIL: $description -- dry-run unexpectedly succeeded" >&2
        printf '%s\n' "$out" | tail -5 >&2 || true
        failures=$((failures + 1))
        return
    fi
    if [ -n "$expected_marker_present" ] && [ ! -f "$state_dir/state/$expected_marker_present" ]; then
        echo "FAIL: $description -- expected marker '$expected_marker_present' missing" >&2
        failures=$((failures + 1))
        return
    fi
    if [ -n "$expected_marker_absent" ] && [ -f "$state_dir/state/$expected_marker_absent" ]; then
        echo "FAIL: $description -- forbidden marker '$expected_marker_absent' is present" >&2
        failures=$((failures + 1))
        return
    fi
    echo "PASS: $description"
}

# ---- Scenario 3: Re-Run mit geaendertem Asset -> abort ----
#
# Pre-seed: run dry-run once. Then overwrite the archive asset in fake-gh
# with different bytes (simulating a third party uploading a different
# tarball). Re-run must abort before release_published.
state3_dir="$(mktemp -d -t cmake-xray-dry-run-s3.XXXXXX)"
state_dirs+=("$state3_dir")
mkdir -p "$state3_dir/state"
run_dry_run v0.0.0-dryrun-s3 "$state3_dir" >/dev/null 2>&1
archive_basename_s3="cmake-xray_0.0.0-dryrun-s3_linux_x86_64.tar.gz"
asset_path_s3="$state3_dir/fake-gh/releases/v0.0.0-dryrun-s3/assets/$archive_basename_s3"
run_in_alpine "$state3_dir" \
    "echo 'tampered-archive-content' > /state/fake-gh/releases/v0.0.0-dryrun-s3/assets/$archive_basename_s3"
# Update metadata.json so the recorded sha256 matches the tampered file
# (simulating a bona-fide divergent upload, not a corrupted state).
run_in_alpine "$state3_dir" "
    apk add --no-cache jq >/dev/null 2>&1
    new_sha=\$(sha256sum /state/fake-gh/releases/v0.0.0-dryrun-s3/assets/$archive_basename_s3 | awk '{print \$1}')
    meta=/state/fake-gh/releases/v0.0.0-dryrun-s3/metadata.json
    jq --arg name '$archive_basename_s3' --arg sha \"\$new_sha\" \
       '.assets = (.assets | map(if .name == \$name then .sha256 = \$sha else . end))' \
       \$meta > \$meta.new && mv \$meta.new \$meta
"
# Remove the release_published marker so we can detect that the re-run
# does NOT recreate it.
rm -f "$state3_dir/state/release_published"
assert_dry_run_aborts "scenario 3 changed-asset re-run aborts before release_published" \
    v0.0.0-dryrun-s3 "$state3_dir" "" "release_published"

# ---- Scenario 4: Re-Run mit geaenderter Checksumme -> abort ----
#
# Same setup as scenario 3 but tamper with the *.sha256 sidecar instead
# of the archive itself. The dry-run rebuilds the sidecar and detects
# the divergent recorded checksum.
state4_dir="$(mktemp -d -t cmake-xray-dry-run-s4.XXXXXX)"
state_dirs+=("$state4_dir")
mkdir -p "$state4_dir/state"
run_dry_run v0.0.0-dryrun-s4 "$state4_dir" >/dev/null 2>&1
sidecar_basename_s4="cmake-xray_0.0.0-dryrun-s4_linux_x86_64.tar.gz.sha256"
run_in_alpine "$state4_dir" "
    apk add --no-cache jq >/dev/null 2>&1
    echo 'corrupted sidecar payload' > /state/fake-gh/releases/v0.0.0-dryrun-s4/assets/$sidecar_basename_s4
    new_sha=\$(sha256sum /state/fake-gh/releases/v0.0.0-dryrun-s4/assets/$sidecar_basename_s4 | awk '{print \$1}')
    meta=/state/fake-gh/releases/v0.0.0-dryrun-s4/metadata.json
    jq --arg name '$sidecar_basename_s4' --arg sha \"\$new_sha\" \
       '.assets = (.assets | map(if .name == \$name then .sha256 = \$sha else . end))' \
       \$meta > \$meta.new && mv \$meta.new \$meta
"
rm -f "$state4_dir/state/release_published"
assert_dry_run_aborts "scenario 4 changed-checksum re-run aborts before release_published" \
    v0.0.0-dryrun-s4 "$state4_dir" "" "release_published"

# ---- Scenario 5: Manifest-Mismatch (notes) -> abort ----
#
# Pre-seed an existing release with deliberately divergent notes. The
# dry-run regenerates canonical notes; comparing them with the stored
# manifest must trip the manifest-mismatch guard.
state5_dir="$(mktemp -d -t cmake-xray-dry-run-s5.XXXXXX)"
state_dirs+=("$state5_dir")
mkdir -p "$state5_dir/state"
run_dry_run v0.0.0-dryrun-s5 "$state5_dir" >/dev/null 2>&1
run_in_alpine "$state5_dir" "
    apk add --no-cache jq >/dev/null 2>&1
    meta=/state/fake-gh/releases/v0.0.0-dryrun-s5/metadata.json
    jq '.notes = \"manually edited release notes\"' \$meta > \$meta.new && mv \$meta.new \$meta
"
rm -f "$state5_dir/state/release_published"
assert_dry_run_aborts "scenario 5 manifest-mismatch (notes) aborts before release_published" \
    v0.0.0-dryrun-s5 "$state5_dir" "" "release_published"

# ---- Scenario 6: OCI-Digest-Mismatch -> abort vor latest ----
#
# Pre-seed the registry with a different image at the version tag, then
# run dry-run. The publish-script's push path observes the digest
# mismatch and aborts before any latest-tag update. release_published
# must NOT be set.
state6_dir="$(mktemp -d -t cmake-xray-dry-run-s6.XXXXXX)"
state_dirs+=("$state6_dir")
mkdir -p "$state6_dir/state"

# Build a deliberately different image (alpine with a marker file) and
# push it under the version tag the dry-run will use. Because the
# version is non-prerelease this will also exercise the latest-update
# path, where the abort matters most.
mismatch_repo="localhost:${registry_port}/cmake-xray"
mismatch_version="0.0.0-mismatch"
mismatch_tag="${mismatch_repo}:${mismatch_version}"
docker rmi -f "$mismatch_tag" >/dev/null 2>&1 || true
docker pull alpine:3.21 >/dev/null 2>&1 || true
docker tag alpine:3.21 "$mismatch_tag"
docker push "$mismatch_tag" >/dev/null

# AP M5-1.6 Tranche I.2: capture the pre-seed digest *before* the
# dry-run runs so we can verify after the abort that the versioned
# tag was not silently overwritten. plan-M5-1-6.md OCI-Image-Vertrag
# verlangt Abbruch *vor* dem Push; ohne diese Pinning fiele ein
# bereits ueberschriebener versioned-Tag-Digest nicht auf, weil der
# alte Test nur latest gegen den (potentiell schon umgeschriebenen)
# versioned_digest verglich.
preseed_versioned_digest=$(docker buildx imagetools inspect "$mismatch_tag" \
    --format '{{.Manifest.Digest}}')

assert_dry_run_aborts "scenario 6 oci-digest-mismatch aborts before release_published" \
    v0.0.0-mismatch "$state6_dir" "" "release_published"

# AP M5-1.6 Tranche I.2: hard invariant -- der versionierte Remote-
# Tag muss nach dem abgebrochenen Dry-Run byte-identisch zum Pre-
# Seed-Stand sein. Die alte Variante (latest_digest = versioned_digest)
# erkennt diesen Fall nicht, weil sie nur prueft, dass `latest` nicht
# auf den *neuen* Versions-Digest zeigt -- der Versions-Tag selbst
# konnte still ueberschrieben werden, bevor der Workflow abbrach.
post_versioned_digest=$(docker buildx imagetools inspect "$mismatch_tag" \
    --format '{{.Manifest.Digest}}' 2>/dev/null || true)
if [ "$post_versioned_digest" = "$preseed_versioned_digest" ]; then
    echo "PASS: scenario 6 versioned tag is byte-identical to pre-seed (digest=${post_versioned_digest})"
else
    echo "FAIL: scenario 6 versioned tag was overwritten despite digest mismatch" >&2
    echo "       pre-seed: ${preseed_versioned_digest}" >&2
    echo "       post:     ${post_versioned_digest:-<unset>}" >&2
    failures=$((failures + 1))
fi

# Belt-and-braces: latest must not have been updated to the dry-run's
# new image either. The publish-script logic refuses to touch latest
# when the version-tag digest does not match the dry-run's push.
latest_digest=$(docker buildx imagetools inspect "${mismatch_repo}:latest" \
    --format '{{.Manifest.Digest}}' 2>/dev/null || true)
if [ -n "$latest_digest" ] && [ "$latest_digest" = "$post_versioned_digest" ]; then
    echo "FAIL: scenario 6 latest tag was updated despite digest mismatch" >&2
    failures=$((failures + 1))
else
    echo "PASS: scenario 6 latest tag stays out of the picture (digest=${latest_digest:-<unset>})"
fi

# ---- Scenario 7: Extra-Remote-Asset -> abort ----
#
# AP M5-1.6 Tranche H.3: pre-seed a release that has an additional
# asset name beyond the M5 allowlist (linux archive + sidecar). The
# helper must detect the extra remote asset as a manifest-mismatch
# and abort before release_published; without the H.3 check, a stale
# `*-darwin-arm64.tar.gz` from a pre-Tranche-E world or a manual
# operator upload would silently survive an automated re-run.
state7_dir="$(mktemp -d -t cmake-xray-dry-run-s7.XXXXXX)"
state_dirs+=("$state7_dir")
mkdir -p "$state7_dir/state"
run_dry_run v0.0.0-dryrun-s7 "$state7_dir" >/dev/null 2>&1
run_in_alpine "$state7_dir" "
    apk add --no-cache jq >/dev/null 2>&1
    meta=/state/fake-gh/releases/v0.0.0-dryrun-s7/metadata.json
    jq '.assets += [{\"name\": \"cmake-xray_0.0.0-dryrun-s7_darwin_arm64.tar.gz\", \"sha256\": \"deadbeef\"}]' \
       \$meta > \$meta.new && mv \$meta.new \$meta
"
rm -f "$state7_dir/state/release_published"
assert_dry_run_aborts "scenario 7 extra-remote-asset aborts before release_published" \
    v0.0.0-dryrun-s7 "$state7_dir" "" "release_published"

# ---- Scenario 8: Non-localhost XRAY_DRY_RUN_REGISTRY -> early abort ----
#
# AP M5-1.6 Tranche H.4: plan-Vertrag verlangt Fake-Publisher und
# lokale Registry; eine versehentlich auf ghcr.io zeigende
# XRAY_DRY_RUN_REGISTRY haette den Dry-Run an einen echten
# Registry-Endpunkt geschickt. Der Guard greift vor jedem anderen
# Setup, deshalb kein State-Dir und keine Registry-/fake-gh-
# Vorbereitung noetig. Wir umgehen die `run_dry_run`-Wrapper, die
# selbst `XRAY_DRY_RUN_REGISTRY=localhost:...` setzt, und ueberschreiben
# es absichtlich mit einer ghcr.io-Form.
state8_dir="$(mktemp -d -t cmake-xray-dry-run-s8.XXXXXX)"
state_dirs+=("$state8_dir")
mkdir -p "$state8_dir/state"
out8_rc=0
out8=$(docker run --rm \
    -v "$repo_root:/workspace" \
    -v /var/run/docker.sock:/var/run/docker.sock \
    -v "$state8_dir:/state" \
    --network host \
    -e "XRAY_DRY_RUN_REGISTRY=ghcr.io/intentional-typo" \
    -e "XRAY_DRY_RUN_KEEP_STATE=true" \
    "$image" \
    bash scripts/release-dry-run.sh v0.0.0-dryrun-s8 --state-dir /state 2>&1) || out8_rc=$?
if [ "$out8_rc" -eq 0 ]; then
    echo "FAIL: scenario 8 non-localhost registry should abort, dry-run succeeded" >&2
    failures=$((failures + 1))
elif printf '%s' "$out8" | grep -F -q "is not a localhost-form"; then
    if [ -f "$state8_dir/state/draft_release_created" ] \
       || [ -f "$state8_dir/state/oci_image_published" ] \
       || [ -f "$state8_dir/state/release_published" ]; then
        echo "FAIL: scenario 8 registry guard fired but state markers present" >&2
        failures=$((failures + 1))
    else
        echo "PASS: scenario 8 non-localhost registry aborts before any state transition"
    fi
else
    echo "FAIL: scenario 8 dry-run aborted but did not mention registry guard" >&2
    printf '%s\n' "$out8" | tail -5 >&2 || true
    failures=$((failures + 1))
fi

# ---- Scenario 9: macOS-/Windows-Archiv im Asset-Pfad -> abort vor release_published ----
#
# AP M5-1.7 Tranche C.4: plan-M5-1-7.md "Release- und Artefaktgrenzen"
# verlangt einen Dry-Run-Negativfall, in dem ein macOS-/Windows-Archiv
# in der offiziellen Asset-Liste den Publish-Pfad vor Upload abbricht.
# Der existierende Allowlist-Guard aus AP M5-1.6 (release-allowlist.sh,
# zeile 207 in release-dry-run.sh) ist die einzige Stelle, an der die
# offizielle Linux-Allowlist den finalen Publish blockiert; dieses
# Szenario haengt einen darwin-Asset ueber den neuen --extra-asset-Flag
# in den Asset-Pfad ein und prueft, dass der Dry-Run vor jedem
# state-changing Schritt (kein draft_release_created, kein
# oci_image_published, kein release_published) abbricht und der Fehler
# auf die Allowlist-Verletzung verweist.
state9_dir="$(mktemp -d -t cmake-xray-dry-run-s9.XXXXXX)"
state_dirs+=("$state9_dir")
mkdir -p "$state9_dir/state" "$state9_dir/extra-assets"

# Synthetic darwin archive in the host-side state dir; mounts in at
# /state/extra-assets when the dry-run container runs. File contents do
# not matter -- the allowlist guard inspects basenames only.
darwin_basename_s9="cmake-xray_0.0.0-dryrun-s9_darwin_arm64.tar.gz"
touch "$state9_dir/extra-assets/$darwin_basename_s9"

out9_rc=0
out9=$(docker run --rm \
    -v "$repo_root:/workspace" \
    -v /var/run/docker.sock:/var/run/docker.sock \
    -v "$state9_dir:/state" \
    --network host \
    -e "XRAY_DRY_RUN_REGISTRY=localhost:${registry_port}" \
    -e "XRAY_DRY_RUN_KEEP_STATE=true" \
    "$image" \
    bash scripts/release-dry-run.sh v0.0.0-dryrun-s9 \
        --state-dir /state \
        --extra-asset "/state/extra-assets/$darwin_basename_s9" \
        2>&1) || out9_rc=$?

if [ "$out9_rc" -eq 0 ]; then
    echo "FAIL: scenario 9 macos/windows archive should abort, dry-run succeeded" >&2
    failures=$((failures + 1))
elif ! printf '%s' "$out9" | grep -F -q "M5 release allowlist"; then
    echo "FAIL: scenario 9 dry-run aborted but did not mention M5 release allowlist" >&2
    printf '%s\n' "$out9" | tail -10 >&2 || true
    failures=$((failures + 1))
elif ! printf '%s' "$out9" | grep -F -q "$darwin_basename_s9"; then
    echo "FAIL: scenario 9 allowlist error did not name the darwin asset" >&2
    printf '%s\n' "$out9" | tail -10 >&2 || true
    failures=$((failures + 1))
elif [ -f "$state9_dir/state/draft_release_created" ] \
     || [ -f "$state9_dir/state/oci_image_published" ] \
     || [ -f "$state9_dir/state/release_published" ]; then
    echo "FAIL: scenario 9 allowlist guard fired but state markers present" >&2
    failures=$((failures + 1))
else
    echo "PASS: scenario 9 macos/windows archive aborts before any state transition"
fi

# ---- Scenario 10: stable version updates a pre-existing latest digest ----
#
# Regression for the v1.2.0 release-pipeline failure: cmd_latest in
# scripts/oci-image-publish.sh used to abort hard whenever
# `${repo}:latest` already existed with a digest that differed from the
# new versioned-tag digest. Every release after the first hits exactly
# that condition (latest currently points at the previous stable
# release), so v1.2.0 was the first release that reached the buggy
# code path -- and failed in the GHA "Publish OCI image (latest tag for
# stable releases)" step. Scenarios 1-9 all use prerelease versions
# (`-` suffix), which release-dry-run.sh skips for `cmd_latest`, so the
# bug had zero coverage. This scenario exercises the stable code path:
# pre-seed `latest` with a different image, then run the full dry-run
# for a non-prerelease version, and assert that latest is updated to
# the new versioned digest by the end.
state10_dir="$(mktemp -d -t cmake-xray-dry-run-s10.XXXXXX)"
state_dirs+=("$state10_dir")
mkdir -p "$state10_dir/state"

s10_repo="localhost:${registry_port}/cmake-xray"
s10_latest_tag="${s10_repo}:latest"

# Pre-seed latest with an unrelated image so its digest differs from
# whatever the dry-run will push at :0.0.10.
docker pull alpine:3.21 >/dev/null 2>&1 || true
docker tag alpine:3.21 "$s10_latest_tag"
docker push "$s10_latest_tag" >/dev/null
preseed_latest_digest=$(inspect_digest "$s10_latest_tag")

s10_output=$(run_dry_run v0.0.10 "$state10_dir" 2>&1) || {
    echo "FAIL: scenario 10 stable-version dry-run failed" >&2
    printf '%s\n' "$s10_output" | tail -20 >&2 || true
    failures=$((failures + 1))
}
assert_output_contains "scenario 10 stable dry-run reaches release_published" \
    "$s10_output" "[dry-run] release_published"
assert_marker "scenario 10 stable dry-run created oci_image_published marker" \
    "$state10_dir" "oci_image_published"

# After the dry-run, latest must point at the new versioned digest, not
# the pre-seed alpine digest. This is the core regression: with the
# old cmd_latest pre-push abort, the dry-run would have failed before
# touching latest, and the digest would still equal preseed_latest_digest.
s10_versioned_digest=$(inspect_digest "${s10_repo}:0.0.10")
s10_post_latest_digest=$(inspect_digest "$s10_latest_tag")
if [ -z "$s10_versioned_digest" ]; then
    echo "FAIL: scenario 10 versioned tag :0.0.10 not present after dry-run" >&2
    failures=$((failures + 1))
elif [ "$s10_post_latest_digest" = "$preseed_latest_digest" ]; then
    echo "FAIL: scenario 10 latest still points at pre-seed digest (${preseed_latest_digest})" >&2
    echo "       cmd_latest pre-push abort regression?" >&2
    failures=$((failures + 1))
elif [ "$s10_post_latest_digest" != "$s10_versioned_digest" ]; then
    echo "FAIL: scenario 10 latest digest ${s10_post_latest_digest:-<unset>} does not match versioned ${s10_versioned_digest}" >&2
    failures=$((failures + 1))
else
    echo "PASS: scenario 10 latest updated from pre-seed to new versioned digest (${s10_versioned_digest})"
fi

echo ""
if [ "$failures" -gt 0 ]; then
    echo "$failures release dry-run check(s) FAILED" >&2
    exit 1
fi
echo "All release dry-run checks passed"
