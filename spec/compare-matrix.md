# Compare Compatibility Matrix

See [`report-compare.md`](./report-compare.md) for the Compare output
contract and [`report-json.md`](./report-json.md) for the Analyze input
contract.

M6 AP 1.6 supports only Analyze JSON `format_version=6` inputs for
`cmake-xray compare`. Version 6 is the first Analyze contract that carries
`inputs.project_identity` and `inputs.project_identity_source`.

| baseline_format_version | current_format_version | Status | Notes |
|---|---|---|---|
| 6 | 6 | OK | Full M6 compare over summary, TU ranking, include hotspots, target graph, target hubs, configuration, and data availability. |
| 5 | 5 | rejected | v5 has no project identity fields. |
| 5 | 6 | rejected | No cross-version migration in M6. |
| 6 | 5 | rejected | No cross-version migration in M6. |
| 1, 2, 3, 4 | any | rejected | Older contracts are not compare inputs in M6. |
| any | 1, 2, 3, 4 | rejected | Older contracts are not compare inputs in M6. |
| 7+ | any | rejected | Future contracts require an explicit matrix update. |
| any | 7+ | rejected | Future contracts require an explicit matrix update. |

For `(6, 6)`, all fields consumed by Compare are required by the Analyze
input contract. M6 has no renamed fields, removed fields, or default mapping
for missing fields. Impact reports are not comparable and are rejected before
field mapping.
