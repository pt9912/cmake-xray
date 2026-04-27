# Plan M5 - AP 1.5 CLI-Modi `--verbose` und `--quiet` einfuehren

## Ziel

AP 1.5 fuehrt command-lokale CLI-Ausgabemodi fuer diagnoseorientierte und automationsfreundlich knappe Ausgaben ein.

`--verbose` soll zusaetzlichen Kontext liefern, ohne maschinenlesbare oder artefaktorientierte Reports zu veraendern. `--quiet` soll erfolgreiche Console-Ausgaben reduzieren und bei Artefaktformaten keine Status- oder Erfolgstexte in stdout mischen. Beide Modi duerfen die bestehende Fehlersemantik nicht verwischen und duerfen keine Verbosity-Parameter in fachliche Ports oder Reportadapter tragen.

Nach AP 1.5 koennen `cmake-xray analyze --verbose`, `cmake-xray analyze --quiet`, `cmake-xray impact --verbose` und `cmake-xray impact --quiet` genutzt werden. Die Optionen sind command-lokal nach dem Subcommand positioniert und gegenseitig exklusiv.

## Scope

Umsetzen:

- command-lokale Optionen `--verbose` und `--quiet` fuer `analyze` und `impact`.
- gegenseitiger Ausschluss von `--verbose` und `--quiet`.
- CLI-internes Modell `OutputVerbosity` mit `normal`, `quiet` und `verbose`.
- Console-Quiet-Emission fuer Analyze und Impact.
- Console-Verbose-Emission fuer Analyze und Impact.
- Verbose-`stderr`-Diagnostik fuer Artefaktformate, ohne Reportinhalte zu veraendern.
- Quiet-Vertrag fuer Artefaktformate: stdout-Reports und Dateireports bleiben byte-identisch zum Normalmodus.
- Fehlerkontext in Verbose-Modus fuer CLI-/Eingabe-/Render-/Schreibfehler, ohne Stacktraces oder interne Implementierungsdetails.
- Golden- und CLI-Tests fuer Quiet, Verbose, gegenseitigen Ausschluss, command-lokale Position und Artefakt-Byte-Stabilitaet.

Nicht veraendern:

- Fachliche Analyseentscheidungen in `ProjectAnalyzer` oder `ImpactAnalyzer`.
- `ReportWriterPort`, `GenerateReportPort` oder Adapter-Signaturen.
- JSON-, DOT-, HTML- und Markdown-Artefaktinhalte im Quiet-/Verbose-Modus.
- bestehende Console-Normalmodus-Goldens.
- Exit-Code-Semantik.
- `--top`-Semantik.

## Nicht umsetzen

Nicht Bestandteil von AP 1.5:

- globale `--quiet`-/`--verbose`-Optionen vor dem Subcommand.
- `--debug`, Stacktraces, Exception-Typen oder interne Quellpfade in Fehlermeldungen.
- neue JSON-/HTML-/DOT-/Markdown-Felder nur fuer Verbose.
- ein allgemeiner `OutputVerbosity`-Parameter in Ports oder Adaptern.
- Logging-Framework oder konfigurierbare Log-Level.
- persistente Konfigurationsdateien fuer Verbosity.
- Erfolgsmeldungen bei Dateiausgabe.
- Aenderungen an Release-Artefakten oder OCI-Image.

Diese Punkte bleiben ausserhalb von M5 oder folgen in spaeteren Arbeitspaketen.

## Voraussetzungen aus AP 1.1 bis AP 1.4

AP 1.5 baut auf folgenden M5-Vertraegen auf:

- `ReportInputs` ist in `AnalysisResult` und `ImpactResult` vorhanden.
- `ReportInputs` transportiert Eingabeprovenienz fuer neue M5-Artefaktadapter.
- `AnalyzeProjectRequest` und `AnalyzeImpactRequest` transportieren Eingabepfade und `report_display_base`.
- `ReportWriterPort` und `GenerateReportPort` bleiben ergebnisobjektzentriert.
- `markdown`, `json`, `dot` und `html` sind artefaktorientierte Formate und koennen mit `--output` geschrieben werden.
- `console` bleibt standardmaessig stdout-orientiert und unterstuetzt `--output` nicht.
- Artifact-Reports werden vor Emission vollstaendig gerendert und bei `--output` ueber den gemeinsamen Atomic-Writer geschrieben.

AP 1.5 darf bestehende AP-1.1- bis AP-1.4-Goldens nur fuer explizite neue Quiet-/Verbose-Goldenfaelle erweitern. Normalmodus-Goldens bleiben byte-stabil.

Falls HTML aus AP 1.4 noch nicht umgesetzt ist, muss AP 1.5 die `html`-Regeln als Vertrag vorbereiten, aber Tests fuer lauffaehiges `--format html` duerfen erst mit AP 1.4 aktiv werden. Falls HTML bereits umgesetzt ist, gelten alle AP-1.5-Artefaktstabilitaetsregeln auch fuer HTML.

## Dateien

Voraussichtlich zu aendern:

- `src/adapters/cli/cli_adapter.{h,cpp}`
- `src/main.cpp`
- `tests/e2e/test_cli.cpp`
- `tests/e2e/run_e2e.sh`
- `README.md`
- `docs/guide.md`
- `docs/quality.md`

Neue Dateien, falls noetig:

- `src/adapters/cli/output_verbosity.h`
- `tests/e2e/testdata/m5/verbosity/console-analyze-quiet.txt`
- `tests/e2e/testdata/m5/verbosity/console-analyze-verbose.txt`
- `tests/e2e/testdata/m5/verbosity/console-impact-quiet.txt`
- `tests/e2e/testdata/m5/verbosity/console-impact-verbose.txt`
- weitere Golden-Dateien fuer `stderr`-Verbose-Fehlerkontext, falls der bestehende Testaufbau Golden-Dateien statt Inline-Assertions bevorzugt

## CLI-Vertrag

### Optionsposition

`--quiet` und `--verbose` sind command-lokale Optionen:

- gueltig: `cmake-xray analyze --quiet ...`
- gueltig: `cmake-xray analyze --verbose ...`
- gueltig: `cmake-xray impact --quiet ...`
- gueltig: `cmake-xray impact --verbose ...`
- nicht Teil des M5-Vertrags: `cmake-xray --quiet analyze ...`
- nicht Teil des M5-Vertrags: `cmake-xray --verbose impact ...`

Regeln:

- Die Optionen werden an den `analyze`- und `impact`-Subcommands registriert, nicht global an der Root-App.
- Tests pruefen die command-lokale Position.
- Globale Positionen duerfen CLI11-Usage-Fehler liefern; AP 1.5 dokumentiert keinen stabilen Hilfetext fuer diese globalen Fehler, nur nonzero Exit und Text auf `stderr`.

### Gegenseitiger Ausschluss

`--quiet` und `--verbose` sind gegenseitig exklusiv.

Regeln:

- Wenn beide Optionen beim selben Subcommand gesetzt sind, gewinnt der CLI-Parser-/Usage-Fehler vor Eingabevalidierung, Formatverfuegbarkeit, Analyse und Reporterzeugung.
- Ergebnis: Textfehler auf `stderr`, nonzero Exit, kein stdout-Report, keine Zieldatei.
- Tests decken beide Subcommands ab.

### CLI-Modell

AP 1.5 fuehrt ein CLI-internes Modell ein:

```cpp
enum class OutputVerbosity {
    normal,
    quiet,
    verbose,
};
```

Regeln:

- `OutputVerbosity` lebt in `src/adapters/cli/` oder bleibt file-local in `cli_adapter.cpp`, wenn keine Header-Nutzung noetig ist.
- `CliOptions` speichert genau einen abgeleiteten Verbosity-Wert.
- Das Modell wird nicht in Hexagon-Services, Driving Ports, Driven Ports oder Output-Adapter-Signaturen aufgenommen.
- `ReportWriterPort` und `GenerateReportPort` bleiben unveraendert.
- `ConsoleReportAdapter` bleibt fuer den Normalmodus nutzbar, wird aber nicht zur allgemeinen Verbosity-Abstraktion.

## Verbosity-Policy

### Gemeinsame Regeln

Verbosity ist eine CLI-Emission-Policy.

Regeln:

- Normalmodus ohne `--quiet` oder `--verbose` bleibt rueckwaertskompatibel.
- Quiet beeinflusst keine Exit-Codes.
- Verbose beeinflusst keine Exit-Codes.
- Quiet und Verbose veraendern keine fachlichen Analyseergebnisse.
- Quiet und Verbose veraendern `top_limit` nicht.
- Quiet und Verbose duerfen keine neuen Reportadapter-Parameter erzeugen.
- Quiet und Verbose duerfen keine Artefaktinhalte fuer `markdown`, `json`, `dot` oder `html` veraendern.
- Bei `--output` gibt es auch in Quiet und Normalmodus keine Erfolgsmeldungen auf stdout.

### Formatmatrix

| Format | Normal | Quiet | Verbose |
|---|---|---|---|
| `console` | bestehender Console-Report byte-stabil | CLI-eigene knappe Console-Emission | CLI-eigene detailreiche Console-Emission |
| `markdown` ohne `--output` | Markdown auf stdout | byte-identisch zu Normal auf stdout | byte-identisch zu Normal auf stdout; zusaetzliche Verbose-Diagnostik nur auf stderr |
| `markdown` mit `--output` | Datei, stdout leer | Datei byte-identisch, stdout leer | Datei byte-identisch, stdout leer; zusaetzliche Verbose-Diagnostik nur auf stderr |
| `json` ohne `--output` | JSON auf stdout | byte-identisch zu Normal auf stdout | byte-identisch zu Normal auf stdout; zusaetzliche Verbose-Diagnostik nur auf stderr |
| `json` mit `--output` | Datei, stdout leer | Datei byte-identisch, stdout leer | Datei byte-identisch, stdout leer; zusaetzliche Verbose-Diagnostik nur auf stderr |
| `dot` ohne `--output` | DOT auf stdout | byte-identisch zu Normal auf stdout | byte-identisch zu Normal auf stdout; zusaetzliche Verbose-Diagnostik nur auf stderr |
| `dot` mit `--output` | Datei, stdout leer | Datei byte-identisch, stdout leer | Datei byte-identisch, stdout leer; zusaetzliche Verbose-Diagnostik nur auf stderr |
| `html` ohne `--output` | HTML auf stdout | byte-identisch zu Normal auf stdout | byte-identisch zu Normal auf stdout; zusaetzliche Verbose-Diagnostik nur auf stderr |
| `html` mit `--output` | Datei, stdout leer | Datei byte-identisch, stdout leer | Datei byte-identisch, stdout leer; zusaetzliche Verbose-Diagnostik nur auf stderr |

Regeln:

- "Byte-identisch" bezieht sich auf den Reportinhalt bei gleichem Ergebnisobjekt, Format und `top_limit`.
- Verbose-`stderr` fuer erfolgreiche Artefaktformate ist erlaubt, aber stdout beziehungsweise Datei bleibt der alleinige Reporttraeger.
- Quiet erzeugt bei erfolgreichen Artefaktformaten keine zusaetzliche `stderr`-Diagnostik.
- Service-Diagnostics bleiben bei Artefaktformaten Bestandteil des jeweiligen Reports, nicht automatische `stderr`-Warnungen.

## Console-Vertrag

`--format console` ist ein CLI-owned Emissionspfad. Quiet und Verbose werden in der CLI-Schicht entschieden.

### Console Normal

Normalmodus:

- nutzt weiter den bestehenden Console-Reportpfad.
- bleibt fuer bestehende Goldens byte-stabil.
- erhaelt keine neuen `ReportInputs`-Abschnitte.
- erhaelt keine Verbosity-Parameter in Ports oder Adaptern.

### Console Quiet Analyze

Console Quiet fuer `analyze` gibt eine knappe Zusammenfassung aus.

Pflichtzeilen in dieser Reihenfolge:

1. `analysis: ok`
2. `translation units: <count>`
3. `ranking entries shown: <returned> of <total>`
4. `include hotspots shown: <returned> of <total>`
5. `diagnostics: <count>`

Optionale Zeilen:

- `target metadata: <status>` nur, wenn Target-Metadaten nicht `not_loaded` sind.
- `warnings: present` nur, wenn reportweite oder itembezogene Diagnostics mit Severity `warning` vorhanden sind.

Regeln:

- Quiet Analyze zeigt keine einzelnen Translation Units.
- Quiet Analyze zeigt keine einzelnen Hotspots.
- Quiet Analyze zeigt keine Target-Listen.
- Quiet Analyze zeigt keine vollstaendigen Diagnostic-Messages.
- Quiet Analyze beachtet `top_limit` fuer die `shown`-Zaehler.
- Quiet Analyze stdout endet mit genau einem Newline.
- Quiet Analyze stderr bleibt bei erfolgreicher Analyse leer.

### Console Quiet Impact

Console Quiet fuer `impact` gibt eine knappe Zusammenfassung aus.

Pflichtzeilen in dieser Reihenfolge:

1. `impact: ok`
2. `changed file: <display path>`
3. `affected translation units: <count>`
4. `classification: <direct|heuristic>`
5. `affected targets: <count>`
6. `diagnostics: <count>`

Optionale Zeilen:

- `target metadata: <status>` nur, wenn Target-Metadaten nicht `not_loaded` sind.
- `warnings: present` nur, wenn reportweite oder itembezogene Diagnostics mit Severity `warning` vorhanden sind.

Regeln:

- Quiet Impact zeigt keine einzelnen Translation Units.
- Quiet Impact zeigt keine einzelnen Targets.
- Quiet Impact zeigt keine vollstaendigen Diagnostic-Messages.
- Quiet Impact stdout endet mit genau einem Newline.
- Quiet Impact stderr bleibt bei erfolgreicher Analyse leer.

### Console Verbose Analyze

Console Verbose fuer `analyze` gibt eine detailreiche, deterministische Emission aus.

Pflichtsections in dieser Reihenfolge:

1. `verbose analysis summary`
2. `inputs`
3. `observation`
4. `translation unit ranking`
5. `include hotspots`
6. `targets`
7. `diagnostics`

Regeln:

- `inputs` nutzt `AnalysisResult::inputs`, nicht Legacy-Presentation-Felder.
- Fehlende optionale Inputs erscheinen als `not provided`.
- `observation` zeigt Observation Source, Target Metadata Status und Include-Analyse-Heuristik.
- `translation unit ranking` zeigt bis `top_limit` Eintraege wie der Normalmodus, plus Directory, Score und alle Item-Diagnostics.
- `include hotspots` zeigt bis `top_limit` Hotspots und bis `top_limit` Kontext-Translation-Units pro Hotspot.
- `targets` zeigt eindeutige Targets nach `display_name`, `type`, `unique_key`, sofern Target-Metadaten vorhanden sind.
- `diagnostics` zeigt reportweite Diagnostics vollstaendig in Modellreihenfolge.
- Ranking-Eintraege werden nach `rank`, `source_path`, `directory`, `unique_key` sortiert und danach auf `top_limit` begrenzt.
- Include-Hotspots werden nach `affected_translation_units.size()` absteigend, dann `header_path` aufsteigend sortiert und danach auf `top_limit` begrenzt.
- Hotspot-Kontext-Translation-Units werden pro Hotspot nach `source_path`, `directory`, `unique_key` sortiert und danach auf `top_limit` begrenzt.
- Item-Diagnostics und reportweite Diagnostics werden in der Reihenfolge ihres `std::vector`-Indexes ausgegeben; der Index ist der Tie-Breaker und wird nicht aus Maps oder Sets rekonstruiert.
- Targets werden immer nach `display_name`, `type`, `unique_key` sortiert.
- Verbose Analyze stdout endet mit genau einem Newline.
- Verbose Analyze stderr bleibt bei erfolgreichem Console-Report leer.

### Console Verbose Impact

Console Verbose fuer `impact` gibt eine detailreiche, deterministische Emission aus.

Pflichtsections in dieser Reihenfolge:

1. `verbose impact summary`
2. `inputs`
3. `observation`
4. `directly affected translation units`
5. `heuristically affected translation units`
6. `directly affected targets`
7. `heuristically affected targets`
8. `diagnostics`

Regeln:

- `inputs` nutzt `ImpactResult::inputs`, nicht Legacy-Presentation-Felder.
- Fehlende optionale Inputs erscheinen als `not provided`.
- `changed_file` stammt fuer Provenienz aus `ImpactResult::inputs.changed_file`.
- `observation` zeigt Observation Source und Target Metadata Status.
- Translation Units zeigen `source_path`, `directory`, Impact-Art und Targets.
- Impact-Listen werden nicht ueber `top_limit` begrenzt.
- Targets werden nach `display_name`, `type`, `unique_key` sortiert.
- `diagnostics` zeigt reportweite Diagnostics vollstaendig in Modellreihenfolge.
- Direct- und Heuristic-Translation-Unit-Sections werden getrennt nach `ImpactKind` gebildet; innerhalb jeder Section wird nach `source_path`, `directory`, `unique_key` sortiert.
- Direct- und Heuristic-Target-Sections werden getrennt nach `TargetImpactClassification` gebildet; innerhalb jeder Section wird nach `display_name`, `type`, `unique_key` sortiert.
- Translation-Unit-Zielzuordnungen innerhalb einer Zeile werden nach `display_name`, `type`, `unique_key` sortiert.
- Reportweite Diagnostics werden in der Reihenfolge ihres `std::vector`-Indexes ausgegeben; der Index ist der Tie-Breaker und wird nicht aus Maps oder Sets rekonstruiert.
- Verbose Impact stdout endet mit genau einem Newline.
- Verbose Impact stderr bleibt bei erfolgreichem Console-Report leer.

### Console Escaping und Stabilitaet

Console Quiet und Verbose sind Textreports.

Regeln:

- Keine ANSI-Farben in M5.
- Keine Terminalbreiten-Erkennung.
- Keine Tabellen, die von Terminalbreite abhaengen.
- Keine Zeitstempel, Hostnamen oder Prozess-IDs.
- Pfade und Messages werden als Anzeige-Strings aus dem Modell ausgegeben.
- Newlines in Diagnostic-Messages werden fuer Console Verbose deterministisch als ` / ` normalisiert.
- Quiet/Verbose-Goldens fixieren Abschnittsreihenfolge, Labeltexte und Leerzeilen.

## Artefaktformat-Vertrag

Artefaktformate sind `markdown`, `json`, `dot` und `html`.

### Quiet fuer Artefaktformate

Quiet fuer Artefaktformate ist reportinhaltlich ein Noop.

Regeln:

- Ohne `--output` ist stdout byte-identisch zum Normalmodus.
- Mit `--output` ist die Datei byte-identisch zum Normalmodus.
- Mit `--output` bleibt stdout leer.
- Erfolgreiche Quiet-Artefaktaufrufe lassen stderr leer.
- Quiet erzeugt keine Erfolgsmeldungen.
- Quiet unterdrueckt keine Fehler.

### Verbose fuer Artefaktformate

Verbose fuer Artefaktformate veraendert den Reportinhalt nicht.

Regeln:

- Ohne `--output` ist stdout byte-identisch zum Normalmodus.
- Mit `--output` ist die Datei byte-identisch zum Normalmodus.
- Mit `--output` bleibt stdout leer.
- Zusaetzliche Verbose-Diagnostik geht ausschliesslich nach stderr.
- JSON stdout bleibt syntaktisch gueltiges JSON ohne vor- oder nachgelagerten Text.
- DOT stdout bleibt syntaktisch gueltiger DOT-Quelltext ohne vor- oder nachgelagerten Text.
- HTML stdout bleibt vollstaendiges HTML ohne vor- oder nachgelagerten Text.
- Markdown stdout bleibt reiner Markdown-Report ohne vor- oder nachgelagerten Text.

Verbose-`stderr` fuer erfolgreiche Artefaktformate enthaelt maximal:

1. `verbose: report_type=<analyze|impact>`
2. `verbose: format=<markdown|json|dot|html>`
3. `verbose: output=<stdout|file>`
4. `verbose: compile_database_source=<value>`
5. `verbose: cmake_file_api_source=<value>`
6. `verbose: observation_source=<value>`
7. `verbose: target_metadata=<value>`
8. fuer `impact`: `verbose: changed_file_source=<value>`

Regeln:

- Reihenfolge ist exakt wie oben.
- `top_limit` erscheint nicht in Verbose-`stderr` fuer Artefaktformate. Bei `analyze` beeinflusst `top_limit` ausschliesslich den Reportinhalt; bei `impact` gibt es keinen `top_limit`.
- Fehlende optionale Werte werden als `not provided` geschrieben.
- Es werden keine Hostpfade ergaenzt, die nicht bereits im Ergebnis vorhanden sind.
- Keine Stacktraces, Typnamen oder internen Funktionsnamen.

## Fehlervertrag

### Gemeinsame Fehlerregeln

- Parser- und Usage-Fehler bleiben Textfehler auf `stderr`.
- Eingabefehler bleiben Textfehler auf `stderr`.
- Render- und Schreibfehler bleiben Textfehler auf `stderr`.
- `--format console --output <path>` ist immer ein Usage-Fehler: Text auf `stderr`, nonzero Exit, leeres stdout, keine Zieldatei.
- Der stderr-Inhalt fuer `--format console --output <path>` ist exakt:
  1. `error: --output is not supported with --format console`
  2. `hint: use an artifact-oriented format such as --format markdown, --format json, --format dot or --format html when writing a report file`
- Quiet und Verbose aendern Exit-Codes nicht.
- Quiet unterdrueckt keine Fehler.
- Verbose darf Fehlerkontext auf `stderr` ergaenzen, aber keine Stacktraces oder internen Implementierungsdetails ausgeben.
- Fehler erzeugen keine JSON-, DOT- oder HTML-Fehlerdokumente.

### Fehlerpraezedenz

1. CLI-Parser- und Usage-Fehler, einschliesslich gegenseitiger Ausschluss von `--quiet` und `--verbose`.
2. Formatverfuegbarkeit.
3. Reportoptionen wie `--output` mit `--format console`.
4. command-spezifische Pflichtoptionen wie `impact --changed-file`.
5. Eingabevalidierung.
6. Analyse.
7. Render-Preconditions.
8. Rendern.
9. Schreiben oder stdout-Emission.

Regeln:

- Quiet/Verbose wird erst wirksam, wenn Parsing und Usage-Validierung erfolgreich waren.
- Bei Parser-/Usage-Fehlern kann der CLI-Parser eigenen Hilfetext ausgeben; AP 1.5 testet nur Text auf `stderr`, nonzero Exit und keinen stdout-Report.
- Bei Fehlern nach erfolgreichem Parsing darf Verbose zusaetzliche Kontextzeilen nach der normalen Fehlermeldung ausgeben.

### Verbose-Fehlerkontext

Verbose-Fehlerkontext ist kurz und stabil.

Erlaubte Zeilen:

- `verbose: command=<analyze|impact>`
- `verbose: format=<format>`
- `verbose: output=<stdout|file>`
- `verbose: validation_stage=<parse|usage|input|analysis|render|write>`

Regeln:

- Zeilen erscheinen nach der normalen Fehlermeldung.
- Fuer Renderfehler mit `--verbose` ist die stderr-Sequenz exakt:
  1. `error: cannot render report: <message>`
  2. `verbose: command=<analyze|impact>`
  3. `verbose: format=<format>`
  4. `verbose: output=<stdout|file>`
  5. `verbose: validation_stage=render`
- Fuer Schreibfehler mit `--verbose --output` ist die stderr-Sequenz exakt:
  1. `error: cannot write report: <path>: <reason>`
  2. `hint: check the output path and directory permissions`
  3. `verbose: command=<analyze|impact>`
  4. `verbose: format=<format>`
  5. `verbose: output=file`
  6. `verbose: validation_stage=write`
- Weitere stderr-Zeilen sind fuer Render- und Schreibfehler in AP 1.5 nicht zulaessig.
- Fuer Render- und Schreibfehler ohne `--verbose` bleibt stderr auf die bestehende Fehlermeldung und bestehende Hint-Zeilen beschraenkt.
- `validation_stage` wird aus der CLI-Schicht gesetzt, nicht aus Exception-Typen abgeleitet.
- Keine C++-Typnamen.
- Keine Stacktraces.
- Keine absoluten Hostpfade, ausser sie stammen bereits aus expliziten Eingabeargumenten oder fachlichen Ergebnisdaten.

## Implementierungsreihenfolge

Die Umsetzung erfolgt in drei verbindlichen Tranchen plus einer optionalen Haertungstranche. Jede Tranche endet mit einem vollstaendigen Lauf der Docker-Gates aus `README.md` ("Tests und Quality Gates") und `docs/quality.md`. Die globalen Abnahmekriterien dieses Plans gelten zusaetzlich am Ende von Tranche C.

DoD-Checkboxen in diesem Plan tracken den Liefer-/Abnahmestatus: `[x]` markiert eine in einem konkreten Commit ausgelieferte Anforderung, `[ ]` markiert eine offene Anforderung. Liefer-Stand zum Zeitpunkt der Tranche-A-Vorbereitung:

- Tranche A ausgeliefert in vorliegendem Commit-Set (Hash siehe `git log` nach Commit; CLI-Modell, Parserverflechtung und Artefakt-Noop-Policy stehen).
- Tranche B offen.
- Tranche C offen.
- Tranche D optional und offen.

### Tranche A - CLI-Modell, Parservertrag und Artefakt-Noop-Policy

Fokus auf Parser, Policy und Artefakt-Byte-Stabilitaet. Tranche A liefert noch keinen sichtbaren Verbosity-Effekt: Console-Normalmodus laeuft weiter ueber den bestehenden `ConsoleReportAdapter`, und alle Artefaktformate erzeugen unveraendert byte-identische Ausgaben unabhaengig von `--quiet` oder `--verbose`. Die Tranche aendert ausschliesslich die CLI-Schicht und nutzt keine neuen Ports oder Adapter.

Voraussichtlich neue Datei:

- `src/adapters/cli/output_verbosity.h` mit `enum class OutputVerbosity { normal, quiet, verbose };`. Die Header-Variante ist gewaehlt, weil `CliOptions` (in `cli_adapter.h`) das Enum als Feld traegt; eine Datei-lokale Definition in `cli_adapter.cpp` waere nur tragfaehig, falls `CliOptions` ebenfalls Datei-lokal bliebe, was sie nicht ist.

Voraussichtlich zu aendernde Dateien:

- `src/adapters/cli/cli_adapter.{h,cpp}` (CliOptions-Erweiterung, command-lokale Optionen, Mutual-Exclusion, Parser-Wiring).
- `src/main.cpp` (kein neuer Parameter, aber Composition Root muss die neue CliOptions akzeptieren).
- `tests/e2e/test_cli.cpp` (Parser-, Mutual-Exclusion-, globaler-Positions- und Artefakt-Noop-Tests).
- `tests/adapters/test_port_wiring.cpp` (Architektur-Assertion, dass `OutputVerbosity` nicht in Port- oder Adapter-Signaturen auftaucht).
- `docs/quality.md` (Verbosity-Testumfang in der "Tests und Quality Gates"-Sektion vermerken).

Implementierungsschritte:

1. `output_verbosity.h` anlegen. Enum bleibt CLI-internal: keine Verwendung in `src/hexagon/` oder `src/adapters/output/`. Pruefen ueber `cmake/check_hexagon_boundary.sh` (existierender Boundary-Test); zusaetzlich eine Architektur-Assertion in `tests/adapters/test_port_wiring.cpp`, die per `static_assert` oder Test sicherstellt, dass weder `ReportWriterPort` noch `GenerateReportPort` noch ein Output-Adapter-Konstruktor (`ConsoleReportAdapter`, `MarkdownReportAdapter`, `JsonReportAdapter`, `DotReportAdapter`, `HtmlReportAdapter`) ein `OutputVerbosity`-Argument akzeptiert.
2. `CliOptions` (in `cli_adapter.h`) um ein `OutputVerbosity verbosity{OutputVerbosity::normal}`-Feld erweitern. Default `normal` haelt das bestehende Verhalten fuer Aufrufe ohne Flags byte-stabil.
3. CLI11-Subcommand-Optionen `--quiet` und `--verbose` an beiden Subcommands `analyze` und `impact` registrieren. Beide Optionen sind Flags ohne Wert. Verarbeitung in der Subcommand-Callback-Phase, nicht global vor dem Subcommand.
4. Gegenseitigen Ausschluss umsetzen. Vorzugsweise ueber CLI11s `option->excludes(other_option)`-Mechanismus, weil das den Usage-Fehler vor dem Callback liefert; falls CLI11 die `excludes`-Beziehung pro Subcommand nicht sauber abbildet, fallback auf eine explizite Validierung im Subcommand-Callback, die `ExitCode::cli_usage_error` zurueckgibt.
5. Globale Positionen `cmake-xray --quiet analyze ...` und `cmake-xray --verbose impact ...` werden ausdruecklich nicht unterstuetzt. Tests pruefen nur nonzero Exit und nicht-leeres `stderr`; AP 1.5 dokumentiert keinen stabilen Hilfetext fuer diese Faelle.
6. Composition Root in `src/main.cpp` so anpassen, dass das neue Feld in `CliOptions` ohne Aenderung an Ports oder Adaptern fliesst. `select_report_port` und `handle_*_result` bleiben signaturkompatibel.
7. Artefakt-Noop-Policy konkret in `cli_adapter.cpp` verdrahten: `handle_analysis_result` und `handle_impact_result` lesen `options.verbosity` aus, leiten ihn aber weder an `ReportGenerator` noch an `ReportWriterPort` weiter. Im Tranche-A-Stand bleibt die Variable danach im Scope ungenutzt; Tranche B fuegt den Console-Quiet/Verbose-Switch hinzu, Tranche C fuegt den Verbose-stderr-Block hinzu. Bis dahin bestaetigt der Architekturtest aus Schritt 1, dass keine Port-Signatur durch Tranche A ungewollt erweitert wurde.
8. CLI-Tests fuer Parser-Vertrag in `tests/e2e/test_cli.cpp`:
   - `analyze --quiet` und `analyze --verbose` werden akzeptiert (Exit `0`, kein Reportabbruch).
   - `impact --quiet` und `impact --verbose` werden akzeptiert.
   - `analyze --quiet --verbose` und `impact --quiet --verbose` liefern `ExitCode::cli_usage_error`.
   - `cmake-xray --quiet analyze ...` und `cmake-xray --verbose impact ...` liefern nonzero Exit (kein stabiler Hilfetext).
   - `impact --verbose` ohne `--changed-file` liefert `ExitCode::cli_usage_error` mit Text `impact requires --changed-file` auf stderr und kein `verbose:`-Block, weil Pflichtoptionen-Pruefung Praezedenzstufe 4 (vor Eingabevalidierung) ist und Verbose-Kontext erst in Tranche C aktiv wird.
   - `analyze --format console --output report.txt` und `impact --format console --output report.txt` liefern weiterhin die exakte zweizeilige stderr-Sequenz aus dem Fehlervertrag (`error: --output is not supported with --format console` plus die `hint:`-Zeile). Falls die exakte Hint-Zeile in vorherigen APs anders formuliert war, wird sie hier angeglichen, weil der Plan die Wortlaute verbindlich vorgibt.
9. CLI-Tests fuer Artefakt-Noop in `tests/e2e/test_cli.cpp`. Pro Format mindestens ein Quiet- und ein Verbose-Test:
   - `analyze --format json --quiet` und `analyze --format json --verbose` liefern stdout-byte-identisch zum Normalmodus mit demselben `top_limit`.
   - `analyze --format markdown --quiet` und `analyze --format markdown --verbose` liefern stdout-byte-identisch zum Normalmodus.
   - `analyze --format dot --quiet` und `analyze --format dot --verbose` analog.
   - `analyze --format html --quiet` und `analyze --format html --verbose` analog.
   - `--output`-Pfad: bei jedem Format mindestens ein Test, der mit `--quiet` und einer mit `--verbose` schreibt; die Datei muss byte-identisch zum Normalmodus sein, stdout bleibt leer. Quiet laesst stderr leer; Verbose darf in Tranche A noch leer bleiben, weil die Verbose-stderr-Sequenz erst in Tranche B/C aktiviert wird.
10. Symmetrische Tests fuer `impact --format <fmt>` mit `--quiet`/`--verbose` analog zu Schritt 9.
11. `docs/quality.md` um eine kurze Sub-Sektion erweitern, die den Tranche-A-Testumfang nennt: Mutual-Exclusion, command-lokale Position, Artefakt-Byte-Stabilitaet, kein Verbose-stderr-Inhalt im Erfolgsfall (Vorbereitung fuer Tranche C).

Sub-Risiken Tranche A:

- CLI11-`excludes` zwischen Subcommand-Optionen wird je nach CLI11-Version unterschiedlich gehandhabt; ein expliziter Validator im Callback ist der Fallback und muss vor `validate_changed_file_required` und vor der Format-Verfuegbarkeitspruefung greifen, sonst werden Praezedenzregeln aus `Fehlerpraezedenz` verletzt.
- Globale Positionen vor dem Subcommand koennen je nach CLI11-Konfiguration entweder ein "unknown option" liefern oder das Flag stillschweigend an die Root-App binden. Tranche A pinnt nur "nonzero Exit" und "nicht-leeres stderr"; jeder stabilere Vertrag wird bewusst spaeter aufgenommen, falls eine globale Variante eingefuehrt wird.
- Artefakt-Byte-Stabilitaet pro Format und Subcommand muss explizit getestet werden, weil ein vergessenes `--quiet`-Flag in einem Renderer-Pfad in Tranche A unbemerkt bleiben koennte. Mindestens ein Test pro `(Subcommand, Format, Modus)`-Kombination ist Pflicht.
- `OutputVerbosity` darf nicht in JSON-/HTML-/DOT-Adapter-Tests "leck schlagen". Falls Adapter-Konstruktoren in Tests einen weiteren Parameter sehen, ist das Anzeichen einer ungewollten Abhaengigkeitsausweitung und wird vor Tranche-Abschluss zurueckgerollt.

Definition of Done Tranche A:

- [x] `output_verbosity.h` existiert; `CliOptions` traegt das Feld; Default `normal`.
- [x] `--quiet` und `--verbose` existieren command-lokal fuer beide Subcommands.
- [x] gleichzeitige Nutzung liefert Usage-Fehler vor Eingabevalidierung und Reporterzeugung.
- [x] globale Positionen sind nicht Teil des Vertrags und liefern nonzero Usage-Fehler ohne stabile Hilfetexte.
- [x] Quiet veraendert Artefaktreports fuer Markdown, JSON, DOT und HTML byte-stabil nicht.
- [x] Verbose veraendert Artefaktreports fuer Markdown, JSON, DOT und HTML byte-stabil nicht.
- [x] `--output` erzeugt weder im Normal- noch im Quiet-/Verbose-Modus eine Erfolgsmeldung auf stdout.
- [x] `--format console --output <path>` liefert exakt die zwei dokumentierten stderr-Zeilen.
- [x] `impact --verbose` ohne `--changed-file` liefert Usage-Fehler ohne `verbose:`-Block.
- [x] `ReportWriterPort` und `GenerateReportPort` enthalten kein `OutputVerbosity`.
- [x] `ConsoleReportAdapter`, `MarkdownReportAdapter`, `JsonReportAdapter`, `DotReportAdapter` und `HtmlReportAdapter` haben keinen Verbosity-Parameter im Konstruktor.
- [x] `docs/quality.md` listet den Tranche-A-Testumfang.
- [x] Docker-Gates aus `README.md` und `docs/quality.md` sind gruen.

### Tranche B - Console Quiet und Console Verbose

Console bekommt CLI-eigene Quiet-/Verbose-Emissionen. Console-Normalmodus laeuft weiter ueber den `ConsoleReportAdapter` und bleibt byte-stabil; Quiet und Verbose werden in der CLI-Schicht entschieden und sind keine zusaetzlichen `ReportWriterPort`-Implementierungen.

Tranche B ist intern in zwei Sub-Tranchen B.1 und B.2 gegliedert. Die Sub-Tranchen sind ausschliesslich Implementierungsreihenfolge, kein Release-Schnitt: beide Sub-Tranchen landen im selben Tranche-B-Commit-Set und werden gemeinsam ueber die Docker-Gates aus `README.md` und `docs/quality.md` abgenommen. "Abnahme Tranche B" und "Definition of Done Tranche B" am Ende dieses Abschnitts gelten am Ende von B.2 fuer das gesamte Tranche-B-Set.

Voraussichtlich neue Dateien:

- `src/adapters/cli/cli_console_renderers.{h,cpp}`. Die Renderer leben in einer eigenen Translation Unit, weil die Verbose-Funktion sieben bzw. acht Sektionen mit Sortier-, Escape- und Limit-Regeln traegt; in `cli_adapter.cpp` wuerde sie die Lizard-CCN-/Funktionslaengen-Grenzen aus `docs/quality.md` reissen. Header exportiert nur freie Funktionen wie `render_console_quiet_analyze(const AnalysisResult&, std::size_t top_limit)` und gibt `std::string` zurueck.
- `tests/e2e/testdata/m5/verbosity/` mit Konsolen-Goldens; Manifest analog zu `m5/json-reports/manifest.txt` und `m5/html-reports/manifest.txt` (Tranche-B-Eintraege landen alle in `manifest.txt`).

Voraussichtlich zu aendernde Dateien:

- `src/adapters/cli/cli_adapter.cpp` (Verbosity-Switch in `handle_analysis_result` und `handle_impact_result`, Aufruf der neuen Renderer fuer `--format console`).
- `src/adapters/cli/CMakeLists.txt` und `src/adapters/CMakeLists.txt` (neue Translation Unit registrieren).
- `tests/CMakeLists.txt` (neuen Adaptertest `test_cli_console_renderers.cpp` registrieren; B.1 entscheidet diese Datei zwingend ein, weil die Renderer file-lokale Sortier-/Escape-Hilfen exportieren werden, die in `test_cli.cpp` direkt nicht reachable sind).
- `tests/adapters/test_cli_console_renderers.cpp` (Adaptertests fuer Quiet- und Verbose-Renderer).
- `tests/e2e/test_cli.cpp` (CLI-End-to-End-Pfade fuer `--format console --quiet/--verbose`, inkl. der `--quiet/--verbose --format console --output`-Usage-Fehler-Kombinationen).
- `tests/e2e/run_e2e.sh` (Binary-Smokes erst in Tranche C).

#### Sub-Tranche B.1 - Console Quiet fuer Analyze und Impact

Quiet ist die kuerzere Variante. B.1 implementiert die fuenf bzw. sechs Pflichtzeilen aus dem Console-Vertrag und legt die zugehoerigen Goldens an. Adaptertests reichen fuer den Renderer; Binary-Smokes ueber `e2e_binary` folgen erst in Tranche C.

1. `cli_console_renderers.{h,cpp}` anlegen mit zwei freien Funktionen `render_console_quiet_analyze(const AnalysisResult&, std::size_t top_limit)` und `render_console_quiet_impact(const ImpactResult&)`. Beide geben `std::string` mit genau einem trailing Newline zurueck. B.1 fuehrt bewusst noch keine Sortier- oder Escape-Hilfen ein; alle gemeinsamen Render-Helper, die spaeter B.2 braucht (z. B. `format_severity`, `format_target_label`), werden in B.2 angelegt und nicht retroaktiv aus B.1 herausgezogen, damit B.1-Reviewer in B.2 nichts an B.1 nachsehen muessen. Header-Kommentar in `cli_console_renderers.h` weist explizit darauf hin, dass B.2 zwei weitere Funktionen `render_console_verbose_*` ergaenzt.
2. Pflichtzeilen Quiet-Analyze in dokumentierter Reihenfolge implementieren: `analysis: ok`, `translation units: <count>`, `ranking entries shown: <returned> of <total>`, `include hotspots shown: <returned> of <total>`, `diagnostics: <count>`. `<returned>` respektiert `top_limit`. Optionalzeilen `target metadata: <status>` (nur wenn nicht `not_loaded`) und `warnings: present` (nur wenn mindestens eine reportweite oder item-bezogene Diagnostic mit Severity `warning` existiert).
3. Pflichtzeilen Quiet-Impact in dokumentierter Reihenfolge implementieren: `impact: ok`, `changed file: <display path>`, `affected translation units: <count>`, `classification: <direct|heuristic>`, `affected targets: <count>`, `diagnostics: <count>`. Optionalzeilen `target metadata` und `warnings: present` analog. `<display path>` stammt aus `ImpactResult::inputs.changed_file`. Render-Preconditions aus AP 1.4 gelten auch fuer Quiet: ist `inputs.changed_file == std::nullopt` oder `inputs.changed_file_source == unresolved_file_api_source_root`, lehnt die CLI Quiet vor dem Renderer mit Textfehler auf `stderr`, nonzero Exit und leerem stdout ab (gleicher Vertrag wie HTML in AP 1.4 / `cli_adapter.cpp:236-258`).
4. `handle_analysis_result` und `handle_impact_result` in `cli_adapter.cpp` so erweitern, dass `--format console --quiet` die neuen Renderer aufruft, statt den `ConsoleReportAdapter` zu nutzen. Die in Schritt 3 genannten Render-Preconditions werden vor dem Renderer geprueft (Praezedenzstufe 7 aus dem Fehlervertrag).
5. Goldens unter `tests/e2e/testdata/m5/verbosity/` anlegen. Die Pfade bleiben POSIX-only, weil Console-Quiet keine Pfadnormalisierung am `<display path>` durchfuehrt, die zwischen Linux und Windows-MSYS divergieren wuerde; ein per-Plattform-Fork (`*_windows.txt`) ist fuer B.1-Goldens nicht vorgesehen, und der Adaptertest aus Schritt 6 deckt synthetische Pfadeingaben unabhaengig vom Hostpfad ab:
   - `console-analyze-quiet.txt` (m3/report_project, no-targets, has-hotspots, has-diagnostics).
   - `console-analyze-quiet-targets.txt` (m4/with_targets, has-targets, has-hotspots, has-diagnostics).
   - `console-impact-quiet.txt` (m3/report_impact_header + include/common/config.h, heuristic, no-targets).
   - `console-impact-quiet-targets.txt` (m4/with_targets + include/common/shared.h, heuristic, has-targets).
   - `console-analyze-quiet-empty.txt` (m5/dot-fixtures/multi_tu_compile_db, lean: keine Hotspots, keine Targets, keine Diagnostics; deckt das Weglassen der Optionalzeilen).
   - `console-impact-quiet-empty.txt` (m4/file_api_only + include/common/shared.h, 0 affected TUs, 0 targets; deckt Optionalzeilen-Auslassung und `affected translation units: 0`).
6. Adaptertests in `tests/adapters/test_cli_console_renderers.cpp`. CLI-End-to-End-Pfade landen zusaetzlich in `tests/e2e/test_cli.cpp`:
   - Pflichtzeilen-Reihenfolge fuer Analyze und Impact (Adaptertest mit synthetischen Result-Objekten).
   - Optionalzeilen erscheinen nur unter den dokumentierten Bedingungen.
   - `top_limit` beeinflusst nur die `shown`-Zaehler, nicht die `count`-Gesamtwerte.
   - `--quiet` zeigt keine einzelnen Translation-Unit-Pfade, keine Hotspot-Header, keine Target-Listen.
   - Stdout endet mit genau einem Newline; stderr bleibt leer.
   - Render-Precondition-Tests: `Quiet impact` lehnt `inputs.changed_file == std::nullopt` und `inputs.changed_file_source == unresolved_file_api_source_root` mit Textfehler auf `stderr` und nonzero Exit ab.
   - CLI-End-to-End: `analyze --quiet --format console --output <path>` und `impact --quiet --format console --output <path>` liefern weiterhin den exakten zweizeiligen Console-Usage-Fehler aus Tranche A; das Verbosity-Flag aendert die Fehlermeldung nicht.
7. Manifest `tests/e2e/testdata/m5/verbosity/manifest.txt` neu anlegen und Tranche-B.1-Eintraege fuehren. Manifest-/Verzeichnis-Paritaetstest wird durch `tests/validate_html_reports.py`-Stilkopie nach `tests/validate_verbosity_reports.py` umgesetzt: stdlib-only, prueft beidseitige Manifest-/Verzeichnis-Paritaet und fuehrt einen Negativtest gegen ein bewusst kaputtes Fixture (z. B. `tests/adapters/fixtures/console_quiet_broken_sample.txt`). Alternative ueber den existierenden `tests/validate_html_reports.py` ist nicht zulaessig, weil der HTML-Validator HTML-Strukturpruefungen macht und auf Console-Text falsch greifen wuerde.

Sub-Risiken B.1:

- Optionalzeilen-Logik (`target metadata`, `warnings: present`) ist die haeufigste Fehlerquelle bei knappen Texten. Goldens muessen sowohl die "Optionalzeile aktiv" als auch die "Optionalzeile fehlt" Zustaende abdecken; `console-analyze-quiet-empty.txt` und `console-analyze-quiet-targets.txt` bilden diese Achsen ab.
- Trailing-Newline-Vertrag: ein versehentliches `"\n\n"` am Ende des Renderers ist von einem `cat`-`printf`-Vergleich nicht direkt sichtbar. Der Adaptertest assertet explizit `output.back() == '\n'` und `output.find("\n\n") == std::string::npos`.
- Render-Precondition-Pfad fuer Console-Quiet: der HTML-Adapter aus AP 1.4 hat denselben Vertrag, aber die Ablehnung dort lebt in `cli_adapter.cpp:236-258`. B.1 muss diese Pruefung **vor** dem Aufruf des Quiet-Renderers ausfuehren, sonst entsteht ein partielles Console-Quiet-Output mit `not provided` als `<display path>`. Die Quelle sollte ein einzelner gemeinsamer Helfer sein, kein Code-Duplikat.

Definition of Done Sub-Tranche B.1:

- [ ] `cli_console_renderers.{h,cpp}` exportiert `render_console_quiet_analyze` und `render_console_quiet_impact`.
- [ ] Console Quiet Analyze erzeugt nur die dokumentierten Pflicht- und Optionalzeilen.
- [ ] Console Quiet Impact erzeugt nur die dokumentierten Pflicht- und Optionalzeilen.
- [ ] Quiet beachtet `top_limit` ausschliesslich fuer die `shown`-Zaehler.
- [ ] Quiet-Goldens decken Targets-aktiv, Targets-leer, Hotspots-aktiv und Hotspots-leer ab.
- [ ] Console-Quiet-Render-Preconditions lehnen `inputs.changed_file == std::nullopt` und `inputs.changed_file_source == unresolved_file_api_source_root` mit Textfehler auf `stderr` und nonzero Exit ab.
- [ ] `--quiet --format console --output <path>` liefert weiterhin den zweizeiligen Console-Usage-Fehler aus Tranche A, unveraendert.
- [ ] Stdout endet mit genau einem Newline, kein Doppel-Newline.
- [ ] Manifest-/Verzeichnis-Paritaet ist durch einen stdlib-only Validator (Negativtest inklusive) abgesichert.
- [ ] Bestehende Console-Normalmodus-Goldens bleiben unveraendert.
- [ ] Docker-Gates aus `README.md` und `docs/quality.md` sind gruen.

#### Sub-Tranche B.2 - Console Verbose fuer Analyze und Impact

Verbose ist die detailreichere Variante. B.2 implementiert die sieben (Analyze) bzw. acht (Impact) Pflichtsections aus dem Console-Vertrag und legt die zugehoerigen Goldens an. Adaptertests reichen fuer die Renderer; Binary-Smokes folgen in Tranche C.

1. `render_console_verbose_analyze(const AnalysisResult&, std::size_t top_limit)` und `render_console_verbose_impact(const ImpactResult&)` in `cli_console_renderers.{h,cpp}` ergaenzen. Rueckgabewert `std::string` mit genau einem trailing Newline. Beide Funktionen rufen pro Pflichtsection einen file-lokalen Helfer auf (`render_verbose_inputs_section`, `render_verbose_observation_section`, `render_verbose_ranking_section`, `render_verbose_hotspots_section`, `render_verbose_targets_section`, `render_verbose_diagnostics_section` fuer Analyze; analoge Helfer fuer Impact). Diese Aufteilung ist Pflicht, nicht Reaktion auf einen Lizard-CCN-Fehlschlag, damit die Top-Level-Funktion nur sequentiell die Helfer aufruft und CCN unterhalb der `docs/quality.md`-Grenzen bleibt.
2. Pflichtsections Verbose-Analyze in dokumentierter Reihenfolge implementieren: `verbose analysis summary`, `inputs`, `observation`, `translation unit ranking`, `include hotspots`, `targets`, `diagnostics`. `inputs` nutzt `AnalysisResult::inputs` als kanonische Quelle; `observation` zeigt Observation Source, Target Metadata Status und ob Include-Analyse-Heuristik aktiv ist; `translation unit ranking` und `include hotspots` respektieren `top_limit` mit den dokumentierten Sortierregeln.
3. Pflichtsections Verbose-Impact in dokumentierter Reihenfolge implementieren: `verbose impact summary`, `inputs`, `observation`, `directly affected translation units`, `heuristically affected translation units`, `directly affected targets`, `heuristically affected targets`, `diagnostics`. Impact-Listen werden nicht ueber `top_limit` begrenzt. Render-Preconditions aus AP 1.4 gelten auch fuer Verbose-Impact: ist `inputs.changed_file == std::nullopt` oder `inputs.changed_file_source == unresolved_file_api_source_root`, lehnt die CLI Verbose vor dem Renderer mit Textfehler auf `stderr`, nonzero Exit und leerem stdout ab. Quelle ist derselbe Helfer wie in B.1, der vom HTML-Pfad in `cli_adapter.cpp:236-258` abgeleitet wird.
4. Sortier- und Tie-Breaker-Vertraege deterministisch implementieren: Ranking nach `rank`, `source_path`, `directory`, `unique_key`; Hotspots nach absteigender Anzahl betroffener TUs, dann `header_path`; Hotspot-Kontext-TUs pro Hotspot nach `source_path`, `directory`, `unique_key`; Targets immer nach `display_name`, `type`, `unique_key`. Diagnostics behalten `std::vector`-Indexreihenfolge; der Index ist explizit dokumentierter Tie-Breaker.
5. Whitespace-Vertrag fuer Verbose: Newlines in Diagnostic-Messages werden deterministisch als ` / ` normalisiert (gleicher Vertrag wie HTML, AP 1.4). Pfade und Targets werden ohne Modifikation aus dem Modell uebernommen.
6. `handle_analysis_result` und `handle_impact_result` so erweitern, dass `--format console --verbose` die neuen Verbose-Renderer aufruft. Der Verbose-stderr-Pfad fuer Console (Verbose-Diagnostik nach Reportausgabe) ist Teil von Tranche C; B.2 setzt nur den stdout-Renderer.
7. Verbose-Artefakt-stderr-Emitter implementieren. Eine neue file-lokale Funktion in `cli_adapter.cpp` (oder ein freier Helfer `emit_verbose_artifact_stderr(streams.err, format, output, result.inputs)`) schreibt nach erfolgreichem Artefakt-Render die im Plan-Abschnitt "Verbose fuer Artefaktformate" dokumentierten 7 Zeilen (`analyze`) bzw. 8 Zeilen (`impact`) in exakter Reihenfolge: `verbose: report_type=...`, `verbose: format=...`, `verbose: output=...`, `verbose: compile_database_source=...`, `verbose: cmake_file_api_source=...`, `verbose: observation_source=...`, `verbose: target_metadata=...`, fuer `impact` zusaetzlich `verbose: changed_file_source=...`. Die Funktion wird nur aufgerufen, wenn `options.verbosity == OutputVerbosity::verbose` und der Artefakt-Render erfolgreich war; im Quiet- und Normalmodus bleibt stderr leer. `top_limit` taucht nicht auf.
8. Goldens unter `tests/e2e/testdata/m5/verbosity/` ergaenzen:
   - `console-analyze-verbose.txt` (m3/report_project, no-targets, has-hotspots, has-diagnostics).
   - `console-analyze-verbose-targets.txt` (m4/with_targets, has-targets, has-hotspots, has-diagnostics).
   - `console-analyze-verbose-file-api-only.txt` (m4/file_api_only, file-API-only, has-targets).
   - `console-analyze-verbose-empty.txt` (m5/dot-fixtures/multi_tu_compile_db, lean).
   - `console-impact-verbose.txt` (m3/report_impact_header + include/common/config.h, heuristic).
   - `console-impact-verbose-direct.txt` (m4/file_api_only + src/main.cpp, direct, has-targets).
   - `console-impact-verbose-targets.txt` (m4/with_targets + include/common/shared.h, heuristic, has-heuristic-targets).
   - `console-impact-verbose-empty.txt` (m4/file_api_only + include/common/shared.h, 0 affected TUs).
9. Adaptertests in `tests/adapters/test_cli_console_renderers.cpp` und `tests/e2e/test_cli.cpp`:
   - Pflichtsection-Reihenfolge fuer Analyze und Impact.
   - `inputs` nutzt `ReportInputs`, nicht Legacy-Felder; fehlende Optionalwerte erscheinen als `not provided`.
   - `observation` zeigt die drei dokumentierten Werte (Observation Source, Target Metadata Status, Include-Analyse-Heuristik) genau einmal.
   - Sortierregeln pro Sektion durch Tie-Breaker-Tests abgesichert (gleicher `rank`/`source_path`/`directory` aber unterschiedlicher `unique_key`).
   - Newline-Normalisierung in Diagnostic-Messages.
   - `top_limit` begrenzt Analyze-Listen, beruehrt aber Impact-Listen nicht.
   - Stdout endet mit genau einem Newline; stderr bleibt leer im Console-Erfolgsfall.
   - Render-Precondition-Tests fuer Verbose-Impact: `inputs.changed_file == std::nullopt` und `inputs.changed_file_source == unresolved_file_api_source_root` werden mit Textfehler abgelehnt.
   - Direct+heuristic-Trennung in einem einzigen Verbose-Impact-Goldenfall ist mangels passender Fixture (AP-1.4-Reviewer-Befund) nicht erzwingbar; stattdessen pinnt ein synthetischer Test in `tests/adapters/test_cli_console_renderers.cpp` mit gestelltem `ImpactResult`, dass beide Sections gleichzeitig populiert die dokumentierte Section-Reihenfolge halten.
   - Verbose-Artefakt-stderr aus Schritt 7: ein Adapter- oder CLI-Test pro Subcommand prueft die exakte 7-/8-zeilige stderr-Sequenz fuer mindestens ein Format und stellt sicher, dass `top_limit` nicht erscheint.
   - CLI-End-to-End: `analyze --verbose --format console --output <path>` und `impact --verbose --format console --output <path>` liefern weiterhin den zweizeiligen Console-Usage-Fehler aus Tranche A.
10. Manifest-Update fuer alle B.2-Goldens in `tests/e2e/testdata/m5/verbosity/manifest.txt`.
11. Regressionscheck, dass Console-Normalmodus-Goldens (`tests/e2e/testdata/m3/*` etc.) byte-stabil bleiben. Der Test laeuft ueber denselben `cmake-xray --format console`-Pfad ohne `--quiet`/`--verbose`.

Sub-Risiken B.2:

- Lizard-CCN-/Funktionslaengen-Grenzen sind in den Verbose-Renderern eng, weil sieben bzw. acht Sektionen mit Sortier- und Escape-Logik kombiniert werden. Schritt 1 pinnt deshalb pro Section einen Helfer als verbindlichen Implementierungsschritt, nicht als Reaktion auf einen Lizard-Fehlschlag.
- `inputs`-Section muss `ReportInputs` lesen, nicht die Legacy-Felder `AnalysisResult::compile_database_path` oder `ImpactResult::changed_file`. Ein versehentlicher Rueckgriff auf Legacy-Felder bricht den AP-1.5-Vertrag (gleiches Risiko wie bei AP 1.4 HTML).
- Determinismus bei gleichnamigen Targets: ein Target mit identischem `display_name` und `type` aber unterschiedlichem `unique_key` muss durch den `unique_key`-Tie-Breaker stabil sortiert werden. Ein Test mit zwei Targets `core::EXEC` und `core::STATIC` in der gleichen Section ist Pflicht.
- Verbose-Artefakt-stderr-Reihenfolge ist exakt 7 Zeilen fuer `analyze` und 8 Zeilen fuer `impact`. Ein Adaptertest assertet alle Zeilen und ihre Reihenfolge per `diff`-Aequivalent statt nur per `find`-Substring, sonst kann ein Tippfehler in der `verbose: changed_file_source=...`-Zeile unbemerkt bleiben.
- Verbose-Render-Precondition fuer Impact teilt sich den Helfer mit Quiet (B.1). Falls B.2 versehentlich eine zweite Implementierung anlegt, divergieren die beiden Pfade. Code-Duplikation wird vor Tranche-Abschluss zurueckgerollt.

Definition of Done Sub-Tranche B.2:

- [ ] `cli_console_renderers.{h,cpp}` exportiert `render_console_verbose_analyze` und `render_console_verbose_impact`; pro Pflichtsection existiert ein file-lokaler Helfer.
- [ ] Console Verbose Analyze enthaelt alle sieben Pflichtsections in dokumentierter Reihenfolge.
- [ ] Console Verbose Impact enthaelt alle acht Pflichtsections in dokumentierter Reihenfolge.
- [ ] `inputs` zieht aus `ReportInputs`; fehlende Optionalwerte erscheinen als `not provided`.
- [ ] `observation` zeigt Observation Source, Target Metadata Status und Heuristik-Marker.
- [ ] Sortier-Tie-Breaker fuer Ranking, Hotspots, Hotspot-Kontext, Targets und Impact-Sections sind getestet.
- [ ] Direct+heuristic-Trennung mit beiden Sections gleichzeitig populiert ist durch einen synthetischen Adaptertest abgedeckt.
- [ ] Newlines in Diagnostic-Messages werden zu ` / ` normalisiert.
- [ ] `top_limit` begrenzt nur Analyze-Listen; Impact bleibt unbegrenzt.
- [ ] Verbose-Impact-Render-Preconditions lehnen `inputs.changed_file == std::nullopt` und `inputs.changed_file_source == unresolved_file_api_source_root` ab; B.1- und B.2-Pruefung teilen denselben Helfer.
- [ ] Verbose-Artefakt-stderr-Emitter erzeugt exakt 7 Zeilen fuer `analyze` und 8 Zeilen fuer `impact` in dokumentierter Reihenfolge ohne `top_limit`-Eintrag.
- [ ] `--verbose --format console --output <path>` liefert weiterhin den zweizeiligen Console-Usage-Fehler aus Tranche A.
- [ ] Stdout endet mit genau einem Newline, kein Doppel-Newline.
- [ ] Console-Normalmodus-Goldens bleiben unveraendert.
- [ ] Docker-Gates aus `README.md` und `docs/quality.md` sind gruen.

#### Abnahme und Definition of Done Tranche B

Die Sub-Tranchen B.1 und B.2 werden gemeinsam abgenommen. Die folgenden Kriterien gelten am Ende von B.2 fuer das gesamte Tranche-B-Set.

Abnahme Tranche B: alle Docker-Gates gruen; Console Quiet und Verbose fuer Analyze und Impact sind golden-getestet; Console-Normalmodus bleibt byte-stabil; keine Port- oder Adapter-Signatur traegt `OutputVerbosity`.

Definition of Done Tranche B:

- [ ] Console Quiet Analyze und Impact erzeugen nur die dokumentierten Kurzfassungen.
- [ ] Console Verbose Analyze enthaelt Inputs, Observation, fachliche Sections und Diagnostics in dokumentierter Reihenfolge.
- [ ] Console Verbose Impact enthaelt Inputs, Observation, Direct-/Heuristic-TU- und -Target-Sections und Diagnostics in dokumentierter Reihenfolge.
- [ ] Console-Normalmodus-Goldens bleiben unveraendert.
- [ ] Keine Port- oder Adapter-Signatur enthaelt `OutputVerbosity`.
- [ ] Manifest fuer Verbosity-Goldens listet alle Quiet-/Verbose-Goldens; Verzeichnis und Manifest gleichen sich beidseitig ab.
- [ ] Docker-Gates aus `README.md` und `docs/quality.md` sind gruen.

### Tranche C - Verbose-Fehlerkontext, Binary-E2E, Dokumentation

Echte Binary-Verifikation, Verbose-Fehlerkontext und Dokumentationsabschluss. Tranche C aktiviert die in Tranche A und B vorbereiteten stderr-Pfade fuer Verbose-Modus, deckt sie ueber CLI- und Binary-E2E-Tests ab und schliesst die Nutzerdoku.

Tranche C ist intern in drei Sub-Tranchen C.1, C.2 und C.3 gegliedert. Die Sub-Tranchen sind ausschliesslich Implementierungsreihenfolge, kein Release-Schnitt: alle drei Sub-Tranchen landen im selben Tranche-C-Commit-Set und werden gemeinsam ueber die Docker-Gates aus `README.md` und `docs/quality.md` abgenommen. "Abnahme Tranche C" und "Definition of Done Tranche C" am Ende dieses Abschnitts gelten am Ende von C.3 fuer das gesamte Tranche-C-Set.

Die AP-1.4-Lehre `README-Pflicht in Tranche C` bleibt verbindlich: README-Update, `docs/guide.md`-Update und `docs/quality.md`-Update gehoeren in dasselbe Commit-Set wie die neuen Tests und werden nicht als separater Folge-Commit nachgezogen.

Voraussichtlich zu aendernde Dateien:

- `src/adapters/cli/cli_adapter.cpp` (Verbose-Fehlerkontext fuer Render-, Schreib-, Eingabefehler ergaenzen).
- `tests/e2e/test_cli.cpp` (CLI-Fehlerpfad-Tests fuer Verbose-Sequenzen, Quiet-Fehler-Nicht-Unterdrueckung).
- `tests/e2e/run_e2e.sh` (Binary-Smokes fuer beide Subcommands x beide Modi und Artefakt-Byte-Stabilitaet).
- `README.md` (Quiet-/Verbose-Optionen in CLI-Dokumentation und Beispielen).
- `docs/guide.md` (Praktische Quiet-/Verbose-Beispiele).
- `docs/quality.md` (Quiet-/Verbose-Goldens, Byte-Stabilitaet, Binary-Smokes als verbindliche Gates).

#### Sub-Tranche C.1 - Verbose-Fehlerkontext und Quiet-Fehlerregel

Vertragsfestschreibung der CLI-stdout-/stderr-/Output-/Fehlerpfade fuer `--quiet` und `--verbose`. Reine Testarbeit plus eine eng begrenzte Aenderung in `cli_adapter.cpp` fuer den Verbose-Fehlerkontext.

Fixture-Wiederverwendung: C.1 fuehrt keine neuen Fehler-Fixturepfade ein. Die Fehlerpfad-Tests nutzen genau die Fixtures, die JSON-, DOT- und HTML-CLI-Tests bereits einsetzen:

- ungueltiges `compile_commands.json`: `tests/e2e/testdata/invalid_syntax/compile_commands.json`.
- ungueltige File-API-Reply-Daten: `tests/e2e/testdata/m4/invalid_file_api/{compile_commands.json,build}`.
- nicht vorhandene Eingaben: Inline-Pfade wie `/nonexistent/compile_commands.json` und `/nonexistent/reply`.
- Schreibfehler: bestehender Test-Hilfspfad ueber ein nicht existierendes Zwischen-Verzeichnis im `temp_dir`, analog zum Markdown-/JSON-/DOT-/HTML-Pfad.
- Render-Fehler: injizierter `ThrowingGenerateReportPort`-Doppelgaenger, kein Fixture.

Implementierungsschritte:

1. Hilfsstruktur `VerboseErrorContext { std::string_view command; std::string_view format; std::string_view output; std::string_view stage; }` plus eine freie Funktion `void format_verbose_error_context(std::ostream& err, const VerboseErrorContext& ctx)` als file-lokale Helfer in `cli_adapter.cpp` einfuehren. Die Funktion schreibt genau vier Zeilen in dokumentierter Reihenfolge (`verbose: command=...`, `verbose: format=...`, `verbose: output=...`, `verbose: validation_stage=...`) und nichts sonst. Aufrufer in `handle_analysis_result`, `handle_impact_result` und `emit_rendered_report` ergaenzen die Fehlerstrecke um diesen Aufruf, wenn `options.verbosity == OutputVerbosity::verbose` ist.
2. `validation_stage` aus der CLI-Schicht setzen, nicht aus Exception-Typen ableiten. Mapping:
   - Renderfehler -> `render`.
   - Schreibfehler ueber Atomic-Writer -> `write`.
   - Eingabeladefehler vor Analyse -> `input`.
   - Analyse-Fehler (`CompileDatabaseError != none`) -> `analysis`.
   - Usage-Fehler vor Subcommand-Callback liefern keinen Verbose-Kontext, weil `--verbose` zu diesem Zeitpunkt noch nicht gebunden ist.
3. Verbose-stderr-Sequenz fuer Render- und Schreibfehler exakt wie im Fehlervertrag (`error:` plus optionaler `hint:` plus die vier `verbose:`-Zeilen). Keine zusaetzlichen Zeilen, keine Stacktraces, keine C++-Typnamen.
4. Quiet-Fehlerregel testen: Quiet darf weder Parser-/Usage-Fehler noch Eingabe-, Render- oder Schreibfehler unterdruecken. Die bestehenden Fehlerpfade liefern weiterhin Text auf stderr, nonzero Exit und kein Reportartefakt.
5. CLI-Tests in `tests/e2e/test_cli.cpp` ergaenzen:
   - Verbose-Renderfehler: `analyze --verbose --format json --output <path>` mit `ThrowingGenerateReportPort` liefert die dokumentierte fuenfzeilige stderr-Sequenz, leeres stdout, unveraenderte Zieldatei.
   - Verbose-Renderfehler fuer Markdown, DOT und HTML analog (eine Sequenz pro Format).
   - Verbose-Schreibfehler: `--verbose --format json --output <missing-dir>/out.json` liefert die dokumentierte sechszeilige stderr-Sequenz (`error:`, `hint:`, vier `verbose:`-Zeilen), leeres stdout, keine Datei.
   - Verbose-Schreibfehler fuer Markdown, DOT und HTML analog.
   - Verbose-Eingabefehler: `--verbose` mit nicht vorhandener Compile-Database liefert die normale Fehlermeldung plus die vier `verbose:`-Zeilen mit `validation_stage=input`.
   - Verbose-Analysefehler: `--verbose` mit ungueltigem File-API-Reply liefert die normale Fehlermeldung plus die vier `verbose:`-Zeilen mit `validation_stage=analysis`.
   - Verbose-Pflichtoptionen-Praezedenz: `impact --verbose` ohne `--changed-file` liefert nur die Usage-Fehlermeldung `impact requires --changed-file` ohne `verbose:`-Block, weil Pflichtoptionen Praezedenzstufe 4 haben und damit vor dem Verbose-Kontext-Helfer aus Schritt 1 greifen.
   - Verbose-Mutual-Exclusion-Praezedenz: `analyze --quiet --verbose` und `impact --quiet --verbose` liefern weiterhin den Tranche-A-Usage-Fehler ohne `verbose:`-Block; Negativtest stellt sicher, dass Schritt 1 nicht versehentlich vor der Mutual-Exclusion-Pruefung greift.
   - Quiet-Renderfehler: `--quiet --format json --output <path>` mit `ThrowingGenerateReportPort` liefert exakt die normale stderr-Fehlermeldung ohne Verbose-Zeilen, nonzero Exit, leeres stdout, unveraenderte Zieldatei.
   - Quiet-Schreibfehler analog.
   - Quiet-Eingabefehler: stderr enthaelt die normale Fehlermeldung; keine Verbose-Zeilen.
6. Tests stellen sicher, dass Verbose-Zeilen weder im Quiet- noch im Normalmodus erscheinen. Negativtest: `--format json` und `--quiet --format json` mit Render-Doppelgaenger duerfen kein `verbose:`-Praefix in stderr enthalten.

Sub-Risiken C.1:

- Render- und Schreibfehler fuer JSON, Markdown, DOT und HTML teilen sich denselben `emit_rendered_report`-Pfad. Der Verbose-Fehlerkontext muss daher genau einmal in dieser Funktion (oder ihrem Aufrufer) gesetzt werden, sonst entsteht Code-Duplikation und Lizard-CCN-Druck. Eine Hilfsstruktur `VerboseErrorContext { command, format, output, stage }` plus eine `format_verbose_error_context(streams, ctx)`-Funktion ist die kanonische Loesung.
- Praezedenzregel "CLI-Usage-Fehler vor allen anderen": `--verbose --quiet` muss vor jedem `verbose:`-stderr-Block greifen, sonst wuerde der Mutual-Exclusion-Test eine ungewollt lange stderr-Sequenz sehen. Tranche-A-Pruefung muss daher in C.1 erneut bestaetigt werden.
- `validation_stage=write` setzt voraus, dass `cannot write report:`-Pfade im Atomic-Writer einheitlich propagieren. Falls AP 1.1 verschiedene Schreibfehlerquellen unterschiedlich nach oben gibt, wird hier die einheitliche Markierung nachgezogen.

Definition of Done Sub-Tranche C.1:

- [ ] `VerboseErrorContext`-Helfer und `format_verbose_error_context`-Funktion existieren als file-lokale Helfer in `cli_adapter.cpp`.
- [ ] Verbose-Renderfehler erzeugen exakt die dokumentierte fuenfzeilige stderr-Sequenz fuer Markdown, JSON, DOT und HTML.
- [ ] Verbose-Schreibfehler erzeugen exakt die dokumentierte sechszeilige stderr-Sequenz fuer Markdown, JSON, DOT und HTML.
- [ ] Verbose-Eingabe- und -Analysefehler ergaenzen die normale Fehlermeldung um vier `verbose:`-Zeilen mit korrektem `validation_stage`.
- [ ] Quiet-Fehlerpfade zeigen weiterhin Text auf stderr und nonzero Exit ohne `verbose:`-Zeilen.
- [ ] Verbose-Zeilen erscheinen weder im Normal- noch im Quiet-Modus.
- [ ] `impact --verbose` ohne `--changed-file` liefert nur den Usage-Fehler ohne `verbose:`-Block.
- [ ] Mutual-Exclusion-Test aus Tranche A bleibt unveraendert gruen und liefert keinen `verbose:`-Block.
- [ ] Docker-Gates aus `README.md` und `docs/quality.md` sind gruen.

#### Sub-Tranche C.2 - Binary-E2E-Smokes

C.2 erweitert `tests/e2e/run_e2e.sh` und damit das CTest-Ziel `e2e_binary` um echte Binary-Smokes fuer Quiet und Verbose. Keine neuen Goldens; reine Verdrahtungsarbeit gegen die in Tranche B angelegten Goldens und gegen die Artefakt-Byte-Stabilitaet aus Tranche A.

1. Binary-Smokes fuer Console Quiet:
   - `analyze --quiet` gegen `console-analyze-quiet.txt`.
   - `impact --quiet` gegen `console-impact-quiet.txt`.
   - Zusatztests, dass stderr leer ist und stdout mit genau einem Newline endet.
2. Binary-Smokes fuer Console Verbose:
   - `analyze --verbose` gegen `console-analyze-verbose.txt`.
   - `impact --verbose` gegen `console-impact-verbose.txt`.
   - Zusatztests, dass stderr leer ist im Erfolgsfall (Verbose-stderr fuer Console wird nur beim Fehler aktiv) und stdout mit genau einem Newline endet.
3. Artefakt-Byte-Stabilitaet ueber `e2e_binary`:
   - `analyze --quiet --format json` ohne `--output` ist stdout-byte-identisch zum bestehenden `analyze_compile_db_only.json`-Goldenpfad.
   - `analyze --verbose --format json` ohne `--output` analog.
   - Wiederholung fuer Markdown, DOT und HTML mit jeweils einem repraesentativen Goldenpfad.
   - `analyze --quiet --format json --output <tempfile>` schreibt eine Datei, die byte-identisch zum entsprechenden Golden ist; stdout und stderr bleiben leer.
   - `analyze --verbose --format json --output <tempfile>` schreibt dieselbe Datei wie der Quiet-Lauf; stdout bleibt leer; stderr enthaelt die in der Verbose-Artefakt-Sektion dokumentierten `verbose:`-Zeilen in exakter Reihenfolge.
4. Verbose-stdout-Validity-Gate ueber die bereits in AP 1.2 / AP 1.3 / AP 1.4 vorhandenen Validatoren:
   - `analyze --verbose --format json` und `impact --verbose --format json` werden zusaetzlich zur Byte-Stabilitaet durch `assert_json_stdout_validates` (Schema-Validator aus AP 1.2) gegen `docs/report-json.schema.json` validiert. Verbose-stderr darf das nicht beeinflussen, weil der Validator nur stdout liest.
   - `analyze --verbose --format dot` und `impact --verbose --format dot` werden ueber `assert_dot_stdout_validates` (DOT-Python-Validator aus AP 1.3) syntax-geprueft.
   - `analyze --verbose --format html` und `impact --verbose --format html` werden ueber den HTML-Struktur-Smoke aus AP 1.4 (`tests/validate_html_reports.py`) validiert; ein neuer `assert_html_stdout_validates`-Helfer wird in `run_e2e.sh` ergaenzt, falls noch nicht vorhanden.
   - Markdown hat keinen syntax-Validator; die Byte-Stabilitaet aus Schritt 3 bleibt ausreichender Schutz.
5. Symmetrische Smokes fuer `impact --quiet/--verbose --format <fmt>` analog zu Schritt 3 und 4.
6. `tests/e2e/run_e2e.sh`-Helfer erweitern: ein neuer `assert_stderr_equals_lines` oder `assert_stderr_matches_sequence` Helfer prueft die exakten Verbose-stderr-Zeilen in dokumentierter Reihenfolge. Implementierung ueber `printf "%s\n" "${expected[@]}" | diff - <(captured_stderr)` oder gleichwertig. Helfer ist Pflicht, weil mindestens 2 Subcommands x 4 Formate = 8 Verbose-stderr-Sequenzen verifiziert werden und `assert_stderr_contains`-Wiederholung sowohl Reihenfolge nicht pinnt als auch Lizard-CCN-Druck erzeugt.

Sub-Risiken C.2:

- Verbose-Artefakt-stderr-Reihenfolge: die 7 dokumentierten Zeilen fuer `analyze` und 8 Zeilen fuer `impact` muessen in exakter Reihenfolge erscheinen. Ein Test, der nur `assert_stderr_contains "verbose: format=json"` benutzt, fixiert die Reihenfolge nicht; der neue Helfer aus Schritt 6 prueft per `diff`-Aequivalent die exakte Position. Off-by-one zwischen `analyze` (kein `changed_file_source`) und `impact` (mit `changed_file_source`) ist die Standardfalle.
- `top_limit` darf nicht in Verbose-Artefakt-stderr auftauchen. Ein Negativ-Smoke prueft `! grep "top_limit"` in der Verbose-stderr-Ausgabe.
- e2e_binary-Laufzeit: Tranche C fuegt etwa zwei Dutzend neue Smokes hinzu. Falls das CTest-Ziel zu lange laeuft, koennen wenig fachlich abgedeckte Faelle gestrichen werden, aber mindestens ein `(Subcommand, Modus, Format)`-Triple pro Format bleibt Pflicht.
- Validity-Gate-Reuse: die Validatoren aus AP 1.2 / 1.3 / 1.4 werden gegen die echte Binary gefahren, nicht gegen Goldens. Das deckt Verbose-stderr-Lecks in stdout indirekt ab, weil ein versehentliches `verbose:` vor dem JSON-Body den Schema-Validator brechen wuerde.

Definition of Done Sub-Tranche C.2:

- [ ] `e2e_binary` deckt `analyze --quiet`, `analyze --verbose`, `impact --quiet` und `impact --verbose` ueber Console-Goldens ab; alle Smokes laufen ueber die echte `cmake-xray`-Binary inklusive `src/main.cpp`.
- [ ] `e2e_binary` deckt Quiet- und Verbose-Artefakt-Byte-Stabilitaet fuer Markdown, JSON, DOT und HTML ab.
- [ ] Verbose-stdout fuer JSON, DOT und HTML wird zusaetzlich durch die jeweiligen Syntax-/Schema-Validatoren aus AP 1.2 / 1.3 / 1.4 verifiziert; Verbose-stderr-Lecks in stdout sind dadurch implizit ausgeschlossen.
- [ ] Verbose-Artefakt-stderr-Reihenfolge ist durch einen Helfer mit `diff`-Aequivalent pro Subcommand und Format exakt gepinnt (7 Zeilen fuer `analyze`, 8 Zeilen fuer `impact`).
- [ ] `top_limit` taucht in Verbose-Artefakt-stderr nicht auf.
- [ ] Bestehende Console-, Markdown-, JSON-, DOT- und HTML-Goldens bleiben byte-stabil.
- [ ] Docker-Gates aus `README.md` und `docs/quality.md` sind gruen.

#### Sub-Tranche C.3 - Nutzerdoku und Quality-Gates-Finalcheck

Doku-Sweep und finaler Coverage-/Lizard-/Clang-Tidy-Check. C.3 schliesst Tranche C ab.

1. `README.md` um `--quiet` und `--verbose` ergaenzen:
   - Erwaehnung in der Feature-Liste unter "Status".
   - Mindestens je ein `cmake-xray analyze --quiet` und `cmake-xray analyze --verbose` Beispiel.
   - Erwaehnung der command-lokalen Position und der gegenseitigen Exklusivitaet.
   - Hinweis, dass Quiet bei Artefaktformaten reportinhaltlich ein Noop ist und Verbose zusaetzliche Diagnostik nur nach `stderr` schreibt.
2. `docs/guide.md` um eine eigene "CLI-Modi: --quiet und --verbose"-Sektion ergaenzen:
   - Praktischer Quiet-Anwendungsfall (CI-Logs knapp halten).
   - Praktischer Verbose-Anwendungsfall (lokale Diagnose).
   - Beispiel fuer Verbose-Fehlerkontext bei Renderfehler.
   - Hinweis, dass Quiet keine Fehler unterdrueckt und Verbose keine Exit-Codes aendert.
   - Verweis auf `docs/quality.md` und Plan-`Verbosity-Policy`-Sektion fuer den vollstaendigen Vertrag.
3. `docs/quality.md` um die in Tranche A bis C gewachsenen Verbosity-Gates ergaenzen:
   - Mutual-Exclusion-Test, command-lokale Position, globale Positionen ohne Vertrag.
   - Console-Quiet- und Console-Verbose-Goldens, Manifest-Verzeichnis-Paritaet.
   - Artefakt-Byte-Stabilitaet ueber alle vier Artefaktformate.
   - Verbose-Fehlerkontext-Sequenzen fuer Render- und Schreibfehler.
   - Binary-E2E-Smokes ueber `e2e_binary`.
4. Coverage-Gate abschliessen. `docker build --target coverage-check --build-arg XRAY_COVERAGE_THRESHOLD=100` muss gruen sein. Pruefpunkte aus Tranche A bis C, die schwer per Stub-Test reachable sind: Verbose-Schreibfehler-Pfad (kann bei kleinen lokalen Tempdirs uebersehen werden), Quiet-Render-Precondition-Pfad (haengt von synthetischen `ImpactResult`-Instanzen ab), Verbose-Artefakt-stderr-Emitter im Erfolgsfall fuer alle vier Formate. Falls Coverage unter 100 % faellt, ist die fehlende Zeile in C.3 zu schliessen, nicht auf Tranche D zu verschieben.
5. Lizard-Gate abschliessen. `docker build --target quality-check` mit Standardgrenzen (`XRAY_LIZARD_MAX_CCN=10`, `XRAY_LIZARD_MAX_LENGTH=50`, `XRAY_LIZARD_MAX_PARAMETERS=5`) muss gruen sein. Pruefpunkte: die in B.2 angelegten per-Section-Helfer halten die Top-Level-Verbose-Renderer unter CCN 10; der `format_verbose_error_context`-Helfer aus C.1 hat CCN 1; falls eine Funktion an die Grenzen stoesst, ist Aufteilung in Hilfs-Funktionen Pflicht und `Verbosity-Policy`/`Console-Vertrag`/`Artefaktformat-Vertrag` bleiben unveraendert.
6. Clang-Tidy-Gate abschliessen. `docker build --target quality-check` mit `XRAY_CLANG_TIDY_MAX_FINDINGS=0` muss gruen sein. Pruefpunkte: keine neuen Findings in `cli_console_renderers.{h,cpp}`, `cli_adapter.cpp`, `output_verbosity.h`. Falls Findings auftauchen, in C.3 beheben.
7. Plan-`Liefer-Stand`-Eintraege fuer Tranche A, B und C auf "ausgeliefert in commit `<hash>`" flippen, sobald der Tranche-C-Commit gelandet ist (analog zur AP-1.4-Praxis).

Sub-Risiken C.3:

- README- und `docs/quality.md`-Aktualisierung muessen mit demselben Commit-Set wie die Tests landen, sonst greift die AP-1.2/1.3/1.4-Lehre und die Doku-Aktualisierung muss als separater Folge-Commit nachgezogen werden.
- Coverage-100%-Gate fuer `src/` kann durch CLI-Verbosity-Pfade kippen, die nur ueber bestimmte Eingabekombinationen erreichbar sind (z. B. ein nie-genutzter Verbose-Schreibfehler-Pfad). C.3 schliesst diese Luecken zwingend, bevor Tranche C abgenommen wird.
- Lizard-CCN-/Funktionslaengen-Grenzen koennen durch die Verbose-Renderer und den Verbose-Fehlerkontext-Helfer knapp werden; falls Refactoring noetig wird, bleiben Plan-Sektionen `Verbosity-Policy`, `Console-Vertrag` und `Artefaktformat-Vertrag` unveraendert und Goldens aus Tranche B bleiben byte-stabil.

Definition of Done Sub-Tranche C.3:

- [ ] `README.md` listet `--quiet` und `--verbose` mit Beispielen.
- [ ] `docs/guide.md` enthaelt eine "CLI-Modi: --quiet und --verbose"-Sektion mit praktischen Anwendungsfaellen.
- [ ] `docs/quality.md` listet Mutual-Exclusion-Test, Console-Goldens, Artefakt-Byte-Stabilitaet, Verbose-Fehlerkontext und Binary-E2E-Smokes als verbindliche Gates.
- [ ] Coverage-, Lizard- und Clang-Tidy-Gates sind gruen, ohne neue Befunde nach Tranche D zu verschieben.
- [ ] Plan-`Liefer-Stand` reflektiert die Tranche-A/B/C-Lieferung mit Commit-Hash.
- [ ] Docker-Gates aus `README.md` und `docs/quality.md` sind gruen.

#### Abnahme und Definition of Done Tranche C

Die Sub-Tranchen C.1, C.2 und C.3 werden gemeinsam abgenommen. Die folgenden Kriterien gelten am Ende von C.3 fuer das gesamte Tranche-C-Set.

Abnahme Tranche C: alle Docker-Gates aus `README.md` und `docs/quality.md` gruen; `e2e_binary` gruen; Quiet- und Verbose-Goldens und Artefakt-Byte-Stabilitaet sind abgedeckt; Verbose-Fehlerkontext folgt exakt den dokumentierten Sequenzen; globale Abnahmekriterien dieses Plans erfuellt.

Definition of Done Tranche C:

- [ ] Verbose-Fehlerkontext ist fuer Render-, Schreib-, Eingabe- und Analysefehler getestet.
- [ ] Quiet-Fehlerpfade zeigen weiterhin Text auf stderr und nonzero Exit ohne `verbose:`-Zeilen.
- [ ] Binary-Smokes decken beide Subcommands und beide Modi ueber Console-Goldens ab und laufen alle ueber die echte `cmake-xray`-Binary inklusive `src/main.cpp`.
- [ ] Binary-Smokes decken Quiet- und Verbose-Artefakt-Byte-Stabilitaet fuer Markdown, JSON, DOT und HTML ab.
- [ ] Verbose-stdout fuer JSON, DOT und HTML wird zusaetzlich durch die jeweiligen Syntax-/Schema-Validatoren aus AP 1.2 / 1.3 / 1.4 verifiziert.
- [ ] Artefakt-stdout und Datei-Inhalt bleiben in Quiet und Verbose byte-stabil; stdout endet mit demselben Newline-Vertrag wie der Normalmodus.
- [ ] Verbose-Artefakt-stderr enthaelt 7 Zeilen fuer `analyze` und 8 Zeilen fuer `impact` in dokumentierter Reihenfolge ohne `top_limit`-Eintrag.
- [ ] `README.md`, `docs/guide.md` und `docs/quality.md` sind aktualisiert.
- [ ] Coverage-, Lizard- und Clang-Tidy-Gates sind gruen.

### Tranche D - Optionale Haertung

Ohne diese Tranche gilt AP 1.5 als abgenommen, sobald Tranche C gruen ist.

- Weitere plattformspezifische CLI-Argument-Positionsfaelle.
- Zusaetzliche Golden-Faelle fuer grosse Diagnostic-Mengen.
- Weitere E2E-Smokes fuer alle Artefaktformate, falls AP 1.2 bis AP 1.4 in anderer Reihenfolge umgesetzt wurden.
- Zusaetzliche gezielte Regressionstests fuer Grenzfaelle, die ueber die verpflichtenden Coverage-, Lizard- und Clang-Tidy-Gates aus Tranche C hinausgehen.

Definition of Done Tranche D:

- [ ] Jede zusaetzliche Haertung ist durch einen fokussierten Test oder ein Golden abgesichert.
- [ ] Keine optionale Haertung veraendert den dokumentierten AP-1.5-Vertrag ohne Update dieses Plans oder der Nutzerdoku.
- [ ] Docker-Gates aus `README.md` und `docs/quality.md` bleiben gruen.

## Entscheidungen

Diese Entscheidungen sind vor Umsetzungsbeginn getroffen und in die Tranchen eingearbeitet:

- Verbosity bleibt CLI-Policy. Begruendung: Fachliche Analyse und Reportadapter sollen nicht wissen, ob die CLI gerade quiet oder verbose ist.
- Keine Port-Signaturaenderung. Begruendung: `ReportWriterPort` und `GenerateReportPort` sind Ergebnis-zu-Report-Vertraege; Verbosity ist kein Reportformatparameter.
- Console ist der einzige Formatpfad mit inhaltlich unterschiedlichen Quiet-/Verbose-Emissionen. Begruendung: Console ist der interaktive CLI-Pfad; Artefaktformate muessen stabil fuer Automatisierung bleiben.
- Verbose-Artifact-Diagnostik geht nur nach stderr. Begruendung: stdout bleibt fuer maschinenlesbare oder weiterleitbare Reports reserviert.
- Keine Erfolgsmeldung bei `--output`. Begruendung: M3/M5-Vertrag verlangt leeres stdout bei erfolgreicher Dateiausgabe.
- Command-lokale Position. Begruendung: Sie ist leicht testbar, vermeidet globale Parser-Sonderfaelle und entspricht dem M5-Scope.
- Quiet aendert keine Exit-Codes. Begruendung: Automatisierung darf nicht andere Fehlerbehandlung brauchen.

## Tests

Parser- und CLI-Tests:

- `analyze --quiet` wird akzeptiert.
- `analyze --verbose` wird akzeptiert.
- `impact --quiet` wird akzeptiert.
- `impact --verbose` wird akzeptiert.
- `analyze --quiet --verbose` liefert Usage-Fehler.
- `impact --quiet --verbose` liefert Usage-Fehler.
- globale `--quiet`-/`--verbose`-Positionen vor dem Subcommand liefern nonzero Usage-Fehler.
- `analyze --format console --output report.txt` liefert exakt die dokumentierte stderr-Fehlermeldung mit Hint, nonzero Exit, leeres stdout und erzeugt keine Datei.
- `impact --format console --output report.txt` liefert exakt die dokumentierte stderr-Fehlermeldung mit Hint, nonzero Exit, leeres stdout und erzeugt keine Datei.
- `impact --verbose` ohne `--changed-file` liefert Text auf stderr, nonzero Exit und keinen stdout-Report.

Console-Tests:

- Console Normal bleibt byte-stabil.
- Console Quiet Analyze entspricht Golden.
- Console Quiet Impact entspricht Golden.
- Console Verbose Analyze entspricht Golden.
- Console Verbose Impact entspricht Golden.
- Quiet zeigt keine vollstaendigen Diagnostic-Messages.
- Verbose zeigt reportweite Diagnostics vollstaendig.
- Verbose zeigt `ReportInputs`-Provenienz.
- Verbose zeigt Observation Source und Target Metadata Status.
- Quiet und Verbose enden mit genau einem Newline.

Artefakt-Tests:

- `--quiet --format markdown` ohne `--output` ist stdout-byte-identisch zum Normalmodus.
- `--verbose --format markdown` ohne `--output` ist stdout-byte-identisch zum Normalmodus.
- `--quiet --format json` ohne `--output` ist stdout-byte-identisch zum Normalmodus.
- `--verbose --format json` ohne `--output` ist stdout-byte-identisch zum Normalmodus und bleibt gueltiges JSON.
- Artefakt-Verbose-stderr enthaelt keinen `top_limit`-Eintrag.
- `--quiet --format dot` und `--verbose --format dot` werden analog getestet, sobald DOT umgesetzt ist.
- `--quiet --format html` und `--verbose --format html` werden analog getestet, sobald HTML umgesetzt ist.
- `--quiet --format json --output out.json` schreibt dieselbe Datei wie Normalmodus und laesst stdout/stderr leer.
- `--verbose --format json --output out.json` schreibt dieselbe Datei wie Normalmodus, laesst stdout leer und schreibt nur dokumentierte Verbose-Zeilen nach stderr.
- Markdown, DOT und HTML bekommen entsprechende `--output`-Tests, soweit implementiert.

Fehlerpfad-Tests:

- Quiet unterdrueckt Parser-/Usage-Fehler nicht.
- Quiet unterdrueckt Eingabefehler nicht.
- Quiet unterdrueckt Render-/Schreibfehler nicht.
- Verbose ergaenzt Fehlerkontext nur auf stderr.
- `--verbose --format json --output out.json` mit simuliertem Renderfehler schreibt exakt die dokumentierte Renderfehler-Sequenz.
- `--verbose --format json --output out.json` mit simuliertem Schreibfehler schreibt exakt die dokumentierte Schreibfehler-Sequenz.
- `--verbose --format markdown --output out.md` mit simuliertem Render- oder Schreibfehler folgt denselben stderr-Sequenzen; DOT und HTML werden entsprechend ergaenzt, sobald die Formate umgesetzt sind.
- Verbose-Fehlerkontext enthaelt keine Stacktraces, C++-Typnamen oder internen Funktionsnamen.
- Fehler erzeugen keine JSON-, DOT- oder HTML-Fehlerdokumente.

Port- und Architekturtests:

- Tests oder statische Code-Reviews stellen sicher, dass `ReportWriterPort` keinen `OutputVerbosity`-Parameter erhaelt.
- Tests oder statische Code-Reviews stellen sicher, dass `GenerateReportPort` keinen `OutputVerbosity`-Parameter erhaelt.
- Formatadapter-Konstruktoren bekommen keinen Verbosity-Parameter.
- Artifact-Goldens bleiben unveraendert.

Binary-E2E-Tests:

- `tests/e2e/run_e2e.sh` fuehrt mindestens einen echten Binary-Smoke fuer `analyze --quiet` aus.
- `tests/e2e/run_e2e.sh` fuehrt mindestens einen echten Binary-Smoke fuer `analyze --verbose` aus.
- `tests/e2e/run_e2e.sh` fuehrt mindestens einen echten Binary-Smoke fuer `impact --quiet` aus.
- `tests/e2e/run_e2e.sh` fuehrt mindestens einen echten Binary-Smoke fuer `impact --verbose` aus.
- Binary-Smokes pruefen, dass Reports nicht in den falschen Stream geschrieben werden.

Rueckwaertskompatibilitaets-Tests:

- Bestehende Normalmodus-Goldens bleiben unveraendert.
- Bestehende Fehler-Exit-Codes bleiben unveraendert.
- Bestehende `--output`-Semantik bleibt unveraendert.
- Bestehende `--top`-Semantik bleibt unveraendert.

## Abnahmekriterien

AP 1.5 ist abnahmefaehig, wenn:

- `cmake-xray analyze --quiet` und `cmake-xray analyze --verbose` lauffaehig sind.
- `cmake-xray impact --quiet` und `cmake-xray impact --verbose` lauffaehig sind.
- `--quiet` und `--verbose` gegenseitig exklusiv sind.
- die Optionen command-lokal nach dem Subcommand dokumentiert und getestet sind.
- Console Quiet fuer Analyze und Impact golden-getestete Kurzfassungen liefert.
- Console Verbose fuer Analyze und Impact golden-getestete detailreiche Ausgaben liefert.
- Console Normalmodus byte-stabil bleibt.
- Artifact-Reports fuer Markdown, JSON, DOT und HTML in Quiet und Verbose byte-identisch zum Normalmodus bleiben, soweit die Formate umgesetzt sind.
- Verbose-Diagnostik fuer Artefaktformate ausschliesslich auf stderr erscheint.
- JSON-, DOT- und HTML-stdout in Verbose weiterhin syntaktisch gueltige Reports ohne Zusatztext bleiben.
- `--output` bei erfolgreicher Dateiausgabe stdout leer laesst.
- Quiet keine Fehler unterdrueckt.
- Verbose keine Exit-Codes aendert.
- Reportports und Formatadapter keine Verbosity-Parameter erhalten.
- Fehlerpfade Text auf stderr liefern und keine JSON-/DOT-/HTML-Fehlerdokumente erzeugen.
- `README.md`, `docs/guide.md` und `docs/quality.md` Quiet/Verbose beschreiben.
- Binary-E2E-Smokes die echte Binary inklusive `src/main.cpp` abdecken.
- Coverage-, Lizard- und Clang-Tidy-Gates nach AP 1.5 gruen bleiben.

## Offene Folgearbeiten

Folgearbeiten ausserhalb von AP 1.5:

- AP 1.6 behandelt Release-Artefakte und OCI-Image.
- Eine spaetere Debug-Option kann detailliertere interne Diagnostik einfuehren, muss dann aber getrennt von `--verbose` spezifiziert werden.
- Eine spaetere globale CLI-Option vor dem Subcommand braucht einen eigenen Parser-Vertrag und eigene Tests.
- Eine spaetere Migration von Console Normal auf `ReportInputs` ist ein eigenes Arbeitspaket und muss bestehende Goldens explizit migrieren.

**Ergebnis**: Die CLI kann sowohl diagnoseorientiert als auch automationsfreundlich knapp betrieben werden, ohne die fachlichen Reportadapter oder maschinenlesbaren Artefakte zu destabilisieren.
