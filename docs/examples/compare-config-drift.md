# Compare Report

- Baseline: `tests/e2e/testdata/m6/json-reports/analyze-include-origin-mix.json` (v6)
- Current: `tests/e2e/testdata/m6/json-reports/analyze-include-origin-mix-scope-project.json` (v6)
- Project identity: `compile-db:b63f61b5bb5f474ee42e7c094c64b1c7aba5635d7c0099a03273ac81276b5e60`
- Project identity source: `fallback_compile_database_fingerprint`

## Summary

| Group | Added | Removed | Changed |
| --- | ---: | ---: | ---: |
| Translation units | 0 | 0 | 0 |
| Include hotspots | 0 | 2 | 0 |
| Target nodes | 0 | 0 | 0 |
| Target edges | 0 | 0 | 0 |
| Target hubs | 0 | 0 | 0 |

### Include Hotspots (removed)

- `neighbor.h` [`unknown`, `direct`]
- `system/vector` [`external`, `direct`]

## Diagnostics

- configuration_drift: `include_filter.include_scope`
  - baseline=`all`, current=`project`
  - severity=`warning`, ci_policy_hint=`review_required`
