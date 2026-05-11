# Impact Analysis Report

- Report type: impact
- Compile database: tests/e2e/testdata/m4/partial\_targets/compile\_commands.json
- Changed file: /project/src/main.cpp
- Affected translation units: 1
- Impact classification: direct
- Observation source: exact
- Target metadata: partial
- Affected targets: 1

## Directly Affected Translation Units
- /project/src/main.cpp [directory: /project/build] [targets: app]

## Heuristically Affected Translation Units
No heuristically affected translation units.

## Directly Affected Targets
- app [type: EXECUTABLE]

## Heuristically Affected Targets
No heuristically affected targets.

## Target Graph Reference

Status: `loaded`.

No direct target dependencies.

## Prioritised Affected Targets

Requested depth: `2`. Effective depth: `0`.

| Display name | Type | Priority class | Graph distance | Evidence strength | Unique key |
|---|---|---|---|---|---|
| `app` | EXECUTABLE | direct | 0 | direct | `app::EXECUTABLE` |

## Diagnostics
- note: 1 of 2 targets have no compilable sources and are not included in the analysis
- note: reverse target graph traversal stopped at depth 0 (no further reachable targets)
