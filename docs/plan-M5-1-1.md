# Plan M5 - AP 1.1 Report-Vertraege fuer neue Formate und Formatversionierung schaerfen

## Ziel

AP 1.1 bereitet die bestehende M3-/M4-Reportstrecke so vor, dass die nachfolgenden M5-Arbeitspakete HTML, JSON und DOT implementieren koennen, ohne Fachlogik in Adapter zu verschieben oder bestehende Console-/Markdown-Vertraege unkontrolliert zu aendern.

Nach AP 1.1 ist noch kein vollstaendiger HTML-/JSON-/DOT-Adapter erforderlich. Erforderlich sind die stabilen Modell-, Port-, CLI- und Datei-Schreibvertraege, auf denen AP 1.2 bis AP 1.4 direkt aufbauen.

## Scope

Umsetzen:

- CLI kennt die M5-Formatwerte `console`, `markdown`, `html`, `json`, `dot`.
- `console` und `markdown` bleiben in AP 1.1 die einzigen lauffaehigen Formate; `html`, `json` und `dot` werden als bekannte, aber noch nicht implementierte Formate mit eindeutiger Fehlermeldung abgelehnt, bis AP 1.2 bis AP 1.4 die jeweiligen Adapter verdrahten.
- `--output` bleibt in AP 1.1 fuer das bestehende lauffaehige Artefaktformat `markdown` erlaubt und fuer `console` verboten; `html`, `json` und `dot` uebernehmen den Schreibvertrag erst mit ihrer Adapter-Implementierung.
- Erfolgreiche `--output`-Aufrufe fuer lauffaehige Artefaktformate schreiben Reports ausschliesslich in die Datei; stdout bleibt leer.
- Reportdateien werden ueber einen gemeinsamen atomaren Schreibpfad geschrieben.
- `AnalysisResult` und `ImpactResult` enthalten ein gemeinsames `ReportInputs`-Value-Object.
- `AnalyzeProjectRequest` und `AnalyzeImpactRequest` transportieren alle Eingabepfade und eine explizite Display-Basis.
- `BuildModelResult` transportiert rohe File-API-Aufloesungsmetadaten ohne parallele Source-Root-Wahrheit.
- Report-Ports bleiben ergebnisobjektzentriert; Adapter bekommen keinen CLI-Kontext.
- Quiet/Verbose bleibt fuer AP 1.5 reserviert; AP 1.1 stellt nur sicher, dass keine Verbosity-Parameter in Reportports oder Artefaktadapter eingefuehrt werden.

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
- `src/hexagon/services/analysis_support.{h,cpp}`
- `src/hexagon/services/project_analyzer.{h,cpp}`
- `src/hexagon/services/impact_analyzer.{h,cpp}`
- `src/adapters/input/cmake_file_api_adapter.{h,cpp}`
- `src/adapters/cli/cli_adapter.{h,cpp}`
- `src/adapters/CMakeLists.txt`
- `src/main.cpp`
- `tests/CMakeLists.txt`
- `tests/adapters/test_cmake_file_api_adapter.cpp`
- `tests/hexagon/test_analysis_support.cpp`
- `tests/hexagon/test_project_analyzer.cpp`
- `tests/hexagon/test_impact_analyzer.cpp`
- `tests/e2e/test_cli.cpp`
- `tests/adapters/test_port_wiring.cpp`

Neue Dateien, falls kein passender Ort vorhanden ist:

- `src/hexagon/model/report_inputs.h`
- `src/hexagon/model/report_format_version.h`
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
    unresolved_file_api_source_root,
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

- `ReportInputs` ist die kanonische Quelle fuer neue M5-Artefaktadapter und den spaeteren JSON-Vertrag.
- Nicht verwendete optionale Eingaben werden im Modell als `std::nullopt` modelliert.
- Die spaetere JSON-Serialisierung gibt alle im jeweiligen Reporttyp definierten `inputs`-Felder immer aus; fehlende Eingaben werden dort als JSON-`null` serialisiert.
- AP 1.1 darf keine offene Weglass-Option fuer Felder einfuehren, die im M5-JSON-Vertrag fuer den jeweiligen Reporttyp definiert sind.
- `changed_file` und `changed_file_source` gehoeren nur zum Impact-Vertrag; sie werden bei Analyze im Modell als `std::nullopt` gehalten, sind aber keine Analyze-JSON-Felder.
- Leere Strings sind fuer fehlende Eingaben verboten.
- `changed_file` und `changed_file_source` sind nur fuer `impact` gesetzt.
- Bei `analyze` bleiben `changed_file` und `changed_file_source` `std::nullopt`.
- Bestehende skalare Felder `compile_database_path` und `changed_file` in Ergebnisobjekten bleiben in AP 1.1 als Legacy-Presentation-Felder erhalten, solange bestehende Console-/Markdown-Adapter sie direkt rendern.
- Neue M5-Artefaktadapter duerfen diese Legacy-Felder nicht als Quelle nutzen.

### Formatversion

AP 1.1 legt die erste maschinenlesbare Reportformatversion fest, auch wenn JSON-Schema und JSON-Adapter erst in AP 1.2 folgen.

Vertrag:

- `format_version` startet mit dem Integerwert `1`.
- Die Konstante lebt in `src/hexagon/model/report_format_version.h`.
- JSON-Adapter, JSON-Schema, Dokumentation und Golden-Tests muessen spaeter dieselbe Konstante beziehungsweise denselben Wert verwenden.
- AP 1.1 implementiert noch keinen JSON-Renderer, aber verhindert, dass AP 1.2 die Versionierungsentscheidung neu treffen muss.

### Ergebnisobjekte

`AnalysisResult` erhaelt:

```cpp
ReportInputs inputs;
```

`ImpactResult` erhaelt:

```cpp
ReportInputs inputs;
```

### Legacy-Presentation-Felder fuer Console/Markdown

AP 1.1 migriert bestehende Console-/Markdown-Adapter nicht auf `ReportInputs`, weil diese Formate byte-stabil bleiben muessen.

Regeln:

- `AnalysisResult::compile_database_path` und `ImpactResult::compile_database_path` behalten exakt die bisher sichtbare Bedeutung fuer bestehende Console-/Markdown-Ausgaben.
- `ImpactResult::changed_file` behaelt exakt die bisher sichtbare Bedeutung fuer bestehende Console-/Markdown-Ausgaben.
- Bei File-API-only-Laeufen bleibt das alte `compile_database_path`-Legacy-Feld so befuellt wie vor AP 1.1, auch wenn `ReportInputs.compile_database_path == std::nullopt` ist und `ReportInputs.cmake_file_api_path` die neue Provenienz traegt.
- Bei Mixed-Input-Laeufen bleiben die alten Legacy-Felder so befuellt, dass bestehende Console-/Markdown-Goldens unveraendert bleiben.
- Legacy-Felder duerfen in AP 1.1 aus bestehender Rueckwaertskompatibilitaetslogik befuellt werden; sie sind keine Quelle fuer neue HTML-/JSON-/DOT-Adapter.
- Eine spaetere Migration von Console/Markdown auf `ReportInputs` ist ein eigenes Arbeitspaket und muss eigene Golden-Aenderungen explizit begruenden.

### `ReportInputs` bei Fehlerergebnissen

`ProjectAnalyzer` und `ImpactAnalyzer` erzeugen `ReportInputs` aus dem Request, bevor Compile Database oder File API geladen werden.

Regeln:

- Wenn ein Service ein `AnalysisResult` oder `ImpactResult` mit Diagnostics oder unvollstaendigen Daten zurueckgibt, muss `inputs` trotzdem aus dem Request befuellt sein.
- Fehler beim Laden von Compile Database oder File API duerfen die bekannte Eingabeprovenienz nicht auf `nullopt` zuruecksetzen.
- Wenn bei File-API-only-Impact ein relativer `changed_file_path` vorliegt und das File-API-Laden fehlschlaegt, bevor eine Source-Root bekannt ist, wird `inputs.changed_file` als normalisierter roher CLI-Relativpfad gesetzt und `inputs.changed_file_source=unresolved_file_api_source_root` verwendet.
- `unresolved_file_api_source_root` ist eine Fehlerergebnis-Provenienz und darf bei erfolgreichen Impact-Ergebnissen nicht auftreten.
- Ausgenommen sind reine CLI-Parser-/Verwendungsfehler, bei denen kein Service aufgerufen und kein Ergebnisobjekt erzeugt wird.
- AP 1.1 definiert keinen stabilen `ReportInputs`-Vertrag fuer ungefangene Exceptions, die kein Ergebnisobjekt erzeugen.

Die Includes in `analysis_result.h` und `impact_result.h` werden entsprechend angepasst.

## Request- und Port-Vertrag

### `AnalyzeProjectRequest`

Bestehenden Request erweitern:

```cpp
struct InputPathArgument {
    std::filesystem::path path;
    bool was_relative{false};
};

struct AnalyzeProjectRequest {
    std::optional<InputPathArgument> compile_commands_path;
    std::optional<InputPathArgument> cmake_file_api_path;
    std::filesystem::path report_display_base;
};
```

`report_display_base` und `was_relative` werden von der CLI gesetzt. In Tests werden sie explizit injiziert. Services duerfen fuer Report-Pfade nicht `std::filesystem::current_path()` lesen und duerfen die urspruengliche Relativitaet eines Arguments nicht aus einem spaeter aufgeloesten Pfad ableiten.

### `AnalyzeImpactRequest`

Den positionalen Impact-Port ersetzen:

```cpp
struct AnalyzeImpactRequest {
    std::optional<InputPathArgument> compile_commands_path;
    InputPathArgument changed_file_path;
    std::optional<InputPathArgument> cmake_file_api_path;
    std::filesystem::path report_display_base;
};

class AnalyzeImpactPort {
public:
    virtual ~AnalyzeImpactPort() = default;
    virtual model::ImpactResult analyze_impact(AnalyzeImpactRequest request) const = 0;
};
```

Alle Produktionsaufrufer in CLI, Composition Root, Tests und Port-Wiring werden auf den Request umgestellt. Der alte positionale virtuelle Portvertrag wird nicht als Produktionspfad behalten, weil er `report_display_base` und `was_relative` nicht korrekt transportieren kann. Falls ein temporaerer nicht-virtueller Testhelper fuer alte Testdaten noetig ist, muss er intern einen vollstaendigen `AnalyzeImpactRequest` mit expliziter Display-Basis bauen und darf nicht von CLI oder Port-Wiring genutzt werden.

### `analysis_support`-Pfadbasis

Bestehende Hilfsfunktionen in `src/hexagon/services/analysis_support.{h,cpp}` duerfen fuer Report- und Impact-Pfadbasen nicht mehr implizit `std::filesystem::current_path()` lesen.

Regeln:

- `compile_commands_base_directory()` oder ein gleichwertiger Ersatz bekommt die benoetigte Fallback-Basis explizit aus dem Request-Kontext.
- Wenn `compile_commands_path` keinen Parent hat, wird `report_display_base` beziehungsweise die fachliche Request-Basis verwendet, nicht der aktuelle Prozess-CWD.
- Alte Helper-Ueberladungen, die ihre Fallback-Basis selbst aus Prozesszustand ableiten, werden entfernt oder auf Testcode beschraenkt.
- Kein Produktionspfad darf nach AP 1.1 eine pfadbasierte Service-Helper-API verwenden, die bei fehlendem Parent implizit `std::filesystem::current_path()` liest.
- Service-Tests muessen einen veraenderten Prozess-CWD abdecken, damit ReportInputs und Impact-Aufloesung stabil bleiben.

## File-API-Aufloesungsmetadaten

`BuildModelResult` erhaelt nur den vom Adapter tatsaechlich verwendeten, rohen File-API-Aufloesungspfad. Das bestehende Feld `source_root` bleibt die fachliche Source-Root fuer File-API-Ergebnisse und wird nicht parallel durch ein zweites Source-Root-Feld ersetzt:

```cpp
std::optional<std::filesystem::path> cmake_file_api_resolved_path;
std::string source_root; // bestehendes Feld, weiterhin File-API-Source-Root
```

Regeln:

- Der `CmakeFileApiAdapter` setzt diese Werte, kennt aber keine Report-Display-Basis.
- Adapter liefern rohe lexikalische Pfade, keine host-spezifisch normalisierten Display-Strings.
- `source_root` bleibt die Quelle fuer bestehende Analyse- und Impact-Basislogik; AP 1.1 fuehrt kein zusaetzliches `cmake_file_api_source_root` mit abweichender Semantik ein.
- Ein leerer `source_root`-String gilt als "Source-Root unbekannt"; ein nicht leerer String gilt als bekannte File-API-Source-Root. Services kapseln diese Regel in einem kleinen Helper, damit `unresolved_file_api_source_root` nicht ueber verstreute Leerstring-Pruefungen entsteht.
- `ReportInputs.cmake_file_api_path` kommt ausschliesslich aus `AnalyzeProjectRequest::cmake_file_api_path` oder `AnalyzeImpactRequest::cmake_file_api_path`, inklusive `was_relative` und `report_display_base`.
- `ReportInputs.cmake_file_api_resolved_path` kommt ausschliesslich aus `BuildModelResult::cmake_file_api_resolved_path` und wird in den Services ueber die Display-Pfadregeln konvertiert.
- Wenn File-API-Laden nach erfolgreicher Build-/Reply-Pfadaufloesung fehlschlaegt, bleibt `BuildModelResult::cmake_file_api_resolved_path` gesetzt und wird in Fehlerergebnissen nach `ReportInputs.cmake_file_api_resolved_path` uebernommen.
- Wenn File-API-Laden vor einer stabilen Build-/Reply-Pfadaufloesung fehlschlaegt, bleibt `BuildModelResult::cmake_file_api_resolved_path == std::nullopt`; Services duerfen in diesem Fall keinen Ersatzpfad erfinden.
- Der Adapter transportiert den originalen CLI-Eingabepfad nicht zurueck; er koennte `was_relative` und `report_display_base` nicht verlaesslich rekonstruieren.
- Keine CLI- oder Adapterzustandswerte werden spaeter an Reportadapter nachgereicht.

## Display-Pfadregeln

Eine gemeinsame Hilfsfunktion in der Hexagon-Service-Schicht oder einem kleinen lokalen Helper. Die Funktion bekommt neben dem Pfad auch die Display-Art, damit ein bereits aufgeloester Pfad nicht versehentlich wie ein originaler CLI-String behandelt wird:

```cpp
enum class ReportPathDisplayKind {
    input_argument,
    resolved_adapter_path,
};

struct ReportPathDisplayInput {
    std::optional<std::filesystem::path> path;
    ReportPathDisplayKind kind{ReportPathDisplayKind::input_argument};
    bool was_relative{false};
};

std::optional<std::string> to_report_display_path(
    ReportPathDisplayInput input,
    const std::filesystem::path& report_display_base);
```

Regeln fuer normale Eingabepfade:

- `compile_database_path` und `cmake_file_api_path` werden als `input_argument` behandelt.
- `was_relative` wird beim Request-Aufbau aus dem urspruenglichen CLI-Argument bestimmt und nicht spaeter aus einem aufgeloesten Pfad rekonstruiert.
- `report_display_base` dient nur als explizite Basis fuer relative normale Eingabepfade und Adapterpfade, damit Services nicht vom Prozess-CWD abhaengen.
- Relative normale Eingabepfade werden gegen `report_display_base` interpretiert und danach als lexikalisch normalisierte Anzeige-Strings relativ zu `report_display_base` ausgegeben, wenn sie auch relativ eingegeben wurden.
- Absolute normale Eingabepfade werden nicht relativiert; sie bleiben absolute Anzeige-Strings.

Beispiele mit `report_display_base=/repo`:

- CLI-Eingabe `build/compile_commands.json` wird als `build/compile_commands.json` angezeigt.
- CLI-Eingabe `./build/../out/compile_commands.json` wird als `out/compile_commands.json` angezeigt.
- CLI-Eingabe `../repo/build/compile_commands.json` wird als `build/compile_commands.json` angezeigt, weil der Pfad relativ zur Display-Basis wieder in `/repo` liegt.
- CLI-Eingabe `../other/build/compile_commands.json` wird als `../other/build/compile_commands.json` angezeigt.
- CLI-Eingabe `/repo/build/compile_commands.json` bleibt `/repo/build/compile_commands.json`, weil absolute Eingaben nicht relativiert werden.

Regeln fuer aufgeloeste Adapterpfade:

- `cmake_file_api_resolved_path` wird als `resolved_adapter_path` behandelt.
- Der Adapter liefert den rohen lexikalischen Build- oder Reply-Pfad, den er tatsaechlich verwendet hat.
- Ist dieser Adapterpfad relativ, wird er gegen `report_display_base` interpretiert und als normalisierter relativer Anzeige-String ausgegeben.
- Ist dieser Adapterpfad absolut, bleibt er absolut; Goldens mit solchen Pfaden verwenden fixture-stabile Werte wie `/project/...`.

Gemeinsame Grenzen:

- `changed_file` nutzt diese allgemeine Display-Regel nicht; dafuer gilt die separate Provenienzregel im naechsten Abschnitt.
- Es erfolgt keine zusaetzliche kanonische Aufloesung ueber Symlinks, Home- oder Temp-Pfade.
- Interne Normalisierungsschluessel werden nicht in `ReportInputs` serialisiert.

## `changed_file`-Provenienz

`ImpactAnalyzer` setzt `changed_file` und `changed_file_source` ueber eine eigene Regel. `report_display_base` wird nicht direkt als Display-Basis fuer `changed_file` verwendet, weil relative `--changed-file`-Werte fachlich gegen die Impact-Provenienz-Basis aufgeloest werden. Indirekt kann `report_display_base` relevant sein, wenn die Compile-Database-Directory fuer einen pfadlosen `compile_commands_path` zuerst ueber die explizite Request-Basis bestimmt werden muss.

Display- und Aufloesungsregeln:

- Die Provenienzentscheidung nutzt ausschliesslich `AnalyzeImpactRequest::changed_file_path.was_relative`, nicht `changed_file_path.path.is_absolute()`.
- Die CLI soll `InputPathArgument::path` als rohen lexikalischen CLI-Pfad transportieren. Falls eine spaetere Vorbereitung den Pfad bereits absolut oder normalisiert ablegt, muss `was_relative` trotzdem die urspruengliche CLI-Relativitaet bewahren und die Provenienz steuern.
- Ist `changed_file_path.was_relative == false`, wird `changed_file` als lexikalisch normalisierter absoluter String ausgegeben und `changed_file_source=cli_absolute` gesetzt.
- Ist `changed_file_path.was_relative == true` und `compile_commands_path` gesetzt, wird der Pfad fuer Analyse und Anzeige gegen die Compile-Database-Directory interpretiert. `changed_file` bleibt der lexikalisch normalisierte relative Pfad zu dieser Directory, nicht relativ zu `report_display_base`.
- Ist `changed_file_path.was_relative == true`, keine Compile Database gesetzt und eine File-API-Source-Root vorhanden, wird der Pfad fuer Analyse und Anzeige gegen diese Source-Root interpretiert. `changed_file` bleibt der lexikalisch normalisierte relative Pfad zu dieser Source-Root.
- Ist `changed_file_path.was_relative == true`, keine Compile Database gesetzt und wegen File-API-Ladefehler keine Source-Root bekannt, wird `changed_file` als normalisierter roher CLI-Relativpfad ausgegeben und `changed_file_source=unresolved_file_api_source_root` gesetzt.
- Wenn `was_relative == true`, aber `path` bereits absolut vorliegt, wird der Anzeige-String relativ zur jeweiligen Provenienz-Basis berechnet; dadurch bleibt ein urspruenglich relatives `--changed-file` auch nach vorgelagerter Normalisierung ein relativer ReportInput.

Quellen:

- `changed_file_source = cli_absolute`, wenn `changed_file_path.was_relative == false`.
- `changed_file_source = compile_database_directory`, wenn `changed_file_path.was_relative == true` und `compile_commands_path` gesetzt ist.
- `changed_file_source = file_api_source_root`, wenn `changed_file_path.was_relative == true`, keine Compile Database gesetzt ist und File-API-Metadaten eine Source-Root liefern.
- `changed_file_source = unresolved_file_api_source_root`, wenn `changed_file_path.was_relative == true`, keine Compile Database gesetzt ist und File-API-Metadaten wegen Ladefehler keine Source-Root liefern.

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
- Bekannte, aber im aktuellen Build noch nicht lauffaehige Werte `html`, `json` und `dot` liefern ebenfalls Exit-Code `2`, aber mit eigener Fehlermeldung `--format <value> is recognized but not implemented in this build`.
- Help-Text nennt alle fuenf Werte.

Validierungsreihenfolge:

1. Syntax und Optionsform werden geparst. Unbekannte Optionen, fehlende Optionswerte und unbekannte `--format`-Werte liefern Exit-Code `2`.
2. Danach wird die Formatverfuegbarkeit geprueft. Ist das Format bekannt, aber in diesem Build nicht implementiert, liefert die CLI sofort den `not implemented in this build`-Fehler.
3. Fuer nicht implementierte Formate werden keine fachlichen Eingaben validiert, keine Adapter oder Ports ausgewaehlt, keine Analyse gestartet und keine Zieldatei erzeugt.
4. Erst fuer lauffaehige Formate folgen Eingabevalidierung, `--output`-Kombinationsvalidierung und Analyse.
5. Deshalb gewinnt bei `--format json|html|dot` der `not implemented`-Fehler gegen gleichzeitig fehlende Eingaben oder ein angegebenes `--output`.
6. Bei lauffaehigem `--format console` gewinnt dagegen die normale `--output`-Kombinationsvalidierung und `--format console --output out.txt` bleibt ein Verwendungsfehler.

CLI-Parser-Anpassung:

- `--changed-file` darf nicht mehr als Parser-`required()` modelliert sein, wenn diese Required-Pruefung vor der Formatverfuegbarkeit greift.
- Die Pflicht fuer `impact --changed-file` wird als fachliche Eingabevalidierung nach der Formatverfuegbarkeitspruefung umgesetzt.
- Dadurch liefert `impact --format json --cmake-file-api <path>` ohne `--changed-file` in AP 1.1 den stabilen `not implemented in this build`-Fehler und keinen Parser-Required-Fehler.
- Fuer lauffaehige Formate bleibt `impact` ohne `--changed-file` ein Verwendungsfehler mit Exit-Code `2`.

`--output`-Validierung:

- In AP 1.1 erfolgreich erlaubt nur fuer `markdown`.
- `html`, `json` und `dot` duerfen syntaktisch mit `--output` kombiniert werden, enden aber bis zur jeweiligen Adapter-Implementierung mit dem definierten `not implemented`-Fehler und duerfen keine Zieldatei erzeugen.
- Verboten fuer `console`.
- Verbotene Kombination bleibt Exit-Code `2`.
- Fehlermeldung nennt, dass `--output` nur fuer artefaktorientierte Formate erlaubt ist.

Report-Port-Auswahl:

- `console` und `markdown` nutzen weiter bestehende Ports.
- Fuer `html`, `json`, `dot` wird in AP 1.1 kein `GenerateReportPort` ausgewaehlt. Die CLI bricht vor Analyse und vor Dateierzeugung mit dem definierten `not implemented`-Fehler ab. AP 1.2 bis AP 1.4 ersetzen diese Sperre formatweise durch echte Port-Verdrahtung.

## Render-Fehlervertrag

Der bestehende Report-Port liefert fuer erfolgreiche Renderings weiter Reportinhalt als `std::string`. AP 1.1 muss fuer den gemeinsamen Dateischreibpfad trotzdem einen expliziten Fehlerkanal definieren, damit Render-Fehler vor dem Ersetzen der Zieldatei abgefangen werden koennen.

Zulaessige Umsetzung:

- Entweder wird ein kleines `RenderResult`-/`Expected<std::string, RenderError>`-Value-Object fuer den CLI-Schreibpfad eingefuehrt.
- Oder der CLI-Schreibpfad faengt dokumentierte Render-Exceptions aus Reportadaptern ab und mappt sie auf einen Render-Fehler.

Regeln:

- Render-Fehler werden vor Temp-Datei-Erstellung oder vor finalem Replace erkannt.
- Bei Render-Fehler bleibt eine bestehende Zieldatei unveraendert.
- Render-Fehler liefern einen CLI-Fehler-Exit-Code ungleich `0` und eine Fehlermeldung auf `stderr`.
- Erfolgreiche Renderings bleiben unveraendert stringbasiert; AP 1.1 fuehrt keinen fachlichen Adapterkontext in den Render-Fehlerkanal ein.

## `--output`-Schreibvertrag

Mit `--output` gilt in AP 1.1 fuer das lauffaehige Artefaktformat `markdown`; AP 1.2 bis AP 1.4 uebernehmen denselben Vertrag fuer `json`, `html` und `dot`:

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
- AP 1.1 garantiert leserseitig atomare Sichtbarkeit: Unter dem Zielnamen erscheint entweder der alte vollstaendige Inhalt oder der neue vollstaendige Inhalt.
- AP 1.1 garantiert keine Crash-Dauerhaftigkeit nach Stromausfall oder Kernel-Abbruch.
- Flush bedeutet hier Stream-/Handle-Flush zur Fehlererkennung vor dem Replace, nicht zwingend `fsync`, Directory-`fsync`, `_commit` oder `FlushFileBuffers`.
- POSIX nutzt `rename` oder `renameat` fuer den finalen Replace.
- Windows unterscheidet Zielzustaende:
  - fuer vorhandene Zielpfade nutzt der Wrapper `ReplaceFileW` oder `MoveFileExW` mit `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`;
  - fuer neue Zielpfade nutzt der Wrapper `MoveFileExW` mit `MOVEFILE_WRITE_THROUGH` ohne Replace-Erwartung oder eine gleichwertige exklusive Move-Primitive.
- Zielpfad darf vor dem Replace nicht geloescht werden.
- Zurueckbleibende Temp-Dateien nach Prozessabbruch sind erlaubt, aber unter dem Zielnamen darf nie ein teilgeschriebener Report sichtbar werden.

## Verbosity-Grenze

AP 1.1 implementiert keine `--quiet`-/`--verbose`-CLI-Semantik. Diese Optionen gehoeren zu AP 1.5. AP 1.1 legt nur die technische Grenze fest, damit AP 1.5 spaeter keine fachlichen Reportports veraendern muss.

Regeln:

- `ReportWriterPort`, `GenerateReportPort` und fachliche Formatadapter bleiben frei von `OutputVerbosity`.
- Artefaktinhalte werden nur durch Ergebnisdaten, Format und bei `analyze` durch `--top` bestimmt.
- `--format console` bleibt der einzige CLI-owned Emissionspfad, aber Quiet/Verbose-Varianten werden erst in AP 1.5 spezifiziert und getestet.
- AP 1.1-Tests pruefen nur, dass keine neuen `OutputVerbosity`-Parameter oder Adapter-Sonderpfade entstehen.

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
- Bestehende M4-File-API- und Mixed-Input-Goldens fuer `console` und `markdown` bleiben ohne neue Optionen byte-stabil.
- `--output` mit Markdown behaelt den M3-stdout-Vertrag: Erfolg laesst stdout leer.
- `ReportInputs` ist in AP 1.1 Modellvorbereitung fuer spaetere JSON-/HTML-/DOT-Adapter und wird nicht neu in bestehenden Console-/Markdown-Reports angezeigt.
- Legacy-Presentation-Felder bleiben fuer bestehende Console-/Markdown-Adapter bewusst erhalten und werden so befuellt, dass die Byte-Stabilitaet auch bei File-API-only- und Mixed-Input-Laeufen gilt.

Verboten:

- Console-/Markdown-Goldens duerfen in AP 1.1 nicht allein deshalb geaendert werden, weil `ReportInputs` intern verfuegbar ist.
- Neue Provenienzsichtbarkeit in Console oder Markdown gehoert nicht zu AP 1.1 und braucht ein eigenes spaeteres Arbeitspaket oder eine explizite Formatentscheidung.
- Neue HTML-/JSON-/DOT-Adapter duerfen nicht ueber die Legacy-Presentation-Felder implementiert werden.

## Implementierungsreihenfolge

1. `ReportInputs` und Enums einfuehren.
2. `format_version = 1` als fachliche Konstante einfuehren.
3. `AnalysisResult` und `ImpactResult` um `inputs` erweitern.
4. `AnalyzeProjectRequest` um `InputPathArgument` und `report_display_base` erweitern.
5. `AnalyzeImpactRequest` einfuehren und den positionalen virtuellen Impact-Portvertrag aus dem Produktionspfad entfernen.
6. `analysis_support`-Pfadhelper so erweitern, dass Fallback-Basen explizit aus dem Request kommen und nicht aus dem Prozess-CWD.
7. `BuildModelResult` um rohe File-API-Aufloesungsmetadaten erweitern.
8. `ProjectAnalyzer` setzt `ReportInputs` fuer `analyze`.
9. `ImpactAnalyzer` setzt `ReportInputs` fuer `impact`, inklusive `changed_file_source`.
10. Legacy-Presentation-Felder fuer bestehende Console-/Markdown-Adapter byte-stabil weiterbefuellen.
11. CLI-Formatvalidierung auf fuenf Werte erweitern.
12. `--output`-Validierung fuer artefaktorientierte Formate erweitern und `console --output` ablehnen.
13. Render-Fehlerkanal fuer den CLI-Schreibpfad definieren.
14. Atomic-Write-/Replace-Wrapper einfuehren.
15. CLI-Schreibpfad auf Atomic-Writer umstellen.
16. Tests aktualisieren und Golden-Kompatibilitaet pruefen.

## Tests

Unit-/Service-Tests:

- Formatversion:
  - fachliche Konstante liefert `format_version = 1`
  - Testname dokumentiert, dass AP 1.2 JSON diesen Wert wiederverwenden muss
- `ReportInputs` bei Compile-Database-only-Analyze:
  - `compile_database_path` gesetzt
  - `compile_database_source=cli`
  - File-API-Felder `nullopt`
- `ReportInputs` bei File-API-only-Analyze:
  - `compile_database_path=nullopt`
  - `cmake_file_api_path` gesetzt
  - `cmake_file_api_resolved_path` aus `BuildModelResult`
  - `cmake_file_api_source=cli`
  - `cmake_file_api_path` kommt aus dem Request, nicht aus `BuildModelResult`
  - altes `AnalysisResult::compile_database_path` bleibt fuer Console/Markdown mit dem bisherigen File-API-Anzeigewert befuellt
- `ReportInputs` bei Mixed-Analyze:
  - Compile Database und File API beide gesetzt
  - resolved File-API-Pfad kommt aus `BuildModelResult`
  - alte skalare Ergebnisfelder behalten die bisherigen Console-/Markdown-Anzeigewerte
- Impact mit relativem `changed_file` plus Compile Database:
  - `changed_file_source=compile_database_directory`
  - `changed_file` ist der normalisierte relative Pfad zur Compile-Database-Directory, zum Beispiel `src/lib.cpp`
- Impact mit relativem `changed_file` plus File API only:
  - `changed_file_source=file_api_source_root`
  - `changed_file` ist der normalisierte relative Pfad zur File-API-Source-Root, zum Beispiel `src/lib.cpp`
- Impact mit relativem `changed_file` plus File API only und File-API-Ladefehler vor bekannter Source-Root:
  - `changed_file_source=unresolved_file_api_source_root`
  - `changed_file` ist der normalisierte rohe CLI-Relativpfad
- Impact mit absolutem `changed_file`:
  - `changed_file_source=cli_absolute`
  - `changed_file` ist der normalisierte absolute Pfad, zum Beispiel `/project/src/lib.cpp`
- Impact mit urspruenglich relativem `changed_file`, dessen `InputPathArgument::path` bereits absolut vorbereitet wurde:
  - `changed_file_path.was_relative=true`
  - `changed_file_source` bleibt `compile_database_directory` oder `file_api_source_root`
  - `changed_file` bleibt ein relativer Anzeige-String zur jeweiligen Provenienz-Basis
- Services verwenden `report_display_base`; ein veraendertes Prozess-CWD darf Ergebnisse nicht aendern.
- `analysis_support`-Pfadhelper nutzen bei pfadlosen Compile-Database-Namen die explizite Request-Basis und nicht `std::filesystem::current_path()`.
- Kein Produktionscode ruft eine `analysis_support`-Helper-Ueberladung auf, die ihre Fallback-Basis implizit aus Prozesszustand ableitet.
- Relative normale Eingabepfade bleiben in `compile_database_path` und `cmake_file_api_path` als normalisierte relative Anzeige-Strings erhalten.
- Absolute normale Eingabepfade bleiben in `compile_database_path` und `cmake_file_api_path` als absolute Anzeige-Strings erhalten.
- `cmake_file_api_resolved_path` folgt der Adapterpfad-Regel und wird nicht aus dem originalen CLI-String rekonstruiert.
- File-API-Fehlerergebnis nach bereits bekannter Build-/Reply-Pfadaufloesung behaelt `cmake_file_api_resolved_path` in `ReportInputs`.
- File-API-Fehlerergebnis ohne stabile Build-/Reply-Pfadaufloesung setzt `ReportInputs.cmake_file_api_resolved_path=nullopt`.
- Leerer `BuildModelResult::source_root` wird als unbekannte Source-Root behandelt und fuehrt bei File-API-only-Impact mit relativem `changed_file` zu `changed_file_source=unresolved_file_api_source_root`.
- `AnalyzeImpactRequest` transportiert `compile_commands_path`, `changed_file_path`, `cmake_file_api_path`, `report_display_base` und die jeweilige `was_relative`-Information.
- Es gibt keinen virtuellen positionalen `AnalyzeImpactPort` mehr im CLI- oder Composition-Root-Pfad.
- Fehlerergebnisse aus `ProjectAnalyzer` und `ImpactAnalyzer`, zum Beispiel fehlgeschlagene Compile-Database- oder File-API-Ladevorgaenge, enthalten trotzdem die aus dem Request bekannten `ReportInputs`.

Adapter-Tests:

- `tests/adapters/test_cmake_file_api_adapter.cpp` prueft, dass `CmakeFileApiAdapter` bei Build-Dir-Eingabe den rohen aufgeloesten Build-/Reply-Pfad in `BuildModelResult::cmake_file_api_resolved_path` setzt.
- `tests/adapters/test_cmake_file_api_adapter.cpp` prueft denselben Vertrag bei direkter Reply-Dir-Eingabe.
- Adaptertests pruefen einen Fehlerfall nach erfolgreicher Build-/Reply-Pfadaufloesung, bei dem `cmake_file_api_resolved_path` gesetzt bleibt.
- Adaptertests pruefen einen Fehlerfall vor stabiler Build-/Reply-Pfadaufloesung, bei dem `cmake_file_api_resolved_path` `std::nullopt` bleibt.
- Adapter-Tests pruefen nur den rohen lexikalischen Adapterpfad; Display-Konvertierung relativ zu `report_display_base` bleibt Service-Testumfang.

CLI-Tests:

- `--format console|markdown` bleibt lauffaehig.
- `--format html|json|dot` wird als bekannter, aber in diesem Build noch nicht implementierter Wert erkannt, liefert Exit-Code `2`, schreibt keine Zieldatei und startet keine Analyse.
- Tests pruefen die stabile Fehlermeldung ohne Arbeitspaketnummer: `recognized but not implemented in this build`.
- `--format json|html|dot` mit fehlenden fachlichen Eingaben liefert trotzdem den `not implemented`-Fehler, weil Formatverfuegbarkeit vor Eingabevalidierung geprueft wird.
- `impact --format json --cmake-file-api <path>` ohne `--changed-file` liefert den `not implemented`-Fehler und keinen Parser-Required-Fehler.
- `--format json|html|dot --output out` liefert ebenfalls den `not implemented`-Fehler und erzeugt keine Datei.
- ungueltiges `--format` ergibt Exit-Code `2`.
- `--output` mit `markdown` wird akzeptiert.
- `--output` mit `html|json|dot` erzeugt bis zur jeweiligen Adapter-Implementierung denselben `not implemented`-Fehler und keine Zieldatei.
- `--format console --output out.txt` ergibt Exit-Code `2`.
- erfolgreicher `--output`-Aufruf laesst stdout leer.
- Fehler und Warnungen gehen nach `stderr`.
- Reportports und Formatadapter erhalten keinen `OutputVerbosity`-Parameter.

CLI-Schreibpfad-Tests:

- simulierter Render-Fehler erzeugt keine ersetzte Zieldatei, liefert Exit-Code ungleich `0` und schreibt eine Fehlermeldung nach `stderr`.
- Render-Fehler werden im CLI-Schreibpfad vor dem Atomic Writer abgefangen; der Atomic Writer bekommt nur fertige Bytes.

Atomic-Writer-Tests:

- neue Datei wird vollstaendig geschrieben.
- vorhandene Datei wird bei Erfolg ersetzt.
- vorhandene Datei bleibt bei simuliertem Write-/Flush-/Replace-Fehler unveraendert.
- Temp-Datei wird exklusiv erstellt; eine vorhandene Temp-Datei wird nicht ueberschrieben.
- neue Zielpfade und vorhandene Zielpfade werden auf Windows ueber die jeweils passende Move-/Replace-Primitive getestet.
- Tests pruefen leserseitige atomare Replace-Semantik, aber keine Crash-Dauerhaftigkeit nach Stromausfall.
- Windows-Pfad nutzt die Windows-Replace-Implementierung oder einen abstrahierten Test-Doppelgaenger mit derselben Semantik.

Regressionstests:

- bestehende Compile-Database-only-Console-Goldens bleiben byte-stabil.
- bestehende Compile-Database-only-Markdown-Goldens bleiben byte-stabil.
- bestehende File-API-Console- und File-API-Markdown-Goldens bleiben byte-stabil.
- bestehende Mixed-Input-Console- und Mixed-Input-Markdown-Goldens bleiben byte-stabil.
- Kein Regressionstest erwartet neue sichtbare `ReportInputs`-Provenienz in Console oder Markdown.
- File-API-only-Regressionen pruefen explizit, dass der bisherige sichtbare Pfad in Console/Markdown unveraendert bleibt, obwohl `ReportInputs.compile_database_path == std::nullopt` ist.

## Abnahmekriterien

AP 1.1 ist abgeschlossen, wenn:

- die neuen Modell- und Request-Vertraege kompiliert und in Services genutzt werden;
- `format_version = 1` als fachliche Konstante existiert und getestet ist;
- kein neuer Reportadapter CLI-Kontext benoetigt;
- `console --output` abgelehnt wird;
- `--output` fuer `markdown` stdout leer laesst;
- `html`, `json` und `dot` als bekannte, aber noch nicht implementierte Formate deterministisch vor Analyse und Dateierzeugung abgelehnt werden;
- `impact --format json|html|dot` ohne `--changed-file` nicht an Parser-Required-Validierung scheitert, sondern vor fachlicher Eingabevalidierung den `not implemented in this build`-Fehler liefert;
- atomarer Write bestehende Zielartefakte bei Fehlern nicht veraendert;
- `AnalyzeImpactPort` im Produktionspfad nur noch den `AnalyzeImpactRequest`-Vertrag anbietet;
- kein Produktionspfad mehr eine `analysis_support`-Helper-API nutzt, die ihre Fallback-Basis aus dem Prozess-CWD ableitet;
- `ReportInputs` fuer Analyze und Impact vollstaendig in Service-Tests abgedeckt ist;
- `ReportInputs` auch fuer Service-Fehlerergebnisse aus dem Request befuellt wird, sofern ein Ergebnisobjekt zurueckgegeben wird;
- File-API-only-Impact-Fehler vor bekannter Source-Root `changed_file_source=unresolved_file_api_source_root` setzen statt falsche Provenienz oder `nullopt` auszugeben;
- `CmakeFileApiAdapter` den rohen aufgeloesten Build-/Reply-Pfad in `BuildModelResult::cmake_file_api_resolved_path` setzt und Adaptertests Build-Dir- sowie Reply-Dir-Eingaben abdecken;
- Compile-Database-only-, File-API- und Mixed-Input-Console-/Markdown-Goldens byte-stabil bleiben;
- `ReportInputs` in AP 1.1 nicht neu in Console oder Markdown sichtbar wird;
- bestehende Console-/Markdown-Adapter nur ueber die Legacy-Presentation-Felder byte-stabil gehalten werden, waehrend neue M5-Artefaktadapter `ReportInputs` verwenden muessen;
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
