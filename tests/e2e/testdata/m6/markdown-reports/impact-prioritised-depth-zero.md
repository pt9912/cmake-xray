# Impact Analysis Report

- Report type: impact
- Compile database: tests/e2e/testdata/m6/file\_api\_loaded/build
- Changed file: src/hub.cpp
- Affected translation units: 1
- Impact classification: direct
- Observation source: derived
- Target metadata: loaded
- Affected targets: 1

## Directly Affected Translation Units
- src/hub.cpp [directory: build] [targets: hub]

## Heuristically Affected Translation Units
No heuristically affected translation units.

## Directly Affected Targets
- hub [type: STATIC\_LIBRARY]

## Heuristically Affected Targets
No heuristically affected targets.

## Target Graph Reference

Status: `loaded`.

| From | To | Resolution | External Target |
|---|---|---|---|
| `src00` | `hub` | resolved |  |
| `src01` | `hub` | resolved |  |
| `src02` | `hub` | resolved |  |
| `src03` | `hub` | resolved |  |
| `src04` | `hub` | resolved |  |
| `src05` | `hub` | resolved |  |
| `src06` | `hub` | resolved |  |
| `src07` | `hub` | resolved |  |
| `src08` | `hub` | resolved |  |
| `src09` | `hub` | resolved |  |

## Prioritised Affected Targets

Requested depth: `0`. Effective depth: `0`.

| Display name | Type | Priority class | Graph distance | Evidence strength | Unique key |
|---|---|---|---|---|---|
| `hub` | STATIC_LIBRARY | direct | 0 | direct | `hub::STATIC_LIBRARY` |

## Diagnostics
No diagnostics.
