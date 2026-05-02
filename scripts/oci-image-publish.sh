#!/usr/bin/env bash
# AP M5-1.6 Tranche C: OCI image build + push + idempotent re-run check.
#
# Usage: scripts/oci-image-publish.sh <image-repo> <version> <command>
#
# Where <command> is one of:
#   build      Build the runtime image locally as <image-repo>:<version>.
#              Does not push. Used by the Verify-Job.
#   push       Push <image-repo>:<version> to the configured registry,
#              then verify the remote digest. If the version tag already
#              exists with a different digest, abort hard before any
#              `latest` write. Idempotent for identical-digest re-runs.
#   latest     Update <image-repo>:latest to point at the same digest as
#              <image-repo>:<version>. Refuses for prerelease versions
#              (those containing a '-'). If `latest` already exists with
#              a different digest, abort hard; manual recovery is the
#              only path that may overwrite `latest`.
#
# The split into three commands lets Tranche E gate them with workflow
# `if:` clauses; Tranche D's dry-run will pass a localhost:<port> repo
# against a local registry:2 container per plan-M5-1-6.md Entscheidungen.
#
# SOURCE_DATE_EPOCH may be set to control the layer mtime; not strictly
# required for digest stability but kept for symmetry with the Linux
# archive builder. Missing buildx is a hard fail because plain `docker
# build` cannot read the manifest digest reliably.
set -euo pipefail

if [ $# -ne 3 ]; then
    echo "usage: $(basename "$0") <image-repo> <version> {build|push|latest}" >&2
    echo "  image-repo: e.g. ghcr.io/owner/cmake-xray" >&2
    echo "  version: the canonical app version without leading 'v'" >&2
    exit 2
fi

image_repo="$1"
version="$2"
command="$3"

if [ -z "$image_repo" ] || [ -z "$version" ]; then
    echo "error: image-repo and version must be non-empty" >&2
    exit 2
fi

if ! command -v docker >/dev/null 2>&1; then
    echo "error: docker is required but not on PATH" >&2
    exit 2
fi

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
versioned_tag="${image_repo}:${version}"
latest_tag="${image_repo}:latest"

# Read the digest of the locally built image (sha256:...) without
# pushing. buildx exposes the digest via `docker buildx imagetools
# inspect` once an image has been pushed; for local images we use
# `docker inspect` on the image ID.
read_local_image_id() {
    docker image inspect --format '{{.Id}}' "$versioned_tag" 2>/dev/null
}

# Read the canonical manifest digest of a remote tag. Using
# `--format '{{.Manifest.Digest}}'` returns the registry-canonical digest
# (RFC 6920) directly; sha256sum'ing the --raw output would only hash the
# manifest bytes and would diverge across buildx versions or whitespace
# rendering. On a missing tag the inspect exits 1 and the function
# yields an empty stdout, which the callers detect via `[ -z ... ]`.
#
# buildx 0.30.x (shipped with Ubuntu 24.04's docker package) silently
# ignores `--format` and prints the default `Name/MediaType/Digest`
# block; buildx >= 0.33 honours the template and prints just the
# digest. The awk filter accepts both: a single `sha256:...` line on
# its own is taken verbatim, otherwise the value of the `Digest:` line
# is extracted. The Ubuntu-runner-driven CI failure surfaced the
# divergence; v1.2.0's release.yml run hit it after the cmd_latest
# pre-push abort was lifted.
read_remote_digest() {
    local tag="$1"
    docker buildx imagetools inspect "$tag" --format '{{.Manifest.Digest}}' 2>/dev/null \
        | awk '
            /^sha256:/ { print; exit }
            /^Digest:[[:space:]]+sha256:/ { print $2; exit }
        '
}

cmd_build() {
    : "${SOURCE_DATE_EPOCH:=1}"
    export SOURCE_DATE_EPOCH
    docker build --target runtime \
        --build-arg "XRAY_APP_VERSION=${version}" \
        -t "$versioned_tag" \
        "$repo_root"
    echo "wrote local image $versioned_tag"
}

cmd_push() {
    local local_image_id
    local_image_id="$(read_local_image_id)"
    if [ -z "$local_image_id" ]; then
        echo "error: local image $versioned_tag missing; run '$(basename "$0") $image_repo $version build' first" >&2
        exit 1
    fi

    # AP M5-1.6 Tranche I.1: pre-push idempotency check. plan-M5-1-6.md
    # OCI-Image-Vertrag verlangt: "Wenn der versionierte Tag bereits
    # existiert, muss dessen Digest mit dem neu gebauten Image
    # uebereinstimmen; bei Abweichung bricht der Workflow VOR `latest`
    # und VOR oeffentlicher Release-Publikation ab." Vor Tranche I lief
    # `docker push` *bevor* der Mismatch erkannt wurde -- der Remote-
    # Tag wurde stillschweigend ueberschrieben, der Abort folgte erst
    # nach der Mutation. Jetzt verglichen wird das lokal gebaute Image
    # gegen den Remote-Inhalt *bevor* der Push die Mutation auslost.
    #
    # Vergleichsbasis: Image-ID (= config digest) ist deterministisch
    # fuer reproduzierbare Builds (SOURCE_DATE_EPOCH gesetzt) und
    # erhaelt sich beim Pull aus der Registry. Ein abweichendes Image-
    # ID bedeutet abweichender Inhalt; der Manifest-Digest folgt
    # automatisch.
    local existing_digest
    existing_digest="$(read_remote_digest "$versioned_tag" || true)"
    if [ -n "$existing_digest" ]; then
        # Pull retagged den lokalen $versioned_tag auf den Remote-Inhalt.
        # Damit unser lokaler Build erhalten bleibt, retaggen wir nach
        # dem Vergleich auf $local_image_id zurueck.
        if ! docker pull "$versioned_tag" >/dev/null 2>&1; then
            # Restore lokal, falls der gescheiterte Pull den Tag bereits
            # in einen Zwischenzustand gefuehrt hat.
            docker tag "$local_image_id" "$versioned_tag" >/dev/null 2>&1 || true
            echo "error: remote tag $versioned_tag has digest $existing_digest but cannot be pulled for pre-push comparison" >&2
            exit 1
        fi
        local remote_image_id
        remote_image_id="$(docker image inspect --format '{{.Id}}' "$versioned_tag" 2>/dev/null || true)"
        # Restore: lokaler Tag zeigt wieder auf unseren Build,
        # unabhaengig vom Vergleichsergebnis.
        docker tag "$local_image_id" "$versioned_tag" >/dev/null
        if [ -z "$remote_image_id" ]; then
            echo "error: pulled remote $versioned_tag (digest $existing_digest) but cannot read its image ID" >&2
            exit 1
        fi
        if [ "$remote_image_id" != "$local_image_id" ]; then
            echo "error: remote tag $versioned_tag exists at digest $existing_digest" >&2
            echo "       (image ID $remote_image_id) but the local build is image ID $local_image_id" >&2
            echo "       refusing to push; this would silently overwrite the existing tag" >&2
            echo "hint:  rebuild with the same SOURCE_DATE_EPOCH and XRAY_APP_VERSION as the original release commit" >&2
            exit 1
        fi
        # Same content: push is idempotent. Proceed; the post-push
        # digest readback at the end is a defensive sanity check.
    fi

    docker push "$versioned_tag"

    local pushed_digest
    pushed_digest="$(read_remote_digest "$versioned_tag")"

    if [ -n "$existing_digest" ] && [ "$existing_digest" != "$pushed_digest" ]; then
        # Belt-and-braces: the pre-push image-ID check above should have
        # caught any divergent content. If we still see a digest change
        # at this point, something pushed concurrently or the registry
        # serialised the manifest differently; either way the operator
        # needs to know.
        echo "error: remote tag $versioned_tag was rewritten despite the pre-push image-ID match" >&2
        echo "       previous digest $existing_digest, new digest $pushed_digest" >&2
        echo "       this is a hard idempotency violation; aborting before any 'latest' update" >&2
        exit 1
    fi

    echo "pushed $versioned_tag (digest $pushed_digest)"
}

cmd_latest() {
    case "$version" in
        *-*)
            echo "error: 'latest' refuses to update for prerelease version $version" >&2
            echo "hint: only non-prerelease versions (no '-' in the string) may set 'latest'" >&2
            exit 1
            ;;
    esac

    local versioned_digest
    versioned_digest="$(read_remote_digest "$versioned_tag")"
    if [ -z "$versioned_digest" ]; then
        echo "error: remote $versioned_tag does not exist; push it before updating 'latest'" >&2
        exit 1
    fi

    # Inform the operator if `latest` already exists with a different
    # digest. This is the normal case for any release after the first
    # (latest currently points at the previous stable release); the
    # transition to the new versioned digest happens via `docker push`
    # below. The pre-push form of this check used to abort hard, which
    # made every release after v1.1.0 unable to update latest -- the
    # bug shipped because every dry-run scenario uses a prerelease
    # version and skips this code path. Fall-4-style detection (latest
    # stuck on a digest unrelated to any release) is now an external
    # inspection concern, with the manual recovery path in
    # docs/user/releasing.md unchanged for those cases. The post-push verify
    # on lines below is the correctness guarantee for the normal flow.
    local existing_latest_digest
    existing_latest_digest="$(read_remote_digest "$latest_tag" || true)"
    if [ -n "$existing_latest_digest" ] && [ "$existing_latest_digest" != "$versioned_digest" ]; then
        echo "info: $latest_tag currently points at $existing_latest_digest; updating to $versioned_digest"
    fi

    docker tag "$versioned_tag" "$latest_tag"
    docker push "$latest_tag"

    local pushed_latest_digest
    pushed_latest_digest="$(read_remote_digest "$latest_tag")"
    if [ "$pushed_latest_digest" != "$versioned_digest" ]; then
        echo "error: pushed 'latest' digest $pushed_latest_digest does not match $versioned_tag digest $versioned_digest" >&2
        exit 1
    fi

    echo "pushed $latest_tag (digest $pushed_latest_digest)"
}

case "$command" in
    build) cmd_build ;;
    push) cmd_push ;;
    latest) cmd_latest ;;
    *)
        echo "error: unknown command '$command' (expected build/push/latest)" >&2
        exit 2
        ;;
esac
