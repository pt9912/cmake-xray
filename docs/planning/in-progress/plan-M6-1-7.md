# Plan M6 - AP 1.7 Referenzdaten, Dokumentation und Abnahme aktualisieren

## Ziel

AP 1.1 bis AP 1.6 fuegen fachliche Funktionalitaet, neue Vertraege
und neue CLI-Optionen hinzu. Jeder dieser APs aktualisiert seine
unmittelbar betroffenen Format-Vertraege (`spec/report-json.md`,
`spec/report-dot.md`, `spec/report-html.md`,
`spec/report-json.schema.json`) und liefert Goldens unter
`tests/e2e/testdata/m6/` mit. Was AP 1.1 bis AP 1.6 NICHT
abschliessend leisten:

- die uebergreifende Nutzerdokumentation (`README.md`, `docs/user/guide.md`),
- das Beispielverzeichnis `docs/examples/`,
- die Qualitaetsdokumentation (`docs/user/quality.md`) auf den
  konsolidierten M6-Testumfang,
- die Performance-Doku (`docs/user/performance.md`) mit der M6-Baseline,
- den CHANGELOG-Eintrag fuer `v1.3.0`,
- die Roadmap-Aktualisierung (`docs/planning/roadmap.md`),
- den Versionspin in `CMakeLists.txt` von `1.2.0` auf `1.3.0`.

AP 1.7 ist der **Schlussstein** der M6-Reihe. Es liefert keinen
neuen fachlichen Code, sondern macht M6 abnahmefaehig: alle
Doku-Quellen sind synchron, das Beispielverzeichnis ist gegen
Generator-Drift gepinnt, der CHANGELOG dokumentiert alle
Versionsspruenge, und der Versionspin ist auf `v1.3.0` gesetzt.

AP 1.7 fuehrt **keinen** weiteren Versionssprung ein. AP 1.7 verwendet
zwei symbolische Release-Konstanten: `M6_ANALYZE_FORMAT_VERSION` ist
exakt der in Code und Schema bereits implementierte finale
`kReportFormatVersion` nach AP 1.6, und `M6_COMPARE_FORMAT_VERSION` ist
der in Code und Schema implementierte finale `kCompareFormatVersion`
(`1`). AP 1.7 liest diese Werte aus dem Ist-Stand ab und referenziert
sie in Checklisten, CHANGELOG-Texten und Tests, statt einen eigenen
Entscheidungsbaum oder eine hart codierte Analyze-Formatversion zu
definieren. Wenn AP 1.6 das Analyze-Schema auf `6` gehoben hat,
reflektiert AP 1.7 diesen finalen Stand nur in Doku, Tests und
Changelog.

## Scope

Umsetzen:

- `README.md` mit M6-Beispielen aktualisieren: Target-Graph-Sicht,
  Include-Filter, Schwellenwerte, Compare-Aufrufe.
- `docs/user/guide.md` mit M6-Praxisaufrufen ergaenzen.
- `spec/report-json.md`/`spec/report-json.schema.json` /
  `spec/report-dot.md`/`spec/report-html.md` Final-Sweep:
  - Konsistenzpruefung gegen alle AP-1.2-bis-1.6-Aenderungen.
  - Querverweise zwischen den Dokumenten (z. B. Compare-
    Kompatibilitaetsmatrix-Verweis aus `report-json.md`).
  - Fehlende Beispiele oder Schema-Erlaeuterungen ergaenzen.
- `docs/examples/` mit M6-Beispielen erweitern:
  - Analyze-Beispiele mit Target-Graph, Hubs, Include-Filter,
    Schwellenwerten in allen fuenf Formaten.
  - Impact-Beispiele mit Priorisierung in allen fuenf Formaten.
  - Compare-Beispiele in Console, Markdown, JSON.
- `docs/examples/manifest.txt` und
  `docs/examples/generation-spec.json` um die neuen Beispiele
  erweitern; Drift-Gate gruen.
- `docs/user/quality.md` mit M6-Testumfang aktualisieren:
  - Neue Test-Kategorien aus AP 1.1-1.6 (BFS-Determinismus,
    Schema-Validation, Compile-DB-Fingerprint, Compare-
    Kompatibilitaetsmatrix).
  - Plattformstatus aus M5 weitergefuehrt; AP 1.7 hebt **nicht**
    den Plattformstatus.
- `docs/user/performance.md` mit M6-Baseline:
  - Target-Graph-Extraktion pro Reply-Groesse.
  - Reverse-BFS-Performance pro `--impact-target-depth`.
  - Compare-Performance pro JSON-Eingangsgroesse.
- `CHANGELOG.md` mit `v1.3.0`-Sektion: Added/Changed/Fixed-Gruppen,
  Plattformstatus-Tabelle, Migrations-Hinweise (insb.
  `format_version`-Spruenge).
- `docs/planning/roadmap.md` mit M6 als abgeschlossen markieren; M7 als
  naechsten Phasen-Eintrag (falls in der Roadmap vorgesehen).
- Versionspin in Root-`CMakeLists.txt` von `1.2.0` auf `1.3.0`.
- Konsolidierter Abnahmetest-Lauf ueber Docker-Gates.

Nicht umsetzen:

- Tatsaechliche Release-Veroeffentlichung (Tag, GitHub Release,
  OCI-Image-Push). AP 1.7 bereitet die Veroeffentlichung vor; das
  Release-Workflow-Tagging ist eine eigene CI-Aktion nach AP 1.7.
- Plattform-Hochstufung von macOS oder Windows ueber den M5-Stand
  hinaus. AP 1.7 dokumentiert nur den uebernommenen Status.
- Konfigurationsdatei-Vertrag (siehe AP 1.5 Folgearbeiten).
- M7-Roadmap-Inhalte. AP 1.7 traegt nur "M6 abgeschlossen" und
  ggf. einen Platzhalter ein.

Diese Punkte folgen in spaeteren Phasen oder bleiben bewusst
ausserhalb von M6.

## Voraussetzungen aus AP 1.1-1.6

AP 1.7 startet erst, wenn alle vorhergehenden APs ihre A.x-Tranchen
geliefert haben:

- AP 1.1: Target-Graph-Modelle, Adapter, Service-Helper.
- AP 1.2: `format_version=2`, Reportausgaben fuer Target-Graph
  und Hubs.
- AP 1.3: `format_version=3`, Impact-Priorisierung,
  `--impact-target-depth`/`--require-target-graph`.
- AP 1.4: `format_version=4`, Include-Klassifikation,
  `--include-scope`/`--include-depth`.
- AP 1.5: `format_version=5`, `--analysis`, Schwellenwerte.
- AP 1.6: `kCompareFormatVersion=1`, `cmake-xray compare`,
  Compare-Kompatibilitaetsmatrix.

AP 1.7 prueft, dass alle Liefer-Stand-Bloecke der Sub-Plaene
`docs/planning/plan-M6-1-1.md` bis `docs/planning/plan-M6-1-6.md` mit
Commit-Hashes gepinnt sind. Diese Pruefung ist nicht rein manuell:
`m6_versionspin_consistency` validiert, dass `docs/planning/plan-M6.md` fuer
alle APs 1.1 bis 1.7 nicht-leere Commit-Spalten enthaelt und dass die
Sub-Plan-Liefer-Stand-Bloecke keine Platzhalter mehr enthalten.

## Dateien

Voraussichtlich zu aendern:

- `CMakeLists.txt` (Versionspin)
- `README.md`
- `docs/user/guide.md`
- `spec/report-json.md`
- `spec/report-json.schema.json` (nur falls Final-Sweep
  Inkonsistenzen findet; sonst unveraendert)
- `spec/report-dot.md`
- `spec/report-html.md`
- `spec/report-compare.md` (aus AP 1.6 uebernommen, hier konsolidiert)
- `spec/report-compare.schema.json` (aus AP 1.6 uebernommen, hier validiert)
- `spec/compare-matrix.md` (aus AP 1.6 uebernommen)
- `docs/user/quality.md`
- `docs/user/performance.md`
- `docs/planning/roadmap.md`
- `CHANGELOG.md`
- `docs/examples/` (neue/aktualisierte Beispiele)
- `docs/examples/manifest.txt`
- `docs/examples/generation-spec.json`
- `tests/reference/file-api-performance-manifest.json`
- `tests/hexagon/test_m6_versionspin_consistency.cpp`
- `tests/validate_doc_examples.py` (falls neue Validierungen noetig)
- `docs/planning/plan-M6.md` (Liefer-Stand-Tabelle aus den Sub-Plaenen
  konsolidiert)
- `docs/planning/plan-M6-1-7.md` (eigener Liefer-Stand)

Neue Dateien (optional):

- `docs/migration-m5-to-m6.md` falls Migrations-Hinweise umfangreich
  genug sind, um eigene Datei zu rechtfertigen. Sonst inline in
  `CHANGELOG.md`.

Hard Dependency: `spec/report-compare.md`,
`spec/report-compare.schema.json` und `spec/compare-matrix.md` muessen
vor Abschluss von AP 1.7 existieren. Wenn AP 1.6 sie bereits geliefert
hat, prueft AP 1.7 sie nur gegen den finalen Ist-Stand und korrigiert
Drift. Fehlt eines dieser Artefakte, legt AP 1.7 es nicht als neues
Feature an, sondern schliesst die AP-1.6-Lieferluecke im Audit-Scope
und dokumentiert den Diff in der Liefer-Stand-Tabelle.

## Versionspin

Root-`CMakeLists.txt` wird auf `project(... VERSION 1.3.0)` gehoben.

Regeln (analog M5 AP 1.8):

- Numerische Projektversion: exakt `1.3.0`.
- Release-Builds verwenden `XRAY_APP_VERSION` aus dem validierten
  Tag ohne fuehrendes `v` als kanonische App-/Package-Version.
- `XRAY_VERSION_SUFFIX` ist nur fuer lokale oder nicht
  veroeffentlichende Builds zulaessig.
- Gleichzeitiges Setzen beider Quellen (`XRAY_APP_VERSION` und
  `XRAY_VERSION_SUFFIX`) bricht fail-fast ab.
- `cmake-xray --version`, die kompilierte/generierte
  `ApplicationInfo`-Version, App-/Package-Version-Quelle und
  Release-Tag melden dieselbe veroeffentlichte Semver-Version
  ohne fuehrendes `v`, inkl. Prerelease-Suffix bei Tags wie
  `v1.3.0-rc.1`.
- Abnahmekriterien unterscheiden zwei Kontexte: Der finale M6-Release
  ohne Prerelease-Tag meldet exakt `1.3.0`; ein expliziter
  Pre-Release-Build mit `XRAY_APP_VERSION=1.3.0-rc.1` meldet exakt
  `1.3.0-rc.1`.

## README und Guide

### README.md

Neue Abschnitte oder erweiterte Beispiele:

- **Target-Graph-Sicht**: Beispielaufruf
  `cmake-xray analyze --cmake-file-api build --format console`
  mit Target-Graph- und Hub-Output.
- **Include-Filter**: Beispielaufruf
  `cmake-xray analyze --include-scope project --include-depth direct`
  mit gefiltertem Hotspot-Output.
- **Schwellenwerte**: Beispielaufruf
  `cmake-xray analyze --tu-threshold include_path_count=5 --min-hotspot-tus 5`
  mit gefiltertem Ranking.
- **Impact-Priorisierung**: Beispielaufruf
  `cmake-xray impact --changed-file src/foo.cpp --impact-target-depth 2`
  mit `priority_class`-Sicht.
- **Compare**: Beispielaufruf
  `cmake-xray compare --baseline a.json --current b.json --format markdown --output diff.md`.
- **Plattformstatus-Tabelle**: aus M5 weitergefuehrt (Linux
  `supported`, macOS arm64 / Windows x86_64 `validated_smoke`).
- **Format-Versions-Tabelle**: `kReportFormatVersion` entspricht
  `M6_ANALYZE_FORMAT_VERSION`, `kCompareFormatVersion` entspricht
  `M6_COMPARE_FORMAT_VERSION`; beide verweisen auf die
  Format-Vertrags-Dokumente.

### docs/user/guide.md

Neue Praxisrezepte:

- "Welche Targets sind durch eine Header-Aenderung betroffen?"
- "Wie identifiziere ich Hub-Targets in einem grossen Build?"
- "Wie filtere ich Include-Hotspots auf Projekt-Header?"
- "Wie vergleiche ich zwei Analyzezeitpunkte mit `compare`?"
- "Wann lohnt sich `--require-target-graph` in CI?"

Jedes Rezept enthaelt:

- Ziel.
- Beispielaufruf.
- Erwarteter Output (Auszug).
- Ggf. Verweis auf zugehoeriges Format-Vertrags-Dokument.

## Format-Vertrags-Final-Sweep

AP 1.7 fuehrt eine konsolidierte Pruefung aller Format-Vertrags-
Artefakte durch:

- `spec/report-json.md`: Verweise auf
  `spec/compare-matrix.md` und `spec/report-compare.schema.json`
  sind vollstaendig.
- `spec/report-dot.md`: alle ab AP 1.2 eingefuehrten Knoten- und
  Kantenarten sowie Graph-Attribute sind dokumentiert.
- `spec/report-html.md`: alle ab AP 1.2 eingefuehrten Sections und
  CSS-Klassen sind dokumentiert.
- `spec/report-compare.md`: Compare-Output-Vertrag und Beispiele.
- `spec/report-compare.schema.json`: Compare-JSON-Schema ist
  vollstaendig und passt zu `M6_COMPARE_FORMAT_VERSION`.
- `spec/compare-matrix.md`: Matrix ist identisch zur Matrix in
  `spec/report-json.md`.

Wenn der Final-Sweep Inkonsistenzen findet (z. B. fehlende
Kreuzverweise, veraltete Beispiele in einem Dokument), werden sie
in derselben Tranche behoben. AP 1.7 fuehrt keine neuen Vertraege
ein, korrigiert aber Doku-Drift.

Definition of Done fuer A.2/A.3:

| Dokument/Artefakt | Erwarteter Status | Nachweis |
| --- | --- | --- |
| `spec/report-json.md` / `.schema.json` | finaler M6-Analyze-Vertrag, keine AP-Platzhalter | `json_schema_check`, Crosslinks zu Compare-Doks |
| `spec/report-dot.md` / `spec/report-html.md` | nur Drift-Korrekturen zu AP 1.2-1.5, keine neuen Vertragsinhalte | Golden-/Adapter-Tests, Doku-Review |
| `spec/report-compare.md` / `.schema.json` / `spec/compare-matrix.md` | AP-1.6-Vertrag vollstaendig uebernommen und validiert | Compare-Schema-Test, Matrix-Test |
| `docs/examples/` + Manifest | Beispiele aus bestehenden Goldens, keine neuen Feature-Szenarien | `doc_examples_validation` |
| `README.md`, `docs/user/guide.md`, `docs/user/performance.md`, `docs/planning/roadmap.md` | Nutzer- und Statusdoku synchron zum finalen Ist-Stand | A.3-Review, Docker-Gates |

A.2/A.3 duerfen nur Drift-Korrekturen, Querverweise, Beispiele und
Release-Dokumentation einbringen. Neue fachliche Vertrage oder
Feature-Semantik gehoeren zurueck in die jeweiligen AP-1.1-bis-1.6-
Plaene.

## docs/examples/

Erweitert um neue Beispiele:

- `analyze-target-graph-loaded.{txt,md,html,json,dot}`
- `analyze-target-graph-partial.{txt,md,html,json,dot}` (mit
  External-Kante)
- `analyze-include-scope-project.{txt,md,html,json,dot}`
- `analyze-include-depth-direct.{txt,md,html,json,dot}`
- `analyze-thresholds.{txt,md,html,json,dot}` (mit
  TU- und Hotspot-Schwellen)
- `analyze-disabled-target-hubs.{txt,md,html,json,dot}` (mit
  `--analysis tu-ranking,target-graph`)
- `impact-prioritised.{txt,md,html,json,dot}`
- `impact-require-target-graph-error.txt` (Stderr-Output bei
  fehlendem Target-Graph)
- `compare-typical.{txt,md,json}`
- `compare-config-drift.{txt,md,json}`
- `compare-project-identity-drift.{txt,md,json}`

Pflicht:

- `docs/examples/manifest.txt` enthaelt alle neuen Pfade.
- `docs/examples/generation-spec.json` enthaelt fuer jedes Beispiel
  die `cmake-xray`-Args, sodass `tests/validate_doc_examples.py`
  byteweise gegen den aktuellen Generator-Output validieren kann.
- Doku-Beispiele werden aus den Goldens unter
  `tests/e2e/testdata/m6/` generiert (oder umgekehrt) und sind im
  CTest-Gate `doc_examples_validation` abgedeckt.
- `README.md` und `docs/user/guide.md` verlinken die neuen Beispiele.

## docs/user/quality.md

M6-Testumfang dokumentiert:

- File-API-Adapter-Tests fuer Target-Graph-Extraktion.
- Service-Tests fuer Hub-Erkennung,
  Target-Graph-Impact-Priorisierung, Compile-DB-only-Fallback.
- Service-/Adapter-Tests fuer Include-Herkunft und Include-Tiefe.
- CLI-Tests fuer alle neuen Optionen mit allen Fehlerphrasen.
- Matrix-Tests fuer optionale Analyseabschnitte:
  absichtlich deaktiviert, angefordert aber `not_loaded`,
  angefordert und leer, widerspruechliche Auswahl.
- Golden-Tests fuer Console, Markdown, HTML, JSON und DOT mit
  Target-Graph-Daten und Impact-Priorisierung.
- JSON-Schema-Tests fuer `M6_ANALYZE_FORMAT_VERSION` und
  `M6_COMPARE_FORMAT_VERSION`.
- DOT-Syntax-Tests mit Target-zu-Target-Kanten und Zyklen.
- Compare-Tests fuer added/removed/changed,
  asymmetrische Datenverfuegbarkeit, configuration_drift,
  --allow-project-identity-drift, fehlende Matrix-Eintraege.
- Compile-DB-Fallback-Hash-Goldens mit Unix-/Windows-Pfaden,
  unterschiedlicher Eingabereihenfolge, Deduplikation,
  reproduzierbarer kanonischer JSON-Payload.
- Trade-off-Abnahmetests: gleicher Source-Path-Satz mit
  unterschiedlichen Build-Kontexten -> dieselbe
  fallback-basierte Projektidentitaet.
- Regressionstests, dass M5-Berichte ohne M6-Daten nicht unnoetig
  driften.
- Dokumentationsbeispiele werden gegen aktuelle Generator-Outputs
  oder Goldens validiert.

Plattformstatus aus M5 wird unveraendert weitergefuehrt:

- Linux x86_64: `supported`.
- macOS arm64: `validated_smoke`.
- Windows x86_64: `validated_smoke`.
- AP 1.7 hebt den Plattformstatus **nicht**.

## docs/user/performance.md

Neue M6-Baseline-Sektionen:

### Target-Graph-Extraktion

- Reproduzierbare Reply-Fixtures aus
  `tests/reference/scale_*/build/.cmake/api/v1/reply/`.
- Messung pro Reply-Groesse (Anzahl Targets, Anzahl Kanten).
- Ergebnis-Form: Tabelle mit `target_count`, `edge_count`,
  `extraction_ms`, `git_commit`, `cmake_version`.

### Reverse-BFS-Performance

- Synthetische `target_graph`-Eingabe mit `n` Targets in einer
  linearen Kette.
- Messung pro `--impact-target-depth`-Wert (`0`, `1`, `8`, `32`).
- Ergebnis-Form: Tabelle mit `target_count`, `depth`,
  `bfs_visited_targets`, `bfs_ms`.

### Compare-Performance

- Zwei Analyze-JSON-Berichte unterschiedlicher Groesse.
- Messung pro JSON-Groesse (kleine, mittlere, grosse Reply).
- Ergebnis-Form: Tabelle mit `tu_count`, `hotspot_count`,
  `target_count`, `compare_ms`.

### Hinweise

- AP 1.7 setzt KEINE absolute Performance-Schwelle. Die Baseline
  dient als Referenz fuer kuenftige Phasen.
- Performance-Manifest in `tests/reference/file-api-performance-manifest.json`
  wird um die neuen M6-Eintraege erweitert. Das bestehende Manifest
  bleibt die zentrale Performance-Manifest-Datei, erhaelt aber
  getrennte Datensatztypen/Sections fuer `file_api_extraction`,
  `reverse_bfs` und `compare`. Compare-Datensaetze verwenden
  `kind=compare`, `json_size_class`, `tu_count`, `hotspot_count`,
  `target_count` und `compare_ms`; File-API-spezifische Felder sind
  dort nicht Pflicht.

## CHANGELOG.md

Neuer Abschnitt `[1.3.0] - <Release-Datum>`:

```markdown
## [1.3.0] - YYYY-MM-DD

### Added
- Target graph extraction from CMake File API replies, including
  cycle handling, external dependency markers, and id-collision
  diagnostics (M6 AP 1.1).
- Target graph and target hub sections in console, markdown, HTML,
  JSON, and DOT report formats (M6 AP 1.2).
- Impact prioritisation via reverse target-graph BFS, with new
  CLI options `--impact-target-depth` and `--require-target-graph`
  (M6 AP 1.3).
- Include hotspot classification by origin (project / external /
  unknown) and depth (direct / indirect / mixed); new CLI options
  `--include-scope` and `--include-depth` (M6 AP 1.4).
- Configurable analysis selection and thresholds via
  `--analysis`, `--tu-threshold`, `--min-hotspot-tus`,
  `--target-hub-in-threshold`, `--target-hub-out-threshold`
  (M6 AP 1.5).
- New `cmake-xray compare` subcommand for diffing two analyze
  JSON reports, with structured drift diagnostics
  (configuration_drift, data_availability_drift,
  project_identity_drift) and a documented compatibility matrix
  (M6 AP 1.6).
- Project identity field in analyze JSON, derived from the file
  API source root or a deterministic compile-database fingerprint.

### Changed
- `kReportFormatVersion` bumped from `1` (M5) to
  `M6_ANALYZE_FORMAT_VERSION` as the end state of the M6 series
  (M5 baseline through the final AP 1.6 code/schema state). Intermediate
  APs introduced versions 2 through the final M6 value. Consumers must
  migrate to the final M6 analyze format version; the
  closed-schema rule (`additionalProperties: false`) makes
  unmigrated consumers fail validation.
- `cmake-xray --version` reports `1.3.0`.
- DOT graph attributes now include
  `graph_target_graph_status`, `graph_include_scope`,
  `graph_include_depth`, `graph_include_node_budget_reached`,
  the analysis configuration block, and the impact target depth
  block.
- HTML report sections include `Analysis Configuration`,
  `Target Graph`, `Target Hubs`, `Prioritised Affected Targets`.

### Fixed
- (entries from any AP audit-pass fixups across AP 1.1-1.6)

### Migration
- pre-M6 JSON reports are not consumable by v1.3.0 tools. Re-run
  cmake-xray on your project to regenerate reports with
  `M6_ANALYZE_FORMAT_VERSION` before using `compare`.
- The new `compare` subcommand only accepts `analyze` JSON
  reports. Impact compare is post-M6.

### Platform status
- Linux x86_64: supported (release tarball + OCI image)
- macOS arm64: validated_smoke
- Windows x86_64: validated_smoke
```

## docs/planning/roadmap.md

- M6 wird als abgeschlossen markiert mit Verweis auf den finalen
  AP-Liefer-Stand.
- M7 (falls in der Roadmap vorgesehen) bekommt einen kurzen
  Platzhalter-Eintrag mit "Folgearbeiten aus M6 AP-1.7"-Hinweis,
  oder bleibt offen.

## Konsolidierter Liefer-Stand in `docs/planning/plan-M6.md`

Die Liefer-Stand-Bloecke aus den Sub-Plaenen werden in einer
gemeinsamen Tabelle in `docs/planning/plan-M6.md` konsolidiert:

| AP | Tranche | Commit | Beschreibung |
|---|---|---|---|
| 1.1 | A.1-A.5 | (aus `plan-M6-1-1.md`) | Target-Graph-Modelle und Service-Anbindung |
| 1.2 | A.1-A.4 | (aus `plan-M6-1-2.md`) | Reportausgaben |
| 1.3 | A.1-A.5 | (aus `plan-M6-1-3.md`) | Impact-Priorisierung |
| 1.4 | A.1-A.6 | (aus `plan-M6-1-4.md`) | Include-Klassifikation |
| 1.5 | A.1-A.6 | (aus `plan-M6-1-5.md`) | Schwellenwerte |
| 1.6 | A.1-A.7 | (aus `plan-M6-1-6.md`) | Compare |
| 1.7 | A.1-A.4 | (eigene) | Doku, Beispiele, Abnahme |

## Tests und Abnahme

AP 1.7 fuehrt keine neuen fachlichen Feature-Tests ein, sondern
bestaetigt die in AP 1.1-1.6 geforderten Tests:

- Plan-Test-Liste aus jedem Sub-Plan wird gegen Ist-Stand
  verifiziert (Audit-Pass-Konvention aus M5).
- `docs/examples/`-Drift-Gate (`doc_examples_validation`) ist
  gruen.
- `json_schema_check` ist gruen fuer
  `kReportFormatVersion == M6_ANALYZE_FORMAT_VERSION` und
  `kCompareFormatVersion == M6_COMPARE_FORMAT_VERSION`.
- DOT-Syntax-Gates (`dot -Tsvg` Docker-Pfad und
  `tests/validate_dot_reports.py` native Pfad) gruen.
- HTML-Adapter-Tests fuer Sonderzeichen-Escaping gruen.
- Console-, Markdown-, HTML-, JSON-, DOT-Goldens unter
  `tests/e2e/testdata/m5/` und `m6/` byte-stabil.
  Compare-Goldens sind davon bewusst nur fuer die in AP 1.6
  implementierten Formate `console`/`txt`, `markdown` und `json`
  gefordert; HTML- und DOT-Compare bleiben post-M6.
- Docker-Gates (`test`, `coverage-check`, `quality-check`,
  `runtime`) gruen.

Zusaetzlich ergaenzt AP 1.7 einen Abnahmeselbsttest/Gating-Test fuer
den Release-Schnitt:

- **`m6_versionspin_consistency`**: Test, dass
  `cmake-xray --version` im finalen Release-Kontext exakt `1.3.0`
  ausgibt, dass ein Pre-Release-Kontext den Suffix exakt uebernimmt,
  dass das Schema
  `kReportFormatVersion == M6_ANALYZE_FORMAT_VERSION` und
  `kCompareFormatVersion == M6_COMPARE_FORMAT_VERSION` widerspiegelt,
  und dass keine Roh-Version `1.2.0` mehr im Code uebrig ist.
  Konkreter Testpfad: `tests/hexagon/test_m6_versionspin_consistency.cpp`
  mit CTest-Name `m6_versionspin_consistency`.

## Implementierungsreihenfolge

Innerhalb von **A.1 (Versionspin und CHANGELOG)**:

1. Root-`CMakeLists.txt` auf `project(... VERSION 1.3.0)`.
2. `cmake-xray --version` liefert im finalen Release-Kontext
   `1.3.0` und im expliziten Pre-Release-Kontext den gesetzten
   Prerelease-Suffix.
3. CHANGELOG `[1.3.0]`-Sektion mit Added/Changed/Fixed/Migration/
   Platform status.
4. Versionspin-Konsistenztest.

Innerhalb von **A.2 (Format-Vertrags-Final-Sweep und docs/examples/)**:

5. Format-Vertrags-Dokumente konsolidieren, Kreuzverweise
   nachziehen.
6. `docs/examples/` mit M6-Beispielen erweitern.
7. `docs/examples/manifest.txt` und `generation-spec.json`
   aktualisieren.
8. `validate_doc_examples.py` gegen aktuellen Generator-Output
   gruen.

Innerhalb von **A.3 (README, guide, quality, performance, roadmap)**:

9. `README.md` mit M6-Beispielen.
10. `docs/user/guide.md` mit M6-Praxisrezepten.
11. `docs/user/quality.md` mit M6-Testumfang.
12. `docs/user/performance.md` mit M6-Baseline.
13. `docs/planning/roadmap.md` mit M6 als abgeschlossen.

Innerhalb von **A.4 (Audit-Pass und Liefer-Stand-Konsolidierung)**:

14. Plan-Test-Listen aus AP 1.1-1.7 gegen Ist-Stand verifizieren.
15. Konsolidierter Liefer-Stand in `docs/planning/plan-M6.md`.
16. Docker-Gates ausfuehren.
17. Liefer-Stand-Block in diesem Dokument aktualisieren.

## Tranchen-Schnitt

AP 1.7 wird in vier Sub-Tranchen geliefert:

- **A.1 Versionspin und CHANGELOG**: Root-`CMakeLists.txt` auf
  `1.3.0`, CHANGELOG-Eintrag, Versionspin-Konsistenztest.
- **A.2 Format-Vertrags-Final-Sweep und docs/examples/**: Konsistenz-
  Pruefung der Format-Vertrags-Dokumente, neue Beispiele, Drift-
  Gate gruen.
- **A.3 README, guide, quality, performance, roadmap**: Nutzer-
  und Entwickler-Doku synchronisieren.
- **A.4 Audit-Pass und Liefer-Stand-Konsolidierung**: Plan-Test-
  Verifikation, gemeinsame Liefer-Stand-Tabelle, Docker-Gates.

Begruendung: A.1 ist hart an den Versionssprung gebunden und
sollte als erstes landen, damit alle Folge-Tranchen gegen `1.3.0`
verifizieren koennen. A.2 ist Format-Doku-zentriert und unabhaengig
von den Nutzer-Dokumenten. A.3 baut auf A.2 auf, weil
README/Guide auf die finalen Format-Vertraege verweisen. A.4 ist
der Audit-Schluss.

**Harte Tranchen-Abhaengigkeit zu allen vorherigen APs**: AP 1.7
darf nicht starten, bevor die Liefer-Stand-Bloecke der Sub-Plaene
`docs/planning/plan-M6-1-1.md` bis `docs/planning/plan-M6-1-6.md` mit Commit-Hashes
gepinnt sind. Das Gate ist maschinenlesbar: `m6_versionspin_consistency`
prueft die konsolidierte Tabelle in `docs/planning/plan-M6.md` und die
Sub-Plan-Liefer-Stand-Bloecke auf nicht-leere Commit-Felder.

## Liefer-Stand

Wird nach dem Schnitt der A-Tranchen mit Commit-Hashes befuellt.
Bis dahin ist AP 1.7 nicht abnahmefaehig.

- A.1 (Versionspin und CHANGELOG): **lokal umgesetzt**.
  Root-`CMakeLists.txt` meldet `1.3.0`; `CHANGELOG.md` enthaelt den
  `[1.3.0]`-Abschnitt mit Added/Changed/Fixed/Migration/Platform status.
  `m6_versionspin_consistency` prueft Release-Version, Analyze-/Compare-
  Formatkonstanten, Schema-Konstanz und verbliebene Produktions-Code-
  Vorkommen der Rohversion `1.2.0`; `m6_prerelease_version_context`
  pinnt den expliziten Prerelease-Kontext `1.3.0-rc.1`. Lokaler
  Testlauf gruen: `make docker-test` (47/47), `make docs-check`,
  `git diff --check`. Commit: `821a84f`.
- A.2 (Format-Vertrags-Final-Sweep und docs/examples/): noch nicht
  ausgeliefert.
- A.3 (README, guide, quality, performance, roadmap): noch nicht
  ausgeliefert.
- A.4 (Audit-Pass und Liefer-Stand-Konsolidierung): noch nicht
  ausgeliefert.

## Abnahmekriterien

AP 1.7 ist abgeschlossen, wenn:

- Root-`CMakeLists.txt` meldet exakt `project(... VERSION 1.3.0)`;
- `cmake-xray --version` liefert im finalen Release-Kontext exakt
  `1.3.0`; ein expliziter Pre-Release-Build mit
  `XRAY_APP_VERSION=1.3.0-rc.1` liefert exakt `1.3.0-rc.1`;
- `kReportFormatVersion == M6_ANALYZE_FORMAT_VERSION` und
  `kCompareFormatVersion == M6_COMPARE_FORMAT_VERSION` in C++ und
  Schema;
- `CHANGELOG.md` enthaelt einen finalen `[1.3.0]`-Abschnitt mit
  Added/Changed/Fixed/Migration/Platform-status;
- `README.md` und `docs/user/guide.md` enthalten M6-Beispiele und
  -Praxisrezepte ohne tote Verweise;
- `docs/user/quality.md` listet den M6-Testumfang vollstaendig;
- `docs/user/performance.md` enthaelt eine M6-Baseline fuer
  Target-Graph-Extraktion, Reverse-BFS und Compare;
- `docs/planning/roadmap.md` markiert M6 als abgeschlossen;
- `docs/planning/plan-M6.md` enthaelt den konsolidierten Liefer-Stand
  ueber alle sieben APs;
- alle Format-Vertrags-Dokumente
  (`report-json.md`, `report-json.schema.json`, `report-dot.md`,
  `report-html.md`, `report-compare.md`) sind konsistent zum Ist-Stand;
- `spec/report-compare.schema.json` und `spec/compare-matrix.md`
  existieren, sind gegen den finalen Ist-Stand validiert und enthalten
  keine offenen AP-1.6-Platzhalter;
- `docs/examples/` ist um M6-Beispiele erweitert und das
  Drift-Gate `doc_examples_validation` ist gruen; Compare-Beispiele
  sind dabei auf `txt`/Console, Markdown und JSON begrenzt;
- `m6_versionspin_consistency`-Test ist gruen;
- alle anderen CTest-Gates aus M5 und AP 1.1-1.6 sind gruen;
- der Docker-Gate-Lauf gemaess `docs/user/quality.md` ist gruen;
- `git diff --check` sauber.

## Offene Folgearbeiten

Diese Punkte werden bewusst in spaeteren Phasen umgesetzt:

- Tatsaechliche Release-Veroeffentlichung von `v1.3.0` (Tag,
  GitHub Release, OCI-Image-Push). AP 1.7 bereitet vor; das
  Tagging laeuft ueber den bestehenden Release-Workflow aus M5
  AP 1.6.
- Plattform-Hochstufung von macOS oder Windows ueber den M5-Stand
  hinaus. M7 oder spaeter.
- Konfigurationsdatei (`cmake-xray.toml` o. ae.) fuer
  wiederkehrende Analyseprofile (offen aus AP 1.5).
- Cross-Mode-Compare zwischen File-API und Compile-DB-Fallback
  (offen aus AP 1.6).
- Impact-Compare als eigener Diff-Vertrag (offen aus AP 1.6).
- HTML- und DOT-Compare-Output (offen aus AP 1.6).
