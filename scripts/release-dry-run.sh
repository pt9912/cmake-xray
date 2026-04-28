#!/usr/bin/env bash
# AP M5-1.6 Tranche D: orchestrator that runs the full release sequence
# locally against fake publishers. The script enforces DRY_RUN and
# FAKE_PUBLISHER, refuses any real GitHub or registry endpoint, and
# reports the three plan-mandated state transitions:
#
#   draft_release_created -> oci_image_published -> release_published
#
# Usage:
#   scripts/release-dry-run.sh <tag> [--state-dir <dir>]
#
#   <tag> is the SemVer release tag with leading 'v' (e.g. v1.2.3 or
#   v1.2.3-rc.1); the script validates it via scripts/validate-release-tag.sh.
#
#   --state-dir <dir> places fake-gh state, the docker registry container
#   and the orchestrator state files into <dir> instead of a temp dir.
#   Used by tests/release/test_release_dry_run.sh to share state across
#   sequential dry-run invocations (so re-run scenarios can pre-seed the
#   directory and observe the second run).
#
# Environment:
#   XRAY_DRY_RUN_REGISTRY    pre-existing local registry host:port; the
#                            script skips the registry-start/stop logic
#                            and uses this one. Tests share one registry
#                            across scenarios via this variable. Plan-
#                            Vertrag (Tranche H.4): only `localhost:PORT`
#                            or `127.0.0.1:PORT` are accepted; any other
#                            form aborts unless XRAY_DRY_RUN_ALLOW_REMOTE_REGISTRY
#                            is set, mirroring the GH_TOKEN-override
#                            shape.
#   XRAY_DRY_RUN_ALLOW_REMOTE_REGISTRY
#                            set to '1' (or any non-empty value) to
#                            override the localhost-form check on
#                            XRAY_DRY_RUN_REGISTRY. Refuse to use this
#                            outside test-infrastructure exercises.
#   XRAY_DRY_RUN_KEEP_STATE  set to 'true' to skip state-dir cleanup so
#                            the test harness can inspect the disposition
#                            after the orchestrator returns.
#
# State markers under <state-dir>/state/:
#   draft_release_created
#   oci_image_published
#   release_published
# Each is a non-empty file written when the corresponding transition has
# completed; the orchestrator exits with a non-zero status if a marker
# is missing at the expected point in the sequence.
set -euo pipefail

if [ $# -lt 1 ]; then
    echo "usage: $(basename "$0") <tag> [--state-dir <dir>]" >&2
    exit 2
fi

tag="$1"
shift

state_dir=""
while [ $# -gt 0 ]; do
    case "$1" in
        --state-dir) state_dir="$2"; shift 2 ;;
        --state-dir=*) state_dir="${1#--state-dir=}"; shift ;;
        *) echo "error: unknown flag '$1'" >&2; exit 2 ;;
    esac
done

if [ -n "${GH_TOKEN:-}" ] && [ -z "${XRAY_DRY_RUN_ALLOW_GH_TOKEN:-}" ]; then
    echo "error: GH_TOKEN is set; refuse to continue with real GitHub credentials" >&2
    echo "hint: unset GH_TOKEN or set XRAY_DRY_RUN_ALLOW_GH_TOKEN=1 if intentional" >&2
    exit 1
fi

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
validate_tag_script="$repo_root/scripts/validate-release-tag.sh"
build_archive_script="$repo_root/scripts/build-release-archive.sh"
oci_publish_script="$repo_root/scripts/oci-image-publish.sh"
fake_gh_script="$repo_root/scripts/release-fake-gh.sh"
allowlist_script="$repo_root/scripts/release-allowlist.sh"
idempotency_script="$repo_root/scripts/release-publish-idempotency.sh"

for required in "$validate_tag_script" "$build_archive_script" \
                "$oci_publish_script" "$fake_gh_script" "$allowlist_script" \
                "$idempotency_script"; do
    if ! [ -x "$required" ]; then
        echo "error: required script $required missing or not executable" >&2
        exit 2
    fi
done

if ! command -v jq >/dev/null 2>&1; then
    echo "error: jq is required" >&2
    exit 2
fi

if ! command -v docker >/dev/null 2>&1; then
    echo "error: docker is required" >&2
    exit 2
fi

# 1. Tag validation -- single source of truth for the canonical version.
version="$(bash "$validate_tag_script" "$tag")"
if [ -z "$version" ]; then
    echo "error: validate-release-tag.sh produced empty version for $tag" >&2
    exit 1
fi
echo "[dry-run] tag=$tag version=$version"

# 2. Set up state directory.
created_state_dir="false"
if [ -z "$state_dir" ]; then
    state_dir="$(mktemp -d -t cmake-xray-dry-run.XXXXXX)"
    created_state_dir="true"
fi
mkdir -p "$state_dir/state" "$state_dir/fake-gh" "$state_dir/bin" \
         "$state_dir/release-out"

cleanup() {
    if [ "$created_state_dir" = "true" ] && [ "${XRAY_DRY_RUN_KEEP_STATE:-}" != "true" ]; then
        rm -rf "$state_dir"
    fi
    if [ -n "${managed_registry:-}" ]; then
        docker stop "$managed_registry" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

# 3. Set up fake gh on PATH.
ln -sf "$fake_gh_script" "$state_dir/bin/gh"
export PATH="$state_dir/bin:$PATH"
export XRAY_FAKE_GH_DIR="$state_dir/fake-gh"
export DRY_RUN="true"
export FAKE_PUBLISHER="true"

# 4. Local registry: reuse an existing one or start a fresh container.
# AP M5-1.6 Tranche H.4: plan-Vertrag verlangt Fake-Publisher und
# lokale Registry; ein versehentliches Setzen von
# XRAY_DRY_RUN_REGISTRY=ghcr.io/foo wuerde den Dry-Run an einen
# echten Registry-Endpunkt schicken. Vor Tranche H wurde der Wert
# ungeprueft uebernommen. Akzeptable Formen sind localhost:PORT
# und 127.0.0.1:PORT (mit Pfad-Suffix, da das Test-Setup
# `localhost:15000/cmake-xray` nutzt). Ein Override existiert
# symmetrisch zu GH_TOKEN ueber XRAY_DRY_RUN_ALLOW_REMOTE_REGISTRY.
managed_registry=""
if [ -n "${XRAY_DRY_RUN_REGISTRY:-}" ]; then
    registry="$XRAY_DRY_RUN_REGISTRY"
    if [ -z "${XRAY_DRY_RUN_ALLOW_REMOTE_REGISTRY:-}" ]; then
        case "$registry" in
            localhost:*|127.0.0.1:*) ;;
            *)
                echo "error: XRAY_DRY_RUN_REGISTRY='$registry' is not a localhost-form" >&2
                echo "       only 'localhost:PORT' or '127.0.0.1:PORT' is permitted; the" >&2
                echo "       dry-run refuses to push to non-local registries" >&2
                echo "hint:  unset XRAY_DRY_RUN_REGISTRY or set" >&2
                echo "       XRAY_DRY_RUN_ALLOW_REMOTE_REGISTRY=1 if intentional" >&2
                exit 1
                ;;
        esac
    fi
    echo "[dry-run] using pre-existing registry at $registry"
else
    # Pick a free port; default 15000 keeps off macOS AirPlay's :5000.
    registry_port="${XRAY_DRY_RUN_REGISTRY_PORT:-15000}"
    registry_name="cmake-xray-dry-run-registry-$$"
    docker run --rm -d -p "${registry_port}:5000" --name "$registry_name" \
        registry:2 >/dev/null
    managed_registry="$registry_name"
    registry="localhost:${registry_port}"
    echo "[dry-run] started managed registry at $registry ($registry_name)"

    # Wait for the registry to accept HTTP.
    for _ in $(seq 1 20); do
        if curl -fsS "http://$registry/v2/" >/dev/null 2>&1; then
            break
        fi
        sleep 0.5
    done
fi

image_repo="${registry}/cmake-xray"
versioned_tag="${image_repo}:${version}"

# 5. Build the linux release archive (skip when a re-run already has a
# byte-identical artifact in the state dir; the build is reproducible
# under a fixed SOURCE_DATE_EPOCH so re-running build-release-archive.sh
# would only burn cmake cycles re-emitting the same bytes).
archive_basename="cmake-xray_${version}_linux_x86_64.tar.gz"
archive_path="$state_dir/release-out/$archive_basename"
sha_path="$archive_path.sha256"
if [ -f "$archive_path" ] && [ -f "$sha_path" ] \
    && (cd "$state_dir/release-out" && sha256sum -c "$archive_basename.sha256" >/dev/null 2>&1); then
    echo "[dry-run] reusing existing archive $archive_path"
else
    SOURCE_DATE_EPOCH=1 bash "$build_archive_script" "$version" "$state_dir/release-out"
    if [ ! -f "$archive_path" ] || [ ! -f "$sha_path" ]; then
        echo "error: archive build did not produce expected files" >&2
        exit 1
    fi
    echo "[dry-run] built $archive_path"
fi

# AP M5-1.6 Tranche E: enforce the M5 allowlist on the candidate assets
# before any state-changing GitHub or OCI operation. Plan-Veroeffentlichungs-
# kette: "Jeder Publish-Schritt verwendet eine explizite Allowlist fuer
# offizielle Asset-Namen ... Dateien ausserhalb dieser Allowlist fuehren
# zum Abbruch statt zu Upload oder implizitem Ignorieren." The dry-run
# triggers the same guard so divergence between dry-run and the real
# release.yml stays impossible.
bash "$allowlist_script" "$version" "$archive_path" "$sha_path"
echo "[dry-run] allowlist accepted ${archive_basename} and sidecar"

# 6. Generate a release-notes file (kept minimal; Tranche E will source
# the canonical CHANGELOG).
notes_file="$state_dir/release-notes.md"
{
    echo "Release $tag"
    echo
    echo "Automated dry-run release for $tag."
    echo
    echo "Assets:"
    echo "- OCI image: ${versioned_tag}"
    echo "- Linux archive: ${archive_basename}"
} > "$notes_file"

# 7. Create or reuse the draft GitHub release.
# AP M5-1.6 Tranche G.3: die Asset-/Notes-/SHA-Idempotenz-Logik liegt
# nun in scripts/release-publish-idempotency.sh, sodass Dry-Run und
# echter Release-Workflow byte-gleiche Asserts fahren. Vor Tranche G
# war die Logik nur hier inline.
bash "$idempotency_script" --draft "$tag" "$notes_file" \
    "$archive_path" "$sha_path"
date > "$state_dir/state/draft_release_created"
echo "[dry-run] draft_release_created"

# 8. Build, push and verify the OCI image (versioned tag plus optional latest).
SOURCE_DATE_EPOCH=1 bash "$oci_publish_script" "$image_repo" "$version" build >/dev/null
bash "$oci_publish_script" "$image_repo" "$version" push >/dev/null
case "$version" in
    *-*)
        echo "[dry-run] skipping :latest update for prerelease $version"
        ;;
    *)
        bash "$oci_publish_script" "$image_repo" "$version" latest >/dev/null
        ;;
esac
date > "$state_dir/state/oci_image_published"
echo "[dry-run] oci_image_published"

# 9. Promote the draft to a public release.
gh release edit "$tag" --draft=false >/dev/null
date > "$state_dir/state/release_published"
echo "[dry-run] release_published"

echo "[dry-run] success: tag=$tag version=$version state=$state_dir"
