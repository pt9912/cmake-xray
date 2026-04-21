#!/bin/bash
# Verifies that src/hexagon/ does not include adapter or external library headers.
# Registered as a CTest test to catch violations in CI and local runs.

set -euo pipefail

HEXAGON_DIR="${1:?Usage: $0 <path-to-src/hexagon>}"

FORBIDDEN_PATTERNS='adapters/|nlohmann/|CLI/|doctest/|simdjson|rapidjson|cxxopts|argparse'

VIOLATIONS=$(grep -rn '#include' "$HEXAGON_DIR" | grep -E "$FORBIDDEN_PATTERNS" || true)

if [ -n "$VIOLATIONS" ]; then
    echo "Hexagon boundary violation — the following includes are forbidden:"
    echo "$VIOLATIONS"
    exit 1
fi

echo "Hexagon boundary check passed."
exit 0
