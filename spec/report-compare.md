# Compare Report Contract

## Zweck

Dieses Dokument ist der verbindliche Vertrag fuer `cmake-xray compare`.
Der maschinenlesbare JSON-Report wird durch
[`report-compare.schema.json`](./report-compare.schema.json) formal
validiert. Die erlaubten Analyze-Eingangsversionen stehen in
[`compare-matrix.md`](./compare-matrix.md). Textausgaben (`console`,
`markdown`) sind menschenlesbare Projektionen desselben Compare-Modells.

`compare` fuehrt in M6 keinen Schema-Migrationspfad aus. Es akzeptiert
ausschliesslich Analyze-JSON mit `format_version=6` auf beiden Seiten,
weil erst dieser Analyze-Vertrag `inputs.project_identity` und
`inputs.project_identity_source` garantiert.

## Geltungsbereich

- `cmake-xray compare --format json`
- `cmake-xray compare --format markdown`
- `cmake-xray compare --format console`
- `cmake-xray compare --format json|markdown --output <path>`

Analyze- und Impact-JSON bleiben im Vertrag
[`report-json.md`](./report-json.md) beschrieben. Impact-JSON ist kein
gueltiger Compare-Eingang und wird vor Reporterzeugung abgelehnt.

## JSON Top-Level

Jeder Compare-JSON-Report enthaelt diese Pflichtfelder in genau dieser
Reihenfolge:

| Feld | Typ | Pflicht | Beschreibung |
| --- | --- | --- | --- |
| `format` | string | ja | Konstante `cmake-xray.compare`. |
| `format_version` | integer | ja | Konstante `1` (`M6_COMPARE_FORMAT_VERSION`). |
| `report_type` | string | ja | Konstante `compare`. |
| `inputs` | object | ja | Eingangsdateien, deren Analyze-Formatversionen und Projektidentitaet. |
| `summary` | object | ja | Zaehler fuer alle Diff-Gruppen. |
| `diffs` | object | ja | Added/removed/changed-Listen je fachlicher Gruppe. |
| `diagnostics` | object | ja | Drift-Diagnostics fuer Konfiguration, Datenverfuegbarkeit und Projektidentitaet. |

Alle Objekte sind geschlossen (`additionalProperties: false`). Leere
Diff-Gruppen und Diagnostic-Listen werden als leere Arrays ausgegeben;
ein fehlender Projektidentitaetsdrift wird als JSON-`null`
serialisiert.

## `inputs`

| Feld | Typ | Pflicht | Beschreibung |
| --- | --- | --- | --- |
| `baseline_path` | string | ja | Anzeige-Pfad des Baseline-Reports. |
| `current_path` | string | ja | Anzeige-Pfad des Current-Reports. |
| `baseline_format_version` | integer | ja | Analyze-`format_version` der Baseline; in M6 `6`. |
| `current_format_version` | integer | ja | Analyze-`format_version` des Current-Reports; in M6 `6`. |
| `project_identity` | string \| null | ja | Gemeinsame Projektidentitaet oder `null`, wenn `--allow-project-identity-drift` den Fallback-Drift explizit erlaubt. |
| `project_identity_source` | string | ja | `cmake_file_api_source_root` oder `fallback_compile_database_fingerprint`. |

## `summary`

`summary` enthaelt fuer jede Diff-Gruppe drei Integer-Zaehler:

| Gruppe | Felder |
| --- | --- |
| Translation Units | `translation_units_added`, `translation_units_removed`, `translation_units_changed` |
| Include Hotspots | `include_hotspots_added`, `include_hotspots_removed`, `include_hotspots_changed` |
| Target Nodes | `target_nodes_added`, `target_nodes_removed`, `target_nodes_changed` |
| Target Edges | `target_edges_added`, `target_edges_removed`, `target_edges_changed` |
| Target Hubs | `target_hubs_added`, `target_hubs_removed`, `target_hubs_changed` |

Die Zaehler entsprechen exakt den Laengen der jeweiligen Arrays unter
`diffs`.

## `diffs`

`diffs` enthaelt die Gruppen `translation_units`, `include_hotspots`,
`target_nodes`, `target_edges` und `target_hubs`. Jede Gruppe enthaelt
die Arrays `added`, `removed` und `changed`.

| Diff-Typ | Identitaetsfelder | `changes` |
| --- | --- | --- |
| Translation Unit | `source_path`, `build_context_key` | optional bei `changed` |
| Include Hotspot | `header_path`, `include_origin`, `include_depth_kind` | optional bei `changed` |
| Target Node | `unique_key` | optional bei `changed` |
| Target Edge | `from_unique_key`, `to_unique_key`, `kind` | optional bei `changed` |
| Target Hub | `unique_key`, `direction` (`inbound` oder `outbound`) | nicht vorhanden |

Ein `changes`-Eintrag besteht aus `field`, `baseline_value` und
`current_value`. Werte sind skalare JSON-Werte (`string`, `integer`,
`boolean`, `null`) oder Arrays aus solchen Werten.

## `diagnostics`

| Feld | Typ | Beschreibung |
| --- | --- | --- |
| `configuration_drifts` | array | Konfigurationsunterschiede, die den Vergleich nicht abbrechen, aber Review erfordern. |
| `data_availability_drifts` | array | Unterschiedliche `target_graph`-/`target_hubs`-Verfuegbarkeit zwischen Baseline und Current. |
| `project_identity_drift` | object \| null | Nur gesetzt, wenn `--allow-project-identity-drift` einen Same-Mode-Fallback-Drift erlaubt. |

`configuration_drifts[]` enthaelt `field`, `baseline_value`,
`current_value`, `severity="warning"` und
`ci_policy_hint="review_required"`.

`data_availability_drifts[]` enthaelt `section` (`target_graph` oder
`target_hubs`) sowie `baseline_state` und `current_state` mit den
Werten `loaded`, `partial`, `not_loaded` oder `disabled`.

`project_identity_drift` enthaelt `baseline_project_identity`,
`current_project_identity`, `baseline_source_path_count`,
`current_source_path_count` und `shared_source_path_count`.

## Beispiele

Generator-gepinnte Beispiele liegen unter `docs/examples/`:

- `compare-typical.{txt,md,json}`
- `compare-config-drift.{txt,md,json}`
- `compare-project-identity-drift.{txt,md,json}`

Die Beispiele werden ueber `docs/examples/generation-spec.json` gegen
den aktuellen Generatorausgang validiert und durch
`doc_examples_validation` in CTest abgedeckt.
