# Project Analysis Report

- Report type: analyze
- Compile database: tests/e2e/testdata/m3/report\_project/compile\_commands.json
- Translation units: 3
- Translation unit ranking entries: 2 of 3
- Include hotspot entries: 2 of 2
- Top limit: 2
- Include analysis heuristic: yes

## Translation Unit Ranking
1. src/app/main.cpp [directory: build/app]
    Metrics: arg_count=8, include_path_count=2, define_count=1
    Diagnostics:
    - warning: could not resolve include "generated/version.h" from src/app/main.cpp
2. src/tools/tool.cpp [directory: build/tools]
    Metrics: arg_count=7, include_path_count=1, define_count=1

## Include Hotspots
1. Header: include/common/config.h
    Affected translation units: 3
    Translation units:
    - src/app/main.cpp [directory: build/app]
    - src/lib/core.cpp [directory: build/lib]
    - src/tools/tool.cpp [directory: build/tools]
2. Header: include/common/shared.h
    Affected translation units: 3
    Translation units:
    - src/app/main.cpp [directory: build/app]
    - src/lib/core.cpp [directory: build/lib]
    - src/tools/tool.cpp [directory: build/tools]

## Diagnostics
- note: include-based results are heuristic; conditional or generated includes may be missing
