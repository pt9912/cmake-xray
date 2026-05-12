# Project Analysis Report

- Report type: analyze
- Compile database: tests/e2e/testdata/m4/multi\_target/build
- Translation units: 3
- Translation unit ranking entries: 3 of 3
- Include hotspot entries: 0 of 0
- Top limit: 10
- Include analysis heuristic: yes
- Observation source: derived
- Target metadata: loaded
- Translation units with target mapping: 3 of 3

## Analysis Configuration

- Sections: `tu-ranking`, `include-hotspots`, `target-graph`, `target-hubs`
- TU thresholds: `arg_count=0`, `include_path_count=0`, `define_count=0`
- Min hotspot TUs: `2`
- Target hub thresholds: in=`10`, out=`10`

### Section States

| Section | State |
|---|---|
| tu-ranking | active |
| include-hotspots | active |
| target-graph | active |
| target-hubs | active |

## Translation Unit Ranking
1. src/shared.cpp [directory: build] [targets: app, core]
    Metrics: arg_count=6, include_path_count=1, define_count=1
    Diagnostics:
    - warning: cannot read source file for include analysis: src/shared.cpp
2. src/main.cpp [directory: build] [targets: app]
    Metrics: arg_count=5, include_path_count=1, define_count=0
    Diagnostics:
    - warning: cannot read source file for include analysis: src/main.cpp
3. src/shared.cpp [directory: build] [targets: app, core]
    Metrics: arg_count=5, include_path_count=1, define_count=0
    Diagnostics:
    - warning: cannot read source file for include analysis: src/shared.cpp

## Include Hotspots

Filter: `scope=all`, `depth=all`. Excluded: `0` unknown, `0` mixed.

No include hotspots found.

## Direct Target Dependencies

Status: `loaded`.

No direct target dependencies.

## Target Hubs

| Direction | Threshold | Targets |
|---|---|---|
| Inbound | 10 | No incoming hubs. |
| Outbound | 10 | No outgoing hubs. |

## Diagnostics
- note: include-based results are heuristic; conditional or generated includes may be missing
