# Plan M5 - AP 1.8 Referenzdaten, Dokumentation und Abnahme aktualisieren

## Ziel

Die neuen Formate und Release-Pfade sind nur dann nutzbar, wenn Beispiele, Goldens und Dokumentation konsistent sind.

M5 aktualisiert mindestens:

- Versionsquellen fuer `v1.2.0`: Root-`CMakeLists.txt` fuer die numerische Projektversion sowie eine kompilierte oder generierte App-/Package-Version-Quelle wie `XRAY_APP_VERSION` oder `XRAY_VERSION_SUFFIX`, die von `ApplicationInfo` und Paketmetadaten fuer `--version` inklusive Prerelease-Suffix verwendet wird
- `docs/examples/` mit repraesentativen Markdown-, HTML-, JSON- und DOT-Ausgaben
- `README.md` mit Formatwahl, quiet/verbose und Release-/Container-Nutzung
- `docs/guide.md` mit praktischen Aufrufen fuer alle M5-Formate
- `docs/releasing.md` mit finalem Artefaktumfang und Release-Verifikation
- `docs/quality.md` mit M5-Testumfang und Plattform-Smokes
- `docs/performance.md` mit File-API-Performance-Baseline als erster Folge-Meilenstein nach M4
- `CHANGELOG.md` fuer `v1.2.0`

## 1.8 Scope

Tests und Abnahme muessen mindestens abdecken:

- Adapter-Unit-Tests fuer HTML, JSON und DOT
- HTML-Adapter-/Golden-Tests mit Sonderzeichen in Pfaden und Diagnostics, mindestens `<`, `>`, `&`, Anfuehrungszeichen und Backslashes, und Assertions auf korrekt escaped HTML ohne Rohinjektion
- CLI-Tests fuer Formatwahl, ungueltige Formatwerte und `--format markdown|html|json|dot --output <path>` fuer `analyze` und `impact`
- CLI-Negativtest, dass `--format console --output <path>` abgelehnt wird, weil `--output` auf artefaktorientierte Formate begrenzt bleibt
- CLI-Tests fuer atomare Dateiausgabe: erfolgreiche Writes erzeugen vollstaendige Reports, vorhandene Zieldateien werden bei Erfolg ersetzt, und Fehlerfaelle lassen vorhandene Zieldateien auf Linux, macOS und Windows unveraendert
- Unit-/Integrationstests fuer den Atomic-Replace-Wrapper, die existierende Ziele ohne vorheriges Loeschen ersetzen, simulierte Fehlerpfade mit intakter alter Datei pruefen und unter Windows die gewaehlte `ReplaceFileW`-/`MoveFileExW`-Semantik abdecken
- CLI-Tests, dass `--quiet` und `--verbose` command-lokal nach `analyze` bzw. `impact` akzeptiert werden und globale Positionen vor dem Subcommand nicht Teil des M5-Vertrags sind
- CLI-Tests fuer `--verbose`, `--quiet` und gegenseitigen Ausschluss
- CLI-Golden-Tests, dass `--quiet --format json|dot|html|markdown` ohne `--output` denselben stdout-Report wie der Normalmodus ausgibt und keine Erfolgsmeldungen in stdout mischt; mit `--output` muss stdout fuer alle Reportformate auch im Normal- und Quiet-Modus leer bleiben
- Golden-Output-Tests fuer `analyze` und `impact` in allen neuen Formaten
- Regressionstests, dass bestehende Console-/Markdown-Goldens fuer Compile-Database-only-, File-API- und Mixed-Input-Laeufe im Normalmodus byte-stabil bleiben und `ReportInputs` in AP 1.1 nicht neu in diesen bestehenden Formaten sichtbar wird
- Golden- und CLI-Tests, dass `--top` bei `analyze` fuer Markdown, HTML, JSON und DOT konsistent wirkt und kein Artefaktformat implizit vollstaendige Listen ausgibt
- DOT-Golden-Tests, dass `analyze --top N` fuer ausgegebene Top-Hotspots hoechstens `context_limit` betroffene Translation Units als Kontextknoten enthaelt, das globale `node_limit`-/`edge_limit`-Budget einhaelt, gekuerzten Kontext mit den verpflichtenden `context_*`-Node-Attributen kennzeichnet und die verpflichtenden `graph_*`-Graph-Attribute immer ausgibt
- DOT-Golden-Tests fuer gleichzeitige `context_limit`-, `node_limit`- und `edge_limit`-Kuerzung, damit `context_total_count`, `context_returned_count`, mehrfach referenzierte Kontext-Translation-Units und das Entfernen unverbundener reiner Kontextknoten deterministisch gezaehlt werden
- DOT-Golden-Tests, dass `impact --format dot` das feste Impact-`node_limit`-/`edge_limit`-Budget einhaelt und die verpflichtenden Graph-Attribute `graph_node_limit`, `graph_edge_limit` und `graph_truncated` immer ausgibt, mit `graph_truncated=false` im ungekuerzten Fall
- DOT-Golden-Tests fuer erreichte `edge_limit`-Faelle, die die globale Kantenprioritaet, lexikografische Tie-Breaker und die exakte Attributlexik fuer Integer, Boolean und Strings pruefen
- CLI- und Port-Tests, dass `impact` in M5 keine `--top`-Option akzeptiert und ImpactResult-basierte JSON-, HTML- und Markdown-Reports keine implizite fachliche Ergebnisbegrenzung oder JSON-`limit`-/`truncated`-Felder einfuehren; `impact --format dot` bleibt die visualisierungsorientierte Ausnahme mit festem `node_limit`-/`edge_limit`-Budget
- JSON-Schema-/Golden-Tests fuer `inputs.cmake_file_api_path` und `inputs.cmake_file_api_resolved_path` bei File-API- und Mixed-Input-Laeufen sowie fuer `limit`, `total_count`, `returned_count` und `truncated` bei gekuerzten und ungekuerzten `analyze`-Listen
- JSON-Schema-/Golden-Tests fuer `ReportInputs`-Pfad-Provenienz: CLI-relative Pfade, absolute Pfade, Build-Dir-vs.-Reply-Dir bei `--cmake-file-api` und relative `--changed-file` muessen stabile Display-Pfade und passende `*_source`-Enums liefern
- JSON-Schema-/Golden-Tests fuer feste JSON-`format`-Werte `cmake-xray.analysis` und `cmake-xray.impact`
- JSON-Schema-/Golden-Tests fuer alle in JSON v1 erlaubten `*_source`-Werte und Nullability-Regeln: `compile_database_source=cli|null`, `cmake_file_api_source=cli|null` und `changed_file_source=compile_database_directory|file_api_source_root|cli_absolute`; absolute-Pfad-Goldens muessen synthetische, fixture-stabile Pfade verwenden. `changed_file_source=unresolved_file_api_source_root` bleibt ein Modellwert fuer File-API-Fehlerergebnisse und wird in JSON v1 per Schema-/Adapter-Negativtest abgelehnt, weil nicht wiederherstellbare File-API-Ladefehler als Textfehler ohne JSON-Report enden.
- JSON-Schema-/Golden-Tests fuer Mixed-Input-Impact-Laeufe, dass relative `changed_file`-Pfade auf Basis der `compile_commands.json`-Directory interpretiert werden und `inputs.changed_file_source=compile_database_directory` ausgeben; File-API-only-Laeufe verwenden `file_api_source_root`, absolute CLI-Pfade verwenden `cli_absolute`
- Adapter-/Service-Tests, dass der vom `CmakeFileApiAdapter` aufgeloeste Build-/Reply-Pfad als roher lexikalischer Pfad ueber `BuildModelResult` transportiert wird, `ProjectAnalyzer`/`ImpactAnalyzer` daraus relativ zur `report_display_base` den Display-Pfad fuer `ReportInputs.cmake_file_api_resolved_path` erzeugen und kein CLI-Zustand in Adapter nachgereicht wird
- Port-/Service-Tests fuer `AnalyzeImpactRequest`, damit `compile_commands_path`, `changed_file_path`, `cmake_file_api_path` und `report_display_base` explizit transportiert und fuer `ReportInputs` genutzt werden
- JSON-Schema-/Golden-Tests, dass `inputs.compile_database_path` und `inputs.cmake_file_api_path` bei `analyze` immer vorhanden sind und fehlende Eingaben als `null`, nie als leerer String oder weggelassenes Feld, erscheinen
- JSON-Schema-/Golden-Tests, dass `inputs.compile_database_path`, `inputs.cmake_file_api_path` und `inputs.changed_file` bei `impact` immer vorhanden sind; nicht verwendete optionale Analysequellen erscheinen als `null`, waehrend `inputs.changed_file` wegen der CLI-Pflicht fuer `impact` immer ein String ist
- Service-Tests, dass `ProjectAnalyzer` und `ImpactAnalyzer` Report-Pfade relativ zur expliziten `invocation_cwd`-/`report_display_base`-Request-Basis serialisieren und nicht vom Prozess-CWD abhaengen
- Tests, dass `--verbose` JSON-, Markdown-, HTML- und DOT-Artefakte nicht gegenueber dem Normalmodus veraendert und Zusatzdiagnostik nur Console/`stderr` betrifft
- Console-Golden-Tests fuer Quiet-, Normal- und Verbose-Modus, die die groben Abschnittsvertraege und die Normalmodus-Rueckwaertskompatibilitaet absichern
- Smoke-Test fuer Docker-Runtime-Image
- Smoke-Test fuer Linux-Release-Artefakt nach Entpacken
- Pruefsummen-Test, dass die SHA-256-Datei existiert, den richtigen Linux-Archivnamen referenziert und gegen das erzeugte Archiv erfolgreich validiert
- Versionscheck, dass Root-`CMakeLists.txt` die numerische Basisversion meldet und `cmake-xray --version`, die kompilierte bzw. generierte `ApplicationInfo`-Version, App-/Package-Version-Quelle und Release-Tag dieselbe veroeffentlichte Semver-Version ohne fuehrendes `v` melden, inklusive Prerelease-Suffix bei Tags wie `v1.2.0-rc.1`
- CLI-Test, dass `cmake-xray --version` ohne Subcommand funktioniert, ausschliesslich die App-Version ohne fuehrendes `v` nach stdout schreibt und keine Analyse-/Report-Pfade initialisiert
- automatisierter Release-Test oder Workflow-Schritt fuer erlaubte Semver-Tags, Prerelease-Tags und Ablehnung ungueltiger Tags
- automatisierter Release-Dry-Run fuer Draft-Release, OCI-Image-Publish und finale oeffentliche Release-Publikation als letzten Schritt: `scripts/release-dry-run.sh` und ein gleichnamiger bzw. eindeutig benannter Workflow-Job muessen GitHub-Schritte mit einem Fake-Publisher samt Assertions fuer Zustandsuebergaenge und Reihenfolge sowie OCI-Schritte mit lokaler Test-Registry fuer Tagging, `latest`-Regel, Push und Manifest-Pruefung ausfuehren, ohne externe Veroeffentlichung
- dokumentierter manueller Dry-Run nur fuer Recovery-Pfade, die echte externe Publish-Zustaende in GitHub Releases oder GHCR voraussetzen
- Performance-Messung fuer den CMake-File-API-Pfad auf den bestehenden Referenzgroessen und Dokumentation der M5-Baseline in `docs/performance.md`
- Validierung, dass JSON syntaktisch gueltig ist, `format_version` enthaelt, den Pflichtfeld-, Typ-, Enum-, Nullability- und Array-Regeln aus `docs/report-json.md` entspricht und gegen `docs/report-json.schema.json` validiert
- Validierung, dass DOT syntaktisch durch Graphviz `dot -Tsvg` oder einen gleichwertigen DOT-Parser akzeptiert wird und Escaping-Goldens korrekt verarbeitet werden

**Ergebnis**: M5 ist nicht nur implementiert, sondern ueber Beispiele, Referenzdaten und Release-Dokumentation nachvollziehbar abnehmbar.

## Nicht umsetzen

- Neue fachliche Analysefeatures in `AnalysisResult` und `ImpactResult`
- Neue Target-Graph-Funktionen oder M6-Graph-Semantik
- Vollstaendige macOS-/Windows-Releasefreigabe im M5-Umfang
- Echte externe Release- oder Artefaktveroeffentlichung ausserhalb der bestehenden Gates; interne Dry-Run-, Fake-Publisher-, Pruefsummen-, Packaging- und Manifest-Validierung bleibt ausdruecklich Bestandteil von AP 1.8

## Voraussetzungen aus AP 1.1 bis AP 1.7

- Stabiler Report- und IO-Vertrag aus AP 1.1
- JSON-Adapter-Vertrag und CLI-Auslieferung aus AP 1.2
- DOT-Adapter-Vertrag, Limits und Teststrategie aus AP 1.3
- HTML-Adapter-Vertrag und Ausgabe aus AP 1.4
- Verbosity-Vertrag aus AP 1.5
- Release-Artefakt- und OCI-Kontrakte aus AP 1.6
- Plattform- und Smoke-Vertrag aus AP 1.7

## Dateien

- `docs/examples/`
- `docs/report-html.md`
- `docs/report-json.md`
- `docs/report-dot.md`
- `docs/guide.md`
- `README.md`
- `docs/releasing.md`
- `docs/quality.md`
- `docs/performance.md`
- `CHANGELOG.md`
- `tests/e2e/test_cli.cpp`
- `tests/e2e/run_e2e_lib.sh`
- `tests/e2e/run_e2e_artifacts.sh`
- `tests/e2e/run_e2e_smoke.sh`
- `tests/e2e/run_e2e_verbosity.sh`
- `tests/e2e/test_cli_verbosity.cpp`
- `tests/e2e/testdata/m5/`
- `tests/adapters/test_port_wiring.cpp`
- `tests/adapters/test_json_report_adapter.cpp`
- `tests/adapters/test_dot_report_adapter.cpp`
- `tests/adapters/test_html_report_adapter.cpp`
- `tests/adapters/test_atomic_report_writer.cpp`
- `tests/hexagon/test_project_analyzer.cpp`
- `tests/hexagon/test_impact_analyzer.cpp`
- `tests/release/test_release_dry_run.sh`
- `scripts/release-dry-run.sh`
- `.github/workflows/release.yml`
- `.github/workflows/test.yml`
- `.github/workflows/build.yml`

## Implementierungsreihenfolge

1. Referenzbeispiele und Formatdokumentation abschliessen
2. Dokumentation- und Changelog-Aktualisierung auf aktuellen M5-Vertrag bringen
3. E2E- und Adapter-Goldens konsolidieren, inklusive Sonderzeichen-, Top-Limit- und Atomic-Writer-Szenarien
4. CLI-Goldens fuer Formatwahl, `--output`, `--quiet`, `--verbose` und `--version` finalisieren
5. Release- und Plattform-Tests finalisieren und Smoke-/Pruefungsartefakte einbinden
6. Release-Dry-Run- und Packaging-Nachweise verifizieren
7. Abnahmedokumente und `docs/quality.md` auf finalen Status aktualisieren

## Abnahmekriterien

- Alle in diesem AP genannten Format- und Dokumentationsfälle sind mit Tests oder Goldens belegt
- Neue Formatausgaben sind in Dokumentation und Praxisbeispielen verlinkt und reproduzierbar
- `--output` funktioniert mit `markdown`, `html`, `json`, `dot` auf allen Plattformen als Atomic-Write inkl. Fehlererhaltung
- Quiet-/Verbose-Verhalten ist in CLI-Goldens für alle Ausgabeformate stabil und regressionsgesichert
- JSON- und DOT-Goldens entsprechen `docs/report-json.md` bzw. `docs/report-dot.md` sowie den validierten Schema-/Syntax-Gates
- Release-Scope und Versionierung sind dokumentiert und in Smoke/Dry-Run nachweislich konsistent
- `--top`-, `changed_file_source`-, und Provenienzregeln sind in Goldens und Schema-Tests belegt
- `release`-, `releasing`- und `quality`-Dokumentation sind auf den finalen M5-Ablauf abgeglichen

## Offene Folgearbeiten

- Weiterfuehrende Performance-Iteration in `docs/performance.md` nach den ersten M5-Baselines
- M6-Target-Graph-Semantik und Target-Priorisierung
- Vereinfachung der Plattform-Smoke-Auswertung auf einen festen, reproduzierbaren schema-validierten Bericht
