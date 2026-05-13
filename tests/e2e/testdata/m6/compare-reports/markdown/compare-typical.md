# Compare Report

- Baseline: `baseline.json` (v6)
- Current: `current.json` (v6)
- Project identity: `compile-db:same`
- Project identity source: `fallback_compile_database_fingerprint`

## Summary

| Group | Added | Removed | Changed |
| --- | ---: | ---: | ---: |
| Translation units | 1 | 1 | 1 |
| Include hotspots | 0 | 0 | 1 |
| Target nodes | 0 | 0 | 1 |
| Target edges | 1 | 0 | 0 |
| Target hubs | 0 | 1 | 0 |

### Translation Units (added)

- `src/new.cpp` [`build`]

### Translation Units (removed)

- `src/old.cpp` [`build`]

### Translation Units (changed)

- `src/app.cpp` [`build`]

| Field | Baseline | Current |
| --- | --- | --- |
| `metrics.score` | `10` | `13` |
| `targets` | `app, core` | `app, ui` |

### Include Hotspots (changed)

- `include/config.h` [`project`, `direct`]

| Field | Baseline | Current |
| --- | --- | --- |
| `affected_total_count` | `2` | `3` |

### Target Nodes (changed)

- `app::EXECUTABLE`

| Field | Baseline | Current |
| --- | --- | --- |
| `display_name` | `app` | `app-renamed` |

### Target Edges (added)

- `app` -> `core` [`direct`]

### Target Hubs (removed)

- `legacy` [`inbound`]

## Diagnostics

- configuration_drift: `include_filter.include_scope`
  - baseline=`all`, current=`project`
  - severity=`warning`, ci_policy_hint=`review_required`
- data_availability_drift: `target_graph`
  - baseline=`loaded`, current=`partial`
- project_identity_drift
  - baseline=`compile-db:old`, current=`compile-db:new`
  - baseline_source_paths=2, current_source_paths=3, shared_source_paths=2
