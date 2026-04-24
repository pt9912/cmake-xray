# Project Analysis Report

- Report type: analyze
- Compile database: tests/e2e/testdata/m4/file\_api\_only/build
- Translation units: 2
- Translation unit ranking entries: 2 of 2
- Include hotspot entries: 0 of 0
- Top limit: 10
- Include analysis heuristic: yes
- Observation source: derived
- Target metadata: loaded
- Translation units with target mapping: 2 of 2

## Translation Unit Ranking
1. src/main.cpp [directory: build] [targets: app]
    Metrics: arg_count=8, include_path_count=2, define_count=1
    Diagnostics:
    - warning: cannot read source file for include analysis: src/main.cpp
2. src/core.cpp [directory: build] [targets: core]
    Metrics: arg_count=5, include_path_count=1, define_count=0
    Diagnostics:
    - warning: cannot read source file for include analysis: src/core.cpp

## Include Hotspots
No include hotspots found.

## Diagnostics
- note: include-based results are heuristic; conditional or generated includes may be missing
