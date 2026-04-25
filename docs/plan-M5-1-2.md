# Plan M5 - AP 1.2 JsonReportAdapter fuer stabile maschinenlesbare Ausgaben umsetzen

Der JSON-Export ist fuer Automatisierung, CI-Auswertung und Folgewerkzeuge gedacht. Deshalb muss das Format stabiler und expliziter sein als die menschenlesbaren Textberichte.

Ein JSON-Bericht fuer `analyze` soll mindestens enthalten:

- `format`: fester Formatbezeichner `cmake-xray.analysis`
- `format_version`: Schema-/Formatversion, initial `1`
- `report_type`: `analyze`
- `inputs`: verwendete Eingabequellen aus dem strukturierten `ReportInputs`-Modell mit den Pflichtfeldern `compile_database_path`, `compile_database_source`, `cmake_file_api_path`, `cmake_file_api_resolved_path` und `cmake_file_api_source`
- `summary`: Translation-Unit-Anzahl, Ranking-Anzahl, Hotspot-Anzahl, Top-Limit, Beobachtungsherkunft und Target-Metadatenstatus
- `translation_unit_ranking`: Objekt mit `limit`, `total_count`, `returned_count`, `truncated` und deterministisch sortierten `items` inklusive Metriken, Diagnostics und Target-Zuordnungen
- `include_hotspots`: Objekt mit `limit`, `total_count`, `returned_count`, `truncated` und deterministisch sortierten `items`
- `diagnostics`: reportweite Diagnostics

Ein JSON-Bericht fuer `impact` soll mindestens enthalten:

- `format`: fester Formatbezeichner `cmake-xray.impact`
- `format_version`: Schema-/Formatversion, initial `1`
- `report_type`: `impact`
- `inputs`: verwendete Eingabequellen aus dem strukturierten `ReportInputs`-Modell mit den Pflichtfeldern `compile_database_path`, `compile_database_source`, `cmake_file_api_path`, `cmake_file_api_resolved_path`, `cmake_file_api_source`, `changed_file` und `changed_file_source`
- `summary`: Anzahl betroffener Translation Units, Klassifikation, Beobachtungsherkunft, Target-Metadatenstatus und Anzahl betroffener Targets
- `directly_affected_translation_units`
- `heuristically_affected_translation_units`
- `directly_affected_targets`
- `heuristically_affected_targets`
- `diagnostics`

Wichtig:

- `inputs` darf nur Felder enthalten, die stabil im fachlichen Ergebnis- oder Request-Modell verfuegbar sind; `cmake_file_api_path` ist als Feld fuer M5 verpflichtend, damit File-API- und Mixed-Input-Laeufe vollstaendig dokumentiert werden koennen
- Alle im jeweiligen `inputs`-Vertrag genannten Felder werden immer ausgegeben. Fehlende optionale Eingaben erscheinen als JSON-`null`, nie als leerer String und nie als weggelassenes Feld. Bei `impact` ist `changed_file` wegen der CLI-Pflicht ein String; `changed_file_source` ist einer der dokumentierten Enum-Werte.
- JSON folgt bei `analyze` der CLI-`--top`-Begrenzung fuer Ranking- und Hotspot-Listen, muss aber ueber `limit`, `total_count`, `returned_count` und `truncated` eindeutig anzeigen, ob die Ausgabe gekuerzt wurde; eine unlimitierte JSON-Ausgabe waere nur nach expliziter Schema-Entscheidung zulaessig
- `impact`-JSON enthaelt in M5 keine `limit`-/`truncated`-Felder, solange der CLI- und Port-Vertrag keine Impact-Begrenzung kennt; alle betroffenen Translation Units und Targets aus dem `ImpactResult` werden ausgegeben
- AP 1.2 implementiert nur JSON. Markdown-/HTML- und DOT-Regeln werden hier nur als Kompatibilitaetsrahmen verstanden; die konkrete HTML-Ausgestaltung folgt in AP 1.4, DOT-Budgetierung und Graph-Metadaten folgen in AP 1.3
- JSON darf keine DOT-/HTML-/Markdown-spezifischen Metadaten wie `graph_*`, `context_*` oder menschenlesbare Kuerzungshinweise einfuehren
- JSON-Ausgaben muessen gueltiges UTF-8 und syntaktisch gueltiges JSON sein
- Felder mit leerer Menge werden als leere Arrays ausgegeben, nicht weggelassen, sofern sie Teil des Formatvertrags sind
- optionale fachliche Informationen koennen `null` sein, wenn das Schema dies explizit dokumentiert
- numerische Metriken bleiben numerisch und werden nicht als lokalisierte Strings gerendert
- die Reihenfolge von Objektfeldern soll im Adapter stabil bleiben, auch wenn JSON semantisch keine Feldreihenfolge garantiert
- Pfade werden als Anzeige-Strings ausgegeben; kanonische Normalisierungsschluessel duerfen nur aufgenommen werden, wenn sie keinen instabilen Hostbezug in Goldens einfuehren
- `docs/report-json.md` ist der verbindliche lesbare M5-Vertrag und dokumentiert fuer alle Felder Typ, Pflichtstatus, erlaubte Enum-Werte, Nullability, leere-Array-Regeln und Aenderungsregeln fuer kuenftige `format_version`-Erhoehungen; zusaetzlich entsteht ein maschinenlesbares Schema-Artefakt `docs/report-json.schema.json`, gegen das Golden-Outputs validiert werden
- `docs/report-json.md` und `docs/report-json.schema.json` entstehen vor oder im selben Umsetzungsschritt wie `JsonReportAdapter`; der Adapter darf nicht ohne verbindlichen Item-Vertrag fuer Ranking-, Hotspot-, Diagnostic-, Translation-Unit- und Target-Objekte landen
- Der Item-Vertrag legt Pflichtkeys, optionale Keys, Enum-Schreibweisen, numerische Typen und deterministische Sortier-Tie-Breaker fest; Adaptertests pruefen exakte Keys und mindestens einen Tie-Breaker pro sortierter Item-Liste
- Tests pruefen nicht nur syntaktisch gueltiges JSON, sondern auch Pflichtfelder, Array-statt-Weglassen-Regeln, numerische Typen, bekannte Enum-Werte und `null` nur an dokumentierten Stellen

Vorgesehene Artefakte:

- `src/adapters/output/json_report_adapter.h`
- `src/adapters/output/json_report_adapter.cpp`
- `src/adapters/cli/cli_adapter.{h,cpp}` fuer die Freischaltung von `--format json` und `--output` mit JSON
- `src/main.cpp` fuer die Composition-Root-Verdrahtung des JSON-Adapters
- `src/adapters/CMakeLists.txt`
- `tests/CMakeLists.txt`
- `tests/adapters/test_json_report_adapter.cpp`
- `tests/adapters/test_port_wiring.cpp`
- `tests/e2e/test_cli.cpp`
- JSON-Golden-Outputs unter `tests/e2e/testdata/m5/`
- Dokumentation `docs/report-json.md` als verbindlicher lesbarer Formatvertrag und `docs/report-json.schema.json` als maschinenlesbares Validierungsartefakt; `docs/guide.md` verweist darauf nur nutzungsorientiert
- CTest-integrierter Schema-Validierungstest, der alle JSON-Goldens gegen `docs/report-json.schema.json` validiert und fehlschlaegt, wenn ein JSON-Golden nicht erfasst ist
- CLI-/E2E-Tests fuer `analyze --format json`, `impact --format json`, `--format json --output <path>` und die Abwesenheit unstrukturierter Zusatztexte auf stdout
- CLI-/E2E-Tests, dass erfolgreiche `--format json`-Aufrufe ohne `--output` gueltiges JSON auf stdout und leeres stderr liefern
- CLI-/E2E-Tests, dass erfolgreiche `--format json --output <path>`-Aufrufe gueltiges JSON in die Datei schreiben sowie stdout und stderr leer lassen

**Ergebnis**: `cmake-xray` besitzt einen versionierten, automatisierbaren Reportvertrag fuer Analyse- und Impact-Ergebnisse.
