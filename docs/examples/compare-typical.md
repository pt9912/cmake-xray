# Compare Report

- Baseline: `tests/e2e/testdata/m6/json-reports/analyze-file-api-loaded.json` (v6)
- Current: `tests/e2e/testdata/m6/json-reports/analyze-file-api-partial.json` (v6)
- Project identity: `/project`
- Project identity source: `cmake_file_api_source_root`

## Summary

| Group | Added | Removed | Changed |
| --- | ---: | ---: | ---: |
| Translation units | 2 | 10 | 0 |
| Include hotspots | 0 | 0 | 0 |
| Target nodes | 0 | 0 | 0 |
| Target edges | 0 | 0 | 0 |
| Target hubs | 0 | 0 | 0 |

### Translation Units (added)

- `src/app.cpp` [`build`]
- `src/lib.cpp` [`build`]

### Translation Units (removed)

- `src/hub.cpp` [`build`]
- `src/src00.cpp` [`build`]
- `src/src01.cpp` [`build`]
- `src/src02.cpp` [`build`]
- `src/src03.cpp` [`build`]
- `src/src04.cpp` [`build`]
- `src/src05.cpp` [`build`]
- `src/src06.cpp` [`build`]
- `src/src07.cpp` [`build`]
- `src/src08.cpp` [`build`]

## Diagnostics

- data_availability_drift: `target_graph`
  - baseline=`loaded`, current=`partial`
- data_availability_drift: `target_hubs`
  - baseline=`loaded`, current=`partial`
