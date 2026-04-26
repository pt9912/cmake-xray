#!/usr/bin/env bash
# Graphviz-backed DOT syntax gate for cmake-xray (AP M5-1.3 Tranche A).
#
# Iterates the M5 DOT manifest and runs `dot -T<format>` against every listed
# golden. Output is discarded; the test only checks that Graphviz parses each
# golden without errors. Used by Docker stages where graphviz is installed
# via apt; native CI uses tests/validate_dot_reports.py instead.
#
# Tranche D extends the gate so the same harness can validate alternate
# Graphviz output formats — pass the format as an optional 4th argument
# (default: svg). Multiple format gates can register against the same
# manifest by passing different formats.
#
# Usage:
#   validate_dot_via_graphviz.sh <dot-binary> <reports-dir> <manifest> [format]

set -euo pipefail

# Prevent MSYS2/Git Bash from rewriting Unix-style paths and mirror the
# AP-1.2 lesson from run_e2e.sh so this script is safe to call from
# tests/e2e/run_e2e.sh on Windows once Tranche C wires it up.
export MSYS_NO_PATHCONV=1
export MSYS2_ARG_CONV_EXCL="*"

native_path() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -m "$1"
    else
        printf '%s' "$1"
    fi
}

if [ "$#" -lt 3 ] || [ "$#" -gt 4 ]; then
    echo "usage: $0 <dot-binary> <reports-dir> <manifest> [format]" >&2
    exit 2
fi

DOT_BIN="$(native_path "$1")"
REPORTS_DIR="$(native_path "$2")"
MANIFEST="$(native_path "$3")"
FORMAT="${4:-svg}"

if [ ! -x "$DOT_BIN" ] && ! command -v "$DOT_BIN" >/dev/null 2>&1; then
    echo "error: dot binary not executable: $DOT_BIN" >&2
    exit 2
fi

if [ ! -d "$REPORTS_DIR" ]; then
    echo "error: reports directory not found: $REPORTS_DIR" >&2
    exit 2
fi

if [ ! -f "$MANIFEST" ]; then
    echo "error: manifest not found: $MANIFEST" >&2
    exit 2
fi

failures=0
while IFS= read -r raw_line; do
    line="${raw_line#"${raw_line%%[![:space:]]*}"}"
    line="${line%"${line##*[![:space:]]}"}"
    [ -z "$line" ] && continue
    case "$line" in \#*) continue ;; esac
    target="$REPORTS_DIR/$line"
    if [ ! -f "$target" ]; then
        echo "error: manifest entry has no matching DOT file: $target" >&2
        failures=$((failures + 1))
        continue
    fi
    if ! "$DOT_BIN" "-T$FORMAT" "$target" > /dev/null; then
        echo "error: graphviz ($FORMAT) rejected $target" >&2
        failures=$((failures + 1))
    fi
done < "$MANIFEST"

if [ "$failures" -gt 0 ]; then
    echo "$failures DOT golden(s) failed Graphviz $FORMAT validation" >&2
    exit 1
fi
exit 0
