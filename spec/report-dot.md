# DOT Report Contract

> **Implementierungsstand:** AP M5-1.3 ist abgeschlossen. Tranche A (Vertrag, Helfer, Syntax-Gates), Tranche B (Adapter, Wiring, produktive CLI-Freischaltung von `--format dot`), Tranche C (E2E-Goldens, CLI-/Stream-/Fehler-Tests, Nutzerdoku) und die optionale Tranche D (Plattformpfad-, ASCII-Escape-, Multi-Format-Graphviz- und Budget-Boundary-Haertung) sind gemerged und durch die Docker-Gates (`test`, `coverage-check`, `quality-check`, `runtime`) abgesichert. `cmake-xray analyze --format dot` und `cmake-xray impact --format dot` sind produktiv nutzbar; die CLI lehnt `--format dot` nicht mehr als `recognized but not implemented` ab.

## Zweck

Dieses Dokument ist der verbindliche Vertrag fuer den Graphviz-DOT-Report von `cmake-xray`. Der Vertrag wird durch CTest-Gates gegen `dot -Tsvg` (Docker-Pfad) und `tests/validate_dot_reports.py` (native CI-Fallback) abgesichert, durch Goldens unter `tests/e2e/testdata/m5/dot-reports/` byte-stabil festgehalten und durch Adapter-Unit-Tests in `xray_tests` gegen die Vertragsregeln geprueft.

DOT-Ausgaben sind Graphviz-Quelltext, kein gerendertes Bild. Tooling-Konsumenten rendern die Ausgabe selbst, etwa via `dot -Tsvg` oder `dot -Tpng`.

## Geltungsbereich

- `cmake-xray analyze --format dot`
- `cmake-xray impact --format dot`
- `cmake-xray analyze|impact --format dot --output <path>`

Andere Formate (`console`, `markdown`, `json`, `html`) sind nicht Teil dieses Vertrags.

## Allgemeine Regeln

- DOT-Ausgaben sind gueltiges UTF-8 und syntaktisch gueltiger Graphviz-DOT-Quelltext.
- Der Top-Level-Graph ist ein gerichteter Graph (`digraph`).
- Analyze verwendet den Graphnamen `cmake_xray_analysis`.
- Impact verwendet den Graphnamen `cmake_xray_impact`.
- Der Graph ist deterministisch: gleiche Eingabedaten und gleicher wirksamer `--top`-Wert erzeugen byte-stabile DOT-Ausgabe.
- Datums-, Laufzeit-, Hostnamen- oder Build-Umgebungsinformationen erscheinen nicht.
- Kommentare sind erlaubt, aber nicht Teil des stabilen Vertrags. Golden-Tests duerfen nicht auf Kommentare angewiesen sein.
- Alle fachlich relevanten Informationen stehen in Graph-, Node- oder Edge-Attributen, nicht ausschliesslich in Kommentaren.
- DOT erzeugt keine JSON-, HTML- oder Markdown-spezifischen Metadaten.
- DOT erzeugt keine Target-zu-Target-Kanten. Diese gehoeren zu einer kuenftigen Target-Graph-Analyse.
- Erfolgreiche `--format dot`-Aufrufe ohne `--output` schreiben ausschliesslich DOT-Quelltext nach stdout; stderr bleibt leer.
- Erfolgreiche `--format dot --output <path>`-Aufrufe schreiben den Bericht atomar in die Datei und lassen stdout und stderr leer.

## Attributlexik und Escaping

- Integer werden als unquoted ASCII-Dezimalzahlen ohne Vorzeichen geschrieben.
- Booleans werden als unquoted lowercase `true` oder `false` geschrieben.
- Strings werden als quoted DOT-Strings geschrieben.
- Stringwerte escapen mindestens Backslash (`\\`), Anfuehrungszeichen (`\"`), Newline (`\n`), Carriage Return (`\r`) und Tab (`\t`) deterministisch.
- Unbekannte Steuerzeichen werden deterministisch durch `\xHH`-Sequenzen ersetzt; rohe Steuerzeichen sind in Goldens nicht zulaessig.
- UTF-8-Bytes ueber `0x7F` werden als rohe Bytes ausgegeben; Graphviz akzeptiert UTF-8 in quoted Strings.

## Labels

- Labels werden deterministisch aus dem jeweiligen Anzeige-Wert gebildet und sind nie die einzige Quelle fuer fachliche Identitaet.
- Fuer Pfadlabels wird zuerst die letzte Pfadkomponente verwendet; Trenner sind `/` und `\`. Wenn keine letzte Komponente bestimmbar ist, wird der vollstaendige Anzeige-Pfad verwendet.
- Fuer Target-Labels wird `TargetInfo::display_name` verwendet.
- Labels mit hoechstens 48 Zeichen werden unveraendert ausgegeben.
- Labels mit mehr als 48 Zeichen werden per Middle-Truncation auf exakt 48 Zeichen gekuerzt: die ersten 22 Zeichen, dann `...`, dann die letzten 23 Zeichen.
- Gekuerzte Labels verwenden ASCII-`...`, keine Unicode-Ellipse.
- Vollstaendige Pfade, Namen, Directories, Typen und Schluessel bleiben in den jeweiligen `path`-, `name`-, `directory`-, `type`- und `unique_key`-Attributen unverkuerzt erhalten.

## DOT-IDs

- Node-IDs sind stabile ASCII-IDs und keine rohen Pfade.
- Der Adapter bildet IDs aus Knotentyp und stabiler Position nach der dokumentierten Sortier- und Budgetlogik, zum Beispiel `tu_1`, `hotspot_1`, `target_1` und `changed_file`.
- Edge-Ausgaben referenzieren ausschliesslich diese stabilen IDs.
- Rohe nutzergelieferte Pfade, Target-Namen und Diagnostics erscheinen nie unescaped als ID.
- Innerhalb eines Knotentyps wird nach fachlicher Identitaet dedupliziert: Translation Units nach `TranslationUnitReference::unique_key`, Include-Hotspots nach Anzeige-Pfad, Targets nach `TargetInfo::unique_key`, geaenderte Datei als einzelner Impact-Knoten.

## Statement-Reihenfolge

- Die Ausgabe beginnt mit der `digraph`-Zeile.
- Danach folgen Graph-Attribute in exakt dieser Reihenfolge: `xray_report_type`, `format_version`, `graph_node_limit`, `graph_edge_limit`, `graph_truncated`.
- Danach folgen Node-Statements in der fuer Analyze beziehungsweise Impact dokumentierten Knotenprioritaet.
- Danach folgen Edge-Statements in der fuer Analyze beziehungsweise Impact dokumentierten Kantenprioritaet.
- Die Ausgabe endet mit der schliessenden Graph-Klammer.

## Attribut-Reihenfolge innerhalb von Statements

- Translation-Unit-Knoten: `kind`, optional `impact`, optional `rank`, optional `context_only`, `label`, `path`, `directory`, `unique_key`.
- Include-Hotspot-Knoten: `kind`, `label`, `path`, `context_total_count`, `context_returned_count`, `context_truncated`.
- Target-Knoten: `kind`, optional `impact`, `label`, `name`, `type`, `unique_key`.
- Changed-File-Knoten: `kind`, `label`, `path`.
- Kanten: `kind`, optional `style`.
- Optionale Attribute werden nur ausgegeben, wenn sie fuer den jeweiligen Knoten- oder Kantentyp definiert sind; wenn sie erscheinen, behalten sie die oben festgelegte Position.

## Gemeinsame Graph-Attribute

Jeder DOT-Report enthaelt mindestens:

| Attribut | Typ | Beschreibung |
| --- | --- | --- |
| `xray_report_type` | quoted string | `analyze` oder `impact`. |
| `format_version` | integer | identisch mit `xray::hexagon::model::kReportFormatVersion`, ab AP M6-1.5 A.3 `5` (vorher `4` ab AP M6-1.4 A.3, `3` ab AP M6-1.3, `2` ab AP M6-1.2). |
| `graph_node_limit` | integer | wirksames Node-Budget fuer diesen Report. |
| `graph_edge_limit` | integer | wirksames Edge-Budget fuer diesen Report. |
| `graph_truncated` | boolean | `true`, wenn ein Kandidatenknoten oder eine Kandidatenkante wegen `node_limit`, `edge_limit` oder Analyze-`context_limit` nicht im finalen Graph enthalten ist. Auch im ungekuerzten Fall verpflichtend ausgegeben (`false`). |
| `graph_target_graph_status` | quoted string | `not_loaded`, `loaded`, `partial`, `disabled` (v5). Pflicht ab AP M6-1.2 fuer Analyze und Impact. Spiegelt `AnalysisResult::target_graph_status` bzw. `ImpactResult::target_graph_status`. `disabled` (v5) bedeutet, dass die Target-Graph-Section nicht ueber `--analysis` angefordert wurde; semantisch unterschiedlich von `not_loaded` (angefordert, aber Daten nicht verfuegbar). |
| `graph_impact_target_depth_requested` | integer | nur Impact-DOT, Pflicht ab AP M6-1.3. Validierter angeforderter Reverse-BFS-Tiefenwert, identisch mit `ImpactResult::impact_target_depth_requested`. |
| `graph_impact_target_depth_effective` | integer | nur Impact-DOT, Pflicht ab AP M6-1.3. Tatsaechlich erreichte Tiefe, identisch mit `ImpactResult::impact_target_depth_effective`. Bei `graph_target_graph_status="not_loaded"` immer `0`. |
| `graph_include_scope` | quoted string | nur Analyze-DOT, Pflicht ab AP M6-1.4. `all`, `project`, `external` oder `unknown`. Spiegelt `AnalysisResult::include_scope_effective`. |
| `graph_include_depth` | quoted string | nur Analyze-DOT, Pflicht ab AP M6-1.4. `all`, `direct` oder `indirect`. Spiegelt `AnalysisResult::include_depth_filter_effective`. |
| `graph_include_node_budget_reached` | boolean | nur Analyze-DOT, Pflicht ab AP M6-1.4. Spiegelt `AnalysisResult::include_node_budget_reached`. |
| `graph_analysis_sections` | quoted string | nur Analyze-DOT, Pflicht ab AP M6-1.5. Kommagetrennte Liste der `effective_sections` aus `AnalysisResult::analysis_configuration`. Z. B. `"tu-ranking,include-hotspots,target-graph,target-hubs"` fuer `--analysis all`. |
| `graph_min_hotspot_tus` | integer | nur Analyze-DOT, Pflicht ab AP M6-1.5. Wirksamer `--min-hotspot-tus`-Wert; Default `2`. |
| `graph_target_hub_in_threshold` | integer | nur Analyze-DOT, Pflicht ab AP M6-1.5. Wirksamer `--target-hub-in-threshold`-Wert; Default `10`. |
| `graph_target_hub_out_threshold` | integer | nur Analyze-DOT, Pflicht ab AP M6-1.5. Wirksamer `--target-hub-out-threshold`-Wert; Default `10`. |
| `graph_tu_threshold_arg_count` | integer | nur Analyze-DOT, Pflicht ab AP M6-1.5. `--tu-threshold arg_count`-Wert; `0` = keine Filterung. |
| `graph_tu_threshold_include_path_count` | integer | nur Analyze-DOT, Pflicht ab AP M6-1.5. `--tu-threshold include_path_count`-Wert; `0` = keine Filterung. |
| `graph_tu_threshold_define_count` | integer | nur Analyze-DOT, Pflicht ab AP M6-1.5. `--tu-threshold define_count`-Wert; `0` = keine Filterung. |

`graph_truncated` bezieht sich auf den finalen Graphzustand nach Entfernen reiner, unverbundener Kontextknoten.

Die `graph_impact_target_depth_*`-Attribute erscheinen ausschliesslich in
Impact-DOT, unabhaengig vom `graph_target_graph_status`. Analyze-DOT
gibt sie nicht aus. Umgekehrt gibt nur Analyze-DOT die `graph_include_*`-
und die `graph_analysis_sections`/`graph_min_hotspot_tus`/
`graph_target_hub_*_threshold`/`graph_tu_threshold_*`-Attribute aus;
Impact-DOT traegt sie nicht, weil sowohl der Include-Filter- als auch der
Analyse-Konfigurations-Vertrag analyze-only sind.

DOT serialisiert bewusst NICHT den vollstaendigen
`analysis_section_states`-Block aus dem JSON-Vertrag. Per
[plan-M6-1-5.md](../docs/planning/done/plan-M6-1-5.md) §663-669
enthaelt der DOT-Vertrag nur konfigurationsrelevante Werte und die
angeforderte, validierte `effective_sections`-Liste. Vollstaendige
Section-States sind in JSON, HTML, Console und Markdown sichtbar; DOT
bleibt auf Graph-Metadaten und Graph-Inhalte beschraenkt. Wenn eine
Section `disabled` oder `not_loaded` ist, werden ihre Knoten und Kanten
nicht ausgegeben; die `graph_*`-Konfigurations-Attribute bleiben
trotzdem vorhanden.

## Analyze-Vertrag

### Knoten

Ein Analyze-DOT-Graph enthaelt nur Knoten aus vorhandenen `AnalysisResult`-Daten:

- Translation Units aus der Top-N-Ranking-Sicht.
- Include-Hotspots beziehungsweise Include-Dateien aus der Top-N-Hotspot-Sicht.
- Target-Knoten aus vorhandenen M4-Target-Zuordnungen primaerer Top-Ranking-Translation-Units.
- begrenzte Kontext-Translation-Units fuer ausgegebene Include-Hotspots.
- ab AP M6-1.2: weitere Target-Knoten aus `AnalysisResult::target_graph.nodes`, dedupliziert gegen die M4-Target-Zuordnungs-Knoten ueber `TargetInfo::unique_key`.
- ab AP M6-1.2: synthetische `external_target`-Knoten fuer `<external>::*`-Ziele, die als Endpunkt einer `target_dependency`-Kante mit `resolution=external` im finalen Graph erscheinen.

Pflichtattribute fuer Translation-Unit-Knoten:

- `kind="translation_unit"`.
- `label`: gekuerztes oder ungekuerztes Leselabel.
- `path`: vollstaendiger Anzeige-Pfad aus `TranslationUnitReference::source_path`.
- `directory`: Anzeige-Directory aus `TranslationUnitReference::directory`.
- `unique_key`: fachlicher Translation-Unit-Schluessel aus `TranslationUnitReference::unique_key`.

Optionale Translation-Unit-Attribute:

- `rank`: Integer fuer primaere Ranking-Knoten.
- `context_only`: Boolean, wenn die Translation Unit nur als Hotspot-Kontext und nicht als primaerer Ranking-Knoten enthalten ist.

Pflichtattribute fuer Include-Hotspot-Knoten:

- `kind="include_hotspot"`.
- `label`: gekuerztes oder ungekuerztes Leselabel.
- `path`: vollstaendiger Anzeige-Pfad aus `IncludeHotspot::header_path`.
- `origin` (Pflicht ab AP M6-1.4): quoted string `project`, `external` oder `unknown`. Spiegelt `IncludeHotspot::origin`. Erscheint zwischen `path` und `context_total_count`.
- `depth_kind` (Pflicht ab AP M6-1.4): quoted string `direct`, `indirect` oder `mixed`. Spiegelt `IncludeHotspot::depth_kind`. Erscheint direkt nach `origin`.
- `context_total_count`: Integer.
- `context_returned_count`: Integer.
- `context_truncated`: Boolean.

Pflichtattribute fuer Target-Knoten:

- `kind="target"`.
- `label`: Target-Anzeigename.
- `name`: Target-Anzeigename aus `TargetInfo::display_name`.
- `type`: Target-Typ aus `TargetInfo::type`.
- `unique_key`: fachlicher Target-Schluessel aus `TargetInfo::unique_key`.

Pflichtattribute fuer `external_target`-Knoten (ab AP M6-1.2):

- `kind="external_target"`.
- `label`: gekuerztes Leselabel aus dem rohen `raw_id`-Wert (Middle-Truncation an).
- `external_target_id`: vollstaendiger `raw_id`-Wert ohne Middle-Truncation. Beide Attribute sind als DOT-String-Literale escaped (Backslash, Anfuehrungszeichen, Newline usw. nach dem M5-Escape-Vertrag).

### Kanten

Analyze-DOT erzeugt nur folgende Kantenarten:

- Translation Unit zu Include-Hotspot, wenn der Hotspot-Kontext aus `IncludeHotspot` ableitbar ist.
- primaere Top-Ranking-Translation Unit zu Target, wenn fuer diese Translation Unit eine Target-Zuordnung vorhanden ist.
- ab AP M6-1.2: Target zu Target (`target_dependency`), wenn `AnalysisResult::target_graph.edges` mindestens eine direkte Abhaengigkeit enthaelt und beide Endknoten im finalen Graph stehen.

Pflichtattribute fuer Kanten:

- `kind`: `tu_include_hotspot`, `tu_target` oder `target_dependency`.
- `style` (nur `target_dependency`): `solid` fuer `resolution=resolved`, `dashed` fuer `resolution=external`.
- `resolution` (nur `target_dependency`): `resolved` oder `external`.
- `external_target_id` (nur `target_dependency` mit `resolution=external`): vollstaendiger `raw_id`-Wert, identisch mit dem gleichnamigen Knotenattribut.

Regeln:

- Analyze-`tu_*`-Kanten tragen kein `style`-Attribut.
- Analyze-Kanten tragen kein `label`-Attribut.
- `context_only`-Translation-Units erzeugen keine `tu_target`-Kanten und keine Target-Knoten.
- Kanten werden nur ausgegeben, wenn beide Endknoten im finalen Graph enthalten sind.
- Es werden keine Include-zu-Include-Kanten erzeugt.
- Bis AP M6-1.2 wurden keine Target-zu-Target-Kanten erzeugt; AP M6-1.2 fuehrt `target_dependency` ueber das `AnalysisResult::target_graph`-Modell ein.

### Budgets

```text
context_limit = min(top_limit, 5)
node_limit = max(25, 4 * top_limit + 10)
edge_limit = max(40, 6 * top_limit + 20)
```

`top_limit` ist im M5-CLI-Vertrag positiv. Der bestehende `CLI::PositiveNumber`-Validator lehnt `0` vor jeder Formatselektion ab; AP 1.3 fuegt keine eigene `--top`-Validierung hinzu.

`context_limit` gilt pro ausgegebenem Hotspot, erzeugt aber keinen unbegrenzten Gesamtgraphen, weil zusaetzlich `node_limit` und `edge_limit` gelten.

### Knotenprioritaet

Knotenkandidaten werden deterministisch in dieser Reihenfolge aufgenommen:

1. primaere Top-Ranking-Translation-Units.
2. Top-Hotspot-Knoten.
3. Target-Knoten fuer primaere Top-Ranking-Translation-Units.
4. Hotspot-Kontext-Translation-Units, sortiert nach `source_path`, `directory` und `unique_key`.
5. ab AP M6-1.2: weitere Target-Knoten aus `AnalysisResult::target_graph.nodes`, dedupliziert ueber `TargetInfo::unique_key` gegen Prioritaet 3.
6. ab AP M6-1.2: synthetische `external_target`-Knoten, ID-Vergabe `external_<index>` in der Sortierreihenfolge `to_unique_key` aufsteigend (deterministisch und unabhaengig von der Quelle).

Tie-Breaker:

- Translation Units nach `source_path`, `directory` und `unique_key`, wenn die fachliche Ranking-Sortierung gleich ist.
- Include-Hotspots nach Anzeige-Pfad, wenn Hotspot-Metriken gleich sind.
- Targets nach `display_name`, `type` und `unique_key`.
- Kontext-Translation-Units nach `source_path`, `directory` und `unique_key`.

Wenn `node_limit` erreicht ist, werden spaetere Kandidaten nicht aufgenommen und `graph_truncated=true` gesetzt.

### Hotspot-Kontext

Fuer einen Top-Hotspot werden hoechstens `context_limit` der in `IncludeHotspot` gespeicherten betroffenen Translation Units als Kontextknoten ausgegeben.

- `context_total_count` zaehlt pro Hotspot die eindeutigen betroffenen Translation Units vor `context_limit` und vor globalem Graph-Budget.
- `context_returned_count` zaehlt pro Hotspot die eindeutigen Kontext-Translation-Units, die nach `context_limit`, `node_limit` und `edge_limit` tatsaechlich als Knoten im finalen Graph enthalten und durch eine Kontextkante mit diesem Hotspot verbunden sind.
- Mehrfach referenzierte Kontext-Translation-Units werden pro Hotspot in dessen `context_*`-Werten gezaehlt, aber als Graphknoten nur einmal gegen `node_limit`.
- Eine Translation Unit, die zugleich Top-Ranking-Knoten und Hotspot-Kontext ist, wird in Prioritaet 1 aufgenommen und zaehlt nur dort gegen `node_limit`; in Prioritaet 4 wird nur die zulaessige Kontextkante gebildet.
- `context_truncated=true`, wenn `context_total_count > context_returned_count`.
- Reine Kontextknoten ohne verbleibende Kontextkante werden nach Anwendung des `edge_limit` entfernt.

### Kantenprioritaet

Kantenkandidaten werden nur fuer vorhandene Endknoten gebildet und priorisiert:

1. Translation-Unit-zu-Include-Hotspot-Kanten.
2. Translation-Unit-zu-Target-Kanten.
3. ab AP M6-1.2: `target_dependency`-Kanten zwischen Targets bzw. zwischen Target und synthetischem `external_target`-Knoten. Innerhalb dieser Kantenart wird nach `(from_unique_key, to_unique_key)` priorisiert.

Kantenrichtung:

- `tu_include_hotspot`: Quelle ist die Translation Unit, Ziel ist der Include-Hotspot.
- `tu_target`: Quelle ist die primaere Top-Ranking-Translation-Unit, Ziel ist das Target.
- `target_dependency`: Quelle ist das konsumierende Target, Ziel ist das Ziel-Target oder ein synthetischer `external_target`-Knoten.

## Impact-Vertrag

### Knoten

Ein Impact-DOT-Graph enthaelt nur Knoten aus vorhandenen `ImpactResult`-Daten:

- geaenderte Datei.
- direkt betroffene Translation Units.
- heuristisch betroffene Translation Units.
- betroffene Targets aus vorhandenen M4-Target-Zuordnungen.

Pflichtattribute fuer den Knoten der geaenderten Datei:

- `kind="changed_file"`.
- `label`: gekuerztes oder ungekuerztes Leselabel.
- `path`: vollstaendiger Anzeige-Pfad aus `ReportInputs.changed_file`.

Pflichtattribute fuer Translation-Unit-Knoten:

- `kind="translation_unit"`.
- `impact`: `direct` oder `heuristic`.
- `label`, `path`, `directory`, `unique_key`.

Pflichtattribute fuer Target-Knoten:

- `kind="target"`.
- `impact`: `direct` oder `heuristic`. Optional: Knoten, die nur ueber
  `target_graph.nodes` und nicht ueber Affected-Targets im Graph stehen,
  fuehren `impact` nicht.
- `label`, `name`, `type`, `unique_key`.

Wenn ein Target sowohl direkt als auch heuristisch betroffen ist, gewinnt `impact="direct"`.

Ab AP M6-1.3 ergaenzen optional drei Reverse-BFS-Attribute, sobald
`graph_target_graph_status != not_loaded` und das Target in
`ImpactResult::prioritized_affected_targets` enthalten ist:

- `priority_class`: `direct`, `direct_dependent` oder `transitive_dependent` (quoted string).
- `graph_distance`: integer in `[0, 32]`. Korrespondenz zur
  `priority_class`: `direct=0`, `direct_dependent=1`,
  `transitive_dependent>=2`.
- `evidence_strength`: `direct`, `heuristic` oder `uncertain` (quoted string).

Diese drei Attribute koexistieren mit dem bestehenden M5-`impact`-Attribut
(`direct`/`heuristic` aus `ImpactedTarget::classification`). `impact` ist
die M5-Klassifikation, `evidence_strength` ist die AP-1.3-Klassifikation
aus der Reverse-BFS und kann sich von `impact` unterscheiden (z. B. bei
Reverse-Hops, deren Owner nicht in `affected_targets` ist). Beide
Attribute bleiben in v3 erhalten.

`prioritised`-Attribute erscheinen NICHT in Analyze-DOT, weil
Reverse-BFS in M6 ausschliesslich auf der Impact-Sicht laeuft.

### Kanten

Impact-DOT erzeugt nur folgende Kantenarten:

| `kind` | Quelle | Ziel | `style` |
| --- | --- | --- | --- |
| `changed_file_direct_tu` | geaenderte Datei | direkt betroffene Translation Unit | `solid` |
| `changed_file_heuristic_tu` | geaenderte Datei | heuristisch betroffene Translation Unit | `dashed` |
| `direct_tu_target` | direkt betroffene Translation Unit | Target | `solid` |
| `heuristic_tu_target` | heuristisch betroffene Translation Unit | Target | `dashed` |

Style-Attribute sind Teil des Vertrags und nicht Graphviz-versionabhaengig.

Impact-Kanten tragen kein `label`-Attribut.

### Budgets

```text
node_limit = 100
edge_limit = 200
```

- `impact` erhaelt kein `--top`.
- Das zugrunde liegende `ImpactResult` bleibt unbegrenzt; nur die Graphsicht ist begrenzt.

### Knotenprioritaet

1. geaenderte Datei.
2. direkt betroffene Translation Units, sortiert nach `source_path`, `directory`, `unique_key`.
3. heuristisch betroffene Translation Units, sortiert nach `source_path`, `directory`, `unique_key`.
4. Target-Knoten, sortiert nach `display_name`, `type`, `unique_key`.

### Kantenprioritaet

1. `changed_file_direct_tu`.
2. `changed_file_heuristic_tu`.
3. `direct_tu_target`.
4. `heuristic_tu_target`.

## Fehlervertrag

- CLI-Parser-/Verwendungsfehler bleiben Textfehler auf `stderr`. Beispiele: `impact --format dot` ohne `--changed-file`. Kein DOT-Fehlergraph.
- Nicht wiederherstellbare Eingabefehler (fehlende Datei, ungueltiges JSON, ungueltige File-API-Reply-Daten) bleiben Textfehler auf `stderr`. Kein DOT-Fehlergraph.
- `ReportInputs.changed_file_source=unresolved_file_api_source_root` ist ein File-API-Fehlerergebnis und wird in AP 1.3 nicht als DOT-Graph gerendert.
- Render-, Schreib- und Output-Fehler liefern Textfehler auf `stderr` und keinen partiellen DOT-Output. Bestehende Zieldateien bleiben bei Fehlern unveraendert.
- Service-Ergebnisse mit Diagnostics oder Teildaten werden als regulaere DOT-Reports ausgegeben. Diagnostics werden in AP 1.3 nicht als eigene Diagnostic-Knoten modelliert.

## Versionierung

- `format_version` startet bei `1` und entspricht `xray::hexagon::model::kReportFormatVersion`.
- `format_version` wird erhoeht bei:
  - Entfernen oder Aendern eines Pflichtattributs.
  - Aendern der Statement-Reihenfolge oder Attribut-Reihenfolge.
  - Aendern der Sortier- oder Tie-Breaker-Regeln.
  - Aendern des Label-Kuerzungsalgorithmus oder der Escape-Regeln.
  - Aendern der Budget-Formeln fuer Analyze oder der festen Impact-Budgets.
  - Hinzufuegen einer neuen Knoten- oder Kantenart.
- Eine spaetere Vertragsversion kann konfigurierbare Budgets, Diagnostic-Knoten oder Target-zu-Target-Kanten einfuehren, muss dann aber `format_version` erhoehen und Goldens, Tests sowie dieses Dokument synchron aktualisieren.

## Validierung

- DOT-Goldens unter `tests/e2e/testdata/m5/dot-reports/` werden in CI doppelt validiert:
  - Docker-Stages installieren `graphviz` via apt im `toolchain`-Layer und rendern jedes Golden mit `dot -Tsvg`. Das SVG-Ergebnis wird nicht versioniert; das Gate prueft nur die erfolgreiche Verarbeitung.
  - Native CI-Matrizen rufen `tests/validate_dot_reports.py` auf. Das Skript ist reine Python-Standardbibliothek, akzeptiert dieselben Goldens und Pfadkonventionen wie der Graphviz-Pfad und ist plattformuebergreifend stabil.
- Goldens und Manifest werden ueber den jeweiligen Validator beidseitig abgeglichen.
- E2E-Tests vergleichen die echten Binary-Outputs byte-stabil gegen die Goldens und validieren denselben Output zusaetzlich gegen das Syntax-Gate.
