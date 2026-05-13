# Project Analysis Report

- Report type: analyze
- Compile database: tests/e2e/testdata/m6/file\_api\_loaded/build
- Translation units: 11
- Translation unit ranking entries: 10 of 11
- Include hotspot entries: 0 of 0
- Top limit: 10
- Include analysis heuristic: yes
- Observation source: derived
- Target metadata: loaded
- Translation units with target mapping: 11 of 11

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
1. src/hub.cpp [directory: build] [targets: hub]
    Metrics: arg_count=3, include_path_count=0, define_count=0
    Diagnostics:
    - warning: cannot read source file for include analysis: src/hub.cpp
2. src/src00.cpp [directory: build] [targets: src00]
    Metrics: arg_count=3, include_path_count=0, define_count=0
    Diagnostics:
    - warning: cannot read source file for include analysis: src/src00.cpp
3. src/src01.cpp [directory: build] [targets: src01]
    Metrics: arg_count=3, include_path_count=0, define_count=0
    Diagnostics:
    - warning: cannot read source file for include analysis: src/src01.cpp
4. src/src02.cpp [directory: build] [targets: src02]
    Metrics: arg_count=3, include_path_count=0, define_count=0
    Diagnostics:
    - warning: cannot read source file for include analysis: src/src02.cpp
5. src/src03.cpp [directory: build] [targets: src03]
    Metrics: arg_count=3, include_path_count=0, define_count=0
    Diagnostics:
    - warning: cannot read source file for include analysis: src/src03.cpp
6. src/src04.cpp [directory: build] [targets: src04]
    Metrics: arg_count=3, include_path_count=0, define_count=0
    Diagnostics:
    - warning: cannot read source file for include analysis: src/src04.cpp
7. src/src05.cpp [directory: build] [targets: src05]
    Metrics: arg_count=3, include_path_count=0, define_count=0
    Diagnostics:
    - warning: cannot read source file for include analysis: src/src05.cpp
8. src/src06.cpp [directory: build] [targets: src06]
    Metrics: arg_count=3, include_path_count=0, define_count=0
    Diagnostics:
    - warning: cannot read source file for include analysis: src/src06.cpp
9. src/src07.cpp [directory: build] [targets: src07]
    Metrics: arg_count=3, include_path_count=0, define_count=0
    Diagnostics:
    - warning: cannot read source file for include analysis: src/src07.cpp
10. src/src08.cpp [directory: build] [targets: src08]
     Metrics: arg_count=3, include_path_count=0, define_count=0
     Diagnostics:
     - warning: cannot read source file for include analysis: src/src08.cpp

## Include Hotspots

Filter: `scope=all`, `depth=all`. Excluded: `0` unknown, `0` mixed.

No include hotspots found.

## Direct Target Dependencies

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

## Target Hubs

| Direction | Threshold | Targets |
|---|---|---|
| Inbound | 10 | `hub` |
| Outbound | 10 | No outgoing hubs. |

## Diagnostics
- note: include-based results are heuristic; conditional or generated includes may be missing
