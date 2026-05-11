# Project Analysis Report

- Report type: analyze
- Compile database: tests/e2e/testdata/m6/include\_origin\_mix/compile\_commands.json
- Translation units: 2
- Translation unit ranking entries: 2 of 2
- Include hotspot entries: 2 of 2
- Top limit: 10
- Include analysis heuristic: yes

## Translation Unit Ranking
1. src/a.cpp [directory: build/a]
    Metrics: arg_count=8, include_path_count=2, define_count=0
2. src/b.cpp [directory: build/b]
    Metrics: arg_count=8, include_path_count=2, define_count=0

## Include Hotspots

Filter: `scope=project`, `depth=all`. Excluded: `1` unknown, `0` mixed.

| Header | Origin | Depth | Affected TUs | Context |
|---|---|---|---|---|
| `include/config.h` | `project` | `mixed` | 2 | src/a.cpp [directory: build/a] / src/b.cpp [directory: build/b] |
| `include/wrapper.h` | `project` | `direct` | 2 | src/a.cpp [directory: build/a] / src/b.cpp [directory: build/b] |

## Diagnostics
- note: include-based results are heuristic; conditional or generated includes may be missing
