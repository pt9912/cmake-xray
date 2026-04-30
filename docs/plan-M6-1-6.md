# Plan M6 - AP 1.6 Vergleich zweier Analysezeitpunkte einfuehren

## Ziel

`F-11` aus dem Lastenheft verlangt einen Vergleich zweier
Analysezeitpunkte. AP 1.6 implementiert das als
`cmake-xray compare`-Subkommando, das zwei stabile
Analyze-JSON-Berichte (von AP 1.2 bis AP 1.5 standardisierte
maschinenlesbare Reportbasis) gegenueberstellt und Aenderungen an
Build-Komplexitaet, Include-Hotspots, Target-Struktur und
Konfiguration sichtbar macht.

Konkret:

- `cmake-xray compare --baseline <a.json> --current <b.json>` liest
  beide JSON-Berichte, validiert sie, prueft Projektidentitaet, und
  produziert einen strukturierten Diff in Console, Markdown oder
  JSON.
- Compare arbeitet auf stabilen Identitaeten und Vergleichsschluesseln,
  nicht auf rohen Anzeigezeilen.
- Diagnostics machen Konfigurations-Drift, Datenverfuegbarkeits-Drift
  und Projektidentitaets-Drift sichtbar, ohne den Vergleich
  abzubrechen, sofern die Eingaben grundsaetzlich vergleichbar sind.
- Ein eigener JSON-Output-Vertrag (`format: cmake-xray.compare`) macht
  Compare-Ergebnisse maschinenlesbar reproduzierbar und CI-tauglich.

AP 1.6 ist `analyze`-only. Impact-Berichte werden vom
`compare`-Subkommando aktiv abgelehnt; Impact-Compare ist ein
expliziter Folgeumfang nach M6.

AP 1.6 fuehrt einen **eigenen** Format-Versions-Counter
`xray::hexagon::model::kCompareFormatVersion` mit Initialwert `1` ein,
unabhaengig von `kReportFormatVersion` aus AP 1.2-1.5. Damit kann der
Compare-Vertrag in spaeteren APs unabhaengig vom Analyze-/Impact-
Vertrag versioniert werden.

## Scope

Umsetzen:

- Neues CLI-Subkommando `compare` mit `--baseline`, `--current`,
  `--format`, `--output`, `--allow-project-identity-drift`.
- JSON-Reader fuer Analyze-JSON in der Adapter-Schicht (mit Schema-
  Validierung und Versions-Pruefung).
- Compare-Service im Hexagon mit Diff-Logik fuer:
  - Summary-Kennzahlen
  - TU-Ranking-Eintraege (added/removed/changed)
  - Include-Hotspots (added/removed/changed)
  - Target-Graph-Knoten und -Kanten (added/removed/changed)
  - Target-Hubs (added/removed/changed)
- `project_identity`-Vertrag in der bestehenden Analyze-JSON-
  Serialisierung (siehe AP-1.6-Ergaenzung an `report-json.md`).
- Compile-DB-Fingerprint-Algorithmus (SHA-256 ueber kanonische
  JSON-Payload).
- Compare-Kompatibilitaetsmatrix in `docs/report-json.md` und
  in einer neuen Datei `docs/compare-matrix.md` oder als Sektion in
  `docs/report-json.md`.
- Pfadnormalisierungs-Helper fuer Compare-Schluessel.
- Diff-Result-Modell und Diagnostik: drei serialisierte
  Output-Diagnostics (`configuration_drift`, `data_availability_drift`,
  `project_identity_drift`) plus CLI-Fehlerfall
  `incompatible_format_version`.
- Console-, Markdown- und JSON-Ausgabe fuer Compare-Ergebnisse.
- Eigener JSON-Output-Vertrag mit
  `format: cmake-xray.compare`,
  `format_version=1`,
  `report_type: compare`.
- Schema-Datei `docs/report-compare.schema.json` (oder neue Sektion
  in `report-json.schema.json`).
- Goldens fuer Console, Markdown und JSON.

Nicht umsetzen:

- HTML- und DOT-Ausgabe fuer `compare`. Beide sind in M6 explizit
  ausgeschlossen; HTML braucht ein neues UX-Konzept fuer
  Vergleichsdarstellung, DOT braucht einen Differenzgraphen-Vertrag.
- Vergleich von Impact-JSON-Berichten. Impact-Compare ist ein
  Folgeumfang mit eigenen Schluessel-, Versions- und Diff-Regeln
  fuer Impact-spezifische Felder.
- Cross-Mode-Vergleich zwischen File-API-`project_identity` und
  `fallback_compile_database_fingerprint`. Beide Modi haben
  unterschiedliche Provenienzgarantien; Cross-Mode-Compare ist
  Folgeumfang.
- Migration von v1-v4-Goldens fuer `compare`. Compare unterstuetzt in
  M6 ausschliesslich `kReportFormatVersion=5` (Stand nach AP 1.5);
  aeltere Versionen werden aktiv abgelehnt.
- Diff-Goldens unter `m5/`. Compare-Goldens leben ausschliesslich
  unter `m6/compare-reports/`.
- CLI-Optionen fuer Diff-Filterung (z. B. `--diff-only-added`).
  Konsumenten filtern den maschinenlesbaren Output selbst.

Diese Punkte folgen in spaeteren Arbeitspaketen oder bleiben bewusst
ausserhalb von M6.

## Voraussetzungen aus AP 1.1-1.5

AP 1.6 baut auf folgenden Vertraegen auf:

- Stabiler Analyze-JSON-Vertrag aus AP 1.2-1.5
  (`format: cmake-xray.analysis`, `format_version=5`).
- `target_graph`/`target_graph_status`/`target_hubs` aus AP 1.1-1.2.
- `include_filter`-Block, `IncludeOrigin`/`IncludeDepthKind`-Felder
  pro Hotspot aus AP 1.4.
- `analysis_configuration`/`analysis_section_states` aus AP 1.5.
- `kReportFormatVersion=5` (Pflicht-Stand nach AP 1.5).

AP 1.6 ergaenzt den bestehenden Analyze-JSON-Vertrag um:

- `inputs.project_identity` (string).
- `inputs.project_identity_source` (string-enum:
  `cmake_file_api_source_root` oder
  `fallback_compile_database_fingerprint`).

Diese beiden Felder sind in v5 Pflichtfelder; ohne sie kann AP 1.6
keine Compare-Eingangsvalidierung durchfuehren. Damit besteht eine
**harte Tranchen-Reihenfolge**: AP-1.6-A.1 (project_identity in
Analyze-JSON) muss in derselben oder einer vorhergehenden Tranche
landen wie AP-1.5-A.3 (Schema v5), oder AP 1.5 nimmt
`project_identity` direkt mit auf. **Empfehlung**: AP 1.5 ergaenzt
das Schema um die `project_identity`-Felder (sie sind ohnehin
v5-konsistent), und AP 1.6-A.1 nutzt sie ohne weitere
Schemaaenderung.

In diesem Plan wird davon ausgegangen, dass AP 1.5 die Felder
`inputs.project_identity` und `inputs.project_identity_source` schon
mit aufnimmt. Falls AP 1.5 das nicht tut, hebt AP 1.6 stattdessen
`kReportFormatVersion` von `5` auf `6`. Dieser Pfad wird im
**Tranchen-Schnitt**-Abschnitt unten ausdruecklich beschrieben.

## Dateien

Voraussichtlich zu aendern:

- `src/adapters/cli/cli_adapter.{h,cpp}` (neues `compare`-
  Subkommando)
- `src/main.cpp`
- `src/hexagon/model/analysis_result.h` (`project_identity`-Felder,
  falls AP 1.5 sie nicht aufgenommen hat)
- `src/hexagon/model/report_inputs.h`
  (`project_identity`-Provenienz, falls als ReportInputs-Erweiterung
  modelliert)
- `src/hexagon/services/project_analyzer.{h,cpp}` (project_identity
  setzen)
- `src/hexagon/services/analysis_support.{h,cpp}` (Pfadnormalisierung
  fuer Compare)
- `src/adapters/output/json_report_adapter.cpp` (project_identity
  serialisieren)
- `src/adapters/cli/cli_adapter.{h,cpp}` (Subkommando)
- `docs/report-json.md`
- `docs/report-json.schema.json`
- `tests/adapters/test_json_report_adapter.cpp`
- `tests/hexagon/test_project_analyzer.cpp`
- `tests/e2e/test_cli.cpp`
- `tests/e2e/testdata/m6/compare-reports/manifest.txt`
- `docs/examples/manifest.txt`
- `docs/examples/generation-spec.json`

Neue Dateien:

- `src/hexagon/model/compare_result.h`
- `src/hexagon/services/compare_service.{h,cpp}`
- `src/hexagon/ports/driving/compare_analysis_port.h`
- `src/adapters/input/analysis_json_reader.{h,cpp}` (JSON-Reader
  fuer Analyze-Reports)
- `src/adapters/output/console_compare_adapter.{h,cpp}`
- `src/adapters/output/markdown_compare_adapter.{h,cpp}`
- `src/adapters/output/json_compare_adapter.{h,cpp}`
- `src/hexagon/services/compile_db_fingerprint.{h,cpp}` (Hash-
  Berechnung)
- `src/hexagon/services/compare_path_normalization.{h,cpp}` (oder
  Wiederverwendung in `analysis_support`)
- `docs/report-compare.md` (eigener Compare-Output-Vertrag) oder
  Compare-Sektion in `docs/report-json.md`
- `docs/report-compare.schema.json` (eigenes Compare-Schema)
- `docs/compare-matrix.md` (Compatibility-Matrix)
- `tests/hexagon/test_compare_service.cpp`
- `tests/hexagon/test_compile_db_fingerprint.cpp`
- `tests/adapters/test_analysis_json_reader.cpp`
- `tests/adapters/test_console_compare_adapter.cpp`
- `tests/adapters/test_markdown_compare_adapter.cpp`
- `tests/adapters/test_json_compare_adapter.cpp`
- `tests/e2e/testdata/m6/compare-reports/<tier>/baseline-*.json`
  (Eingangsdaten fuer Compare-Tests)
- `tests/e2e/testdata/m6/compare-reports/<format>/manifest.txt`

## Format-Versionierung

AP 1.6 fuehrt einen **eigenen** Versions-Counter ein:

```cpp
namespace xray::hexagon::model {

inline constexpr int kCompareFormatVersion = 1;

}  // namespace xray::hexagon::model
```

Begruendung:

- Compare ist ein neuer Reporttyp mit eigener Daten-Struktur,
  unabhaengig von Analyze und Impact.
- Ein eigener Counter laesst Compare in Zukunft versionieren, ohne
  jedes Mal Analyze und Impact mitzuziehen.
- `format: cmake-xray.compare` traegt `format_version=1` aus
  `kCompareFormatVersion`, identisch zur Analyze-/Impact-Konvention.
- AP 1.6 traegt **keinen** Sprung an `kReportFormatVersion` ein, sofern
  `project_identity` schon in AP 1.5 v5 enthalten ist.

Begruendung gegen einen `kReportFormatVersion`-Sprung in AP 1.6
(falls `project_identity` schon in AP 1.5 enthalten):

- Compare aendert die Analyze-JSON-Struktur nicht.
- Compare braucht `project_identity` als bestehendes Feld, fuegt
  aber kein neues Pflichtfeld an Analyze-JSON hinzu.
- Wenn AP 1.5 `project_identity` nicht aufnimmt, hebt AP 1.6
  stattdessen `kReportFormatVersion` von `5` auf `6` und nimmt die
  Felder selbst auf. Die Abnahmekriterien decken beide Pfade ab.

## CLI-Vertrag

### `cmake-xray compare`

Neues Subkommando neben `analyze` und `impact`. Optionen:

- `--baseline <path>`: Pflicht. Pfad zu einem Analyze-JSON-Bericht.
- `--current <path>`: Pflicht. Pfad zu einem Analyze-JSON-Bericht.
- `--format <value>`: Wert `console` (Default), `markdown` oder
  `json`. `html` und `dot` sind ausdruecklich nicht erlaubt.
- `--output <path>`: Optional. Nur fuer `--format markdown` und
  `--format json` zulaessig. `--output` mit `--format console` ist
  CLI-Verwendungsfehler.
- `--allow-project-identity-drift`: Flag. Aktiviert den Same-Mode-
  Compile-DB-Fallback-Drift-Pfad (siehe project_identity-Vertrag
  unten).

### Fehlerphrasen

CLI-Verwendungsfehler (Exit-Code `2`, Text auf `stderr`, kein
stdout-Report, keine Zieldatei):

- `"compare: --baseline is required"` bei fehlendem `--baseline`.
- `"compare: --current is required"` bei fehlendem `--current`.
- `"compare: --format <value> is not supported; allowed: console, markdown, json"`
  bei `--format html` oder `--format dot`.
- `"compare: --output is not allowed with --format console"`
  bei dieser Kombination.
- `"compare: --output requires a value"` bei leerem `--output`.
- Allgemeine Pfad-Validierungsfehler aus M5
  (Verzeichnis-statt-Datei, fehlende Lese-Berechtigung) bleiben
  unveraendert.

Eingabefehler (Exit-Code `1`, kein Compare-Report, kein
Teil-Output):

- `"compare: cannot read baseline file '<path>'"` bei IO-Fehler.
- `"compare: cannot read current file '<path>'"` bei IO-Fehler.
- `"compare: baseline is not valid JSON"` bei JSON-Parser-Fehler.
- `"compare: current is not valid JSON"` bei JSON-Parser-Fehler.
- `"compare: baseline does not match the analyze JSON schema"`
  bei Schema-Validierungs-Fehler.
- `"compare: current does not match the analyze JSON schema"`
  bei Schema-Validierungs-Fehler.
- `"compare: baseline has report_type='<type>'; only 'analyze' is supported"`
  insbesondere bei `report_type=impact`.
- `"compare: current has report_type='<type>'; only 'analyze' is supported"`
  analog.
- `"compare: format_version combination (<bv>, <cv>) is not in the compatibility matrix"`
  bei nicht dokumentierter Versions-Kombination (siehe
  Compare-Kompatibilitaetsmatrix).
- `"compare: project identity source mismatch (baseline=<bs>, current=<cs>)"`
  bei abweichender `project_identity_source`.
- `"compare: invalid project identity source '<value>'"`
  bei nicht erkanntem `project_identity_source`-Wert.
- `"compare: project identity is empty or unrecoverable"`
  bei leerer/nicht-belastbarer `project_identity`.
- `"compare: project identity differs (baseline='<bp>', current='<cp>'); pass --allow-project-identity-drift to override (only valid for fallback_compile_database_fingerprint)"`
  bei abweichender `project_identity` ohne Drift-Option.

Validierungsreihenfolge:

1. Datei lesen.
2. JSON parsen.
3. `report_type` und `format_version` lesen und die
   Kompatibilitaetsmatrix pruefen.
4. Analyze-JSON-Schema validieren, aber die Felder
   `inputs.project_identity` und `inputs.project_identity_source`
   zusaetzlich fachlich pruefen, damit die spezifischen
   `invalid project identity source`-/`project identity is empty`-
   Fehlerphrasen erreichbar und stabil bleiben.
5. Projektidentitaets-Source, Identitaet und Drift-Regeln pruefen.

### Atomarer Schreibvertrag

`--output` fuer Compare folgt demselben atomaren Schreibvertrag wie
M5: temporaere Datei im Zielverzeichnis, exklusiv erstellt, atomarer
Replace; bei Fehler bleibt Zielartefakt unveraendert.

## `project_identity`-Vertrag

Das Analyze-JSON erhaelt zwei neue Pflichtfelder im `inputs`-Block:

| Feld | Typ | Pflicht | Werte |
| --- | --- | --- | --- |
| `project_identity` | string | ja | nicht leer; siehe Bildungsregeln. |
| `project_identity_source` | string | ja | `cmake_file_api_source_root` oder `fallback_compile_database_fingerprint`. |

### Bildungsregeln fuer `project_identity`

**Modus File-API** (`project_identity_source=cmake_file_api_source_root`):

- Verwendet die normalisierte File-API-Source-Root als
  Projektidentitaet.
- Wert-Format: roher normalisierter Pfad, z. B. `/repo/cmake-xray`.
- Pfadnormalisierung: lexikalisch (Backslashes zu `/`,
  redundante `.`-Segmente entfernt, kein Symlink-Resolve).
- Bedingung fuer Verfuegbarkeit: `BuildModelResult::source_root` ist
  nicht leer und stammt aus einer File-API-Reply.

**Modus Compile-DB-Fallback** (`project_identity_source=fallback_compile_database_fingerprint`):

- Verwendet einen deterministischen SHA-256-Hash ueber die
  normalisierten relativen `source_path`-Werte aller Translation
  Units.
- Wert-Format: `compile-db:<64 lowercase hex chars>`.
- Bedingung fuer Verfuegbarkeit: keine File-API-Source-Root, aber
  mindestens eine TU mit normalisierbarem `source_path`.

**Wenn keiner der beiden Modi belastbar bildbar ist**, wird der
Bericht beim Compare-Aufruf abgelehnt:

- Compile-DB-Fallback ohne Source-Paths → Fehlermeldung
  `"compare: project identity is empty or unrecoverable"`.
- Compile-DB-Fallback-Hash, der bei Reproduktion einen anderen Wert
  liefert (Defekt) → gleiche Fehlermeldung.

### Same-Mode-Anforderung

Beide Eingangsberichte muessen denselben
`project_identity_source`-Wert tragen. Cross-Mode-Vergleiche zwischen
File-API und Compile-DB-Fallback werden in M6 abgelehnt mit
Fehlermeldung `"compare: project identity source mismatch ..."`.

### Identity-Drift im Same-Mode

- Bei `cmake_file_api_source_root`: Abweichende Identitaet ist
  immer ein harter Fehler ohne Drift-Option. File-API-basierte
  Source-Roots sind stabil.
- Bei `fallback_compile_database_fingerprint`: Abweichende Identitaet
  ist normalerweise ein harter Fehler. Mit
  `--allow-project-identity-drift` wird der Vergleich fortgesetzt
  und ein `project_identity_drift`-Diagnostic ausgegeben (siehe
  unten).

## Compile-DB-Fingerprint-Algorithmus

Der Hash basiert auf einer kanonisch serialisierten JSON-Payload:

```json
{
  "kind": "cmake-xray.compile-db-project-identity",
  "version": 1,
  "source_paths": ["src/foo.cpp", "src/bar.cpp"]
}
```

Regeln:

1. **Normalisierung der Source-Paths**:
   - Backslashes zu `/`.
   - Redundante `.`-Segmente entfernt.
   - `..`-Segmente werden NICHT aufgeloest, weil hier keine
     Projektwurzel-Basis verfuegbar ist; ein `..` im Pfad bleibt
     erhalten, was bei korrekt gebauten Compile-DBs nicht vorkommt.
   - Keine Symlink-Aufloesung.
   - Trailing Slashes entfernt.
   - Windows-Drive-Letters lowercase.
   - Pfade werden NICHT case-gefoldet (POSIX bleibt case-sensitiv).
2. **Deduplizierung**: Identische Pfade werden zusammengezogen.
3. **Sortierung**: lexikografisch aufsteigend nach UTF-8-Byte-Folge.
4. **Kanonische JSON-Payload**:
   - Objekt-Schluessel exakt in der Reihenfolge `kind`, `version`,
     `source_paths`.
   - String-Escaping nach RFC 8259.
   - Keine unnoetigen Whitespace-Zeichen (kompakte Form, kein
     Newline, kein Indent).
5. **Hash-Berechnung**: SHA-256 ueber die UTF-8-Bytes der kanonischen
   JSON-Payload. Ausgabe als 64 lowercase Hex-Zeichen.
6. **Final-Wert**: `compile-db:<hash>`.

Akzeptierter Trade-off:

- Unterschiedliche Build-Kontexte mit identischem normalisiertem
  `source_path`-Satz erhalten denselben `project_identity`-Wert.
  Fachliche Unterschiede aus Build-Kontexten erscheinen erst ueber
  TU-Vergleichsschluessel und werden dort als Diff-Eintraege
  sichtbar.
- Diese Einschraenkung ist in M6 akzeptiert und wird in
  Abnahmetests explizit gepinnt.

## Compare-Kompatibilitaetsmatrix

In M6 unterstuetzt `compare` genau die Analyze-JSON-Version, die nach
AP 1.6 das `project_identity`-Feld enthaelt. Wenn AP 1.5
`project_identity` schon in `format_version=5` aufgenommen hat, ist nur
`(5, 5)` erlaubt. Wenn AP 1.6 A.1 dafuer auf `format_version=6` heben
muss, ist nur `(6, 6)` erlaubt. Gemischte Versionspaare bleiben
abgelehnt.

| baseline_format_version | current_format_version | Status | Hinweise |
|---|---|---|---|
| 5 | 5 | OK, falls `project_identity` in v5 enthalten ist | Vollstaendiger Vergleich aller M6-Sektionen. |
| 6 | 6 | OK, falls AP 1.6 A.1 Analyze-JSON auf v6 hebt | Vollstaendiger Vergleich aller M6-Sektionen. |
| 5 | 6 | abgelehnt | Keine Cross-Version-Migration in M6. |
| 6 | 5 | abgelehnt | Keine Cross-Version-Migration in M6. |
| 1, 2, 3, 4 | beliebig | abgelehnt | Aeltere Versionen werden in M6 nicht produziert; Migration erforderlich. |
| beliebig | 1, 2, 3, 4 | abgelehnt | gleicher Grund. |
| 7+ | beliebig | abgelehnt | Zukuenftige Schemaversion; Compare braucht eine erweiterte Matrix. |
| beliebig | 7+ | abgelehnt | gleicher Grund. |

Die Matrix lebt sowohl in `docs/report-json.md` als auch in einer
neuen Datei `docs/compare-matrix.md`, mit Pflichtinhalten pro
erlaubter Versionskombination:

- Pflichtfelder, optionale Felder.
- Default-/Null-Mapping fuer fehlende Felder.
- Umbenannte Felder (in M6 keine).
- Entfernte Felder (in M6 keine).
- Nicht vergleichbare Felder (Impact-Felder werden in jedem Fall
  ausgeschlossen, weil Compare analyze-only ist). Cross-/Mixed-Mode-
  Eingaben erreichen diese Feld-Mapping-Logik in M6 nicht, weil sie
  vorher als `project_identity_source`-Mismatch abgelehnt werden.

Pflicht: AP-1.6-Implementierung darf nicht starten, bevor die
Matrix fuer die `(5, 5)`-Kombination dokumentiert und mit Schema-
und Golden-Tests abgesichert ist.

## Pfadnormalisierung fuer Compare

Pfadbasierte Vergleichsschluessel werden vor dem Vergleich
normalisiert, um Workspace-/Build-/Temp-Praefixe und
Plattform-Unterschiede unsichtbar zu machen:

1. **Relativisierung**: Pfade werden relativ zur im jeweiligen
   JSON dokumentierten Projekt-/Source-Root normalisiert, wenn
   `project_identity_source=cmake_file_api_source_root` ist.
   Bei `project_identity_source=fallback_compile_database_fingerprint`
   gibt es keine belastbare Source-Root; dann werden Pfade relativ zum
   laengsten gemeinsamen Verzeichnis-Praefix des jeweiligen Reports
   normalisiert. Der Praefix wird aus allen TU-`source_path`-Werten und
   allen absoluten Header-/Target-Pfaden gebildet, segment-basiert und
   nach Backslash-Normalisierung. Existiert kein gemeinsamer
   Verzeichnis-Praefix mit mindestens einem Segment, bleiben Pfade
   lexikalisch normalisierte Anzeige-Pfade. Absolute Workspace-, Build-
   und Temp-Praefixe duerfen nicht zu fachlichen Diffs fuehren.
2. **Backslashes zu `/`**.
3. **Redundante `.`-Segmente entfernt**.
4. **`..`-Segmente nur innerhalb der bekannten Projekt-/Source-
   Root-Basis aufgeloest**. Ein `..`, das nach Aufloesung ueber die
   Basis hinausginge, bleibt im Pfad.
5. **Trailing slash entfernt**.
6. **Windows-Drive-Letters lowercase**.
7. **Keine Symlink- oder Realpath-Aufloesung**.
8. **Pfadvergleich case-sensitiv**. Eine spaetere plattformspezifische
   Case-Folding-Option waere ein eigener Vertragsumfang, weil sie
   fachliche Namenskollisionen verdecken kann.

## Vergleichsidentitaet pro Diff-Gruppe

Compare baut Identitaeten aus fachlichen Schluesseln, nicht aus
rohen Anzeigezeilen.

| Diff-Gruppe | Vergleichs-Identitaet |
|---|---|
| Translation Units | `(normalized_source_path, normalized_build_context_key)`. `normalized_source_path` ist der gemaess Pfadnormalisierung relativisierte Pfad; `normalized_build_context_key` ist der `directory`-Wert nach derselben Normalisierung. |
| Include-Hotspots | `(normalized_header_display_path, include_origin, include_depth_kind)`. Damit werden mixed-Hotspots von rein-direkten/rein-indirekten Hotspots unterschieden. |
| Target-Knoten | `unique_key` aus `target_graph.nodes`. |
| Target-Kanten | `(from_unique_key, to_unique_key, kind)`. |
| Target-Hubs | `(unique_key, direction)` mit `direction in {inbound, outbound}`. |

`include_origin` und `include_depth_kind` als Teil der Hotspot-
Identitaet ist eine bewusste Designentscheidung: ein Header, der von
`project, direct` zu `project, mixed` wechselt, wird dann als ein
Eintrag entfernt und ein Eintrag hinzugefuegt verzeichnet, statt als
`changed`. Das macht den Klassen-Wechsel sichtbar; wer beide
Klassen-Status mergen will, kann auf der `header_path`-Ebene
nachfiltern.

## Diff-Logik

Pro Diff-Gruppe drei Faelle:

- `added`: Identitaet existiert in `current`, nicht in `baseline`.
- `removed`: Identitaet existiert in `baseline`, nicht in `current`.
- `changed`: Identitaet existiert in beiden, aber mindestens ein
  vergleichswertiges Feld unterscheidet sich.

Vergleichswertige Felder pro Diff-Gruppe:

- **Translation Units**: `metrics.arg_count`, `metrics.include_path_count`,
  `metrics.define_count`, `metrics.score`, `targets`-Liste,
  `diagnostics`-Liste. Aenderungen am `rank` werden NICHT als
  `changed` gefuehrt, weil Rang-Aenderungen oft Konsequenz
  anderer Aenderungen sind und nicht eigenstaendig fachlich
  relevant.
- **Include-Hotspots**: `affected_total_count`, Liste der
  betroffenen TU-Referenzen (Identitaeten), `diagnostics`-Liste.
- **Target-Knoten**: `display_name`, `type`. (Beide sind
  Anzeige-Daten; Aenderungen sind selten, aber moeglich, wenn
  `unique_key`-Wertformel sich aendert oder ein Target unbenannt
  wird.)
- **Target-Kanten**: `resolution` (`resolved`/`external`),
  `from_display_name`, `to_display_name`.
- **Target-Hubs**: keine vergleichswertigen Felder ausser der
  Identitaet selbst; `added` oder `removed` sind die einzigen
  realistischen Faelle. Eine TargetHub-Aenderung kann sich also
  semantisch nur als added/removed darstellen. Das JSON-Schema enthaelt
  trotzdem `target_hubs.changed` als Pflichtschluessel, damit alle
  Diff-Gruppen dieselbe `added`/`removed`/`changed`-Form behalten; der
  Wert ist in M6 immer ein leeres Array.

Listen-Normalisierung:

- Listenfelder werden vor dem `changed`-Vergleich als ungeordnete
  Mengen behandelt und in kanonischer Reihenfolge verglichen.
- `translation_units[].targets`: sortiert nach normalisiertem
  Target-Identitaetsschluessel.
- `translation_units[].diagnostics` und
  `include_hotspots[].diagnostics`: sortiert nach
  `(severity, code/message, normalized_location)`; gleiche Eintraege
  werden dedupliziert.
- `include_hotspots[].affected_translation_units`: sortiert nach
  normalisiertem TU-Vergleichsschluessel.
- Reine Reihenfolgeaenderungen in diesen Listen erzeugen keinen
  `changed`-Diff und duerfen die Ausgabe-Reihenfolge nicht
  nondeterministisch machen.

Sortierung der Diff-Eintraege:

- Reihenfolge der Diff-Gruppen: `added`, `removed`, `changed`.
- Innerhalb einer Gruppe: nach Vergleichs-Identitaet, dann nach
  Anzeige-Name.

## Diagnostic-Vertrag fuer Compare

Compare-Diagnostics sind reicher als Analyze-Diagnostics, weil sie
mehr strukturelle Informationen brauchen. Drei Diagnostic-Typen werden
im Compare-Output serialisiert:

### `configuration_drift`

Felder:

| Feld | Typ | Pflicht | Werte |
| --- | --- | --- | --- |
| `field` | string | ja | Punkt-getrennte Pfad-Notation, z. B. `analysis_configuration.tu_thresholds.include_path_count`. |
| `baseline_value` | JSON value | ja | Wert in baseline; erlaubt sind string, integer, boolean, null und Arrays dieser skalaren Werte. |
| `current_value` | JSON value | ja | Wert in current; gleicher Typumfang wie `baseline_value`. |
| `severity` | string | ja | `warning` in M6. |
| `ci_policy_hint` | string | ja | `review_required` in M6. |

Erkannte Drift-Felder:

- `analysis_configuration.analysis_sections`
- `analysis_configuration.tu_thresholds.<metric>` (pro Metrik)
- `analysis_configuration.min_hotspot_tus`
- `analysis_configuration.target_hub_in_threshold`
- `analysis_configuration.target_hub_out_threshold`
- `include_filter.include_scope`
- `include_filter.include_depth`

Verhalten:

- `configuration_drift` ist KEIN Abbruchgrund.
- `severity=warning` und `ci_policy_hint=review_required` markieren
  den Vergleich fuer CI als "soll nochmal angeschaut werden", ohne
  dass `compare` selbst nonzero endet.

### `data_availability_drift`

Felder:

| Feld | Typ | Pflicht | Werte |
| --- | --- | --- | --- |
| `section` | string | ja | `target_graph` oder `target_hubs`. |
| `baseline_state` | string | ja | `loaded`, `partial`, `not_loaded`, `disabled`. |
| `current_state` | string | ja | `loaded`, `partial`, `not_loaded`, `disabled`. |

Verhalten:

- Ausgegeben, wenn `baseline_state != current_state`.
- Wenn nur eine Seite die Daten enthaelt, wird die betroffene
  Diff-Gruppe (Target-Graph- oder Target-Hubs-Diff) **nicht**
  berechnet; der Datenabfall bzw. -zuwachs ist nur ueber das
  Diagnostic sichtbar.
- KEIN Abbruchgrund.

### `project_identity_drift`

Felder:

| Feld | Typ | Pflicht | Werte |
| --- | --- | --- | --- |
| `baseline_project_identity` | string | ja | Compile-DB-Hash der baseline. |
| `current_project_identity` | string | ja | Compile-DB-Hash der current. |
| `baseline_source_path_count` | integer | ja | Anzahl Source-Paths in baseline. |
| `current_source_path_count` | integer | ja | Anzahl Source-Paths in current. |
| `shared_source_path_count` | integer | ja | Anzahl gemeinsamer Source-Paths in beiden Berichten. |

Verhalten:

- Erscheint nur, wenn `--allow-project-identity-drift` aktiviert
  ist UND beide Berichte
  `project_identity_source=fallback_compile_database_fingerprint`
  haben UND `project_identity` sich unterscheidet.
- Die drei Source-Path-Zaehler basieren auf demselben normalisierten,
  deduplizierten und sortierten Source-Path-Satz wie der
  Compile-DB-Fingerprint. Rohpfad-Duplikate oder Schreibvarianten
  duerfen die Zaehler nicht erhoehen.
- KEIN Abbruchgrund (in diesem speziellen Pfad).

### CLI-Fehler `incompatible_format_version`

Wird in M6 als CLI-Eingabefehler vor Reporterzeugung behandelt
(siehe Fehlerphrasen oben). Es gibt kein Compare-Output mit dieser
Diagnostic; der Bericht wird abgelehnt. In Tests und
Abnahmekriterien wird dieser Fall deshalb als CLI-Fehlerfall, nicht als
serialisierter Diagnostic-Typ behandelt.

## Compare-Modell

```cpp
namespace xray::hexagon::model {

enum class CompareDiffKind {
    added,
    removed,
    changed,
};

struct CompareDiagnosticConfigurationDrift {
    std::string field;
    JsonValue baseline_value;  // string|integer|boolean|null|array
    JsonValue current_value;   // string|integer|boolean|null|array
    std::string severity{"warning"};
    std::string ci_policy_hint{"review_required"};
};

struct CompareDiagnosticDataAvailabilityDrift {
    std::string section;
    std::string baseline_state;
    std::string current_state;
};

struct CompareDiagnosticProjectIdentityDrift {
    std::string baseline_project_identity;
    std::string current_project_identity;
    std::size_t baseline_source_path_count;
    std::size_t current_source_path_count;
    std::size_t shared_source_path_count;
};

struct CompareInputs {
    std::string baseline_path;
    std::string current_path;
    int baseline_format_version;
    int current_format_version;
    std::optional<std::string> project_identity;  // shared oder null bei erlaubtem Drift
    std::string project_identity_source;
};

struct CompareSummary {
    int translation_units_added;
    int translation_units_removed;
    int translation_units_changed;
    int include_hotspots_added;
    int include_hotspots_removed;
    int include_hotspots_changed;
    int target_nodes_added;
    int target_nodes_removed;
    int target_nodes_changed;
    int target_edges_added;
    int target_edges_removed;
    int target_edges_changed;
    int target_hubs_added;
    int target_hubs_removed;
    int target_hubs_changed{0};  // in M6 immer 0
};

struct CompareResult {
    CompareInputs inputs;
    CompareSummary summary;
    // Diff-Listen pro Gruppe; jedes Item enthaelt Identitaet plus
    // alte/neue Feldwerte fuer changed.
    std::vector<TranslationUnitDiff> translation_unit_diffs;
    std::vector<IncludeHotspotDiff> include_hotspot_diffs;
    std::vector<TargetNodeDiff> target_node_diffs;
    std::vector<TargetEdgeDiff> target_edge_diffs;
    std::vector<TargetHubDiff> target_hub_diffs;
    // Diagnostics
    std::vector<CompareDiagnosticConfigurationDrift> configuration_drifts;
    std::vector<CompareDiagnosticDataAvailabilityDrift> data_availability_drifts;
    std::optional<CompareDiagnosticProjectIdentityDrift> project_identity_drift;
};

}  // namespace xray::hexagon::model
```

## Compare-Service

Neuer Driving Port `CompareAnalysisPort` im Hexagon:

```cpp
namespace xray::hexagon::ports::driving {

struct CompareAnalysisRequest {
    std::filesystem::path baseline_path;
    std::filesystem::path current_path;
    bool allow_project_identity_drift{false};
};

class CompareAnalysisPort {
public:
    virtual ~CompareAnalysisPort() = default;
    virtual model::CompareResult compare(CompareAnalysisRequest request) const = 0;
};

}  // namespace xray::hexagon::ports::driving
```

Service-Implementierung in `src/hexagon/services/compare_service.{h,cpp}`:

1. JSON-Reader laedt baseline und current ueber
   `AnalysisJsonReader` (Adapter; siehe unten).
2. Schema-Validierung (Reader-intern oder vorher).
3. project_identity-Validierung. Bei Mismatch ohne Drift-Option:
   Service wirft Eingabefehler (kein `CompareResult`).
4. Compare-Kompatibilitaetsmatrix-Lookup. Bei nicht erlaubter
   Versions-Kombination: Service wirft Eingabefehler.
5. Diff-Logik pro Gruppe.
6. Diagnostics-Sammlung.

Adapter `AnalysisJsonReader` in `src/adapters/input/analysis_json_reader.{h,cpp}`:

- Liest die Datei.
- Parst JSON.
- Validiert gegen das Analyze-Schema.
- Liefert ein adapter-spezifisches Datenmodell, das `CompareService`
  konsumieren kann (NICHT direkt `AnalysisResult`, weil das fuer
  reine Producer-Pfade gedacht ist).

## Compare-Output-Vertrag

### JSON

`format: cmake-xray.compare`, `format_version=1`,
`report_type: compare`.

Feldreihenfolge:

1. `format`
2. `format_version`
3. `report_type`
4. `inputs`
5. `summary`
6. `diffs`
7. `diagnostics`

`inputs`-Schema:

| Feld | Typ | Pflicht | Werte |
| --- | --- | --- | --- |
| `baseline_path` | string | ja | Anzeige-Pfad zur baseline-Datei. |
| `current_path` | string | ja | Anzeige-Pfad zur current-Datei. |
| `baseline_format_version` | integer | ja | Erlaubte Matrix-Version (`5` oder `6`, siehe Matrix). |
| `current_format_version` | integer | ja | Erlaubte Matrix-Version (`5` oder `6`, siehe Matrix). |
| `project_identity` | string\|null | ja | shared identity oder `null` bei erlaubtem `project_identity_drift`. |
| `project_identity_source` | string | ja | shared source. |

`summary`-Schema: alle 15 Zaehler aus `CompareSummary`.
`target_hubs_changed` ist aus Einheitlichkeitsgruenden Pflicht, bleibt
in M6 aber immer `0`, weil Hub-Diffs nur added/removed kennen.

`diffs`-Schema: ein Objekt mit fuenf Pflichtschluesseln
(`translation_units`, `include_hotspots`, `target_nodes`,
`target_edges`, `target_hubs`), jeder davon ein Objekt mit
`added`/`removed`/`changed` als Pflichtschluessel und Array-Werten.
`target_hubs.changed` ist aus Einheitlichkeitsgruenden Pflicht, bleibt
in M6 aber immer `[]`, weil Hub-Diffs nur added/removed kennen.

Diff-Item-Schemas pro Gruppe variieren je nach Identitaet (siehe
oben).

`diagnostics`-Schema: Objekt mit Pflichtschluesseln
`configuration_drifts`, `data_availability_drifts`,
`project_identity_drift` (optional). Erste zwei sind Arrays, dritte
ist Objekt oder `null`.

### Console

Beispiel:

```
cmake-xray compare
  baseline: /repo/baseline.json (v5)
  current:  /repo/current.json  (v5)
  project_identity: /repo (cmake_file_api_source_root)

Summary:
  Translation units: +3 -1 ~5
  Include hotspots:  +2 -0 ~1
  Target nodes:      +0 -0 ~0
  Target edges:      +1 -0 ~0
  Target hubs:       +0 -1

Translation Units (added):
  src/new_file.cpp [target: app::EXECUTABLE]

Translation Units (removed):
  src/old_file.cpp [target: legacy::STATIC_LIBRARY]

Translation Units (changed):
  src/foo.cpp:
    metrics.include_path_count: 3 -> 5
    targets: app::EXECUTABLE -> app::EXECUTABLE, common::STATIC_LIBRARY

Diagnostics:
  configuration_drift: analysis_configuration.tu_thresholds.include_path_count
    baseline=0, current=5
    severity=warning, ci_policy_hint=review_required
```

### Markdown

Wie Console, aber mit Markdown-Syntax (Headings `## Translation Units
(added)`, Tabellen fuer changed-Diffs, Inline-Code fuer
Identitaeten).

## Tests

Service-Tests `tests/hexagon/test_compare_service.cpp`:

- Identische baseline und current: leerer Diff, leere Summary-
  Counts, kein Diagnostic.
- TU added/removed/changed: jede Gruppe mit dezidierten Tests.
- Hotspot-Identitaet einschliesslich `include_origin`/`include_depth_kind`:
  Wechsel `project, direct` -> `project, mixed` ist ein
  added+removed-Paar, nicht ein changed.
- Target-Graph-Knoten und -Kanten: added/removed/changed.
- Target-Hubs: added/removed; `target_hubs.changed=[]` und
  `summary.target_hubs_changed=0` sind hart gepinnt.
- Cross-Mode-Compare, z. B. Compile-DB-only baseline + File-API
  current: harter CLI-Fehler
  `"compare: project identity source mismatch ..."`; es wird kein
  Compare-Output erzeugt.
- Same-Mode-Vergleich mit unterschiedlicher Datenverfuegbarkeit, z. B.
  File-API-basierte baseline mit Target-Graph und File-API-basierte
  current ohne ladbare Target-Graph-Daten: `data_availability_drift`
  fuer `target_graph` und `target_hubs`; betroffene Diff-Gruppen
  werden uebersprungen.
- Configuration-Drift: --analysis-Aenderung, tu-thresholds-Aenderung,
  hub-thresholds-Aenderung, alle als configuration_drifts ausgegeben.
- project_identity_drift: Compile-DB-Hash unterschiedlich,
  --allow-project-identity-drift gesetzt, Diagnostic mit allen
  fuenf Pflichtfeldern.
- Compare-Pfadnormalisierung im Compile-DB-Fallback: absolute
  Workspace-/Temp-Praefixe unterscheiden sich zwischen baseline und
  current, aber gleicher relativer Pfad-Suffix erzeugt keinen
  fachlichen Diff.

CLI-Tests `tests/e2e/test_cli.cpp`:

- `compare` ohne `--baseline` ergibt Exit-Code `2` mit der
  passenden Fehlermeldung.
- `compare` ohne `--current` analog.
- `compare --baseline a.json --current b.json --format html`
  ergibt Exit-Code `2`.
- `compare --baseline a.json --current b.json --format dot` analog.
- `compare --baseline a.json --current b.json --format console --output out.txt`
  ergibt Exit-Code `2`.
- `compare --baseline missing.json --current b.json` ergibt
  Exit-Code `1` mit cannot-read-Fehlermeldung.
- `compare --baseline broken.json --current b.json` mit
  ungueltigem JSON ergibt Exit-Code `1`.
- `compare --baseline impact.json --current analyze.json` ergibt
  Exit-Code `1` mit `report_type='impact'`-Ablehnung.
- Versions-Kombinationen ausserhalb der Matrix: Exit-Code `1`.
- project_identity-Mismatch ohne Drift-Option: Exit-Code `1`.
- project_identity-Source-Mismatch: Exit-Code `1`.

Adapter-Tests:

- JSON-Reader: parsen, validieren, mapping zu interner Datenstruktur;
  alle Fehlerpfade.
- Compile-DB-Fingerprint: kanonische Payload, lexikografische
  Sortierung, deterministische Reproduktion mit denselben Eingangs-
  Source-Paths in unterschiedlicher Reply-Reihenfolge.
- Console-/Markdown-/JSON-Compare-Adapter: Output-Vertrag pro
  Diff-Gruppe und Diagnostic byteweise gepinnt.

Schema-Tests:

- `kCompareFormatVersion == 1` mit Schema-Konstante.
- Compare-Schema validiert ein Beispiel-Compare-JSON.
- Negativtest: ein Compare-JSON mit `format != cmake-xray.compare`
  validiert NICHT.
- Negativtest: ein Compare-JSON ohne `inputs` oder ohne `summary`
  validiert NICHT.
- Compare-Kompatibilitaetsmatrix-Tests:
  - `(5, 5)` ist erlaubt.
  - `(4, 5)`, `(5, 4)`, `(6, 5)`, `(5, 7)` und `(7, 6)` sind
    abgelehnt.

E2E-/Golden-Tests:

- Goldens unter `tests/e2e/testdata/m6/compare-reports/<format>/`:
  - `compare-empty.<ext>`: identische baseline und current.
  - `compare-typical.<ext>`: alle Diff-Typen vertreten.
  - `compare-config-drift.<ext>`: configuration_drift-Diagnostic.
  - `compare-data-availability-drift.<ext>`: Same-Mode-File-API-
    Eingaben mit unterschiedlicher Target-Graph-Datenverfuegbarkeit.
  - `compare-project-identity-drift.<ext>`: --allow-project-identity-drift
    aktiviert.
- Eingangsdaten unter `tests/e2e/testdata/m6/compare-reports/inputs/`
  (Analyze-JSON-Files baseline-* und current-*).

Compile-DB-Fingerprint-Tests `tests/hexagon/test_compile_db_fingerprint.cpp`:

- Unix-Pfade, Windows-Pfade, gemischt: alle ergeben deterministischen
  Hash nach Normalisierung.
- Eingangsreihenfolge: zufaellig vs. sortiert ergeben gleichen Hash
  (Sortierung ist Teil des Algorithmus).
- Duplikate werden dedupliziert.
- Akzeptierter Trade-off: gleicher Source-Path-Satz mit
  unterschiedlichen Build-Kontexten ergibt denselben Hash; Test
  pinnt das ausdruecklich.

## Rueckwaertskompatibilitaet

Pflicht:

- Bestehende `cmake-xray analyze`- und `cmake-xray impact`-Aufrufe
  bleiben unveraendert. AP 1.6 fuegt nur das `compare`-Subkommando
  hinzu.
- Die analyze-JSON-Pflichtfelder `project_identity` und
  `project_identity_source` werden in v5 aufgenommen
  (Empfehlung an AP 1.5).

Verboten:

- Stille Verhaltensaenderung an `analyze`- oder `impact`-Output.
- Migrations-Banner. AP 1.6 ist additive Funktionalitaet.

## Implementierungsreihenfolge

Innerhalb von **A.1 (project_identity in Analyze-JSON)**:

1. `project_identity`- und `project_identity_source`-Felder im
   Analyze-Modell und JSON-Adapter ergaenzen (falls AP 1.5 das nicht
   getan hat).
2. Schema-Erweiterung.
3. Compile-DB-Fingerprint-Helper implementieren.
4. Service-Tests fuer Fingerprint und project_identity-Setzung.

Innerhalb von **A.2 (Compare-Modell und JSON-Reader)**:

5. `CompareResult`-Modell und Diff-Item-Strukturen.
6. `AnalysisJsonReader`-Adapter mit Schema-Validierung.
7. Reader-Tests.

Innerhalb von **A.3 (Compare-Service und Diff-Logik)**:

8. `CompareAnalysisPort` und `CompareService`.
9. Diff-Logik pro Gruppe.
10. Diagnostics-Sammlung.
11. Service-Tests.

Innerhalb von **A.4 (CLI und Compare-Schema)**:

12. CLI-Subkommando `compare` mit allen Optionen und Fehlerphrasen.
13. `kCompareFormatVersion=1` als Konstante.
14. `docs/report-compare.schema.json` und Compare-
    Kompatibilitaetsmatrix in `docs/compare-matrix.md`.
15. CLI-E2E-Tests.

Innerhalb von **A.5 (JSON-Compare-Adapter)**:

16. `json_compare_adapter` implementieren.
17. JSON-Goldens.
18. Schema-Validation gegen Compare-Goldens.

Innerhalb von **A.6 (Console- und Markdown-Compare-Adapter)**:

19. `console_compare_adapter` und `markdown_compare_adapter`.
20. Console- und Markdown-Goldens.

Innerhalb von **A.7 (Audit-Pass)**:

21. Eingangsdaten-Goldens unter `m6/compare-reports/inputs/`
    erzeugen.
22. CLI-E2E-Tests durchlaufen.
23. Docker-Gates ausfuehren.
24. Liefer-Stand-Block aktualisieren.

## Tranchen-Schnitt

AP 1.6 wird in sieben Sub-Tranchen geliefert. Wegen der hohen
Komplexitaet ist die Tranchen-Anzahl hoeher als bei den vorherigen
APs.

- **A.1 project_identity in Analyze-JSON**: Falls AP 1.5 die Felder
  nicht aufnimmt, fuegt AP-1.6-A.1 sie hinzu und hebt
  `kReportFormatVersion` von `5` auf `6`. Falls AP 1.5 die Felder
  schon aufnimmt, ist A.1 leer und kann verschmolzen werden.
- **A.2 Compare-Modell und JSON-Reader**: Modellstrukturen und
  Eingangs-Reader. Schema- und CLI-Anpassungen folgen in A.4.
- **A.3 Compare-Service und Diff-Logik (atomar)**: Service-Logik,
  Diff-Berechnung, Diagnostics. Reader und Modelle aus A.2 sind
  Pflicht-Voraussetzung.
- **A.4 CLI und Compare-Schema (atomar)**: CLI-Subkommando,
  `kCompareFormatVersion=1`, Compare-Schema, Compare-
  Kompatibilitaetsmatrix-Doku.
- **A.5 JSON-Compare-Adapter**: JSON-Adapter, JSON-Goldens, Schema-
  Validation.
- **A.6 Console- und Markdown-Compare-Adapter**: Beide Text-Adapter
  und ihre Goldens.
- **A.7 Audit-Pass**.

**Begruendung**: A.1 muss zuerst, weil project_identity die
Eingangsvalidierung speist. A.2 baut auf v5-Schema (oder v6, falls
A.1 das Schema hebt). A.3 ist atomar wegen der Diff-/Diagnostic-
Komplexitaet. A.4 ist atomar wegen Schema-CLI-Kohaerenz. A.5 und
A.6 koennen parallel entwickelt werden.

**Harte Tranchen-Abhaengigkeit zu AP 1.5**: A.1 muss nach AP-1.5-A.3
landen, weil AP 1.5 v5 setzt und A.1 entweder darauf aufbaut oder
es zu v6 erweitert.

## Liefer-Stand

Wird nach dem Schnitt der A-Tranchen mit Commit-Hashes befuellt.
Bis dahin ist AP 1.6 nicht abnahmefaehig.

- A.1 (project_identity in Analyze-JSON): noch nicht ausgeliefert.
- A.2 (Compare-Modell und JSON-Reader): noch nicht ausgeliefert.
- A.3 (Compare-Service und Diff-Logik): noch nicht ausgeliefert.
- A.4 (CLI und Compare-Schema): noch nicht ausgeliefert.
- A.5 (JSON-Compare-Adapter): noch nicht ausgeliefert.
- A.6 (Console- und Markdown-Compare-Adapter): noch nicht ausgeliefert.
- A.7 (Audit-Pass): noch nicht ausgeliefert.

## Abnahmekriterien

AP 1.6 ist abgeschlossen, wenn:

- `cmake-xray compare --baseline <a.json> --current <b.json>`
  produktiv nutzbar ist mit den Formaten `console`, `markdown`,
  `json`;
- `kCompareFormatVersion == 1` in C++ und Schema;
- Compare-Output traegt
  `format: cmake-xray.compare`,
  `format_version: 1`,
  `report_type: compare`;
- `--allow-project-identity-drift` ausschliesslich im Same-Mode-
  Compile-DB-Fallback-Pfad wirkt; alle anderen Drift-Faelle
  bleiben CLI-Eingabefehler;
- die Compare-Kompatibilitaetsmatrix in
  `docs/report-json.md`/`docs/compare-matrix.md` dokumentiert ist;
- die Matrix ausschliesslich `(5, 5)` (oder `(6, 6)` falls A.1 v6
  einfuehrt) als erlaubte Versionskombination listet;
- der Compile-DB-Fingerprint deterministisch reproduzierbar ist
  (gleicher Source-Path-Satz nach Normalisierung -> gleicher Hash);
- impact-JSON-Eingaben aktiv abgelehnt werden;
- HTML- und DOT-Compare-Formate aktiv abgelehnt werden;
- alle drei Output-Diagnostics (`configuration_drift`,
  `data_availability_drift`, `project_identity_drift`) und der
  separate CLI-Fehler `incompatible_format_version` korrekt behandelt
  werden;
- der Compile-DB-Trade-off (gleicher Source-Path-Satz mit
  unterschiedlichen Build-Kontexten erhaelt dieselbe Identitaet) in
  Tests sichtbar ist;
- alle Compare-Goldens unter `tests/e2e/testdata/m6/compare-reports/`
  byte-stabil sind;
- der Docker-Gate-Lauf gemaess `docs/quality.md` (Tranchen-Gate fuer
  M6 AP 1.6) gruen ist;
- `git diff --check` sauber ist.

## Offene Folgearbeiten

Diese Punkte werden bewusst in spaeteren Arbeitspaketen umgesetzt:

- HTML- und DOT-Compare-Output. Beide brauchen eigene Vertraege
  (Vergleichsdarstellung, Differenzgraph) und sind nach M6 zu
  bewerten.
- Impact-Compare. Eigener Diff-Vertrag fuer
  `prioritized_affected_targets`, `affected_translation_units`,
  `evidence_strength`-Aenderungen. Folgeumfang nach M6.
- Cross-Mode-Compare zwischen File-API-`project_identity` und
  `fallback_compile_database_fingerprint`. Eigene Override-Option
  und Risiko-Diagnostics; Folgeumfang nach M6.
- Konfigurationsdatei fuer wiederkehrende Compare-Profile (z. B.
  Toleranz-Schwellen pro Diff-Gruppe). Nach M6 zu bewerten.
- Migrations-Pfad fuer aeltere Versionen. Falls v1-v4-Goldens
  spaeter unterstuetzt werden sollen, muss die Compare-
  Kompatibilitaetsmatrix erweitert werden.
