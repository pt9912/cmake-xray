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
# `if:` clauses; Tranche D's dry-run can also call them against a local
# registry by overriding $XRAY_OCI_REGISTRY_INSECURE for plaintext HTTP.
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

read_remote_digest() {
    local tag="$1"
    docker buildx imagetools inspect "$tag" --raw 2>/dev/null \
        | sha256sum 2>/dev/null \
        | awk '{print "sha256:" $1}'
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
    if [ -z "$(read_local_image_id)" ]; then
        echo "error: local image $versioned_tag missing; run '$(basename "$0") $image_repo $version build' first" >&2
        exit 1
    fi

    # Idempotency: if the remote tag already exists, compare the manifest
    # digest before pushing. A matching digest means the previous push
    # already wrote the same content — re-run is a no-op. A mismatch is
    # a hard error because pushing would silently rewrite the tag.
    local existing_digest
    existing_digest="$(read_remote_digest "$versioned_tag" || true)"

    docker push "$versioned_tag"

    local pushed_digest
    pushed_digest="$(read_remote_digest "$versioned_tag")"

    if [ -n "$existing_digest" ] && [ "$existing_digest" != "$pushed_digest" ]; then
        echo "error: remote tag $versioned_tag already existed with digest $existing_digest" >&2
        echo "       the new push produced a different digest $pushed_digest" >&2
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

    local existing_latest_digest
    existing_latest_digest="$(read_remote_digest "$latest_tag" || true)"

    if [ -n "$existing_latest_digest" ] && [ "$existing_latest_digest" != "$versioned_digest" ]; then
        echo "error: remote $latest_tag already points at digest $existing_latest_digest" >&2
        echo "       which differs from the validated $versioned_tag digest $versioned_digest" >&2
        echo "       'latest' may only be updated by the documented manual recovery path; aborting" >&2
        exit 1
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
