# Impact Analysis Report

- Report type: impact
- Compile database: tests/e2e/testdata/m6/file\_api\_partial/build
- Changed file: src/app.cpp
- Affected translation units: 1
- Impact classification: direct
- Observation source: derived
- Target metadata: loaded
- Affected targets: 1

## Directly Affected Translation Units
- src/app.cpp [directory: build] [targets: app]

## Heuristically Affected Translation Units
No heuristically affected translation units.

## Directly Affected Targets
- app [type: STATIC\_LIBRARY]

## Heuristically Affected Targets
No heuristically affected targets.

## Target Graph Reference

Status: `partial`.

| From | To | Resolution | External Target |
|---|---|---|---|
| `app` | `ghost::@99` | external | `ghost::@99` |

## Prioritised Affected Targets

Requested depth: `2`. Effective depth: `0`.

| Display name | Type | Priority class | Graph distance | Evidence strength | Unique key |
|---|---|---|---|---|---|
| `app` | STATIC_LIBRARY | direct | 0 | direct | `app::STATIC_LIBRARY` |

## Diagnostics
- warning: target graph partially loaded; impact prioritisation uses available edges only
- note: reverse target graph traversal stopped at depth 0 (no further reachable targets)
- note: target 'app' references unknown target id 'ghost::@99'
