# Project Analysis Report

- Report type: analyze
- Compile database: tests/e2e/testdata/m3/report\_top\_limit/compile\_commands.json
- Translation units: 3
- Translation unit ranking entries: 1 of 3
- Include hotspot entries: 1 of 2
- Top limit: 1
- Include analysis heuristic: yes

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
| target-graph | not_loaded |
| target-hubs | not_loaded |

## Translation Unit Ranking
1. src/app/main.cpp [directory: build/app]
    Metrics: arg_count=8, include_path_count=2, define_count=1
    Diagnostics:
    - warning: could not resolve include "generated/version.h" from src/app/main.cpp

## Include Hotspots

Filter: `scope=all`, `depth=all`. Excluded: `0` unknown, `0` mixed.

Showing 1 of 2 include hotspots.

| Header | Origin | Depth | Affected TUs | Context |
|---|---|---|---|---|
| `include/common/config.h` | `project` | `direct` | 3 | src/app/main.cpp [directory: build/app] / src/lib/core.cpp [directory: build/lib] / src/tools/tool.cpp [directory: build/tools] |

## Diagnostics
- note: include-based results are heuristic; conditional or generated includes may be missing
