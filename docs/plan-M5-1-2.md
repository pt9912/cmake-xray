# Plan M5 - AP 1.2 JsonReportAdapter fuer stabile maschinenlesbare Ausgaben umsetzen

Der JSON-Export ist fuer Automatisierung, CI-Auswertung und Folgewerkzeuge gedacht. Deshalb muss das Format stabiler und expliziter sein als die menschenlesbaren Textberichte.

Ein JSON-Bericht fuer `analyze` soll mindestens enthalten:

- `format`: fester Formatbezeichner `cmake-xray.analysis`
- `format_version`: Schema-/Formatversion aus `xray::hexagon::model::kReportFormatVersion`, initial `1`
- `report_type`: `analyze`
- `inputs`: verwendete Eingabequellen aus dem strukturierten `ReportInputs`-Modell mit den Pflichtfeldern `compile_database_path`, `compile_database_source`, `cmake_file_api_path`, `cmake_file_api_resolved_path` und `cmake_file_api_source`
- `summary`: Translation-Unit-Anzahl, Ranking-Anzahl, Hotspot-Anzahl, Top-Limit, Beobachtungsherkunft und Target-Metadatenstatus
- `translation_unit_ranking`: Objekt mit `limit`, `total_count`, `returned_count`, `truncated` und deterministisch sortierten `items` inklusive Metriken, Diagnostics und Target-Zuordnungen
- `include_hotspots`: Objekt mit `limit`, `total_count`, `returned_count`, `truncated` und deterministisch sortierten `items`
- `diagnostics`: reportweite Diagnostics

Ein JSON-Bericht fuer `impact` soll mindestens enthalten:

- `format`: fester Formatbezeichner `cmake-xray.impact`
- `format_version`: Schema-/Formatversion aus `xray::hexagon::model::kReportFormatVersion`, initial `1`
- `report_type`: `impact`
- `inputs`: verwendete Eingabequellen aus dem strukturierten `ReportInputs`-Modell mit den Pflichtfeldern `compile_database_path`, `compile_database_source`, `cmake_file_api_path`, `cmake_file_api_resolved_path`, `cmake_file_api_source`, `changed_file` und `changed_file_source`
- `summary`: Anzahl betroffener Translation Units, Klassifikation, Beobachtungsherkunft, Target-Metadatenstatus und Anzahl betroffener Targets
- `directly_affected_translation_units`
- `heuristically_affected_translation_units`
- `directly_affected_targets`
- `heuristically_affected_targets`
- `diagnostics`

Wichtig:

- `format` ist der maschinenlesbare Dokumenttypbezeichner des JSON-Reports und nicht der CLI-Wert `--format json`; `report_type` bleibt der kurze Workflow-Wert fuer `analyze` oder `impact`
- `inputs` darf nur Felder enthalten, die stabil im fachlichen Ergebnis- oder Request-Modell verfuegbar sind; `cmake_file_api_path` ist als Feld fuer M5 verpflichtend, damit File-API- und Mixed-Input-Laeufe vollstaendig dokumentiert werden koennen
- Alle im jeweiligen `inputs`-Vertrag genannten Felder werden immer ausgegeben. Fehlende optionale Eingaben erscheinen als JSON-`null`, nie als leerer String und nie als weggelassenes Feld. Bei `impact` ist `changed_file` wegen der CLI-Pflicht ein String; `changed_file_source` ist einer der dokumentierten Enum-Werte.
- JSON folgt bei `analyze` der CLI-`--top`-Begrenzung fuer Ranking- und Hotspot-Listen, muss aber ueber `limit`, `total_count`, `returned_count` und `truncated` eindeutig anzeigen, ob die Ausgabe gekuerzt wurde; eine unlimitierte JSON-Ausgabe waere nur nach expliziter Schema-Entscheidung zulaessig
- `impact`-JSON enthaelt in M5 keine `limit`-/`truncated`-Felder, solange der CLI- und Port-Vertrag keine Impact-Begrenzung kennt; alle betroffenen Translation Units und Targets aus dem `ImpactResult` werden ausgegeben
- AP 1.2 implementiert nur JSON. Markdown-/HTML- und DOT-Regeln werden hier nur als Kompatibilitaetsrahmen verstanden; die konkrete HTML-Ausgestaltung folgt in AP 1.4, DOT-Budgetierung und Graph-Metadaten folgen in AP 1.3
- JSON darf keine DOT-/HTML-/Markdown-spezifischen Metadaten wie `graph_*`, `context_*` oder menschenlesbare Kuerzungshinweise einfuehren
- `JsonReportAdapter` importiert `xray::hexagon::model::kReportFormatVersion` aus AP 1.1; eine zweite Formatversionskonstante im Adapter, Schema-Generator oder Testcode ist nicht zulaessig
- `docs/report-json.schema.json` darf `format_version` mit `const: 1` absichern, aber dieser Schema-Wert muss in einem CTest-Gate gegen `xray::hexagon::model::kReportFormatVersion` geprueft werden; ein Schema-Wert, der von der C++-Konstante abweicht, laesst AP 1.2 fehlschlagen
- CLI-Parser-/Verwendungsfehler, zum Beispiel fehlende Pflichtoptionen oder unbekannte Optionen, bleiben Textfehler auf `stderr` und erzeugen kein JSON-Fehlerobjekt
- Nicht wiederherstellbare Eingabefehler vor Reporterzeugung, zum Beispiel nicht vorhandene Eingabedateien, ungueltige `compile_commands.json`-Dateien oder ungueltige CMake-File-API-Reply-Verzeichnisse, bleiben fuer `--format json` Textfehler auf `stderr`, liefern Exit-Code ungleich `0` und erzeugen kein JSON-Fehlerobjekt
- Service-Ergebnisse mit Diagnostics oder Teildaten werden als regulaere JSON-Reports mit `diagnostics` ausgegeben; erfolgreiche Reporterzeugung bleibt Exit-Code `0`, sofern die bestehende CLI-Semantik fuer diese Serviceergebnisse nicht bereits Fehlercodes vorsieht
- Schreib-, Render- und Output-Fehler bei `--format json` liefern Textfehler auf `stderr`, Exit-Code ungleich `0` und kein partielles JSON auf stdout; bei `--output` bleibt eine bestehende Zieldatei unveraendert
- JSON-Ausgaben muessen gueltiges UTF-8 und syntaktisch gueltiges JSON sein
- Felder mit leerer Menge werden als leere Arrays ausgegeben, nicht weggelassen, sofern sie Teil des Formatvertrags sind
- optionale fachliche Informationen koennen `null` sein, wenn das Schema dies explizit dokumentiert
- numerische Metriken bleiben numerisch und werden nicht als lokalisierte Strings gerendert
- die Reihenfolge von Objektfeldern soll im Adapter stabil bleiben, auch wenn JSON semantisch keine Feldreihenfolge garantiert
- Pfade werden als Anzeige-Strings ausgegeben; kanonische Normalisierungsschluessel duerfen nur aufgenommen werden, wenn sie keinen instabilen Hostbezug in Goldens einfuehren
- `docs/report-json.md` ist der verbindliche lesbare M5-Vertrag und dokumentiert fuer alle Felder Typ, Pflichtstatus, erlaubte Enum-Werte, Nullability, leere-Array-Regeln und Aenderungsregeln fuer kuenftige `format_version`-Erhoehungen; zusaetzlich entsteht ein maschinenlesbares Schema-Artefakt `docs/report-json.schema.json`, gegen das Golden-Outputs validiert werden
- `docs/report-json.schema.json` verwendet JSON Schema Draft 2020-12 und enthaelt `$schema: "https://json-schema.org/draft/2020-12/schema"`
- Schema-Validierung erfolgt ueber ein repository-lokales Python-Testskript mit der Python-Bibliothek `jsonschema`; die Dependency wird in `tests/requirements-json-schema.txt` mit Version-Pin dokumentiert, CI installiert diese Requirements vor jedem `ctest`-Pfad, und lokale CTest-Laeufe schlagen mit klarer Installationsanweisung fehl, wenn der Validator fehlt
- Die Validator-Bootstrap-Strategie deckt alle M5-Testwege ab: Docker-Test-Image, Docker-Coverage-Image, native Build-Matrix und Release-Matrix. Entweder installiert jeder dieser Wege `tests/requirements-json-schema.txt` vor `ctest`, oder CMake/CTest stellt eine gemeinsame Bootstrap-Schicht bereit, die in allen Wegen genutzt wird.
- `docs/report-json.md` und `docs/report-json.schema.json` entstehen vor oder im selben Umsetzungsschritt wie `JsonReportAdapter`; der Adapter darf nicht ohne verbindlichen Item-Vertrag fuer Ranking-, Hotspot-, Diagnostic-, Translation-Unit- und Target-Objekte landen
- Der Item-Vertrag legt Pflichtkeys, optionale Keys, Enum-Schreibweisen, numerische Typen und deterministische Sortier-Tie-Breaker fest; Adaptertests pruefen exakte Keys und mindestens einen Tie-Breaker pro sortierter Item-Liste
- Tests pruefen nicht nur syntaktisch gueltiges JSON, sondern auch Pflichtfelder, Array-statt-Weglassen-Regeln, numerische Typen, bekannte Enum-Werte und `null` nur an dokumentierten Stellen
- E2E-Tests erzeugen die JSON-Ausgaben mit der echten CLI, vergleichen diese byte-stabil oder strukturell deterministisch gegen die Golden-Dateien und validieren genau die erzeugten Ausgaben gegen `docs/report-json.schema.json`; reine Schema-Validierung statischer Goldens reicht fuer AP 1.2 nicht aus
- Die AP-1.1-Sperre fuer `--format json` wird in AP 1.2 entfernt oder auf andere noch nicht implementierte Formate begrenzt; `--format json` darf den Fehler `recognized but not implemented` nicht mehr liefern
- AP 1.2 setzt den gemeinsamen Atomic-Writer aus AP 1.1 als vorhandene Voraussetzung voraus. Das Arbeitspaket muss vor der JSON-Freischaltung einen Test- oder CMake-Check besitzen, der den M5-konformen Writer referenziert; ohne diesen Check bleibt `--format json --output` nicht freigegeben.

Vorgesehene Artefakte:

- `src/adapters/output/json_report_adapter.h`
- `src/adapters/output/json_report_adapter.cpp`
- `src/adapters/cli/cli_adapter.{h,cpp}` fuer die Freischaltung von `--format json` und `--output` mit JSON
- `src/main.cpp` fuer die Composition-Root-Verdrahtung des JSON-Adapters
- `src/adapters/CMakeLists.txt`
- `tests/CMakeLists.txt`
- `Dockerfile` fuer Docker-Test-, Coverage- und Coverage-Check-Targets, die `ctest` ausfuehren
- `.github/workflows/test.yml` fuer die Docker-Test- und Coverage-Pfade
- `.github/workflows/build.yml` fuer die native Build-Matrix
- `.github/workflows/release.yml`, falls Release-Smokes oder Release-CTest-Laeufe den JSON-Schema-Validator ausfuehren
- `tests/adapters/test_json_report_adapter.cpp`
- `tests/adapters/test_port_wiring.cpp`
- `tests/e2e/test_cli.cpp`
- `tests/validate_json_schema.py` oder ein gleichwertiges repository-lokales Validierungsskript
- `tests/requirements-json-schema.txt` mit gepinnter `jsonschema`-Dependency fuer CI und lokale Reproduktion
- JSON-Golden-Outputs unter `tests/e2e/testdata/m5/`
- Dokumentation `docs/report-json.md` als verbindlicher lesbarer Formatvertrag und `docs/report-json.schema.json` als maschinenlesbares Validierungsartefakt; `docs/guide.md` verweist darauf nur nutzungsorientiert
- CTest-integrierter Schema-Validierungstest, der alle JSON-Goldens gegen `docs/report-json.schema.json` validiert und fehlschlaegt, wenn ein JSON-Golden nicht erfasst ist
- CTest-integrierter Versionskonsistenztest, der den `format_version`-Schema-`const`-Wert gegen `xray::hexagon::model::kReportFormatVersion` prueft
- CLI-/E2E-Tests fuer `analyze --format json`, `impact --format json`, `--format json --output <path>` und die Abwesenheit unstrukturierter Zusatztexte auf stdout
- CLI-/E2E-Tests muessen die erzeugten JSON-Reports gegen die Golden-Dateien vergleichen; dieselben erzeugten Reports werden anschliessend gegen das Schema validiert, damit Adapterausgabe, Golden und Schema nicht auseinanderlaufen
- JSON-Goldens fuer Compile-Database-only-, File-API-only- und Mixed-Input-Laeufe bei `analyze`, inklusive `compile_database_path`, `cmake_file_api_path`, `cmake_file_api_resolved_path`, `compile_database_source` und `cmake_file_api_source`
- JSON-Goldens fuer `impact` mit Compile-Database-only-, File-API-only- und Mixed-Input-Provenienz; relative `changed_file`-Faelle pruefen `compile_database_directory` bei Mixed-Input und `file_api_source_root` bei File-API-only, absolute `changed_file` prueft `cli_absolute`
- CLI-/E2E-Tests, dass erfolgreiche `--format json`-Aufrufe ohne `--output` gueltiges JSON auf stdout und leeres stderr liefern
- CLI-/E2E-Tests, dass erfolgreiche `--format json --output <path>`-Aufrufe gueltiges JSON in die Datei schreiben sowie stdout und stderr leer lassen
- CLI-/E2E-Tests pruefen, dass `--format json` nicht mehr den AP-1.1-Fehler `recognized but not implemented` liefert
- CLI-/E2E-Tests fuer Fehlerpfade: Parser-/Verwendungsfehler, insbesondere `impact --format json` ohne `--changed-file`, Schreibfehler und nicht wiederherstellbare Eingabefehler liefern Text auf stderr und kein JSON-Fehlerobjekt; Service-Diagnostics erscheinen im regulaeren JSON-Report unter `diagnostics`
- E2E-Fehlerfaelle fuer `--format json` decken mindestens nicht vorhandene Eingabepfade, ungueltiges Compile-Database-JSON und ungueltige CMake-File-API-Reply-Daten ab
- JSON-`--output` nutzt den gemeinsamen M5-konformen Atomic-Writer aus AP 1.1; falls dieser Writer noch nicht vorhanden ist, ist AP 1.2 blockiert. Tests pruefen mindestens, dass ein simulierter Schreib-/Replace-Fehler eine bestehende JSON-Zieldatei unveraendert laesst

**Ergebnis**: `cmake-xray` besitzt einen versionierten, automatisierbaren Reportvertrag fuer Analyse- und Impact-Ergebnisse.
