# Project Analysis Report

- Report type: analyze
- Compile database: tests/e2e/testdata/m2/basic\_project/compile\_commands.json
- Translation units: 3
- Translation unit ranking entries: 3 of 3
- Include hotspot entries: 2 of 2
- Top limit: 10
- Include analysis heuristic: yes

## Translation Unit Ranking
1. src/app/main.cpp [directory: build/app]
    Metrics: arg_count=8, include_path_count=2, define_count=1
    Diagnostics:
    - warning: could not resolve include "generated/version.h" from src/app/main.cpp
2. src/tools/tool.cpp [directory: build/tools]
    Metrics: arg_count=7, include_path_count=1, define_count=1
3. src/lib/core.cpp [directory: build/lib]
    Metrics: arg_count=6, include_path_count=1, define_count=0

## Include Hotspots

Filter: `scope=all`, `depth=all`. Excluded: `0` unknown, `0` mixed.

| Header | Origin | Depth | Affected TUs | Context |
|---|---|---|---|---|
| `include/common/config.h` | `project` | `direct` | 3 | src/app/main.cpp [directory: build/app] / src/lib/core.cpp [directory: build/lib] / src/tools/tool.cpp [directory: build/tools] |
| `include/common/shared.h` | `project` | `indirect` | 3 | src/app/main.cpp [directory: build/app] / src/lib/core.cpp [directory: build/lib] / src/tools/tool.cpp [directory: build/tools] |

## Diagnostics
- note: include-based results are heuristic; conditional or generated includes may be missing
