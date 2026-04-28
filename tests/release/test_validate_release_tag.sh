#!/usr/bin/env bash
# AP M5-1.6 Tranche A: smoke tests for scripts/validate-release-tag.sh.
# Covers the four positive and twelve negative cases from plan-M5-1-6.md
# "Smoke- und Validierungstests" (lines 204-205); each case is wired via
# assert_accept (validator returns 0 with the expected normalised version)
# or assert_reject (validator returns non-zero) so a regression in either
# direction surfaces with a clear PASS/FAIL line.
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
validator="$repo_root/scripts/validate-release-tag.sh"

if ! [ -x "$validator" ]; then
    echo "error: $validator missing or not executable" >&2
    exit 2
fi

failures=0

assert_accept() {
    local description="$1" tag="$2" expected_version="$3"
    local actual
    if actual=$("$validator" "$tag" 2>/dev/null); then
        if [ "$actual" = "$expected_version" ]; then
            echo "PASS: $description"
        else
            echo "FAIL: $description -- expected '$expected_version', got '$actual'" >&2
            failures=$((failures + 1))
        fi
    else
        echo "FAIL: $description -- validator rejected valid tag '$tag'" >&2
        failures=$((failures + 1))
    fi
}

assert_reject() {
    local description="$1" tag="$2"
    if "$validator" "$tag" >/dev/null 2>&1; then
        echo "FAIL: $description -- validator accepted invalid tag '$tag'" >&2
        failures=$((failures + 1))
    else
        echo "PASS: $description"
    fi
}

# Positive cases (plan-M5-1-6.md line 204).
assert_accept "v0.0.0 is accepted" "v0.0.0" "0.0.0"
assert_accept "v1.2.3 is accepted" "v1.2.3" "1.2.3"
assert_accept "v1.2.3-rc.1 is accepted" "v1.2.3-rc.1" "1.2.3-rc.1"
assert_accept "v1.2.3-alpha.1-2 is accepted" "v1.2.3-alpha.1-2" "1.2.3-alpha.1-2"

# Negative cases (plan-M5-1-6.md line 205).
assert_reject "1.2.3 (no leading v) is rejected" "1.2.3"
assert_reject "v1.2 (incomplete) is rejected" "v1.2"
assert_reject "v1.2.3.4 (four components) is rejected" "v1.2.3.4"
assert_reject "v01.2.3 (leading zero MAJOR) is rejected" "v01.2.3"
assert_reject "v1.02.3 (leading zero MINOR) is rejected" "v1.02.3"
assert_reject "v1.2.03 (leading zero PATCH) is rejected" "v1.2.03"
assert_reject "v1.2.3- (empty prerelease) is rejected" "v1.2.3-"
assert_reject "v1.2.3-01 (leading zero in prerelease) is rejected" "v1.2.3-01"
assert_reject "v1.2.3-rc..1 (empty prerelease segment) is rejected" "v1.2.3-rc..1"
assert_reject "v1.2.3+build (build metadata) is rejected" "v1.2.3+build"
assert_reject "vfoo is rejected" "vfoo"
assert_reject "v1.2.x is rejected" "v1.2.x"

# Usage-error path: missing argument exits 2 (distinct from validation
# failure exit 1) so consumers can tell the two apart.
if "$validator" >/dev/null 2>&1; then
    echo "FAIL: missing-argument call must exit non-zero" >&2
    failures=$((failures + 1))
else
    rc=0
    "$validator" >/dev/null 2>&1 || rc=$?
    if [ "$rc" -eq 2 ]; then
        echo "PASS: missing-argument call exits 2 (usage error)"
    else
        echo "FAIL: missing-argument call exits $rc, expected 2" >&2
        failures=$((failures + 1))
    fi
fi

echo ""
if [ "$failures" -gt 0 ]; then
    echo "$failures release tag validation test(s) FAILED" >&2
    exit 1
fi
echo "All release tag validation tests passed"
