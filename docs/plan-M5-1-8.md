# Plan M5 - AP 1.8 Referenzdaten, Dokumentation und Abnahme aktualisieren

## Ziel

Die neuen Formate und Release-Pfade sind nur dann nutzbar, wenn Beispiele, Goldens und Dokumentation konsistent sind.

M5 aktualisiert mindestens:

- Versionsquellen fuer `v1.2.0`: Root-`CMakeLists.txt` meldet fuer M5 exakt `project(... VERSION 1.2.0)` als numerische Projektversion; Release-Builds verwenden `XRAY_APP_VERSION` aus dem validierten Tag ohne fuehrendes `v` als kanonische App-/Package-Version fuer `ApplicationInfo`, Paketmetadaten und `--version`, waehrend `XRAY_VERSION_SUFFIX` nur fuer lokale oder nicht veroeffentlichende Builds zulaessig ist und gleichzeitiges Setzen beider Quellen fail-fast abbricht
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
- CLI-Tests fuer atomare Dateiausgabe: auf Linux erzeugen erfolgreiche Writes vollstaendige Reports, vorhandene Zieldateien werden bei Erfolg ersetzt, und Fehlerfaelle lassen vorhandene Zieldateien unveraendert; auf macOS und Windows ist derselbe Nachweis verpflichtend, wenn die Plattform in AP 1.7 `validated_smoke` erreicht, andernfalls werden fehlende oder rote Atomic-Write-/CLI-Gates konkret als `known_limited` dokumentiert
- Unit-/Integrationstests fuer den Atomic-Replace-Wrapper, die existierende Ziele ohne vorheriges Loeschen ersetzen, simulierte Fehlerpfade mit intakter alter Datei pruefen und unter Windows die gewaehlte `ReplaceFileW`-/`MoveFileExW`-Semantik abdecken
- CLI-Tests, dass `--quiet` und `--verbose` command-lokal nach `analyze` bzw. `impact` akzeptiert werden und globale Positionen vor dem Subcommand weiterhin als Usage-Fehler abgelehnt werden: nonzero Exit, Text auf `stderr`, kein stdout-Report und keine Zieldatei
- CLI-Tests fuer `--verbose`, `--quiet` und gegenseitigen Ausschluss
- CLI-Golden-Tests, dass `--quiet --format json|dot|html|markdown` ohne `--output` denselben stdout-Report wie der Normalmodus ausgibt und keine Erfolgsmeldungen in stdout mischt; mit `--output` muss stdout fuer alle Reportformate im Normal-, Quiet- und Verbose-Modus leer bleiben, waehrend Verbose-Zusatzdiagnostik ausschliesslich auf `stderr` erscheinen darf
- Dokumentations- und Stream-Vertrag-Sync fuer `docs/report-json.md`, `docs/report-dot.md` und `docs/report-html.md`: erfolgreiche Normal- und Quiet-Artefaktausgaben bleiben ohne `--output` stdout-only und mit `--output` stdout/stderr-leer; erfolgreiche Verbose-Artefaktausgaben duerfen ausschliesslich Zusatzdiagnostik auf `stderr` ausgeben und Report-stdout beziehungsweise Reportdatei nicht veraendern
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
- Port-/Service-Tests fuer `AnalyzeProjectRequest`, damit `compile_commands_path`, `cmake_file_api_path` und `report_display_base` explizit transportiert und fuer Analyze-`ReportInputs` genutzt werden
- Port-/Service-Tests fuer `AnalyzeImpactRequest`, damit `compile_commands_path`, `changed_file_path`, `cmake_file_api_path` und `report_display_base` explizit transportiert und fuer `ReportInputs` genutzt werden
- JSON-Schema-/Golden-Tests, dass `inputs.compile_database_path` und `inputs.cmake_file_api_path` bei `analyze` immer vorhanden sind und fehlende Eingaben als `null`, nie als leerer String oder weggelassenes Feld, erscheinen
- JSON-Schema-/Golden-Tests, dass `inputs.compile_database_path`, `inputs.cmake_file_api_path` und `inputs.changed_file` bei `impact` immer vorhanden sind; nicht verwendete optionale Analysequellen erscheinen als `null`, waehrend `inputs.changed_file` wegen der CLI-Pflicht fuer `impact` immer ein String ist
- Service-Tests, dass `ProjectAnalyzer` und `ImpactAnalyzer` Report-Pfade relativ zur expliziten `invocation_cwd`-/`report_display_base`-Request-Basis serialisieren und nicht vom Prozess-CWD abhaengen
- Tests, dass `--verbose` JSON-, Markdown-, HTML- und DOT-Artefakte nicht gegenueber dem Normalmodus veraendert und Zusatzdiagnostik nur Console/`stderr` betrifft
- Console-Golden-Tests fuer Quiet-, Normal- und Verbose-Modus, die die groben Abschnittsvertraege und die Normalmodus-Rueckwaertskompatibilitaet absichern
- Smoke-Test fuer Docker-Runtime-Image
- Smoke-Test fuer Linux-Release-Artefakt nach Entpacken
- Pruefsummen-Test, dass die SHA-256-Datei existiert, den richtigen Linux-Archivnamen referenziert, gegen das erzeugte Archiv erfolgreich validiert und die enthaltene Binary-Checksumme beziehungsweise dokumentierte Binary-Pruefsumme mit dem entpackten Artefakt abgleicht
- Reproduzierbarkeits-Smoke fuer das Linux-Archiv: zwei Builds aus gleichem Commit und gleicher Version muessen Archiv-Checksumme, entpackte Dateiliste, Dateimodi und Binary-Checksumme stabil vergleichen
- Versionscheck, dass Root-`CMakeLists.txt` fuer M5 exakt `project(... VERSION 1.2.0)` meldet, Release-Builds `XRAY_APP_VERSION` aus dem validierten Tag ohne fuehrendes `v` verwenden, `XRAY_VERSION_SUFFIX` nur fuer lokale oder nicht veroeffentlichende Builds zulaessig ist, gleichzeitiges Setzen beider Quellen fail-fast abbricht und `cmake-xray --version`, die kompilierte bzw. generierte `ApplicationInfo`-Version, App-/Package-Version-Quelle und Release-Tag dieselbe veroeffentlichte Semver-Version ohne fuehrendes `v` melden, inklusive Prerelease-Suffix bei Tags wie `v1.2.0-rc.1`
- CLI-Test, dass `cmake-xray --version` ohne Subcommand funktioniert, ausschliesslich die App-Version ohne fuehrendes `v` nach stdout schreibt und keine Analyse-/Report-Pfade initialisiert
- automatisierter Release-Test oder Workflow-Schritt fuer erlaubte Semver-Tags, Prerelease-Tags und Ablehnung ungueltiger Tags
- automatisierter Release-Dry-Run fuer Draft-Release, OCI-Image-Publish und finale oeffentliche Release-Publikation als letzten Schritt: `scripts/release-dry-run.sh` und ein gleichnamiger bzw. eindeutig benannter Workflow-Job muessen GitHub-Schritte mit einem Fake-Publisher samt Assertions fuer Zustandsuebergaenge und Reihenfolge sowie OCI-Schritte mit lokaler Test-Registry fuer Tagging, `latest`-Regel, Push und Manifest-Pruefung ausfuehren, ohne externe Veroeffentlichung
- dokumentierter manueller Dry-Run nur fuer Recovery-Pfade, die echte externe Publish-Zustaende in GitHub Releases oder GHCR voraussetzen
- Performance-Messung fuer den CMake-File-API-Pfad auf reproduzierbaren Referenzgroessen und Dokumentation der M5-Baseline in `docs/performance.md`: AP 1.8 erzeugt oder versioniert die fuer `scale_*` noetigen CMake-File-API-Reply-Fixtures ueber `tests/reference/generate_reference_projects.py` oder ein explizites File-API-Fixture-Skript, dokumentiert Generator, CMake-Version, Fixture-Manifest, Reply-Verzeichnis und Erzeugungskommando in `tests/reference/file-api-performance-manifest.json`, und verhindert, dass compile-db-only Referenzdaten als File-API-Baseline missverstanden werden
- Plattform-Abnahme fuer Linux, macOS und Windows gemaess AP 1.7: jede Plattform ist in `docs/quality.md` und `docs/releasing.md` mit Status `supported`, `validated_smoke` oder `known_limited` dokumentiert; macOS und Windows erreichen `validated_smoke` nur mit verpflichtendem CI-Required-Check oder schema-validiertem Smoke-Report fuer Build, Atomic-Replace und normative CLI-Smokes, andernfalls werden konkrete Einschraenkungen als `known_limited` dokumentiert
- Doku-Gate fuer Nutzeraufrufe: `README.md`, `docs/guide.md`, `docs/releasing.md` und `docs/examples/` duerfen keine interne Build-Pfad-Nutzung wie `./build/cmake-xray` voraussetzen und beschreiben Release-Artefakte, Container oder installierte Binary-Pfade als regulaere Nutzerwege; `docs/performance.md` ist als Entwickler- und Messdokument ausgenommen, muss interne Build-Pfade aber als reproduzierbare Messkommandos kennzeichnen und darf sie nicht als normale Nutzeraufrufe darstellen
- Validierung, dass JSON syntaktisch gueltig ist, `format_version` enthaelt, den Pflichtfeld-, Typ-, Enum-, Nullability- und Array-Regeln aus `docs/report-json.md` entspricht und gegen `docs/report-json.schema.json` validiert
- Validierung, dass DOT syntaktisch durch Graphviz `dot -Tsvg` oder einen gleichwertigen DOT-Parser akzeptiert wird und Escaping-Goldens korrekt verarbeitet werden
- Validierung, dass `docs/examples/` keine driftenden Kopien enthaelt: Markdown-, HTML-, JSON- und DOT-Beispiele werden entweder aus den validierten Goldens generiert und in CI gegen den aktuellen Generatorausgang verglichen oder ueber ein CTest-Gate wie `tests/validate_doc_examples.py` und `docs/examples/manifest.txt` selbst validiert; Markdown-Beispiele brauchen dabei einen expliziten Bytevergleich gegen Golden-Outputs oder ein eigenes Markdown-/Doc-Example-Manifest, waehrend JSON, DOT und HTML zusaetzlich in die jeweiligen Schema-, Syntax- und Struktur-Manifeste aufgenommen werden

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
- `docs/report-json.schema.json`
- `docs/report-dot.md`
- `docs/examples/manifest.txt`
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
- `tests/validate_json_schema.py`
- `tests/validate_doc_examples.py`
- `tests/e2e/testdata/m5/json-reports/manifest.txt`
- `tests/e2e/testdata/m5/dot-reports/manifest.txt`
- `tests/e2e/testdata/m5/html-reports/manifest.txt`
- `tests/e2e/testdata/m5/markdown-reports/manifest.txt` oder ein gleichwertiger Markdown-Eintrag in `docs/examples/manifest.txt`
- `tests/validate_dot_reports.py`
- `tests/validate_html_reports.py`
- `tests/validate_verbosity_reports.py`
- `tests/reference/generate_reference_projects.py`
- `tests/reference/file-api-performance-manifest.json`
- `tests/reference/scale_*/build/.cmake/api/v1/reply/`, falls AP 1.8 versionierte Reply-Fixtures statt rein reproduzierbarer Generierung waehlt
- `tests/adapters/test_port_wiring.cpp`
- `tests/adapters/test_json_report_adapter.cpp`
- `tests/adapters/test_dot_report_adapter.cpp`
- `tests/adapters/test_html_report_adapter.cpp`
- `tests/adapters/test_atomic_report_writer.cpp`
- `tests/hexagon/test_project_analyzer.cpp`
- `tests/hexagon/test_impact_analyzer.cpp`
- `tests/release/test_release_dry_run.sh`
- `tests/release/test_release_archive_repro.sh`
- `tests/release/test_validate_release_tag.sh`
- `scripts/build-release-archive.sh`
- `scripts/release-dry-run.sh`
- `scripts/validate-release-tag.sh`
- `.github/workflows/release.yml`
- `.github/workflows/test.yml`
- `.github/workflows/build.yml`

## Implementierungsreihenfolge

1. E2E- und Adapter-Goldens konsolidieren, inklusive Sonderzeichen-, Top-Limit- und Atomic-Writer-Szenarien
2. Validator- und Golden-Manifeste finalisieren, inklusive JSON, DOT, HTML, Verbosity und `docs/examples/`
3. Referenzbeispiele aus Goldens generieren oder an die Manifeste anbinden und Formatdokumentation abschliessen
4. Dokumentation- und Changelog-Aktualisierung auf aktuellen M5-Vertrag bringen
5. CLI-Goldens fuer Formatwahl, `--output`, `--quiet`, `--verbose` und `--version` finalisieren
6. Release- und Plattform-Tests finalisieren und Smoke-/Pruefungsartefakte einbinden
7. Release-Dry-Run- und Packaging-Nachweise verifizieren
8. Abnahmedokumente und `docs/quality.md` auf finalen Status aktualisieren

## Abnahmekriterien

- Alle in diesem AP genannten Format- und Dokumentationsfälle sind mit Tests oder Goldens belegt
- Neue Formatausgaben sind in Dokumentation und Praxisbeispielen verlinkt, reproduzierbar und ueber dieselben Validierungsmanifeste wie Goldens abgesichert
- `--output` funktioniert auf Linux mit `markdown`, `html`, `json`, `dot` als Atomic-Write inkl. Fehlererhaltung; fuer macOS und Windows gilt derselbe Nachweis nur bei Plattformstatus `validated_smoke`, andernfalls muessen die fehlenden oder roten Atomic-Write-/CLI-Gates als konkrete `known_limited`-Einschraenkung dokumentiert sein
- Quiet-/Verbose-Verhalten ist in CLI-Goldens für alle Ausgabeformate stabil und regressionsgesichert
- `--verbose --output <path>` ist fuer Markdown, HTML, JSON und DOT mit leerem stdout und Zusatzdiagnostik nur auf `stderr` golden-getestet
- JSON- und DOT-Goldens entsprechen `docs/report-json.md` bzw. `docs/report-dot.md` sowie den validierten Schema-/Syntax-Gates
- Release-Scope und Versionierung sind dokumentiert und in Smoke/Dry-Run nachweislich konsistent
- Root-`CMakeLists.txt` meldet fuer M5 exakt `project(... VERSION 1.2.0)`; ein verbleibender `1.1.0`-Basiswert ist nicht abnahmefaehig
- Plattformstatus fuer Linux, macOS und Windows ist nach AP 1.7 dokumentiert und ueber Required Checks oder validierte Smoke-Reports belegt; rote, fehlende oder freiwillige macOS-/Windows-Gates fuehren zu `known_limited`, nicht zu `validated_smoke`
- README, Guide, Releasing-Doku und Beispiele beschreiben Nutzeraufrufe ohne interne Build-Pfade wie `./build/cmake-xray`; `docs/performance.md` ist als Entwickler- und Messdokument ausgenommen, muss interne Build-Pfade aber als reproduzierbare Messkommandos kennzeichnen und darf sie nicht als normale Nutzeraufrufe darstellen
- File-API-Performance-Baseline ist ueber versionierte oder reproduzierbar generierte File-API-Reply-Fixtures, `tests/reference/file-api-performance-manifest.json`, konkrete Reply-Verzeichnisse, Generator/CMake-Angaben und dokumentierte Messkommandos nachvollziehbar
- Release-Archiv-Abnahme deckt den vollstaendigen AP-1.6-Vertrag ab: SHA-256-Sidecar, Archivinhalt, Binary-Checksumme und reproduzierbarer Doppelbuild sind automatisiert geprueft
- Markdown-Beispiele in `docs/examples/` sind ueber Generatorvergleich, Bytevergleich gegen Goldens oder ein explizites Markdown-/Doc-Example-Manifest gegen Drift abgesichert
- `--top`-, `changed_file_source`-, und Provenienzregeln sind in Goldens und Schema-Tests belegt
- `release`-, `releasing`- und `quality`-Dokumentation sind auf den finalen M5-Ablauf abgeglichen

## Offene Folgearbeiten

- Weiterfuehrende Performance-Iteration in `docs/performance.md` nach den ersten M5-Baselines
- M6-Target-Graph-Semantik und Target-Priorisierung
- Vereinfachung der Plattform-Smoke-Auswertung auf einen festen, reproduzierbaren schema-validierten Bericht
