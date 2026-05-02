# JSON Report Contract

## Zweck

Dieses Dokument ist der verbindliche Vertrag fuer den maschinenlesbaren JSON-Report von `cmake-xray`. Der Vertrag wird durch `spec/report-json.schema.json` formal abgesichert, durch Goldens unter `tests/e2e/testdata/m5/json-reports/` byte-stabil festgehalten und durch CTest-Gates in jeder Build-Matrix gegen die C++-Konstante `xray::hexagon::model::kReportFormatVersion` abgeglichen.

Tooling, Automatisierung und Folgewerkzeuge duerfen sich auf diesen Vertrag stuetzen. Aenderungen am Vertrag erfordern eine Erhoehung von `format_version` und einen synchronen Update von Schema, Goldens und diesem Dokument.

## Geltungsbereich

- `cmake-xray analyze --format json`
- `cmake-xray impact --format json`
- `cmake-xray analyze|impact --format json --output <path>`

Andere Formate (`console`, `markdown`, `html`, `dot`) sind nicht Teil dieses Vertrags.

## Allgemeine Regeln

- JSON-Ausgaben sind gueltiges UTF-8 und syntaktisch gueltiges JSON.
- Erfolgreiche Ausgaben enthalten keinen unstrukturierten Zusatztext auf stdout.
- Bei `--output <path>` schreibt der Adapter ausschliesslich in die Zieldatei, ueber den gemeinsamen Atomic-Writer aus AP 1.1; stdout und stderr bleiben leer.
- Reportinhalt wird bei `--output` nicht zusaetzlich auf stdout dupliziert.
- Pfade werden als Anzeige-Strings ausgegeben, abgeleitet aus `ReportInputs` und der Display-Pfadlogik aus AP 1.1.
- Kanonische Normalisierungsschluessel (`source_path_key`, `unique_key`, `changed_file_key`) werden nicht serialisiert; sie sind interner Adapter-/Service-Zustand.
- Numerische Metriken bleiben numerisch und werden nicht als lokalisierte Strings gerendert.
- Boolesche Felder verwenden `true`/`false`, niemals `0`/`1` oder `"yes"`/`"no"`.
- Die Reihenfolge der Objektfelder bleibt im Adapter stabil, auch wenn JSON semantisch keine Feldreihenfolge garantiert. Tooling darf sich auf die Reihenfolge nicht verlassen, aber Goldens sind byte-stabil.
- Felder mit leerer Menge werden als leere Arrays ausgegeben, sofern sie Teil des Formatvertrags sind.
- Optionale Pfadfelder erscheinen als JSON-`null` bei `not_provided`, niemals als leerer String und niemals als weggelassenes Feld.
- Der JSON-Report enthaelt keine HTML-, DOT- oder Markdown-spezifischen Metadaten wie `graph_*`, `context_*` oder menschenlesbare Kuerzungshinweise.
- Der JSON-Report enthaelt kein `application`-Feld; Tooling identifiziert das Format ueber `format` und `format_version`.

## Top-Level Felder

Jeder JSON-Report enthaelt diese Pflichtfelder in genau dieser Reihenfolge:

| Feld | Typ | Pflicht | Beschreibung |
| --- | --- | --- | --- |
| `format` | string | ja | Maschinenlesbarer Dokumenttyp. `cmake-xray.analysis` oder `cmake-xray.impact`. |
| `format_version` | integer | ja | Aktuell `4` (M6 AP 1.4 A.3). Erhoeht sich bei vertragsbrechenden Aenderungen. Die strukturellen v4-Erweiterungen (`include_filter`, `origin`/`depth_kind` an Hotspots) folgen mit dem JSON-Adapter-Rollout in M6 AP 1.4 A.4. |
| `report_type` | string | ja | `analyze` oder `impact`. Kurzer Workflow-Identifier; nicht der CLI-Wert `--format json`. |
| `inputs` | object | ja | Eingabeprovenienz aus `ReportInputs`. Schema je Reporttyp unten. |
| `summary` | object | ja | Aggregierte Kennzahlen je Reporttyp. |
| `diagnostics` | array | ja | Reportweite Diagnostics. Leer als `[]`. |

Reporttyp-spezifische Felder folgen nach `summary` und vor `diagnostics`. Die genaue Feldreihenfolge fuer Analyze und Impact ist in den jeweiligen Abschnitten dokumentiert.

## Diagnostics

Diagnostic-Eintraege haben dieses Schema:

| Feld | Typ | Pflicht | Werte |
| --- | --- | --- | --- |
| `severity` | string | ja | `note` oder `warning`. |
| `message` | string | ja | Reine Textbotschaft. |

Sortierung: Diagnostics behalten die Reihenfolge aus dem Modell (`AnalysisResult::diagnostics` bzw. `ImpactResult::diagnostics`). Innerhalb von Items behalten Item-Diagnostics die Reihenfolge aus dem jeweiligen Item-Modell.

## Translation-Unit-Referenzen

Translation-Unit-Referenzen erscheinen in mehreren Listen (Ranking-Items, Hotspot-Items, Impact-Listen) und haben dieses einheitliche Schema:

| Feld | Typ | Pflicht | Beschreibung |
| --- | --- | --- | --- |
| `source_path` | string | ja | Anzeige-Pfad der Translation-Unit-Quelle. |
| `directory` | string | ja | Anzeige-Pfad des Compile-Verzeichnisses. |

Sortierung: nach `source_path` aufsteigend; Tie-Breaker `directory` aufsteigend.

## Targets

Target-Eintraege haben dieses Schema:

| Feld | Typ | Pflicht | Beschreibung |
| --- | --- | --- | --- |
| `display_name` | string | ja | Anzeigename des Targets. |
| `type` | string | ja | Target-Typ aus dem CMake-File-API-Codemodel. |

Sortierung von Target-Listen: nach `display_name` aufsteigend; Tie-Breaker `type` aufsteigend.

## `inputs` (gemeinsamer Anteil)

| Feld | Typ | Pflicht | Werte / Nullability |
| --- | --- | --- | --- |
| `compile_database_path` | string \| null | ja | Anzeige-Pfad der Compile Database; `null` falls nicht angegeben. |
| `compile_database_source` | string \| null | ja | `cli` falls die Compile Database per CLI uebergeben wurde, sonst `null`. |
| `cmake_file_api_path` | string \| null | ja | Anzeige-Pfad des CMake-File-API-Eingabewerts; `null` falls nicht angegeben. |
| `cmake_file_api_resolved_path` | string \| null | ja | Anzeige-Pfad des vom Adapter aufgeloesten Reply-Verzeichnisses; `null`, wenn der Reply-Pfad nicht stabil aufgeloest werden konnte. |
| `cmake_file_api_source` | string \| null | ja | `cli` falls per CLI uebergeben, sonst `null`. |

Regel: jedes der oben gelisteten Felder ist immer im Output, niemals weggelassen. Fehlende Werte sind JSON-`null`.

## Analyze JSON

Format-Identifier und Reporttyp:

- `format`: `cmake-xray.analysis`
- `report_type`: `analyze`

### Feldreihenfolge im Analyze-JSON

1. `format`
2. `format_version`
3. `report_type`
4. `inputs`
5. `summary`
6. `translation_unit_ranking`
7. `include_hotspots`
8. `target_graph_status`
9. `target_graph`
10. `target_hubs`
11. `diagnostics`

### `inputs` (analyze)

Enthaelt die fuenf Felder aus dem gemeinsamen `inputs`-Anteil. `changed_file` und `changed_file_source` sind nicht Teil des Analyze-`inputs`-Vertrags.

### `summary` (analyze)

| Feld | Typ | Pflicht | Werte |
| --- | --- | --- | --- |
| `translation_unit_count` | integer | ja | Anzahl Translation Units aus der Compile Database. |
| `ranking_total_count` | integer | ja | Anzahl rangierter Translation Units im Modell. Identisch mit `translation_unit_count` fuer den derzeitigen Ranking-Pfad, aber explizit ausgewiesen, damit kuenftige Filter erkennbar bleiben. |
| `hotspot_total_count` | integer | ja | Anzahl Hotspots im Modell vor `--top`-Begrenzung. |
| `top_limit` | integer | ja | Effektiver `--top`-Wert, wie er Ranking-, Hotspot- und `affected_translation_units`-Listen begrenzt. |
| `include_analysis_heuristic` | boolean | ja | `true`, wenn Include-Aufloesung heuristisch lief; sonst `false`. |
| `observation_source` | string | ja | `exact` oder `derived`. |
| `target_metadata` | string | ja | `not_loaded`, `loaded` oder `partial`. |

### `translation_unit_ranking`

Container fuer das Ranking mit Limit-Metadaten:

| Feld | Typ | Pflicht | Beschreibung |
| --- | --- | --- | --- |
| `limit` | integer | ja | Effektiver `--top`-Wert. Identisch mit `summary.top_limit`. |
| `total_count` | integer | ja | Anzahl rangierter Translation Units im Modell. |
| `returned_count` | integer | ja | Anzahl Items in `items`. `min(limit, total_count)`. |
| `truncated` | boolean | ja | `true` wenn `returned_count < total_count`. |
| `items` | array | ja | Ranking-Items, deterministisch sortiert. |

Item-Schema:

| Feld | Typ | Pflicht | Beschreibung |
| --- | --- | --- | --- |
| `rank` | integer | ja | 1-basierter Rang aus dem Modell. |
| `reference` | object | ja | Translation-Unit-Referenz mit `source_path` und `directory`. |
| `metrics` | object | ja | `arg_count`, `include_path_count`, `define_count`, `score` (alle integer). |
| `targets` | array | ja | Target-Liste; leer als `[]`. |
| `diagnostics` | array | ja | Item-Diagnostics; leer als `[]`. |

`metrics.score = arg_count + include_path_count + define_count`.

Sortierung der `items`: nach `rank` aufsteigend; Tie-Breaker `reference.source_path` aufsteigend, dann `reference.directory` aufsteigend.

### `include_hotspots`

Container fuer Hotspots mit Limit-Metadaten:

| Feld | Typ | Pflicht | Beschreibung |
| --- | --- | --- | --- |
| `limit` | integer | ja | Effektiver `--top`-Wert. Identisch mit `summary.top_limit`. |
| `total_count` | integer | ja | Anzahl Hotspots im Modell. |
| `returned_count` | integer | ja | Anzahl Items in `items`. `min(limit, total_count)`. |
| `truncated` | boolean | ja | `true` wenn `returned_count < total_count`. |
| `items` | array | ja | Hotspot-Items, deterministisch sortiert. |

Item-Schema:

| Feld | Typ | Pflicht | Beschreibung |
| --- | --- | --- | --- |
| `header_path` | string | ja | Anzeige-Pfad des Headers. |
| `affected_total_count` | integer | ja | Anzahl betroffener Translation Units im Modell vor Begrenzung. |
| `affected_returned_count` | integer | ja | Anzahl Items in `affected_translation_units`. `min(limit, affected_total_count)`. |
| `affected_truncated` | boolean | ja | `true` wenn `affected_returned_count < affected_total_count`. |
| `affected_translation_units` | array | ja | Begrenzte Liste betroffener Translation Units. |
| `diagnostics` | array | ja | Item-Diagnostics; leer als `[]`. |

`affected_translation_units`-Item-Schema:

| Feld | Typ | Pflicht |
| --- | --- | --- |
| `reference` | object | ja |
| `targets` | array | ja |

Limit-Quelle: `affected_translation_units` werden mit demselben effektiven `--top`-Wert pro Hotspot begrenzt; AP 1.2 fuehrt keine zusaetzliche CLI-Option ein.

Sortierung der Hotspot-`items`: nach `affected_total_count` absteigend; Tie-Breaker `header_path` aufsteigend.
Sortierung der `affected_translation_units` pro Hotspot: nach `reference.source_path` aufsteigend; Tie-Breaker `reference.directory` aufsteigend.

## Impact JSON

Format-Identifier und Reporttyp:

- `format`: `cmake-xray.impact`
- `report_type`: `impact`

### Feldreihenfolge im Impact-JSON

1. `format`
2. `format_version`
3. `report_type`
4. `inputs`
5. `summary`
6. `directly_affected_translation_units`
7. `heuristically_affected_translation_units`
8. `directly_affected_targets`
9. `heuristically_affected_targets`
10. `target_graph_status`
11. `target_graph`
12. `prioritized_affected_targets`
13. `impact_target_depth_requested`
14. `impact_target_depth_effective`
15. `diagnostics`

### `inputs` (impact)

Enthaelt zusaetzlich zum gemeinsamen `inputs`-Anteil:

| Feld | Typ | Pflicht | Werte / Nullability |
| --- | --- | --- | --- |
| `changed_file` | string | ja | Anzeige-Pfad der geaenderten Datei aus `ReportInputs.changed_file`. Bei `impact` immer ein String, weil `--changed-file` Pflicht ist. |
| `changed_file_source` | string | ja | `compile_database_directory`, `file_api_source_root` oder `cli_absolute`. |

Wichtig:

- Der Modellwert `unresolved_file_api_source_root` (aus AP 1.1) ist in JSON v1 kein erlaubter `changed_file_source`-Wert. Faelle, in denen die File-API-Source-Root vor stabiler Reply-Aufloesung unbekannt bleibt, werden als Textfehler ohne JSON-Report behandelt.
- AP 1.2 erzeugt kein JSON-Fehlerobjekt fuer Parser-, Eingabe-, Render- oder Schreibfehler. Diese Fehler bleiben Textfehler auf `stderr`.

### `summary` (impact)

| Feld | Typ | Pflicht | Werte |
| --- | --- | --- | --- |
| `affected_translation_unit_count` | integer | ja | Anzahl betroffener Translation Units (direkt + heuristisch). |
| `direct_translation_unit_count` | integer | ja | Anzahl direkt betroffener Translation Units. |
| `heuristic_translation_unit_count` | integer | ja | Anzahl heuristisch betroffener Translation Units. |
| `classification` | string | ja | `direct`, `heuristic` oder `uncertain`. `uncertain`, wenn das Ergebnis heuristisch ist und keine Translation Unit betroffen wurde. |
| `observation_source` | string | ja | `exact` oder `derived`. |
| `target_metadata` | string | ja | `not_loaded`, `loaded` oder `partial`. |
| `affected_target_count` | integer | ja | Anzahl betroffener Targets (direkt + heuristisch); `0`, wenn `target_metadata = not_loaded`. |
| `direct_target_count` | integer | ja | Anzahl direkt betroffener Targets; `0`, wenn `target_metadata = not_loaded`. |
| `heuristic_target_count` | integer | ja | Anzahl heuristisch betroffener Targets; `0`, wenn `target_metadata = not_loaded`. |

### `directly_affected_translation_units` und `heuristically_affected_translation_units`

Beide Listen verwenden dasselbe Item-Schema:

| Feld | Typ | Pflicht |
| --- | --- | --- |
| `reference` | object | ja |
| `targets` | array | ja |

`reference` ist eine Translation-Unit-Referenz; `targets` ist eine Target-Liste, leer als `[]`.

Sortierung: nach `reference.source_path` aufsteigend; Tie-Breaker `reference.directory` aufsteigend.

Eine leere Liste wird als `[]` ausgegeben.

### `directly_affected_targets` und `heuristically_affected_targets`

Beide Listen verwenden dasselbe Item-Schema:

| Feld | Typ | Pflicht |
| --- | --- | --- |
| `display_name` | string | ja |
| `type` | string | ja |

Sortierung: nach `display_name` aufsteigend; Tie-Breaker `type` aufsteigend.

Eine leere Liste wird als `[]` ausgegeben.

### Limit-/Truncation-Vertrag fuer Impact

Impact-Listen werden in M5 nicht ueber `--top` begrenzt. AP 1.2 fuehrt keine `limit`-/`truncated`-Felder fuer Impact-Listen ein, weil der CLI- und Port-Vertrag keine Impact-Begrenzung kennt. Alle betroffenen Translation Units und Targets aus `ImpactResult` werden ausgegeben.

## Fehlervertrag

- Parser-/Verwendungsfehler (`impact --format json` ohne `--changed-file`, unbekannte Optionen, fehlende Pflichtoptionen): Text auf `stderr`, Exit-Code ungleich `0`, kein JSON-Fehlerobjekt.
- Nicht wiederherstellbare Eingabefehler vor Reporterzeugung (nicht vorhandene Eingabedateien, ungueltige `compile_commands.json`, ungueltige File-API-Reply-Daten): Text auf `stderr`, Exit-Code ungleich `0`, kein JSON-Fehlerobjekt.
- Render-, Schreib- und Output-Fehler: Text auf `stderr`, Exit-Code ungleich `0`, kein partielles JSON auf stdout, bestehende Zieldatei bei `--output` unveraendert.
- Service-Ergebnisse mit Diagnostics oder Teildaten erscheinen als regulaere JSON-Reports unter `diagnostics`. Erfolgreiche Reporterzeugung bleibt Exit-Code `0`, sofern die bestehende CLI-Semantik fuer diese Serviceergebnisse nicht bereits Fehlercodes vorsieht.

## Versionierung

- `format_version` startet bei `1` und entspricht `xray::hexagon::model::kReportFormatVersion`.
- `format_version` wird erhoeht bei:
  - Entfernen eines Pflichtfelds.
  - Aendern eines Feldtyps.
  - Entfernen eines Enum-Werts.
  - Aenderung der dokumentierten Sortier- oder Tie-Breaker-Regeln.
  - Aenderung der Bedeutung eines Felds.
- Hinzufuegen eines neuen Pflichtfelds erhoeht ebenfalls `format_version`, weil bestehende Konsumenten das Feld noch nicht erwarten.
- Hinzufuegen eines neuen optionalen Felds, das standardmaessig `null` ist und in geschlossenem Schema explizit erlaubt wird, erhoeht `format_version` ebenfalls, weil das Schema in M5 `additionalProperties: false` setzt.
- Eine spaetere Migration darf neue Felder oder JSON-Fehlerobjekte einfuehren, muss dann aber `format_version` erhoehen und Schema, Goldens sowie dieses Dokument synchron aktualisieren.

Die Konstante lebt in `src/hexagon/model/report_format_version.h`. Schema, Adapter, Tests und Goldens beziehen sich auf genau diese Konstante; ein parallel definierter zweiter Versionswert ist verboten.

## Target-Graph (M6 AP 1.2, `format_version=2`)

Beide Reporttypen tragen die direkte Target-zu-Target-Abhaengigkeitssicht aus
M6 AP 1.1 als Pflichtfelder:

| Feld | Typ | Pflicht | Werte |
| --- | --- | --- | --- |
| `target_graph_status` | string | ja | `not_loaded`, `loaded`, `partial`. |
| `target_graph` | object | ja | Container mit `nodes` und `edges`. Bei `not_loaded` beide leer. |
| `target_graph.nodes` | array | ja | Knoten, sortiert nach `(unique_key, display_name, type)`. |
| `target_graph.edges` | array | ja | Kanten, sortiert nach `(from_unique_key, to_unique_key, kind, from_display_name, to_display_name)`. |

`TargetNode`-Schema:

| Feld | Typ | Pflicht |
| --- | --- | --- |
| `display_name` | string | ja |
| `type` | string | ja |
| `unique_key` | string | ja |

`TargetEdge`-Schema:

| Feld | Typ | Pflicht |
| --- | --- | --- |
| `from_display_name` | string | ja |
| `from_unique_key` | string | ja |
| `to_display_name` | string | ja |
| `to_unique_key` | string | ja |
| `kind` | string (`direct`) | ja |
| `resolution` | string (`resolved`, `external`) | ja |

`<external>::*`-Targets erscheinen NICHT in `target_graph.nodes`, weil sie
keine echten File-API-Targets sind. Sie erscheinen aber als
`to_unique_key="<external>::<raw_id>"` in `target_graph.edges` mit
`resolution="external"`.

`target_graph_status="not_loaded"` impliziert in beiden Reporttypen einen
leeren `target_graph` (`nodes=[]`, `edges=[]`); die Felder bleiben trotzdem
Pflicht.

### `target_hubs` (nur Analyze)

Im Analyze-JSON ergaenzt ein Pflichtcontainer `target_hubs` die Hub-Sicht:

| Feld | Typ | Pflicht | Beschreibung |
| --- | --- | --- | --- |
| `target_hubs.inbound` | array | ja | `TargetNode`-Objekte mit `in_count >= in_threshold`, sortiert nach `(unique_key, display_name, type)`. |
| `target_hubs.outbound` | array | ja | `TargetNode`-Objekte mit `out_count >= out_threshold`, sortiert nach `(unique_key, display_name, type)`. |
| `target_hubs.thresholds.in_threshold` | integer | ja | Wirksamer Schwellenwert; in AP 1.2 hardcoded `10`. |
| `target_hubs.thresholds.out_threshold` | integer | ja | Wirksamer Schwellenwert; in AP 1.2 hardcoded `10`. |

`target_hubs` ist im Impact-JSON bewusst NICHT enthalten und wird vom Schema
ueber die Kombination aus fehlender Property-Deklaration und
`additionalProperties: false` hart abgelehnt. AP 1.3 hat sich gegen
Hubs im Impact-Kontext entschieden; der Impact-Kontext nutzt
stattdessen `prioritized_affected_targets` (siehe naechste Section).

Im Analyze-JSON impliziert `target_graph_status="not_loaded"` zusaetzlich
leere `target_hubs.inbound` und `target_hubs.outbound` sowie das
`thresholds`-Objekt mit den hardcoded `10`/`10`-Defaults.

## Prioritised Impact (M6 AP 1.3, `format_version=3`)

Im Impact-JSON ergaenzen drei neue Pflichtfelder die Reverse-Target-Graph-
Priorisierung:

| Feld | Typ | Pflicht | Werte |
| --- | --- | --- | --- |
| `prioritized_affected_targets` | array | ja | `PrioritizedImpactedTarget`-Eintraege, sortiert nach `(priority_class, graph_distance, evidence_strength, unique_key)`. |
| `impact_target_depth_requested` | integer | ja | Validierter angeforderter Wert in `[0, 32]`; entspricht `AnalyzeImpactRequest::impact_target_depth.value_or(2)`. |
| `impact_target_depth_effective` | integer | ja | Tatsaechlich erreichte Reverse-BFS-Tiefe. `0 <= effective <= requested`; bei `target_graph_status="not_loaded"` immer `0`. |

`PrioritizedImpactedTarget`-Schema:

| Feld | Typ | Pflicht | Werte |
| --- | --- | --- | --- |
| `target` | object | ja | `TargetNode` aus AP 1.2. |
| `priority_class` | string | ja | `direct`, `direct_dependent`, `transitive_dependent`. |
| `graph_distance` | integer | ja | `0` bis `32`; `<= impact_target_depth_requested`. |
| `evidence_strength` | string | ja | `direct`, `heuristic`, `uncertain`. |

Korrespondenz-Regel `priority_class` ↔ `graph_distance`: `direct` gilt
fuer `graph_distance=0`, `direct_dependent` fuer `graph_distance=1`,
`transitive_dependent` fuer `graph_distance>=2`. Ein Eintrag mit
nicht-passender Korrespondenz ist ein Service-Defekt.

`evidence_strength`-Regel: Owner direkt betroffener TUs erben `direct`,
Owner heuristisch betroffener TUs erben `heuristic`, Header-Only- oder
Build-Metadaten-only-Targets ohne TU-Anker erhalten `uncertain`.
Reverse-BFS-Hops uebernehmen die Evidenz vom Frontier-Vorgaenger; bei
gleicher minimaler Distanz gewinnt die staerkste Evidenz
(`direct > heuristic > uncertain`).

Externe `<external>::*`-Targets erscheinen NICHT in
`prioritized_affected_targets`, weil sie keine Owner-Targets sein
koennen; AP 1.3 dropt entsprechende Reverse-BFS-Kandidaten
defense-in-depth.

`prioritized_affected_targets` ist im Analyze-JSON nicht enthalten und
wird vom Schema dort hart abgelehnt; die analoge Hub-Sicht aus
`target_hubs` bleibt analyze-only.

Diagnostic-Korrespondenz:

| Bedingung | Diagnostic-Schwere | Nachricht |
| --- | --- | --- |
| `target_graph_status=not_loaded` mit Seeds | `note` | `target graph not loaded; impact prioritisation degrades to impact seed targets only` |
| `target_graph_status=partial` | `warning` | `target graph partially loaded; impact prioritisation uses available edges only` |
| BFS endet vorzeitig vor Budget mit Seeds | `note` | `reverse target graph traversal stopped at depth N (no further reachable targets)` |
| BFS erreicht Tiefenbudget mit nicht-leerer Frontier | `warning` | `reverse target graph traversal hit depth limit N within a cycle; some transitively dependent targets may be missing` |

## Schema-Validierung

- `spec/report-json.schema.json` ist die formale Schema-Definition (JSON Schema Draft 2020-12).
- Jedes Vertragsobjekt setzt `additionalProperties: false`. Unbekannte Felder werden hart abgelehnt.
- Schema und C++-Konstante `kReportFormatVersion` werden in einem CTest-Gate gegeneinander geprueft. Ein abweichender `const`-Wert im Schema laesst den Gate-Test fehlschlagen.
- Goldens unter `tests/e2e/testdata/m5/json-reports/` und `tests/e2e/testdata/m6/json-reports/` werden ueber `tests/validate_json_schema.py` gegen das Schema validiert. Der Validator gleicht Verzeichnisinhalt und Manifest beidseitig ab. Beide Verzeichnisse enthalten ausschliesslich `format_version=4`-Goldens (AP M6-1.4 A.3 inhaltlich migriert); die `m5/`-/`m6/`-Trennung ist fachlich (Datensatz-Szenarien), nicht versionell.
- Schema-Asymmetrie analyze vs. impact wird durch zwei Negativtests gepinnt: `report_json_schema_rejects_impact_with_target_hubs` und `report_json_schema_rejects_analyze_without_target_hubs`. Beide Tests laufen mit `WILL_FAIL TRUE` und scheitern absichtlich am Schema, damit ein Adapter-Bug, der die Asymmetrie verletzt, sofort sichtbar wird.

## Manifest

`tests/e2e/testdata/m5/json-reports/manifest.txt` und `tests/e2e/testdata/m6/json-reports/manifest.txt` listen alle JSON-Report-Goldens des jeweiligen Datensatz-Szenarien-Verzeichnisses, eine pro Zeile. Leerzeilen und mit `#` beginnende Kommentare sind zulaessig. Der Schema-Validator schlaegt fehl, wenn:

- ein Manifesteintrag nicht im Verzeichnis existiert,
- ein Report-Golden im Verzeichnis nicht im Manifest steht,
- ein Manifesteintrag das Schema nicht erfuellt.

In Tranche A ist das Manifest leer; AP 1.2 Tranche C fuegt die echten Goldens hinzu.
