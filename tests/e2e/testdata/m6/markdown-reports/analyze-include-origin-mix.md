# Project Analysis Report

- Report type: analyze
- Compile database: tests/e2e/testdata/m6/include\_origin\_mix/compile\_commands.json
- Translation units: 2
- Translation unit ranking entries: 2 of 2
- Include hotspot entries: 4 of 4
- Top limit: 10
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
1. src/a.cpp [directory: build/a]
    Metrics: arg_count=8, include_path_count=2, define_count=0
2. src/b.cpp [directory: build/b]
    Metrics: arg_count=8, include_path_count=2, define_count=0

## Include Hotspots

Filter: `scope=all`, `depth=all`. Excluded: `0` unknown, `0` mixed.

| Header | Origin | Depth | Affected TUs | Context |
|---|---|---|---|---|
| `include/config.h` | `project` | `mixed` | 2 | src/a.cpp [directory: build/a] / src/b.cpp [directory: build/b] |
| `include/wrapper.h` | `project` | `direct` | 2 | src/a.cpp [directory: build/a] / src/b.cpp [directory: build/b] |
| `neighbor.h` | `unknown` | `direct` | 2 | src/a.cpp [directory: build/a] / src/b.cpp [directory: build/b] |
| `system/vector` | `external` | `direct` | 2 | src/a.cpp [directory: build/a] / src/b.cpp [directory: build/b] |

## Diagnostics
- note: include-based results are heuristic; conditional or generated includes may be missing
