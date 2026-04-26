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
- Ranking-Eintraege behalten die sortierte Reihenfolge aus `AnalysisResult::translation_units` nach Anwendung von `top_limit`; bei zusaetzlich abgeleiteten Verbose-Listen wird nach `rank`, `source_path`, `directory`, `unique_key` sortiert.
- Include-Hotspots behalten die Reihenfolge aus `AnalysisResult::include_hotspots` nach Anwendung von `top_limit`; bei zusaetzlich abgeleiteten Verbose-Listen wird nach `header_path` sortiert.
- Hotspot-Kontext-Translation-Units werden pro Hotspot nach `source_path`, `directory`, `unique_key` sortiert, sofern die Modellreihenfolge nicht bereits explizit im Reportvertrag verwendet wird.
- Item-Diagnostics und reportweite Diagnostics behalten die Modellreihenfolge.
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
- Reportweite Diagnostics behalten die Modellreihenfolge.
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
- Fehlende optionale Werte werden als `not provided` geschrieben.
- Es werden keine Hostpfade ergaenzt, die nicht bereits im Ergebnis vorhanden sind.
- Keine Stacktraces, Typnamen oder internen Funktionsnamen.

## Fehlervertrag

### Gemeinsame Fehlerregeln

- Parser- und Usage-Fehler bleiben Textfehler auf `stderr`.
- Eingabefehler bleiben Textfehler auf `stderr`.
- Render- und Schreibfehler bleiben Textfehler auf `stderr`.
- `--format console --output <path>` ist immer ein Usage-Fehler: Text auf `stderr`, nonzero Exit, leeres stdout, keine Zieldatei.
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
- Fuer Render- und Schreibfehler mit `--verbose --output` ist die stderr-Sequenz exakt: zuerst die bestehende Fehlermeldung, danach die erlaubten `verbose:`-Zeilen in der oben dokumentierten Reihenfolge. Weitere stderr-Zeilen sind nicht zulaessig.
- Fuer Render- und Schreibfehler ohne `--verbose` bleibt stderr auf die bestehende Fehlermeldung und bestehende Hint-Zeilen beschraenkt.
- `validation_stage` wird aus der CLI-Schicht gesetzt, nicht aus Exception-Typen abgeleitet.
- Keine C++-Typnamen.
- Keine Stacktraces.
- Keine absoluten Hostpfade, ausser sie stammen bereits aus expliziten Eingabeargumenten oder fachlichen Ergebnisdaten.

## Implementierungsreihenfolge

Die Umsetzung erfolgt in drei verbindlichen Tranchen plus einer optionalen Haertungstranche. Jede Tranche endet mit einem vollstaendigen Lauf der Docker-Gates aus `README.md` ("Tests und Quality Gates") und `docs/quality.md`. Die globalen Abnahmekriterien dieses Plans gelten zusaetzlich am Ende von Tranche C.

### Tranche A - CLI-Modell, Parservertrag und Artefakt-Noop-Policy

Noch keine neue Console-Quiet-/Verbose-Emission; Fokus auf Parser, Policy und Byte-Stabilitaet.

1. `OutputVerbosity` in der CLI-Schicht einfuehren.
2. `CliOptions` um Verbosity erweitern.
3. `--quiet` und `--verbose` command-lokal fuer `analyze` und `impact` registrieren.
4. Gegenseitigen Ausschluss ueber CLI11 oder explizite Usage-Validierung implementieren.
5. Globale Positionen vor dem Subcommand als nicht unterstuetzt testen.
6. Artefaktformate so verdrahten, dass Quiet reportinhaltlich ein Noop ist.
7. Artefaktformate so verdrahten, dass Verbose Reportinhalt nicht veraendert und zusaetzliche Diagnostik nur nach stderr geht.
8. Tests fuer `--output` mit Quiet und Verbose bei mindestens JSON und Markdown; DOT und HTML werden einbezogen, wenn die jeweiligen APs umgesetzt sind.
9. `docs/quality.md` um Verbosity-Testumfang ergaenzen.

Abnahme Tranche A: alle Docker-Gates gruen; Parserposition, Exklusivitaet, Artefakt-Byte-Stabilitaet und stdout-/stderr-Vertraege sind getestet.

Definition of Done Tranche A:

- [ ] `--quiet` und `--verbose` existieren command-lokal fuer beide Subcommands.
- [ ] gleichzeitige Nutzung liefert Usage-Fehler.
- [ ] globale Positionen sind nicht Teil des Vertrags und liefern nonzero Usage-Fehler.
- [ ] Quiet veraendert Artefaktreports nicht.
- [ ] Verbose veraendert Artefaktreports nicht.
- [ ] `--output` erzeugt keine Erfolgsmeldung auf stdout.
- [ ] Docker-Gates aus `README.md` und `docs/quality.md` sind gruen.

### Tranche B - Console Quiet und Console Verbose

Console bekommt CLI-eigene Quiet-/Verbose-Emissionen.

1. Console-Normalmodus unveraendert lassen.
2. CLI-owned Quiet-Renderer fuer Analyze implementieren.
3. CLI-owned Quiet-Renderer fuer Impact implementieren.
4. CLI-owned Verbose-Renderer fuer Analyze implementieren.
5. CLI-owned Verbose-Renderer fuer Impact implementieren.
6. Renderer nutzen Ergebnisobjekte und `ReportInputs`, aber keine neuen Ports.
7. Golden-Outputs fuer Console Quiet und Verbose unter `tests/e2e/testdata/m5/verbosity/` anlegen.
8. Tests fuer Diagnostics, Target-Metadaten, File-API-only-, Mixed-Input- und leere Ergebnisfaelle ergaenzen.
9. Regressionscheck, dass Console-Normalmodus-Goldens byte-stabil bleiben.

Abnahme Tranche B: alle Docker-Gates gruen; Console Quiet und Verbose fuer Analyze und Impact sind golden-getestet; Normalmodus bleibt byte-stabil.

Definition of Done Tranche B:

- [ ] Console Quiet Analyze erzeugt nur die dokumentierte Kurzfassung.
- [ ] Console Quiet Impact erzeugt nur die dokumentierte Kurzfassung.
- [ ] Console Verbose Analyze enthaelt Inputs, Observation, fachliche Sections und Diagnostics.
- [ ] Console Verbose Impact enthaelt Inputs, Observation, fachliche Sections und Diagnostics.
- [ ] Console-Normalmodus-Goldens bleiben unveraendert.
- [ ] Keine Port- oder Adapter-Signatur enthaelt `OutputVerbosity`.
- [ ] Docker-Gates aus `README.md` und `docs/quality.md` sind gruen.

### Tranche C - Fehlerkontext, Binary-E2E, Dokumentation

Echte Binary-Verifikation und Dokumentationsabschluss.

1. Verbose-Fehlerkontext fuer Usage-, Eingabe-, Render- und Schreibfehler implementieren.
2. Tests fuer Quiet-Fehlerpfade sicherstellen: Fehler werden nicht unterdrueckt.
3. Tests fuer Verbose-Fehlerpfade sicherstellen: zusaetzlicher Kontext nur auf stderr.
4. `tests/e2e/run_e2e.sh` und `e2e_binary` um Smokes fuer `analyze --quiet`, `analyze --verbose`, `impact --quiet` und `impact --verbose` erweitern.
5. Artifact-Smokes fuer `--quiet --format json|markdown|dot|html` ohne `--output` und mit `--output` ergaenzen, soweit Formate umgesetzt sind.
6. `README.md` um `--quiet` und `--verbose` erweitern.
7. `docs/guide.md` um praktische Quiet-/Verbose-Beispiele erweitern.
8. `docs/quality.md` um Quiet-/Verbose-Goldens, Byte-Stabilitaet und Binary-Smokes erweitern.
9. Coverage-, Lizard- und Clang-Tidy-Gates muessen nach AP 1.5 weiterhin gruen sein; neue Befunde aus der CLI-Schicht werden in Tranche C behoben und nicht auf Tranche D verschoben.

Abnahme Tranche C: alle Docker-Gates aus `README.md` und `docs/quality.md` gruen; `e2e_binary` gruen; Quiet-/Verbose-Goldens und Artefakt-Byte-Stabilitaet sind abgedeckt; globale Abnahmekriterien dieses Plans erfuellt.

Definition of Done Tranche C:

- [ ] Verbose-Fehlerkontext ist fuer die dokumentierten Fehlerstufen getestet.
- [ ] Quiet-Fehlerpfade zeigen weiterhin Text auf stderr und nonzero Exit.
- [ ] Binary-Smokes decken beide Subcommands und beide Modi ab.
- [ ] Artefaktformate bleiben in Quiet und Verbose byte-stabil.
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
- `analyze --format console --output report.txt` liefert Text auf stderr, nonzero Exit, leeres stdout und erzeugt keine Datei.
- `impact --format console --output report.txt` liefert Text auf stderr, nonzero Exit, leeres stdout und erzeugt keine Datei.
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
- `--verbose --format json --output out.json` mit simuliertem Render- oder Schreibfehler schreibt zuerst die normale Fehlermeldung und danach nur die dokumentierten `verbose:`-Kontextzeilen.
- `--verbose --format markdown --output out.md` mit simuliertem Render- oder Schreibfehler folgt derselben stderr-Reihenfolge; DOT und HTML werden entsprechend ergaenzt, sobald die Formate umgesetzt sind.
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
