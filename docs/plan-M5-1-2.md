# Plan M5 - AP 1.2 JsonReportAdapter fuer stabile maschinenlesbare Ausgaben umsetzen

## Ziel

AP 1.2 implementiert den JSON-Report als ersten neuen maschinenlesbaren M5-Reportadapter.

Der JSON-Export ist fuer Automatisierung, CI-Auswertung und Folgewerkzeuge gedacht. Deshalb muss das Format stabiler und expliziter sein als die menschenlesbaren Textberichte: versioniert, dokumentiert, schema-validiert, golden-getestet und ohne unstrukturierte Zusatztexte auf stdout.

Nach AP 1.2 kann `cmake-xray analyze --format json` und `cmake-xray impact --format json` produktiv genutzt werden. Die Ausgabe folgt dem in `docs/report-json.md` dokumentierten Vertrag und verwendet die Formatversion aus AP 1.1.

## Scope

Umsetzen:

- `JsonReportAdapter` fuer Analyze- und Impact-Ergebnisse.
- Freischaltung von `--format json` fuer `analyze` und `impact`.
- Freischaltung von `--output <path>` fuer `--format json` ueber den gemeinsamen M5-Atomic-Writer.
- Dokumentierter JSON-Vertrag in `docs/report-json.md`.
- Maschinenlesbares JSON Schema in `docs/report-json.schema.json`.
- Schema-Validierung fuer Goldens und echte CLI-E2E-Ausgaben.
- E2E-Goldens fuer zentrale Provenienz- und Limit-Vertraege sowie E2E-Tests fuer Fehler- und Stream-Vertraege.
- CI-/Docker-Bootstrap fuer den JSON-Schema-Validator in allen `ctest`-Pfaden.

Nicht veraendern:

- Bestehende Console- und Markdown-Ausgaben.
- Report-Ports so, dass Adapter CLI-Kontext erhalten.
- Verbosity-Verhalten aus AP 1.5.

## Nicht umsetzen

Nicht Bestandteil von AP 1.2:

- HTML-Renderer und HTML-spezifische Formatdetails.
- DOT-Renderer, DOT-Budgetierung und Graph-Metadaten.
- Markdown-/Console-Migration auf `ReportInputs`.
- Release-Artefakte jenseits der noetigen CI-/CTest-Bootstrap-Anpassungen.
- JSON-Fehlerobjekte fuer Parser-, Eingabe-, Render- oder Schreibfehler.

Diese Punkte folgen in anderen M5-Arbeitspaketen oder bleiben bewusst ausserhalb des JSON-Vertrags.

## Voraussetzungen aus AP 1.1

AP 1.2 baut auf folgenden AP-1.1-Vertraegen auf:

- `AnalysisResult` und `ImpactResult` enthalten `ReportInputs`.
- `ReportInputs` ist die kanonische Eingabeprovenienz fuer neue M5-Artefaktadapter.
- `AnalyzeProjectRequest` und `AnalyzeImpactRequest` transportieren Eingabepfade und `report_display_base`.
- `BuildModelResult` transportiert File-API-Aufloesungsmetadaten fuer `cmake_file_api_resolved_path`.
- `xray::hexagon::model::kReportFormatVersion` existiert in `src/hexagon/model/report_format_version.h` und hat initial den Integerwert `1`.
- Der gemeinsame atomare Schreibpfad fuer Reportdateien existiert und ist testbar.
- `html`, `json` und `dot` sind bekannte Formatwerte; AP 1.1 lehnt sie als noch nicht implementiert ab.

AP 1.2 entfernt die AP-1.1-Sperre fuer `--format json` oder begrenzt sie auf andere noch nicht implementierte Formate. `--format json` darf nach AP 1.2 den Fehler `recognized but not implemented` nicht mehr liefern.

Falls der M5-konforme Atomic-Writer aus AP 1.1 noch nicht vorhanden oder nicht testbar ist, ist AP 1.2 insgesamt nicht abnahmefaehig. Eine teilweise Abnahme nur fuer JSON-stdout ohne JSON-Dateiausgabe ist nicht zulaessig.

## Dateien

Voraussichtlich zu aendern:

- `src/adapters/cli/cli_adapter.{h,cpp}`
- `src/main.cpp`
- `src/adapters/CMakeLists.txt`
- `tests/CMakeLists.txt`
- `tests/adapters/test_json_report_adapter.cpp`
- `tests/adapters/test_port_wiring.cpp`
- `tests/e2e/test_cli.cpp`
- `tests/e2e/run_e2e.sh`
- `docs/guide.md`
- `docs/quality.md`
- `Dockerfile`
- `.github/workflows/test.yml`
- `.github/workflows/build.yml`
- `.github/workflows/release.yml`, falls Release-Smokes oder Release-CTest-Laeufe den JSON-Schema-Validator ausfuehren

Neue Dateien:

- `src/adapters/output/json_report_adapter.h`
- `src/adapters/output/json_report_adapter.cpp`
- `docs/report-json.md`
- `docs/report-json.schema.json`
- `tests/validate_json_schema.py` oder ein gleichwertiges repository-lokales Validierungsskript
- `tests/requirements-json-schema.txt`
- JSON-Report-Goldens unter `tests/e2e/testdata/m5/json-reports/`
- Golden-Manifest `tests/e2e/testdata/m5/json-reports/manifest.txt` oder eine gleichwertige explizite Liste aller JSON-Report-Goldens

## JSON-Dokumentvertrag

### Gemeinsame Regeln

Der JSON-Vertrag ist in `docs/report-json.md` verbindlich dokumentiert. Dieses Dokument beschreibt fuer alle Felder:

- Typ.
- Pflichtstatus.
- erlaubte Enum-Werte.
- Nullability.
- Regeln fuer leere Arrays.
- Sortier- und Tie-Breaker-Regeln.
- Aenderungsregeln fuer kuenftige `format_version`-Erhoehungen.

Allgemeine JSON-Regeln:

- JSON-Ausgaben muessen gueltiges UTF-8 und syntaktisch gueltiges JSON sein.
- `format` ist der maschinenlesbare Dokumenttypbezeichner des JSON-Reports und nicht der CLI-Wert `--format json`.
- `report_type` bleibt der kurze Workflow-Wert `analyze` oder `impact`.
- Alle im jeweiligen `inputs`-Vertrag definierten Felder werden immer ausgegeben.
- Fehlende optionale Eingaben erscheinen als JSON-`null`, nie als leerer String und nie als weggelassenes Feld.
- Felder mit leerer Menge werden als leere Arrays ausgegeben, sofern sie Teil des Formatvertrags sind.
- Optionale fachliche Informationen koennen `null` sein, wenn das Schema dies explizit dokumentiert.
- Numerische Metriken bleiben numerisch und werden nicht als lokalisierte Strings gerendert.
- Die Reihenfolge von Objektfeldern bleibt im Adapter stabil, auch wenn JSON semantisch keine Feldreihenfolge garantiert.
- Pfade werden als Anzeige-Strings ausgegeben; kanonische Normalisierungsschluessel duerfen nur aufgenommen werden, wenn sie keinen instabilen Hostbezug in Goldens einfuehren.
- JSON darf keine DOT-/HTML-/Markdown-spezifischen Metadaten wie `graph_*`, `context_*` oder menschenlesbare Kuerzungshinweise einfuehren.

### Analyze-JSON

Ein JSON-Bericht fuer `analyze` enthaelt mindestens:

- `format`: fester Formatbezeichner `cmake-xray.analysis`.
- `format_version`: Formatversion aus `xray::hexagon::model::kReportFormatVersion`, initial `1`.
- `report_type`: `analyze`.
- `inputs`: `compile_database_path`, `compile_database_source`, `cmake_file_api_path`, `cmake_file_api_resolved_path` und `cmake_file_api_source`.
- `summary`: Translation-Unit-Anzahl, Ranking-Anzahl, Hotspot-Anzahl, Top-Limit, `include_analysis_heuristic`, Beobachtungsherkunft und Target-Metadatenstatus.
- `translation_unit_ranking`: Objekt mit `limit`, `total_count`, `returned_count`, `truncated` und deterministisch sortierten `items` inklusive Metriken, Diagnostics und Target-Zuordnungen.
- `include_hotspots`: Objekt mit `limit`, `total_count`, `returned_count`, `truncated` und deterministisch sortierten `items`; Hotspot-Items enthalten betroffene Translation Units mit `affected_total_count`, `affected_returned_count`, `affected_truncated` und `affected_translation_units`.
- `diagnostics`: reportweite Diagnostics.

Analyze-spezifische Regeln:

- JSON folgt bei `analyze` der CLI-`--top`-Begrenzung fuer Ranking- und Hotspot-Listen.
- `limit`, `total_count`, `returned_count` und `truncated` zeigen eindeutig, ob die Ausgabe gekuerzt wurde.
- Eine unlimitierte JSON-Ausgabe waere nur nach expliziter Schema-Entscheidung zulaessig.
- Die Unterliste `affected_translation_units` innerhalb eines Include-Hotspots ist in M5 ebenfalls begrenzt. Sie nutzt denselben effektiven `--top`-Wert als Obergrenze pro Hotspot und dokumentiert die Kuerzung ueber `affected_total_count`, `affected_returned_count` und `affected_truncated`.
- `include_analysis_heuristic` ist ein boolesches Pflichtfeld unter `summary`.
- Schema, Adaptertests und Goldens pruefen `include_analysis_heuristic=true` und `include_analysis_heuristic=false`.

### Impact-JSON

Ein JSON-Bericht fuer `impact` enthaelt mindestens:

- `format`: fester Formatbezeichner `cmake-xray.impact`.
- `format_version`: Formatversion aus `xray::hexagon::model::kReportFormatVersion`, initial `1`.
- `report_type`: `impact`.
- `inputs`: `compile_database_path`, `compile_database_source`, `cmake_file_api_path`, `cmake_file_api_resolved_path`, `cmake_file_api_source`, `changed_file` und `changed_file_source`.
- `summary`: Anzahl betroffener Translation Units, Klassifikation, Beobachtungsherkunft, Target-Metadatenstatus und Anzahl betroffener Targets.
- `directly_affected_translation_units`.
- `heuristically_affected_translation_units`.
- `directly_affected_targets`.
- `heuristically_affected_targets`.
- `diagnostics`.

Impact-spezifische Regeln:

- Bei `impact` ist `changed_file` wegen der CLI-Pflicht ein String.
- `changed_file_source` ist einer der in `docs/report-json.md` dokumentierten Enum-Werte.
- `unresolved_file_api_source_root` aus AP 1.1 ist in JSON v1 kein erlaubter `changed_file_source`-Wert. Dieser Wert beschreibt File-API-Fehlerergebnisse im Modell; AP 1.2 behandelt nicht wiederherstellbare File-API-Ladefehler als Textfehler ohne JSON-Report.
- `impact`-JSON enthaelt in M5 keine `limit`-/`truncated`-Felder, solange der CLI- und Port-Vertrag keine Impact-Begrenzung kennt.
- Alle betroffenen Translation Units und Targets aus dem `ImpactResult` werden ausgegeben.

## Schema- und Formatversionierung

AP 1.2 verwendet die in AP 1.1 eingefuehrte Konstante:

- `JsonReportAdapter` importiert `xray::hexagon::model::kReportFormatVersion`.
- Adapter, Dokumentation, Schema-Tests und Goldens duerfen keine zweite Formatversionskonstante definieren.
- `docs/report-json.schema.json` verwendet JSON Schema Draft 2020-12 und enthaelt `$schema: "https://json-schema.org/draft/2020-12/schema"`.
- Das Schema ist fuer Vertragsobjekte geschlossen: Top-Level, `inputs`, `summary`, Ranking-/Hotspot-Container, Items, Diagnostics, Translation-Unit-Objekte, Target-Objekte und verschachtelte Unterobjekte setzen `additionalProperties: false` oder ein aequivalentes geschlossenes Schema. Unbekannte Felder werden in M5 hart abgelehnt.
- `docs/report-json.schema.json` muss `format_version` mit `const: 1` absichern.
- Der `format_version`-`const`-Wert im Schema wird in einem CTest-Gate gegen `xray::hexagon::model::kReportFormatVersion` geprueft.
- Ein Schema-Wert, der von der C++-Konstante abweicht, laesst AP 1.2 fehlschlagen.

`docs/report-json.md` und `docs/report-json.schema.json` entstehen vor oder im selben Umsetzungsschritt wie `JsonReportAdapter`. Der Adapter darf nicht ohne verbindlichen Item-Vertrag fuer Ranking-, Hotspot-, Diagnostic-, Translation-Unit- und Target-Objekte landen.

## Adapter- und Port-Grenzen

`JsonReportAdapter` rendert fachliche Inhalte ausschliesslich aus `AnalysisResult` und `ImpactResult`.

Regeln:

- Der Adapter bekommt keinen CLI-Kontext.
- Der Adapter verwendet `ReportInputs` als Eingabeprovenienz und nicht die Legacy-Presentation-Felder fuer Console/Markdown.
- Der Adapter importiert `kReportFormatVersion` aus dem Modell.
- Fuer Analyze darf der bestehende Renderparameter `effective_top_limit` beziehungsweise `write_analysis_report(result, effective_top_limit)` genutzt werden, solange er nur die Berichtssicht begrenzt und nicht als CLI-Kontext in JSON-Felder jenseits der dokumentierten Limit-Metadaten durchgereicht wird.
- Alternativ darf `top_limit` als Ergebnis-/Reportview-Feld modelliert werden; AP 1.2 muss sich fuer genau einen Pfad entscheiden und ihn in Port-, Adapter- und Testschnittstellen konsistent umsetzen.
- Der Adapter fuehrt keine HTML-, DOT- oder Markdown-spezifischen Metadaten ein.
- Der Item-Vertrag legt Pflichtkeys, optionale Keys, Enum-Schreibweisen, numerische Typen und deterministische Sortier-Tie-Breaker fest.
- Der Schema-Vertrag fuer `changed_file_source` enthaelt in JSON v1 nur Werte, die in erfolgreichen JSON-Reports auftreten koennen; Modellwerte fuer text-only Fehlerpfade werden nicht vorsorglich als JSON-Enum freigegeben.
- Adaptertests pruefen exakte Keys und mindestens einen Tie-Breaker pro sortierter Item-Liste.
- Report-Ports bleiben ergebnisobjektzentriert.

## CLI-/Output-Vertrag

CLI-Regeln:

- `--format json` ist fuer `analyze` und `impact` lauffaehig.
- `--format json --output <path>` schreibt in eine Datei und nutzt den gemeinsamen M5-konformen Atomic-Writer aus AP 1.1; AP 1.2 ist blockiert, bis dieser Writer vorhanden und testbar ist. Eine teilweise Abnahme ohne JSON-Dateiausgabe gibt es nicht.
- Erfolgreiche `--format json`-Aufrufe ohne `--output` schreiben ausschliesslich gueltiges JSON auf stdout.
- Erfolgreiche `--format json`-Aufrufe ohne `--output` lassen stderr leer.
- Erfolgreiche `--format json --output <path>`-Aufrufe schreiben ausschliesslich in die Zieldatei.
- Erfolgreiche `--format json --output <path>`-Aufrufe lassen stdout und stderr leer.
- Reportinhalt wird bei `--output` nicht zusaetzlich auf stdout dupliziert.
- `--format json` liefert nach AP 1.2 nicht mehr den AP-1.1-Fehler `recognized but not implemented`.

`--top`-Regeln:

- Bei `analyze` beeinflusst `--top` die JSON-Listen `translation_unit_ranking.items` und `include_hotspots.items`.
- Die Begrenzung wird ueber `limit`, `total_count`, `returned_count` und `truncated` maschinenlesbar dokumentiert.
- Bei `impact` fuehrt AP 1.2 keine neue `--top`-Semantik ein.

## Fehlervertrag

Parser- und Usage-Fehler:

- CLI-Parser-/Verwendungsfehler bleiben Textfehler auf `stderr`.
- Beispiele sind fehlende Pflichtoptionen, unbekannte Optionen und `impact --format json` ohne `--changed-file`.
- Diese Fehler erzeugen kein JSON-Fehlerobjekt.
- Exit-Code ist ungleich `0`.

Nicht wiederherstellbare Eingabefehler:

- Fehler vor Reporterzeugung bleiben fuer `--format json` Textfehler auf `stderr`.
- Beispiele sind nicht vorhandene Eingabedateien, ungueltige `compile_commands.json`-Dateien und ungueltige CMake-File-API-Reply-Verzeichnisse.
- Diese Fehler erzeugen kein JSON-Fehlerobjekt.
- Exit-Code ist ungleich `0`.

Service-Ergebnisse mit Diagnostics:

- Service-Ergebnisse mit Diagnostics oder Teildaten werden als regulaere JSON-Reports mit `diagnostics` ausgegeben.
- Erfolgreiche Reporterzeugung bleibt Exit-Code `0`, sofern die bestehende CLI-Semantik fuer diese Serviceergebnisse nicht bereits Fehlercodes vorsieht.

Render-, Schreib- und Output-Fehler:

- Schreib-, Render- und Output-Fehler liefern Textfehler auf `stderr`.
- Exit-Code ist ungleich `0`.
- Render-Fehler vor Emission schreiben kein partielles JSON auf stdout. Bei echten stdout-Transportfehlern koennen bereits geschriebene Bytes nicht zurueckgenommen werden; die CLI erkennt solche Fehler best-effort nach dem Schreiben und meldet sie als Textfehler auf `stderr`.
- Bei `--output` bleibt eine bestehende Zieldatei unveraendert.
- Render-Fehler umfassen JSON-Dump-/Serialisierungsfehler, UTF-8-/Escaping-Fehler und explizite Fehler aus der CLI-internen Render-Abstraktion.
- Render-Fehler werden vor dem Atomic-Writer in Text-`stderr` und nonzero Exit uebersetzt.
- Tests nutzen einen werfenden oder fehlerliefernden Renderer-Stub, damit kein Adapter-spezifischer Sonderfall die Fehlergrenze umgeht.

## Schema-Validierung und CI-Bootstrap

Schema-Validierung:

- Schema-Validierung erfolgt ueber ein repository-lokales Python-Testskript mit der Python-Bibliothek `jsonschema`.
- Die Dependency wird mit allen transitiven Python-Abhaengigkeiten vollstaendig reproduzierbar gepinnt, entweder ueber `tests/requirements-json-schema.txt` mit Hash-/Constraint-Pins oder ueber eine fest gebaute CI-/Docker-Image-Schicht.
- Lokale CTest-Laeufe schlagen mit klarer Installationsanweisung fehl, wenn der Validator fehlt.
- Der CTest-Schema-Test ueberspringt nicht still, wenn der Validator fehlt.
- CTest selbst installiert keine Python-Packages und greift nicht auf das Netzwerk zu. Installation erfolgt ausschliesslich im Bootstrap-Schritt; der Test prueft nur die vorhandene Umgebung und gibt bei fehlendem Validator eine konkrete Installationsanweisung aus.

Bootstrap-Pfade:

- Alle M5-Testwege installieren `tests/requirements-json-schema.txt` vor jedem `ctest`-Pfad oder nutzen eine gemeinsame CMake-/CTest-Bootstrap-Schicht.
- Abgedeckt werden Docker-Test-Image, Docker-Coverage-Image, native Build-Matrix und Release-Matrix.
- `Dockerfile` wird angepasst, wenn Docker-Test-, Coverage- oder Coverage-Check-Targets `ctest` ausfuehren.
- `.github/workflows/test.yml` wird angepasst fuer Docker-Test- und Coverage-Pfade.
- `.github/workflows/build.yml` wird angepasst fuer die native Build-Matrix.
- `.github/workflows/release.yml` wird angepasst, falls Release-Smokes oder Release-CTest-Laeufe den JSON-Schema-Validator ausfuehren.

## Implementierungsreihenfolge

1. JSON-Vertrag in `docs/report-json.md` und `docs/report-json.schema.json` festlegen.
2. Schema-Validator-Skript und gepinnte Requirements anlegen.
3. CTest-Gates fuer Schema-Validierung und Formatversionskonsistenz einhaengen.
4. CI-/Docker-Bootstrap fuer alle `ctest`-Pfade einrichten.
5. `JsonReportAdapter` fuer `AnalysisResult` und `ImpactResult` implementieren.
6. Adaptertests fuer Pflichtfelder, Typen, Enums, Nullability, Sortierung und Tie-Breaker ergaenzen.
7. JSON in Composition Root und Port-Wiring verdrahten.
8. AP-1.1-Sperre fuer `--format json` entfernen.
9. `--format json --output` an den gemeinsamen Atomic-Writer anbinden.
10. E2E-Goldens fuer Analyze und Impact erzeugen.
11. E2E-Tests mit Golden-Vergleich, Schema-Validierung, Stream-Vertrag und Fehlerfaellen ergaenzen.
12. Nutzungsdokumentation in `docs/guide.md` und Test-/Qualitaetsumfang in `docs/quality.md` aktualisieren.
13. Abschliessend pruefen, dass Console-/Markdown-Goldens unveraendert bleiben.

## Tests

Schema- und Vertrags-Tests:

- CTest-integrierter Schema-Validierungstest validiert alle JSON-Goldens gegen `docs/report-json.schema.json`.
- Der Schema-Test validiert ausschliesslich JSON-Report-Goldens aus `tests/e2e/testdata/m5/json-reports/` und gleicht sie gegen das Manifest ab. Er schlaegt fehl, wenn ein Report-Golden im Verzeichnis nicht im Manifest steht oder ein Manifesteintrag fehlt.
- CTest-integrierter Versionskonsistenztest prueft den `format_version`-Schema-`const`-Wert gegen `xray::hexagon::model::kReportFormatVersion`.
- Tests pruefen Pflichtfelder, Array-statt-Weglassen-Regeln, numerische Typen, bekannte Enum-Werte und `null` nur an dokumentierten Stellen.

Adaptertests:

- `tests/adapters/test_json_report_adapter.cpp` prueft Analyze- und Impact-Serialisierung.
- Adaptertests pruefen exakte Item-Keys.
- Adaptertests pruefen mindestens einen Tie-Breaker pro sortierter Item-Liste.
- Adaptertests pruefen `include_analysis_heuristic=true` und `include_analysis_heuristic=false`.

Port- und Wiring-Tests:

- `tests/adapters/test_port_wiring.cpp` prueft, dass `json` an den `JsonReportAdapter` verdrahtet ist.
- Tests pruefen, dass keine zweite Formatversionskonstante im Adapter oder Wiring verwendet wird.
- Port-/Wiring-Tests pruefen ausdruecklich, dass `--format json` nicht in den bestehenden Console-Fallback faellt und keinen Console-Adapter nutzt.
- Binary-Smoke-Tests ueber `tests/e2e/run_e2e.sh` und das CTest-Ziel `e2e_binary` pruefen, dass auch die echte Binary inklusive `src/main.cpp` den JSON-Adapter verdrahtet.

E2E-Golden-Tests:

- E2E-Tests erzeugen JSON-Ausgaben mit der echten CLI.
- Die erzeugten JSON-Reports werden byte-stabil oder strukturell deterministisch gegen Golden-Dateien verglichen.
- Dieselben erzeugten Reports werden gegen `docs/report-json.schema.json` validiert.
- Reine Schema-Validierung statischer Goldens reicht nicht aus.
- JSON-Goldens decken Compile-Database-only-, File-API-only- und Mixed-Input-Laeufe bei `analyze` ab.
- Analyze-Goldens pruefen `compile_database_path`, `cmake_file_api_path`, `cmake_file_api_resolved_path`, `compile_database_source` und `cmake_file_api_source`.
- File-API-Provenienz-Goldens pruefen mindestens einen `--cmake-file-api <build-dir>`-Fall und einen `--cmake-file-api <reply-dir>`-Fall, damit `cmake_file_api_path` und `cmake_file_api_resolved_path` nicht verwechselt werden.
- Analyze-Goldens decken fuer `translation_unit_ranking` und `include_hotspots` jeweils mindestens einen ungekuerzten Fall mit `truncated=false` und einen gekuerzten `--top`-Fall mit `truncated=true`, korrektem `limit`, `total_count` und `returned_count` ab.
- Analyze-Goldens decken fuer Hotspot-Items mindestens einen Fall ab, in dem `affected_translation_units` ueber `affected_total_count`, `affected_returned_count` und `affected_truncated` begrenzt wird.
- JSON-Goldens decken `impact` mit Compile-Database-only-, File-API-only- und Mixed-Input-Provenienz ab.
- Relative `changed_file`-Faelle pruefen `compile_database_directory` bei Mixed-Input und `file_api_source_root` bei File-API-only.
- Absolute `changed_file`-Faelle pruefen `cli_absolute`.
- Schema- oder Adaptertests pruefen, dass `changed_file_source=unresolved_file_api_source_root` in JSON v1 nicht akzeptiert wird.

CLI- und Stream-Tests:

- `analyze --format json` liefert gueltiges JSON auf stdout und leeres stderr.
- `impact --format json` liefert gueltiges JSON auf stdout und leeres stderr.
- Binary-E2E-Smokes fuehren `analyze --format json` und mindestens einen `impact --format json`-Fall ueber die gebaute `cmake-xray`-Binary aus, nicht nur ueber direkt instanziierte CLI-Adaptertests.
- `--format json --output <path>` schreibt gueltiges JSON in die Datei.
- Erfolgreiche `--format json --output <path>`-Aufrufe lassen stdout und stderr leer.
- Tests pruefen, dass `--format json` nicht mehr den AP-1.1-Fehler `recognized but not implemented` liefert.
- Tests pruefen, dass `--format json` tatsaechlich JSON mit `format=cmake-xray.analysis` beziehungsweise `format=cmake-xray.impact` rendert und nicht den Console-Reportpfad trifft.

Fehlerpfad-Tests:

- `impact --format json` ohne `--changed-file` liefert Text auf stderr, nonzero Exit und kein JSON-Fehlerobjekt.
- Nicht vorhandene Eingabepfade liefern Text auf stderr, nonzero Exit und kein JSON-Fehlerobjekt.
- Ungueltiges Compile-Database-JSON liefert Text auf stderr, nonzero Exit und kein JSON-Fehlerobjekt.
- Ungueltige CMake-File-API-Reply-Daten liefern Text auf stderr, nonzero Exit und kein JSON-Fehlerobjekt.
- Schreibfehler liefern Text auf stderr, nonzero Exit und kein JSON-Fehlerobjekt.
- Service-Diagnostics erscheinen im regulaeren JSON-Report unter `diagnostics`.
- CLI-/Schreibpfad-Tests decken einen simulierten JSON-Render-Fehler vor dem Atomic-Writer ab.
- Der simulierte Render-Fehler prueft nonzero Exit, Text auf stderr, leeres stdout und unveraenderte bestehende Zieldatei bei `--output`.

Atomic-Writer-Tests:

- JSON-`--output` nutzt den gemeinsamen M5-konformen Atomic-Writer aus AP 1.1.
- Tests pruefen mindestens, dass ein simulierter Schreib-/Replace-Fehler eine bestehende JSON-Zieldatei unveraendert laesst.

Rueckwaertskompatibilitaets-Tests:

- Bestehende Console-/Markdown-Goldens bleiben unveraendert.
- JSON-Freischaltung fuehrt keine Verbosity-Parameter in Reportports oder Artefaktadapter ein.

## Abnahmekriterien

AP 1.2 ist abnahmefaehig, wenn:

- `cmake-xray analyze --format json` und `cmake-xray impact --format json` lauffaehig sind.
- `--format json --output <path>` ueber den gemeinsamen Atomic-Writer schreibt.
- Erfolgreiche JSON-Stdout-Ausgaben keine unstrukturierten Zusatztexte enthalten.
- Erfolgreiche JSON-Dateiausgaben stdout und stderr leer lassen.
- `docs/report-json.md` und `docs/report-json.schema.json` vorhanden, konsistent und in Tests eingebunden sind.
- Das Schema lehnt unbekannte Felder in allen Vertragsobjekten ab.
- Der Schema-`format_version`-Wert gegen `kReportFormatVersion` geprueft wird.
- Echte CLI-Ausgaben gegen Goldens verglichen und anschliessend gegen das Schema validiert werden.
- Analyze-Goldens Limit-/Truncation-Faelle fuer Ranking und Hotspots abdecken.
- Impact-Goldens die relevanten `changed_file_source`-Faelle abdecken.
- File-API-Goldens Build-Dir- und Reply-Dir-Eingaben fuer `cmake_file_api_resolved_path` abdecken.
- Binary-E2E-Smokes die JSON-Verdrahtung der echten Binary inklusive `src/main.cpp` abdecken.
- Parser-, Eingabe-, Render- und Schreibfehler als Textfehler ohne JSON-Fehlerobjekt getestet sind.
- Docker-, Coverage-, native Build- und Release-CTest-Pfade den JSON-Schema-Validator installieren oder eine gemeinsame Bootstrap-Schicht nutzen.
- CTest bleibt offline und reproduzierbar; fehlende Validator-Dependencies fuehren zu einer klaren Fehlermeldung statt zu Netzwerkzugriffen oder stillen Skips.
- `docs/guide.md` die produktive JSON-Nutzung beschreibt und `docs/quality.md` die neuen JSON-Schema-, Golden- und E2E-Gates auffuehrt.
- Console- und Markdown-Verhalten unveraendert bleibt.

## Offene Folgearbeiten

Folgearbeiten ausserhalb von AP 1.2:

- AP 1.3 implementiert DOT-Rendering, DOT-Budgetierung und Graph-Metadaten.
- AP 1.4 implementiert HTML-Rendering und HTML-spezifische Ausgabe-/Escaping-Regeln.
- AP 1.5 definiert Quiet-/Verbose-Verhalten.
- Eine spaetere JSON-Formatversion kann neue Felder oder JSON-Fehlerobjekte einfuehren, muss dann aber `format_version` erhoehen und `docs/report-json.md` sowie `docs/report-json.schema.json` entsprechend migrieren.

**Ergebnis**: `cmake-xray` besitzt einen versionierten, automatisierbaren Reportvertrag fuer Analyse- und Impact-Ergebnisse.
