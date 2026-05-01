# Impact Analysis Report

- Report type: impact
- Compile database: tests/e2e/testdata/m3/report\_impact\_header/compile\_commands.json
- Changed file: include/common/config.h
- Affected translation units: 3
- Impact classification: heuristic

## Directly Affected Translation Units
No directly affected translation units.

## Heuristically Affected Translation Units
- src/app/main.cpp [directory: build/app]
- src/lib/core.cpp [directory: build/lib]
- src/tools/tool.cpp [directory: build/tools]

## Prioritised Affected Targets

Requested depth: `2`. Effective depth: `0` (no graph).

Target graph not loaded; prioritisation skipped.

## Diagnostics
- warning: could not resolve include "generated/version.h" from src/app/main.cpp
- note: conditional or generated includes may be missing from this result
