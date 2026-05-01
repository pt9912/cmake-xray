# Impact Analysis Report

- Report type: impact
- Compile database: tests/e2e/testdata/m6/file\_api\_cycle/build
- Changed file: src/cycA.cpp
- Affected translation units: 1
- Impact classification: direct
- Observation source: derived
- Target metadata: loaded
- Affected targets: 1

## Directly Affected Translation Units
- src/cycA.cpp [directory: build] [targets: cycA]

## Heuristically Affected Translation Units
No heuristically affected translation units.

## Directly Affected Targets
- cycA [type: STATIC\_LIBRARY]

## Heuristically Affected Targets
No heuristically affected targets.

## Target Graph Reference

Status: `loaded`.

| From | To | Resolution | External Target |
|---|---|---|---|
| `cycA` | `cycB` | resolved |  |
| `cycB` | `cycC` | resolved |  |
| `cycC` | `cycA` | resolved |  |

## Prioritised Affected Targets

Requested depth: `2`. Effective depth: `2`.

| Display name | Type | Priority class | Graph distance | Evidence strength | Unique key |
|---|---|---|---|---|---|
| `cycA` | STATIC_LIBRARY | direct | 0 | direct | `cycA::STATIC_LIBRARY` |
| `cycC` | STATIC_LIBRARY | direct_dependent | 1 | direct | `cycC::STATIC_LIBRARY` |
| `cycB` | STATIC_LIBRARY | transitive_dependent | 2 | direct | `cycB::STATIC_LIBRARY` |

## Diagnostics
- warning: reverse target graph traversal hit depth limit 2 within a cycle; some transitively dependent targets may be missing
