#!/usr/bin/env bash
# AP M5-1.6 Tranche G.3: shared idempotency check for the GitHub release
# publish step. plan-M5-1-6.md "Veroeffentlichungskette" verlangt, dass
# ein Re-Run mit existierendem Release nur dann uebernommen wird, wenn
# Asset-Liste, Checksums und Notes byte-stabil zum lokalen Stand sind;
# Mismatch muss vor `release_published` abbrechen, statt einen Asset
# stillschweigend zu ueberschreiben.
#
# Vor Tranche G war diese Logik nur im Dry-Run-Orchestrator
# (scripts/release-dry-run.sh) inline implementiert; der echte
# Release-Job in .github/workflows/release.yml rief stattdessen
# `gh release upload --clobber`, was einen divergenten Asset ohne
# Pruefung ueberschrieben haette. Dieser Helper extrahiert den
# Idempotenz-Pfad, sodass Dry-Run und Workflow byte-gleiche Logik
# fahren.
#
# Usage:
#   release-publish-idempotency.sh [--draft] <tag> <notes_file> <asset>...
#
#   --draft       create new releases in draft state (matches the
#                 dry-run flow which promotes to public separately).
#                 Without the flag, missing releases are created public.
#   <tag>         the SemVer release tag with leading 'v'.
#   <notes_file>  release-notes file used both for create and for
#                 the manifest-mismatch comparison on existing releases.
#   <asset>...    one or more local asset files (archive plus sidecar).
#                 Allowlist enforcement is the caller's responsibility.
#
# Exit codes:
#   0  release is in the expected state (created or matched)
#   1  manifest mismatch: existing release diverges from local manifest
#   2  usage or environment error
#
# Behaviour for an existing release:
#   - Each asset's local sha256 is compared to the remote sha256;
#     mismatch -> exit 1 with a Klartext message naming the asset.
#   - The local notes file is compared to the remote notes; mismatch ->
#     exit 1.
#   - Only assets whose name is not already present in the release are
#     uploaded; --clobber is intentionally never used so a divergent
#     asset can never be silently replaced.
#   - The notes file is rewritten via `gh release edit --notes-file` so
#     the remote pinned to the local manifest after the check passes.
set -euo pipefail

draft="false"
while [ $# -gt 0 ]; do
    case "$1" in
        --draft) draft="true"; shift ;;
        --)      shift; break ;;
        -*)
            echo "error: unknown flag '$1'" >&2
            exit 2
            ;;
        *) break ;;
    esac
done

if [ $# -lt 3 ]; then
    echo "usage: $(basename "$0") [--draft] <tag> <notes_file> <asset>..." >&2
    exit 2
fi

tag="$1"
notes_file="$2"
shift 2
assets=("$@")

if [ ! -f "$notes_file" ]; then
    echo "error: notes file '$notes_file' does not exist" >&2
    exit 2
fi
for asset in "${assets[@]}"; do
    if [ ! -f "$asset" ]; then
        echo "error: asset '$asset' does not exist" >&2
        exit 2
    fi
done

if ! command -v gh >/dev/null 2>&1; then
    echo "error: gh CLI is required" >&2
    exit 2
fi
if ! command -v jq >/dev/null 2>&1; then
    echo "error: jq is required" >&2
    exit 2
fi
if ! command -v sha256sum >/dev/null 2>&1; then
    echo "error: sha256sum is required" >&2
    exit 2
fi

asset_basename_sha() {
    # AP M5-1.6 Tranche G.3: archive-as-asset has its sha256 in the
    # accompanying *.sha256 sidecar (one space-separated line, hex digest
    # first); other assets (the sidecar itself, or any future
    # non-archive) are hashed live. The sidecar must hash the archive's
    # current content -- otherwise the sha256sum -c gate in the build
    # job would have failed before we ever reached this script.
    local asset="$1"
    case "$asset" in
        *.sha256)
            sha256sum "$asset" | awk '{print $1}'
            ;;
        *)
            local sidecar="${asset}.sha256"
            if [ -f "$sidecar" ]; then
                awk '{print $1}' "$sidecar"
            else
                sha256sum "$asset" | awk '{print $1}'
            fi
            ;;
    esac
}

if ! gh release view "$tag" >/dev/null 2>&1; then
    if [ "$draft" = "true" ]; then
        gh release create "$tag" --draft --title "$tag" \
            --notes-file "$notes_file" "${assets[@]}" >/dev/null
    else
        gh release create "$tag" --verify-tag --title "$tag" \
            --notes-file "$notes_file" "${assets[@]}" >/dev/null
    fi
    echo "release '$tag' created with ${#assets[@]} asset(s)"
    exit 0
fi

# AP M5-1.6 Tranche G.4: log line is the diagnostic anchor that
# tests/release/test_release_dry_run.sh asserts for scenarios 1 and 2
# ("already exists"); also keeps the operator-visible signal that the
# helper is taking the existing-release branch instead of create.
echo "release '$tag' already exists; validating manifest"

# `--json assets,body,isDraft` is the canonical view shape; we rely on
# both real `gh` (since 2022) and the fake-gh wrapper (which ignores
# unknown args and emits the full metadata.json). No fallback: a hard
# `gh` failure should surface, not silently degrade to a different
# JSON shape that the lookups below would shape-mismatch.
existing_meta="$(gh release view "$tag" --json assets,body,isDraft)"

# AP M5-1.6 Tranche G.3: gh's JSON `assets[]` includes a `digest` field
# of the form "sha256:<hex>" but historically also exposed `sha256`;
# this helper is robust to either name. The empty-string fallback
# yields "no existing asset", which lets the upload-missing branch
# below cover both fresh-asset and re-upload-after-failed-upload cases.
remote_asset_sha() {
    local name="$1"
    jq -r --arg name "$name" '
        .assets[]
        | select(.name == $name)
        | (.sha256 // .digest // "")
        | sub("^sha256:"; "")
    ' <<<"$existing_meta"
}

mismatches=0
expected_basenames=()
for asset in "${assets[@]}"; do
    expected_basenames+=("$(basename "$asset")")
done

# AP M5-1.6 Tranche H.3: extra-remote-asset detection. plan-M5-1-6.md
# "Veroeffentlichungskette" verlangt eine *exakte* Asset-Liste; ein
# Remote-Release mit zusaetzlichen Assets, die lokal nicht erwartet
# werden (z. B. ein altes `*-darwin-arm64.tar.gz` aus einer Pre-
# Tranche-E-Welt, ein manueller Operator-Upload, ein Remnant aus
# einem halb-gefailten Run) ist ein Manifest-Mismatch. Vor Tranche
# H.3 wurden solche Asset-Reste stillschweigend uebernommen, weil
# der Helper nur die andere Richtung (lokal-fehlt-remote vs.
# lokal-vs-remote-Sha) geprueft hat.
while IFS= read -r remote_name; do
    [ -z "$remote_name" ] && continue
    found="false"
    for expected in "${expected_basenames[@]}"; do
        if [ "$remote_name" = "$expected" ]; then
            found="true"
            break
        fi
    done
    if [ "$found" = "false" ]; then
        echo "error: remote release has unexpected asset '$remote_name' not in local manifest" >&2
        mismatches=$((mismatches + 1))
    fi
done < <(jq -r '.assets[].name' <<<"$existing_meta")

for asset in "${assets[@]}"; do
    name="$(basename "$asset")"
    expected_sha="$(asset_basename_sha "$asset")"
    existing_sha="$(remote_asset_sha "$name")"
    if [ -n "$existing_sha" ] && [ "$existing_sha" != "$expected_sha" ]; then
        echo "error: existing release asset $name sha256 ($existing_sha)" \
             "does not match local build ($expected_sha)" >&2
        mismatches=$((mismatches + 1))
    fi
done

# AP M5-1.6 Tranche G.4: GitHub's API normalises CRLF -> LF and may
# strip a trailing newline on stored release bodies. A naive byte
# compare would fire a manifest-mismatch on every legitimate re-run
# where the API has touched the notes. Normalise both sides (drop CR,
# strip trailing whitespace per line, strip trailing blank lines)
# and compare via sha256 so the diff stays cheap and order-stable.
normalize_notes() {
    awk '
        { sub(/\r$/, ""); sub(/[ \t]+$/, ""); lines[NR] = $0 }
        END {
            n = NR
            while (n > 0 && lines[n] == "") n--
            for (i = 1; i <= n; i++) print lines[i]
        }
    '
}
expected_notes_sha="$(normalize_notes <"$notes_file" | sha256sum | awk '{print $1}')"
existing_notes="$(jq -r '.body // .notes // ""' <<<"$existing_meta")"
existing_notes_sha="$(printf '%s\n' "$existing_notes" \
    | normalize_notes | sha256sum | awk '{print $1}')"
if [ -n "$existing_notes" ] && [ "$existing_notes_sha" != "$expected_notes_sha" ]; then
    echo "error: existing release notes for '$tag' differ from the local manifest" >&2
    echo "       (expected sha256=$expected_notes_sha, remote sha256=$existing_notes_sha after CRLF normalisation)" >&2
    echo "       this is a manifest-mismatch and must be resolved manually before re-run" >&2
    mismatches=$((mismatches + 1))
fi

if [ "$mismatches" -gt 0 ]; then
    echo "error: $mismatches manifest mismatch(es) for '$tag'; aborting before any release_published transition" >&2
    exit 1
fi

uploaded=0
for asset in "${assets[@]}"; do
    name="$(basename "$asset")"
    existing_sha="$(remote_asset_sha "$name")"
    if [ -z "$existing_sha" ]; then
        gh release upload "$tag" "$asset" >/dev/null
        uploaded=$((uploaded + 1))
    fi
done

gh release edit "$tag" --notes-file "$notes_file" >/dev/null
echo "release '$tag' validated; uploaded $uploaded missing asset(s), notes pinned"
