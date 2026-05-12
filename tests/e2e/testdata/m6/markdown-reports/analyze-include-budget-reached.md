# Project Analysis Report

- Report type: analyze
- Compile database: tests/e2e/testdata/m6/include\_budget\_reached/compile\_commands.json
- Translation units: 1
- Translation unit ranking entries: 1 of 1
- Include hotspot entries: 0 of 0
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
1. src/main.cpp [directory: build]
    Metrics: arg_count=6, include_path_count=1, define_count=0

## Include Hotspots

Filter: `scope=all`, `depth=all`. Excluded: `0` unknown, `0` mixed.

Note: include analysis stopped at 10000 nodes (budget reached).

No include hotspots found.

## Diagnostics
- note: include analysis stopped at 10000 nodes (budget reached); some indirect includes may be missing
- note: include-based results are heuristic; conditional or generated includes may be missing
