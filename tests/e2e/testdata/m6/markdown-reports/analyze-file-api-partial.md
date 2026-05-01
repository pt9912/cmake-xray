# Project Analysis Report

- Report type: analyze
- Compile database: tests/e2e/testdata/m6/file\_api\_partial/build
- Translation units: 2
- Translation unit ranking entries: 2 of 2
- Include hotspot entries: 0 of 0
- Top limit: 10
- Include analysis heuristic: yes
- Observation source: derived
- Target metadata: loaded
- Translation units with target mapping: 2 of 2

## Translation Unit Ranking
1. src/app.cpp [directory: build] [targets: app]
    Metrics: arg_count=3, include_path_count=0, define_count=0
    Diagnostics:
    - warning: cannot read source file for include analysis: src/app.cpp
2. src/lib.cpp [directory: build] [targets: lib]
    Metrics: arg_count=3, include_path_count=0, define_count=0
    Diagnostics:
    - warning: cannot read source file for include analysis: src/lib.cpp

## Include Hotspots
No include hotspots found.

## Direct Target Dependencies

Status: `partial`.

| From | To | Resolution | External Target |
|---|---|---|---|
| `app` | `ghost::@99` | external | `ghost::@99` |

## Target Hubs

| Direction | Threshold | Targets |
|---|---|---|
| Inbound | 10 | No incoming hubs. |
| Outbound | 10 | No outgoing hubs. |

## Diagnostics
- note: include-based results are heuristic; conditional or generated includes may be missing
- note: target 'app' references unknown target id 'ghost::@99'
