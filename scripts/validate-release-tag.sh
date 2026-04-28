#!/usr/bin/env bash
# AP M5-1.6 Tranche A: SemVer-Tag-Validator. Accepts a release tag in the
# vMAJOR.MINOR.PATCH or vMAJOR.MINOR.PATCH-PRERELEASE form (build metadata
# +... is rejected per plan-M5-1-6.md "SemVer-Tag-Muster"). On success the
# canonical app version (the tag without its leading 'v') is printed on
# stdout and exit code is 0; on failure a Klartext message is printed on
# stderr and exit code is 1. Usage error (wrong number of arguments) yields
# exit code 2 to keep it distinct from validation failure for consumers.
#
# The validator is the single source of truth for XRAY_APP_VERSION in CI;
# the release workflow captures stdout into $GITHUB_ENV and forwards it as
# `-DXRAY_APP_VERSION=...` to cmake and `--build-arg XRAY_APP_VERSION=...`
# to buildx (see plan-M5-1-6.md Entscheidungen-Sektion).
set -euo pipefail

if [ $# -ne 1 ]; then
    echo "usage: $(basename "$0") <tag>" >&2
    echo "  tag must match vMAJOR.MINOR.PATCH or vMAJOR.MINOR.PATCH-PRERELEASE" >&2
    exit 2
fi

tag="$1"

# Plan regex from plan-M5-1-6.md "SemVer-Tag-Muster" (line ~76). Anchored on
# both ends. The numeric identifiers reject leading zeros except the literal
# "0"; alphanumeric prerelease identifiers must contain at least one
# non-digit character so a numeric "01" cannot pass through the second
# branch. Build metadata (+...) is intentionally NOT in the grammar.
plan_regex='^v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(-((0|[1-9][0-9]*)|[0-9A-Za-z-]*[A-Za-z-][0-9A-Za-z-]*)(\.((0|[1-9][0-9]*)|[0-9A-Za-z-]*[A-Za-z-][0-9A-Za-z-]*))*)?$'

if [[ ! "$tag" =~ $plan_regex ]]; then
    echo "error: tag '$tag' does not match the M5 release pattern" >&2
    echo "hint: expected vMAJOR.MINOR.PATCH or vMAJOR.MINOR.PATCH-PRERELEASE" >&2
    echo "hint: build metadata (+...) is not permitted in M5 releases" >&2
    exit 1
fi

# Strip the leading 'v' to produce the canonical app version that flows into
# XRAY_APP_VERSION, application_info(), and `cmake-xray --version`.
printf '%s\n' "${tag#v}"
