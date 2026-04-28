#!/usr/bin/env bash
# AP M5-1.6 Tranche E: smoke tests for scripts/release-allowlist.sh.
# Pinnt die Plan-Plattformartefakt-Sperre: offizielle Linux-Archiv-
# Asset-Namen werden akzeptiert; Preview-Artefakte (macOS/Windows) und
# unbekannte Dateien fuehren zum Abbruch mit Exit 1. Eine Allowlist-
# Relaxierung ohne Plan-Aenderung wuerde mindestens einen dieser Tests
# brechen.
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
allowlist_script="$repo_root/scripts/release-allowlist.sh"

if ! [ -x "$allowlist_script" ]; then
    echo "error: $allowlist_script missing or not executable" >&2
    exit 2
fi

failures=0

assert_accept() {
    local description="$1"
    shift
    if "$allowlist_script" "$@" >/dev/null 2>&1; then
        echo "PASS: $description"
    else
        echo "FAIL: $description -- allowlist unexpectedly rejected" >&2
        "$allowlist_script" "$@" 2>&1 | tail -3 >&2 || true
        failures=$((failures + 1))
    fi
}

assert_reject() {
    local description="$1" expected_pattern="$2"
    shift 2
    local out rc=0
    out=$("$allowlist_script" "$@" 2>&1) || rc=$?
    if [ "$rc" -eq 0 ]; then
        echo "FAIL: $description -- allowlist unexpectedly accepted" >&2
        failures=$((failures + 1))
        return
    fi
    if ! printf '%s' "$out" | grep -F -q -- "$expected_pattern"; then
        echo "FAIL: $description -- error output did not contain '$expected_pattern'" >&2
        printf '%s\n' "$out" | head -5 >&2 || true
        failures=$((failures + 1))
        return
    fi
    echo "PASS: $description"
}

work_dir="$(mktemp -d -t cmake-xray-allowlist.XXXXXX)"
trap 'rm -rf "$work_dir"' EXIT

# Fixtures: empty files with the names we want to test against. The
# allowlist only inspects basenames, so file content does not matter.
linux_archive="$work_dir/cmake-xray_1.2.3_linux_x86_64.tar.gz"
linux_sidecar="$work_dir/cmake-xray_1.2.3_linux_x86_64.tar.gz.sha256"
darwin_archive="$work_dir/cmake-xray_1.2.3_darwin_arm64.tar.gz"
windows_archive="$work_dir/cmake-xray_1.2.3_windows_x86_64.zip"
release_notes="$work_dir/release-notes.md"
old_version_archive="$work_dir/cmake-xray_1.0.0_linux_x86_64.tar.gz"
touch "$linux_archive" "$linux_sidecar" "$darwin_archive" "$windows_archive" \
      "$release_notes" "$old_version_archive"

# --- Positivpfad: offizielle Asset-Namen ---
assert_accept "linux archive plus sidecar are accepted" 1.2.3 \
    "$linux_archive" "$linux_sidecar"

# --- Negativpfade: Preview-Artefakte und unbekannte Dateien ---
assert_reject "darwin preview artefact is rejected" \
    "darwin_arm64.tar.gz" 1.2.3 \
    "$linux_archive" "$darwin_archive"

assert_reject "windows preview artefact is rejected" \
    "windows_x86_64.zip" 1.2.3 \
    "$linux_archive" "$windows_archive"

assert_reject "release-notes file is not an upload-able asset" \
    "release-notes.md" 1.2.3 \
    "$linux_archive" "$release_notes"

assert_reject "version-mismatch archive is rejected" \
    "cmake-xray_1.0.0_linux_x86_64.tar.gz" 1.2.3 \
    "$old_version_archive"

# --- Usage-Fehler ---
out=$("$allowlist_script" 2>&1) || rc=$?; rc=${rc:-0}
if [ "$rc" -eq 2 ]; then
    echo "PASS: missing-args call exits 2 (usage error)"
else
    echo "FAIL: missing-args call exits $rc, expected 2" >&2
    failures=$((failures + 1))
fi

out=$("$allowlist_script" "" "$linux_archive" 2>&1) || rc=$?; rc=${rc:-0}
if [ "$rc" -eq 2 ]; then
    echo "PASS: empty-version call exits 2 (usage error)"
else
    echo "FAIL: empty-version call exits $rc, expected 2" >&2
    failures=$((failures + 1))
fi

echo ""
if [ "$failures" -gt 0 ]; then
    echo "$failures release allowlist check(s) FAILED" >&2
    exit 1
fi
echo "All release allowlist checks passed"
