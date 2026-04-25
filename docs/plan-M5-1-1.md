# Plan M5 - AP 1.1 Report-Vertraege fuer neue Formate und Formatversionierung schaerfen

## Ziel

AP 1.1 bereitet die bestehende M3-/M4-Reportstrecke so vor, dass die nachfolgenden M5-Arbeitspakete HTML, JSON und DOT implementieren koennen, ohne Fachlogik in Adapter zu verschieben oder bestehende Console-/Markdown-Vertraege unkontrolliert zu aendern.

Nach AP 1.1 ist noch kein vollstaendiger HTML-/JSON-/DOT-Adapter erforderlich. Erforderlich sind die stabilen Modell-, Port-, CLI- und Datei-Schreibvertraege, auf denen AP 1.2 bis AP 1.4 direkt aufbauen.

## Scope

Umsetzen:

- CLI akzeptiert die M5-Formatwerte `console`, `markdown`, `html`, `json`, `dot`.
- `--output` ist fuer `markdown`, `html`, `json`, `dot` erlaubt und fuer `console` verboten.
- Erfolgreiche `--output`-Aufrufe schreiben Reports ausschliesslich in die Datei; stdout bleibt leer.
- Reportdateien werden ueber einen gemeinsamen atomaren Schreibpfad geschrieben.
- `AnalysisResult` und `ImpactResult` enthalten ein gemeinsames `ReportInputs`-Value-Object.
- `AnalyzeProjectRequest` und `AnalyzeImpactRequest` transportieren alle Eingabepfade und eine explizite Display-Basis.
- `BuildModelResult` transportiert rohe File-API-Aufloesungsmetadaten.
- Report-Ports bleiben ergebnisobjektzentriert; Adapter bekommen keinen CLI-Kontext.
- Console-Quiet/Verbose bleibt ein CLI-owned Sonderfall und veraendert keine Artefaktadapter.

Nicht umsetzen:

- Vollstaendige HTML-/JSON-/DOT-Renderer.
- JSON-Schema-Datei und vollstaendige JSON-Goldens.
- DOT-Graph-Budgetlogik.
- Release-Artefakte.

Diese Punkte folgen in spaeteren M5-Arbeitspaketen.

## Dateien

Voraussichtlich zu aendern:

- `src/hexagon/model/analysis_result.h`
- `src/hexagon/model/impact_result.h`
- `src/hexagon/model/build_model_result.h`
- `src/hexagon/ports/driving/analyze_project_port.h`
- `src/hexagon/ports/driving/analyze_impact_port.h`
- `src/hexagon/services/project_analyzer.{h,cpp}`
- `src/hexagon/services/impact_analyzer.{h,cpp}`
- `src/adapters/input/cmake_file_api_adapter.{h,cpp}`
- `src/adapters/cli/cli_adapter.{h,cpp}`
- `src/adapters/CMakeLists.txt`
- `src/main.cpp`
- `tests/hexagon/test_project_analyzer.cpp`
- `tests/hexagon/test_impact_analyzer.cpp`
- `tests/e2e/test_cli.cpp`
- `tests/adapters/test_port_wiring.cpp`

Neue Dateien, falls kein passender Ort vorhanden ist:

- `src/hexagon/model/report_inputs.h`
- `src/adapters/cli/atomic_report_writer.h`
- `src/adapters/cli/atomic_report_writer.cpp`
- `tests/adapters/test_atomic_report_writer.cpp`

## Modellvertrag

### `ReportInputs`

Neues Value-Object in `src/hexagon/model/report_inputs.h`:

```cpp
namespace xray::hexagon::model {

enum class ReportInputSource {
    cli,
    not_provided,
};

enum class ChangedFileSource {
    compile_database_directory,
    file_api_source_root,
    cli_absolute,
};

struct ReportInputs {
    std::optional<std::string> compile_database_path;
    ReportInputSource compile_database_source{ReportInputSource::not_provided};

    std::optional<std::string> cmake_file_api_path;
    std::optional<std::string> cmake_file_api_resolved_path;
    ReportInputSource cmake_file_api_source{ReportInputSource::not_provided};

    std::optional<std::string> changed_file;
    std::optional<ChangedFileSource> changed_file_source;
};

}  // namespace xray::hexagon::model
```

Regeln:

- `ReportInputs` ist die alleinige Quelle fuer Report-Eingabepfade.
- Nicht verwendete optionale Eingaben werden als `std::nullopt` modelliert und spaeter in JSON als `null` ausgegeben.
- Leere Strings sind fuer fehlende Eingaben verboten.
- `changed_file` und `changed_file_source` sind nur fuer `impact` gesetzt.
- Bei `analyze` bleiben `changed_file` und `changed_file_source` `std::nullopt`.
- Bestehende skalare Felder `compile_database_path` und `changed_file` in Ergebnisobjekten werden entfernt oder nur noch als deprecated Mirror ohne eigene Semantik behalten. Falls sie fuer Rueckwaertskompatibilitaet temporaer bleiben, duerfen neue Adapter sie nicht als Quelle nutzen.

### Ergebnisobjekte

`AnalysisResult` erhaelt:

```cpp
ReportInputs inputs;
```

`ImpactResult` erhaelt:

```cpp
ReportInputs inputs;
```

Die Includes in `analysis_result.h` und `impact_result.h` werden entsprechend angepasst.

## Request- und Port-Vertrag

### `AnalyzeProjectRequest`

Bestehenden Request erweitern:

```cpp
struct AnalyzeProjectRequest {
    std::string_view compile_commands_path;
    std::string_view cmake_file_api_path;
    std::filesystem::path report_display_base;
};
```

`report_display_base` wird von der CLI gesetzt. In Tests wird sie explizit injiziert. Services duerfen fuer Report-Pfade nicht `std::filesystem::current_path()` lesen.

### `AnalyzeImpactRequest`

Den positionalen Impact-Port ersetzen oder gleichwertig erweitern:

```cpp
struct AnalyzeImpactRequest {
    std::string_view compile_commands_path;
    std::filesystem::path changed_file_path;
    std::string_view cmake_file_api_path;
    std::filesystem::path report_display_base;
};

class AnalyzeImpactPort {
public:
    virtual ~AnalyzeImpactPort() = default;
    virtual model::ImpactResult analyze_impact(AnalyzeImpactRequest request) const = 0;
};
```

Alle Aufrufer in CLI, Tests und Port-Wiring werden auf den Request umgestellt.

## File-API-Aufloesungsmetadaten

`BuildModelResult` erhaelt rohe, lexikalische Pfadmetadaten:

```cpp
std::optional<std::filesystem::path> cmake_file_api_input_path;
std::optional<std::filesystem::path> cmake_file_api_resolved_path;
std::optional<std::filesystem::path> cmake_file_api_source_root;
```

Regeln:

- Der `CmakeFileApiAdapter` setzt diese Werte, kennt aber keine Report-Display-Basis.
- Adapter liefern rohe lexikalische Pfade, keine host-spezifisch normalisierten Display-Strings.
- `ProjectAnalyzer` und `ImpactAnalyzer` konvertieren die Pfade relativ zu `report_display_base` in die `ReportInputs`-Display-Strings.
- Keine CLI- oder Adapterzustandswerte werden spaeter an Reportadapter nachgereicht.

## Display-Pfadregeln

Eine gemeinsame Hilfsfunktion in der Hexagon-Service-Schicht oder einem kleinen lokalen Helper:

```cpp
std::optional<std::string> to_report_display_path(
    std::optional<std::filesystem::path> path,
    const std::filesystem::path& report_display_base);
```

Regeln:

- Relative CLI-Pfade werden lexikalisch normalisiert.
- Absolute Pfade bleiben absolute Anzeige-Strings.
- Es erfolgt keine zusaetzliche kanonische Aufloesung ueber Symlinks, Home- oder Temp-Pfade.
- Goldens mit Absolutpfaden verwenden nur fixture-stabile Pfade wie `/project/...`.
- Interne Normalisierungsschluessel werden nicht in `ReportInputs` serialisiert.

## `changed_file`-Provenienz

`ImpactAnalyzer` setzt:

- `changed_file_source = cli_absolute`, wenn `changed_file_path` absolut ist.
- `changed_file_source = compile_database_directory`, wenn `changed_file_path` relativ ist und `compile_commands_path` gesetzt ist.
- `changed_file_source = file_api_source_root`, wenn `changed_file_path` relativ ist, keine Compile Database gesetzt ist und File-API-Metadaten eine Source-Root liefern.

Mixed-Input-Prioritaet:

- Bei `--compile-commands` plus `--cmake-file-api` gewinnt fuer relative `--changed-file` die Compile-Database-Directory.

CLI-Regel:

- `impact` ohne `--changed-file` bleibt ein CLI-Fehler. Es gibt kein JSON-`changed_file_source=not_provided`.

## CLI-Formatvertrag

Formatwerte:

- `console`
- `markdown`
- `html`
- `json`
- `dot`

`--format`-Validierung:

- Unbekannte Werte bleiben CLI-Verwendungsfehler mit Exit-Code `2`.
- Help-Text nennt alle fuenf Werte.

`--output`-Validierung:

- Erlaubt fuer `markdown`, `html`, `json`, `dot`.
- Verboten fuer `console`.
- Verbotene Kombination bleibt Exit-Code `2`.
- Fehlermeldung nennt, dass `--output` nur fuer artefaktorientierte Formate erlaubt ist.

Report-Port-Auswahl:

- `console` und `markdown` nutzen weiter bestehende Ports.
- Fuer `html`, `json`, `dot` duerfen bis zur Implementierung der Adapter Stub-/Placeholder-Ports nur dann verdrahtet werden, wenn sie in Tests klar als noch nicht freigegebene Implementierung markiert sind. Besser ist, AP 1.1 nur Formatvalidierung und Port-Schnittstellen vorzubereiten und die konkrete Verdrahtung in AP 1.2 bis AP 1.4 zu finalisieren.

## `--output`-Schreibvertrag

Mit `--output` gilt fuer alle artefaktorientierten Formate:

- Reportinhalt wird ausschliesslich in die Datei geschrieben.
- Erfolgreicher stdout bleibt leer.
- Warnungen und Fehler gehen nach `stderr`.
- Es gibt keine Erfolgsmeldung auf stdout.
- Bestehende Zieldateien werden nur bei vollstaendig erfolgreichem Rendern, Schreiben, Flushen und Replace ersetzt.
- Bei Render-, Schreib-, Flush- oder Replace-Fehler bleibt eine bestehende Zieldatei unveraendert.

Atomic-Writer-Vertrag:

- Temp-Datei wird im Zielverzeichnis angelegt.
- Temp-Datei wird kollisionssicher und exklusiv erzeugt.
- Temp-Dateiname nutzt ein erkennbares Praefix, zum Beispiel `.cmake-xray-`.
- POSIX nutzt `rename` oder `renameat` fuer den finalen Replace.
- Windows nutzt `ReplaceFileW` oder `MoveFileExW` mit `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`.
- Zielpfad darf vor dem Replace nicht geloescht werden.
- Zurueckbleibende Temp-Dateien nach Prozessabbruch sind erlaubt, aber unter dem Zielnamen darf nie ein teilgeschriebener Report sichtbar werden.

## Verbosity-Grenze

AP 1.1 fuehrt keinen allgemeinen `OutputVerbosity`-Parameter in Reportports ein.

Regeln:

- `ReportWriterPort`, `GenerateReportPort` und fachliche Formatadapter bleiben frei von `OutputVerbosity`.
- Artefaktinhalte werden nur durch Ergebnisdaten, Format und bei `analyze` durch `--top` bestimmt.
- `--format console` ist der einzige CLI-owned Emissionspfad.
- Console-Normalmodus bleibt fuer bestehende Compile-Database-only-Goldens byte-stabil.
- Console-Quiet darf nur Ergebnisstatus, zentrale Zaehler und Warn-/Fehlerhinweise ausgeben.
- Console-Verbose darf zusaetzlich Eingabeprovenienz, Beobachtungsherkunft, Target-Metadatenstatus und vollstaendige Diagnostics ausgeben.
- JSON, HTML, Markdown und DOT duerfen durch Quiet/Verbose nicht inhaltlich veraendert werden.

## Adaptergrenzen

Alle neuen Reportadapter muessen spaeter ausschliesslich aus `AnalysisResult` oder `ImpactResult` rendern.

Verboten:

- CLI-Optionen direkt in Formatadapter reichen.
- Adapter aus globalem Prozesszustand lesen lassen.
- Adapter mit CMake-File-API-Adapterzustand koppeln.
- Adapter eigene Ranking-, Impact- oder Pfadauflogik treffen lassen.

Erlaubt:

- Gemeinsame reine Formatierungshelper fuer Escaping, stabile Sortierung, Pfadanzeige und Diagnostics.
- Adaptereigene Darstellung vorhandener Daten.

## Rueckwaertskompatibilitaet

Pflicht:

- Bestehende M3-/M4-Compile-Database-only-Goldens fuer `console` und `markdown` bleiben ohne neue Optionen byte-stabil.
- `--output` mit Markdown behaelt den M3-stdout-Vertrag: Erfolg laesst stdout leer.

Erlaubte Golden-Aenderung:

- M4-File-API- und Mixed-Input-Goldens duerfen sich gezielt aendern, wenn nur neue `ReportInputs`-Provenienz sichtbar wird.
- Jede solche Aktualisierung muss im Testnamen oder Testkommentar als `ReportInputs`-Provenienz-Aenderung nachvollziehbar sein.

## Implementierungsreihenfolge

1. `ReportInputs` und Enums einfuehren.
2. `AnalysisResult` und `ImpactResult` um `inputs` erweitern.
3. `AnalyzeProjectRequest` um `report_display_base` erweitern.
4. `AnalyzeImpactRequest` einfuehren und CLI/Tests auf Request umstellen.
5. `BuildModelResult` um rohe File-API-Aufloesungsmetadaten erweitern.
6. `ProjectAnalyzer` setzt `ReportInputs` fuer `analyze`.
7. `ImpactAnalyzer` setzt `ReportInputs` fuer `impact`, inklusive `changed_file_source`.
8. Deprecated skalare Pfadfelder entfernen oder als Mirror aus `ReportInputs` befuellen.
9. CLI-Formatvalidierung auf fuenf Werte erweitern.
10. `--output`-Validierung fuer artefaktorientierte Formate erweitern und `console --output` ablehnen.
11. Atomic-Write-/Replace-Wrapper einfuehren.
12. CLI-Schreibpfad auf Atomic-Writer umstellen.
13. Console-Quiet/Verbose als CLI-owned Emission vorbereiten, ohne Reportports zu erweitern.
14. Tests aktualisieren und Golden-Kompatibilitaet pruefen.

## Tests

Unit-/Service-Tests:

- `ReportInputs` bei Compile-Database-only-Analyze:
  - `compile_database_path` gesetzt
  - `compile_database_source=cli`
  - File-API-Felder `nullopt`
- `ReportInputs` bei File-API-only-Analyze:
  - `compile_database_path=nullopt`
  - `cmake_file_api_path` gesetzt
  - `cmake_file_api_resolved_path` aus `BuildModelResult`
  - `cmake_file_api_source=cli`
- `ReportInputs` bei Mixed-Analyze:
  - Compile Database und File API beide gesetzt
  - resolved File-API-Pfad kommt aus `BuildModelResult`
- Impact mit relativem `changed_file` plus Compile Database:
  - `changed_file_source=compile_database_directory`
- Impact mit relativem `changed_file` plus File API only:
  - `changed_file_source=file_api_source_root`
- Impact mit absolutem `changed_file`:
  - `changed_file_source=cli_absolute`
- Services verwenden `report_display_base`; ein veraendertes Prozess-CWD darf Ergebnisse nicht aendern.
- `AnalyzeImpactRequest` transportiert `compile_commands_path`, `changed_file_path`, `cmake_file_api_path`, `report_display_base`.

CLI-Tests:

- `--format console|markdown|html|json|dot` wird akzeptiert.
- ungueltiges `--format` ergibt Exit-Code `2`.
- `--output` mit `markdown|html|json|dot` wird akzeptiert.
- `--format console --output out.txt` ergibt Exit-Code `2`.
- erfolgreicher `--output`-Aufruf laesst stdout leer.
- Fehler und Warnungen gehen nach `stderr`.
- `--quiet --format json|html|markdown|dot` ohne `--output` gibt stdout byte-identisch zum Normalmodus aus.
- `--quiet` und `--verbose` veraendern keine Artefaktinhalte.

Atomic-Writer-Tests:

- neue Datei wird vollstaendig geschrieben.
- vorhandene Datei wird bei Erfolg ersetzt.
- vorhandene Datei bleibt bei simuliertem Render-/Write-/Flush-/Replace-Fehler unveraendert.
- Temp-Datei wird exklusiv erstellt; eine vorhandene Temp-Datei wird nicht ueberschrieben.
- Windows-Pfad nutzt die Windows-Replace-Implementierung oder einen abstrahierten Test-Doppelgaenger mit derselben Semantik.

Regressionstests:

- bestehende Compile-Database-only-Console-Goldens bleiben byte-stabil.
- bestehende Compile-Database-only-Markdown-Goldens bleiben byte-stabil.
- File-API-/Mixed-Golden-Aenderungen sind auf `ReportInputs`-Provenienz begrenzt.

## Abnahmekriterien

AP 1.1 ist abgeschlossen, wenn:

- die neuen Modell- und Request-Vertraege kompiliert und in Services genutzt werden;
- kein neuer Reportadapter CLI-Kontext benoetigt;
- `console --output` abgelehnt wird;
- `--output` fuer artefaktorientierte Formate stdout leer laesst;
- atomarer Write bestehende Zielartefakte bei Fehlern nicht veraendert;
- `AnalyzeImpactPort` nicht mehr vom positionalen Altvertrag abhaengt oder dieser eindeutig als deprecated Wrapper ueber `AnalyzeImpactRequest` implementiert ist;
- `ReportInputs` fuer Analyze und Impact vollstaendig in Service-Tests abgedeckt ist;
- Compile-Database-only-Console-/Markdown-Goldens byte-stabil bleiben;
- `git diff --check` sauber ist.

## Offene Folgearbeiten

Diese Punkte werden bewusst in spaeteren M5-Arbeitspaketen umgesetzt:

- `docs/report-json.md` und `docs/report-json.schema.json`
- `JsonReportAdapter`
- `HtmlReportAdapter`
- `DotReportAdapter`
- DOT-Budget- und Truncation-Implementierung
- HTML-/JSON-/DOT-Goldens
- Release-Workflow und `--version`
