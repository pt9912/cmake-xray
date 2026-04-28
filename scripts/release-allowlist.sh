#!/usr/bin/env bash
# AP M5-1.6 Tranche E: release-allowlist guard for the final publish job.
#
# Usage: scripts/release-allowlist.sh <version> <asset-path> [<asset-path>...]
#
# Validates that every <asset-path>'s basename matches the fixed allowlist
# of officially supported M5 release assets (Linux archive plus its sha256
# sidecar). macOS / Windows preview artefacts and any other unexpected
# files (release notes, debug bundles, etc.) cause a hard abort with a
# Klartext error before the workflow uploads anything.
#
# Plan-Vertrag (plan-M5-1-6.md "Veroeffentlichungskette" und
# "Plattformartefakte macOS/Windows"):
#   - Jeder Publish-Schritt verwendet eine explizite Allowlist fuer
#     offizielle Asset-Namen und OCI-Tags. Dateien ausserhalb dieser
#     Allowlist, insbesondere Preview-Artefakte, fuehren im finalen
#     Release-Job zum Abbruch statt zu Upload oder implizitem Ignorieren.
#   - Der offizielle M5-Release-Pfad muss technisch verhindern, dass
#     Preview-Artefakte in die normale Asset-Liste, in Checksums, in
#     Release-Manifeste oder in GHCR/OCI-Verweise aufgenommen werden.
#
# The allowlist is a Bash array constant per plan-Entscheidung; tests in
# tests/release/test_release_allowlist.sh exercise both the accept and
# the reject paths so a future relaxation of the contract surfaces in
# review rather than silently shipping a preview artefact.
set -euo pipefail

if [ $# -lt 2 ]; then
    echo "usage: $(basename "$0") <version> <asset-path> [<asset-path>...]" >&2
    echo "  version: the canonical app version without leading 'v'" >&2
    echo "  asset-path: full path to a candidate release asset" >&2
    exit 2
fi

version="$1"
shift

if [ -z "$version" ]; then
    echo "error: version must be non-empty" >&2
    exit 2
fi

# Allowlist of official asset basenames. Each entry is the exact filename
# the final release must publish; the version placeholder __VERSION__ is
# replaced with the canonical app version at runtime so the same array
# applies to every release tag. Adding a Plattform here is a deliberate
# Plan-Aenderung (e.g. macOS preview wuerde ueber einen separaten
# Preview-Job mit eigener Allowlist laufen).
allowed_basenames=(
    "cmake-xray___VERSION___linux_x86_64.tar.gz"
    "cmake-xray___VERSION___linux_x86_64.tar.gz.sha256"
)

# Build the resolved allowlist once (basenames substituted) so the
# matching loop is straightforward.
resolved_allowlist=()
for entry in "${allowed_basenames[@]}"; do
    resolved_allowlist+=("${entry//__VERSION__/$version}")
done

failures=0
for asset_path in "$@"; do
    asset_name="$(basename "$asset_path")"
    matched="false"
    for allowed in "${resolved_allowlist[@]}"; do
        if [ "$asset_name" = "$allowed" ]; then
            matched="true"
            break
        fi
    done
    if [ "$matched" != "true" ]; then
        echo "error: asset '$asset_name' is not in the M5 release allowlist" >&2
        echo "       full path: $asset_path" >&2
        failures=$((failures + 1))
    fi
done

if [ "$failures" -gt 0 ]; then
    echo "" >&2
    echo "error: $failures asset(s) failed the M5 release allowlist; aborting publish" >&2
    echo "hint: official M5 assets for version '$version' are:" >&2
    for allowed in "${resolved_allowlist[@]}"; do
        echo "  - $allowed" >&2
    done
    echo "hint: macOS / Windows preview artefacts must use a separate Preview-Publish-Pfad" >&2
    echo "      with its own allowlist; see plan-M5-1-6.md 'Plattformartefakte'." >&2
    exit 1
fi

echo "release-allowlist: ${#@} asset(s) validated against the M5 allowlist for $version"
