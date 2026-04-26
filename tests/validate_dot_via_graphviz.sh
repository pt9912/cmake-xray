#!/usr/bin/env bash
# Graphviz-backed DOT syntax gate for cmake-xray (AP M5-1.3 Tranche A).
#
# Iterates the M5 DOT manifest and runs `dot -Tsvg` against every listed
# golden. The SVG output is discarded; the test only checks that Graphviz
# parses each golden without errors. Used by Docker stages where graphviz
# is installed via apt; native CI uses tests/validate_dot_reports.py
# instead.
#
# Usage:
#   validate_dot_via_graphviz.sh <dot-binary> <reports-dir> <manifest>

set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <dot-binary> <reports-dir> <manifest>" >&2
    exit 2
fi

DOT_BIN="$1"
REPORTS_DIR="$2"
MANIFEST="$3"

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
    if ! "$DOT_BIN" -Tsvg "$target" > /dev/null; then
        echo "error: graphviz rejected $target" >&2
        failures=$((failures + 1))
    fi
done < "$MANIFEST"

if [ "$failures" -gt 0 ]; then
    echo "$failures DOT golden(s) failed Graphviz validation" >&2
    exit 1
fi
exit 0
