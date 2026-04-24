# Project Analysis Report

- Report type: analyze
- Compile database: tests/e2e/testdata/m4/partial\_targets/compile\_commands.json
- Translation units: 1
- Translation unit ranking entries: 1 of 1
- Include hotspot entries: 0 of 0
- Top limit: 10
- Include analysis heuristic: yes
- Observation source: exact
- Target metadata: partial
- Translation units with target mapping: 1 of 1

## Translation Unit Ranking
1. /project/src/main.cpp [directory: /project/build] [targets: app]
    Metrics: arg_count=5, include_path_count=1, define_count=0
    Diagnostics:
    - warning: cannot read source file for include analysis: /project/src/main.cpp

## Include Hotspots
No include hotspots found.

## Diagnostics
- note: 1 of 2 targets have no compilable sources and are not included in the analysis
- note: include-based results are heuristic; conditional or generated includes may be missing
