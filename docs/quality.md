# Qualität

## Zweck

Dieses Dokument beschreibt die verbindlichen Qualitaets- und Metrikpfade fuer `cmake-xray`.
Es dokumentiert keine punktuelle Review-Historie und keine einmaligen KI-Befunde, sondern die aktuell gueltigen, reproduzierbaren Docker-Pruefwege.

## Statische Analyse und Metriken

Die statische Analyse und die Metriken werden Docker-basiert fuer den Produktcode unter `src/` erhoben:

- `clang-tidy` prueft regelbasierte statische Analysebefunde
- `lizard` prueft Komplexitaet, Funktionslaenge und Parameteranzahl
- Report und Gate sind bewusst getrennt

### Report-Pfad

```bash
docker build --target quality -t cmake-xray:quality .
docker run --rm cmake-xray:quality
```

Der `quality`-Stage erzeugt und zeigt diese Artefakte:

- `/workspace/build-quality/quality/summary.txt`
- `/workspace/build-quality/quality/clang-tidy.txt`
- `/workspace/build-quality/quality/lizard.txt`
- `/workspace/build-quality/quality/lizard-warnings.txt`

### Gate-Pfad

```bash
docker build --target quality-check -t cmake-xray:quality-check .
```

Optional lassen sich die Grenzwerte per Build-Argument anpassen:

```bash
docker build --target quality-check \
  --build-arg XRAY_CLANG_TIDY_MAX_FINDINGS=0 \
  --build-arg XRAY_LIZARD_MAX_CCN=10 \
  --build-arg XRAY_LIZARD_MAX_LENGTH=50 \
  --build-arg XRAY_LIZARD_MAX_PARAMETERS=5 \
  -t cmake-xray:quality-check .
```

Aktuelle Standardgrenzen:

- `clang-tidy`: maximal `0` Findings
- `lizard` CCN: maximal `10`
- `lizard` Funktionslaenge: maximal `50`
- `lizard` Parameteranzahl: maximal `5`

Konfigurierbare Build-Argumente:

- `XRAY_CLANG_TIDY_MAX_FINDINGS`
- `XRAY_LIZARD_MAX_CCN`
- `XRAY_LIZARD_MAX_LENGTH`
- `XRAY_LIZARD_MAX_PARAMETERS`

## Testabdeckung

Die Coverage wird ebenfalls Docker-basiert fuer `src/` ermittelt:

- `ctest` fuehrt die Tests aus
- `gcovr` erzeugt den Coverage-Report
- die Instrumentierung wird ueber `XRAY_ENABLE_COVERAGE=ON` aktiviert
- das Gate bezieht sich auf Line-Coverage unter `src/`

### Report-Pfad

```bash
docker build --target coverage -t cmake-xray:coverage .
docker run --rm cmake-xray:coverage
```

Der `coverage`-Stage erzeugt und zeigt diese Artefakte:

- `/workspace/build-coverage/coverage/summary.txt`
- `/workspace/build-coverage/coverage/coverage.txt`

### Gate-Pfad

```bash
docker build --target coverage-check \
  --build-arg XRAY_COVERAGE_THRESHOLD=100 \
  -t cmake-xray:coverage-check .
```

Der Standard fuer den aktuellen M3-/MVP-Stand ist:

- minimale Line-Coverage unter `src/`: `100%`

Das Gate ist per `XRAY_COVERAGE_THRESHOLD` konfigurierbar.

### Coverage-Ausnahmen-Gate

Coverage-Ausnahmen im Produktcode sind nicht Bestandteil des regulaeren Gates. Vor
Coverage- oder Release-Pruefungen muss dieser Befehl den Wert `0` liefern:

```bash
rg "GCOVR""_EXCL_" | wc -l
```

## Laufzeit-Image

Das Runtime-Image ist fuer den aktuellen Stand gehaertet:

- das Binary laeuft nicht als `root`
- das Runtime-Stage legt einen dedizierten Systembenutzer `cmake-xray` an
- das Binary wird diesem Benutzer gehoerig nach `/app/cmake-xray` kopiert

Der Runtime-Smoke-Test fuer Release- und Abnahmepfade ist:

```bash
docker build --target runtime -t cmake-xray .
docker run --rm cmake-xray --help
```

## JSON-Reportvertrag

Der maschinenlesbare JSON-Reportvertrag ist in [docs/report-json.md](./report-json.md) dokumentiert und durch [docs/report-json.schema.json](./report-json.schema.json) (JSON Schema Draft 2020-12) formal abgesichert. Zwei CTest-Gates schuetzen den Vertrag in jeder Build-Matrix:

- **Schema-Wohlgeformtheit und Manifest-Paritaet** ueber `tests/validate_json_schema.py`. Der CTest-Eintrag heisst `report_json_schema_validation`. Er prueft die Schema-Datei gegen das Draft-2020-12-Meta-Schema, gleicht das Manifest unter `tests/e2e/testdata/m5/json-reports/manifest.txt` mit dem Verzeichnis ab und validiert jedes gelistete Golden gegen das Schema.
- **Formatversionskonsistenz** im Test `report-json schema format_version const matches kReportFormatVersion` in `xray_tests`. Er parst das Schema und prueft den `$defs/FormatVersion/const`-Wert gegen die C++-Konstante `xray::hexagon::model::kReportFormatVersion` in `src/hexagon/model/report_format_version.h`.
- **JSON-Adapter-Vertrag** in `xray_tests` ueber `tests/adapters/test_json_report_adapter.cpp`. Pflichtfelder, Typen, Enums, Nullability, Sortier-Tie-Breaker, `include_analysis_heuristic`-Beide-Werte und der Schema-Fail-Fall fuer `unresolved_file_api_source_root` sind dort verbindlich abgesichert.
- **JSON-Wiring** in `xray_tests` ueber `tests/adapters/test_port_wiring.cpp`. Tests stellen sicher, dass `--format json` ueber Composition Root und CLI bis zum `JsonReportAdapter` durchverdrahtet ist und nicht in den Console-Fallback faellt.
- **Binary-E2E-Goldens** ueber das CTest-Ziel `e2e_binary_artifacts` und `tests/e2e/run_e2e_artifacts.sh`. Die echte `cmake-xray`-Binary erzeugt JSON-Berichte fuer Compile-Database-only-, File-API-only- (Build-Dir und Reply-Dir), Mixed-Input-, Truncated- und Hotspot-Truncated-Faelle bei `analyze`, sowie fuer `compile_database_directory`-, `file_api_source_root`-, Mixed-Prioritaet- und `cli_absolute`-Faelle bei `impact`. Die Ausgaben werden byte-stabil gegen die Goldens unter `tests/e2e/testdata/m5/json-reports/` verglichen.

Diese Gates sind verpflichtend und werden ueber alle in diesem Dokument aufgefuehrten Docker-Pfade sowie ueber die nativen Build-Matrizen in `.github/workflows/build.yml` und `.github/workflows/release.yml` ausgefuehrt.

### Validator-Abhaengigkeit

Das Validatorskript benoetigt das Python-Paket `jsonschema` (Version `>=4.10,<5`) inklusive `Draft202012Validator`. Die Bootstrap-Pfade sind:

- **Docker**: das `toolchain`-Stage in [Dockerfile](../Dockerfile) installiert `python3-jsonschema` ueber `apt`. Alle abgeleiteten Stages, die `ctest` ausfuehren (`test`, `coverage`, `coverage-check`), erben die Abhaengigkeit.
- **Native CI** (Linux/macOS/Windows-Matrizen): die GitHub-Workflows installieren das Paket vor `ctest` ueber `python -m pip install -r tests/requirements-json-schema.txt`.
- **Lokale Entwickler**: derselbe `pip install`-Befehl. Auf Debian/Ubuntu reicht alternativ `apt-get install python3-jsonschema`.

CTest schlaegt mit konkreter Installationsanweisung fehl, wenn das Paket nicht installiert ist. Der Test wird nicht still uebersprungen und greift nicht auf das Netzwerk zu.

## DOT-Reportvertrag

Der visualisierungsorientierte DOT-Reportvertrag ist in [docs/report-dot.md](./report-dot.md) dokumentiert. Folgende CTest-Gates sichern den Vertrag:

- **DOT-Python-Syntax-Gate** ueber `tests/validate_dot_reports.py`. Der CTest-Eintrag heisst `dot_report_python_validation`. Er prueft Pflichtattribute (`xray_report_type`, `format_version`, `graph_node_limit`, `graph_edge_limit`, `graph_truncated`), digraph-Namensvertrag, balancierte Klammern und Quoted-String-Escape-Regeln. Der Validator nutzt nur die Python-Standardbibliothek und laeuft auf jedem Host mit `python3` auf dem PATH.
- **DOT-Graphviz-Syntax-Gate** ueber `dot -Tsvg`. Der CTest-Eintrag heisst `dot_report_graphviz_validation` und ist nur registriert, wenn `dot` zur Build-Zeit auffindbar ist. Im Docker-Stage ist `graphviz` per apt installiert; native CI-Matrizen ohne Graphviz fuehren nur den Python-Pfad aus.
- **DOT-Graphviz-Format-Gates (Tranche D)** ueber `dot -Tplain` und `dot -Tjson`. Die CTest-Eintraege heissen `dot_report_graphviz_validation_plain` und `dot_report_graphviz_validation_json`. Sie nutzen denselben Wrapper und dasselbe Manifest wie das `-Tsvg`-Gate und stellen sicher, dass die Goldens in mehreren Graphviz-Output-Backends parseable bleiben.
- **Negativtests** `dot_report_python_validation_rejects_broken_dot` und `dot_report_graphviz_validation_rejects_broken_dot` (Letzterer nur mit Graphviz vorhanden) verifizieren via `WILL_FAIL`, dass beide Gates ein bewusst kaputtes DOT-Fixture unter `tests/adapters/fixtures/dot_broken_sample.dot` ablehnen. So bleibt nachweisbar, dass die Gates echte Verstoesse erkennen und nicht still passen.
- **DOT-Adapter-Vertrag** in `xray_tests` ueber `tests/adapters/test_dot_report_adapter.cpp`. Pflichtattribute, Statement-/Attribut-Reihenfolge, Escape-Regeln (inkl. ASCII-Control-Bytes 0x00..0x1F, DEL und Pfadtrenner-Sonderfaelle wie Windows-Drives und UNC), Label-Truncation (48 Zeichen, Middle-Truncation), Tie-Breaker, Budget-Truncation, Budget-Boundary-Werte (`compute_analyze_budget` an top=0/3/4/5/6) und der Direct-vs-Heuristic-Style-Vertrag sind dort verbindlich abgesichert.
- **DOT-Wiring** in `xray_tests` ueber `tests/adapters/test_port_wiring.cpp`. Tests stellen sicher, dass `--format dot` ueber Composition Root und CLI bis zum `DotReportAdapter` durchverdrahtet ist und nicht in den Console-Fallback faellt.
- **Binary-E2E-Goldens** ueber das CTest-Ziel `e2e_binary_artifacts` und `tests/e2e/run_e2e_artifacts.sh`. Die echte `cmake-xray`-Binary erzeugt DOT-Berichte fuer Compile-Database-only-, File-API-only-, Mixed-Input-, Default-Top-, Top-Truncated-, Budget-Truncated- und Escaping-Faelle bei `analyze`, sowie fuer Compile-Database-, File-API-, Mixed-Input-, Budget-Untruncated-, Heuristic-Edge-, Heuristic-Target-, Truncated- und Absolute-changed-file-Faelle bei `impact`. Jede dieser DOT-Ausgaben wird byte-stabil gegen ein Golden unter `tests/e2e/testdata/m5/dot-reports/` verglichen und zusaetzlich live ueber `tests/validate_dot_reports.py` validiert; per-Plattform-Goldens (`*_windows.dot`) decken host-divergente Pfadsemantik ab.

### Bootstrap-Matrix

| Umgebung | Aktiver Pfad | Installation |
|---|---|---|
| Docker (`test`, `coverage`, `coverage-check`) | Python-Gate plus alle Graphviz-Format-Gates (`-Tsvg`, `-Tplain`, `-Tjson`) | `graphviz` und `python3-jsonschema` via apt im `toolchain`-Stage |
| Native CI Linux/macOS/Windows (`build.yml`, `release.yml`) | Python-Gate (Graphviz-Gates nur bei lokal installiertem `dot`) | reine Standardbibliothek; `python3` durch `setup-python` |
| Lokale Entwicklung | Python-Gate plus Graphviz-Format-Gates, falls `dot` installiert | Graphviz via System-Paketmanager |

CTest selbst installiert keine Systempakete und greift nicht auf das Netzwerk zu. Installation von Graphviz oder Python-Abhaengigkeiten erfolgt ausschliesslich im Bootstrap-Schritt der jeweiligen Umgebung.

## HTML-Reportvertrag

Der menschenlesbare HTML-Reportvertrag ist in [docs/report-html.md](./report-html.md) dokumentiert. AP M5-1.4 fuehrt den HTML-Adapter in drei verbindlichen Tranchen plus optionaler Tranche D ein. Folgende Gates sichern den Vertrag:

- **HTML-Adapter-Vertrag** in `xray_tests` ueber `tests/adapters/test_html_report_adapter.cpp`. Pflichtgeruest (`<!doctype html>`, `<html lang="en">`, einziges `h1`, einziges `main`, `head`-Reihenfolge), Escape-Regeln fuer Textknoten und Attribute, Whitespace-Normalisierung (LF/CRLF/CR/Tab/Control-Bytes), CSS-Pflichtmerkmale (keine externen Ressourcen, keine Animationen, keine `@import`/`url(...)`-Referenzen, System-Fontstack, Druckpfad) und das `kReportFormatVersion`-Meta sind dort verbindlich abgesichert.
- **HTML-Wiring** in `xray_tests` ueber `tests/adapters/test_port_wiring.cpp`. Tests stellen sicher, dass `--format html` ueber Composition Root und CLI bis zum `HtmlReportAdapter` durchverdrahtet ist und nicht in den Console-Fallback faellt.
- **HTML-Render-Preconditions** in `xray_tests` ueber `tests/e2e/test_cli.cpp`. Die in der Impact-Negativfall-Matrix von [docs/plan-M5-1-4.md](./plan-M5-1-4.md) gelisteten HTML-Render-Preconditions sind ueber CLI-Tests verbindlich abgesichert: `changed_file_source=unresolved_file_api_source_root` und `changed_file=std::nullopt` produzieren je einen Textfehler auf `stderr`, einen Nicht-Null-Exit und kein HTML.
- **HTML-CLI-Stream-/Fehlerpfade** in `xray_tests` ueber `tests/e2e/test_cli.cpp` und `tests/e2e/test_cli_render_errors.cpp`. CLI-Tests sichern den `--format html`-stdout-Vertrag, den `--format html --output <path>`-Atomic-Writer-Vertrag, Render-Fehler-Doppelgaenger fuer Analyze und Impact (vollstaendiges Rendern vor Emission, unveraenderte Zieldatei bei Renderfehler), die Negativtests fuer `--top 0` und `impact --top` sowie die Eingabefehlerpfade (nicht vorhandene Compile-Database-/File-API-Eingaben, ungueltiges Compile-Database-JSON, ungueltige File-API-Reply-Daten, Schreibfehler).
- **HTML-Goldens und Manifest** unter `tests/e2e/testdata/m5/html-reports/` mit `manifest.txt`. Goldens decken Compile-DB-only-, File-API-only-, Mixed-Input-, Default-Top-, Top-Untruncated-, Top-Truncated-, Hotspot-Kontext-Kuerzungs-, Same-Source-Path/Different-Directory- und Escaping-Faelle bei `analyze` sowie Compile-DB-, File-API-, Mixed-Input-, Direct-Targets-, No-Affected-Targets-, und Absolute-`--changed-file`-Faelle bei `impact` ab. Per-Plattform-Goldens (`*_windows.html`) decken host-divergente Pfadsemantik fuer `cli_absolute`-Eingaben ab.
- **HTML-Escape-Fixture** unter `tests/e2e/testdata/m5/html-fixtures/escape_paths/compile_commands.json` betreibt das Escaping-Golden mit `<`, `>`, `&`, `"`, `'`, Backslash, Newline und `<script>`-aehnlichen Strings; die DOT-Escape-Fixture bleibt davon unberuehrt.
- **Binary-E2E-Smokes** ueber `tests/e2e/run_e2e_artifacts.sh` und das CTest-Ziel `e2e_binary_artifacts`. Die echte `cmake-xray`-Binary erzeugt HTML-Berichte fuer alle gelisteten Faelle bei `analyze` und `impact`; Ausgaben werden byte-stabil gegen die Goldens verglichen. Per-Plattform-Branching ueber `case "$(uname -s)"` waehlt das passende `*_windows.html`-Golden auf MSYS-/Cygwin-Hosts.
- **Optionaler HTML-Struktur-Smoke** (`tests/validate_html_reports.py`, falls in Tranche D angelegt) bleibt standardbibliotheks-only und prueft `<!doctype html>`, einziges `h1`, kein `script`, keine externen Ressourcen. AP 1.4 verzichtet im Standardpfad auf den Smoke, weil die byte-stabilen Goldens und die Adaptertests die Struktur bereits vollstaendig sichern.

CTest selbst installiert keine Systempakete und greift nicht auf das Netzwerk zu. AP 1.4 fuehrt keine neuen pip-Dependencies ein; falls eine spaetere HTML-Vertragsversion das doch tut, ist Hash-Pinning ueber `pip-compile --generate-hashes` Pflicht.

## CLI-Verbosity (`--quiet` / `--verbose`)

Der command-lokale Verbosity-Vertrag ist in [docs/plan-M5-1-5.md](./plan-M5-1-5.md) dokumentiert. AP M5-1.5 fuehrt `--quiet` und `--verbose` in drei verbindlichen Tranchen plus optionaler Tranche D ein. Tranchen A, B und C sind abgeschlossen und sichern folgende Gates:

- **CLI-Modell und Architektur-Tabu** in `xray_tests` ueber `tests/adapters/test_port_wiring.cpp`. `static_assert`-basierte Architektur-Pruefungen stellen sicher, dass `ConsoleReportAdapter`, `MarkdownReportAdapter`, `JsonReportAdapter`, `DotReportAdapter` und `HtmlReportAdapter` weiterhin default-konstruierbar sind und `GenerateReportPort::generate_analysis_report` und `generate_impact_report` ihre dokumentierten Signaturen behalten. Eine zukuenftige Erweiterung um einen `OutputVerbosity`-Parameter wuerde die `static_assert`-Sequenz brechen.
- **Mutual-Exclusion und command-lokale Position** in `xray_tests` ueber `tests/e2e/test_cli_verbosity.cpp`. Tests pruefen, dass `--quiet` und `--verbose` an beiden Subcommands akzeptiert werden, gemeinsam zu einem Usage-Fehler fuehren, und globale Positionen vor dem Subcommand mit nonzero Exit abgelehnt werden. Mutual-Exclusion-Praezedenz: stderr enthaelt fuer den Mutual-Exclusion-Fehler keinen `verbose:`-Block.
- **Fehlervertrag-Praezedenz** in `xray_tests` ueber `tests/e2e/test_cli_verbosity.cpp`. `impact --verbose` ohne `--changed-file` liefert die Pflichtoptionen-Usage-Meldung ohne `verbose:`-Block; `--quiet/--verbose --format console --output <path>` liefert weiterhin die exakte zweizeilige `--output is not supported with --format console`-Sequenz. Tranche-D-Cross-Tests pinnen die Reihenfolge zusaetzlich fuer Kombinationen, in denen mehrere Praezedenzstufen gleichzeitig verletzt sind (Stufe 1 vor 3, 1 vor 4, 3 vor 4, 1 vor 3 vor 4, 4 vor Verbose-Block).
- **Order-Independence von `--quiet`/`--verbose`** in `xray_tests` ueber `tests/e2e/test_cli_verbosity.cpp`. Pinnt, dass die Flags an jeder Position hinter dem Subcommand akzeptiert werden, indem mehrere Argument-Reihenfolgen denselben Console-Quiet-/Verbose-Golden treffen.
- **Console-Quiet- und Console-Verbose-Goldens** unter `tests/e2e/testdata/m5/verbosity/` (Manifest-Verzeichnis-Paritaet ueber `tests/validate_verbosity_reports.py`). Pro Subcommand sind Quiet- und Verbose-Goldens fuer jeweils mindestens vier Fixturevarianten (Default, Targets, File-API-Only, Empty/Direct) versioniert; CLI-Tests in `tests/e2e/test_cli_verbosity.cpp` und Binary-Smokes in `tests/e2e/run_e2e_verbosity.sh` (CTest-Ziel `e2e_binary_verbosity`) decken sie ab.
- **Artefakt-Byte-Stabilitaet** in `xray_tests` und `e2e_binary_verbosity`. Pro `(Subcommand, Format, Modus)`-Kombination ueber Markdown, JSON, DOT und HTML pruefen Tests, dass `--quiet` und `--verbose` stdout-byte-identisch zum Normalmodus sind und `--output`-Dateien byte-identisch geschrieben werden.
- **Verbose-Artefakt-stderr-Sequenz** in `xray_tests` und `e2e_binary_verbosity`. Pro `(Subcommand, Format)`-Paar pinnt ein diff-basierter Helfer (`assert_stderr_equals_file` in `run_e2e_lib.sh`, `==`-Vergleich in `test_cli_verbosity.cpp`) die exakten 7-Zeilen-Sequenz fuer `analyze` bzw. 8-Zeilen-Sequenz fuer `impact`. Negativ-Smoke schliesst `top_limit` aus dem Verbose-Block aus.
- **Verbose-Fehlerkontext** in `xray_tests` ueber `tests/e2e/test_cli_verbosity.cpp`. Render-, Schreib-, Eingabe- und Analysefehler im Verbose-Modus enthalten exakt die im Plan dokumentierten 5- bzw. 6-Zeilen-Sequenzen mit `validation_stage` aus der CLI-Schicht. Quiet-Fehlerpfade zeigen weiterhin Text auf stderr und nonzero Exit ohne `verbose:`-Zeilen.
- **Verbose-stdout-Validity-Gates** in `e2e_binary_verbosity`. JSON-, DOT- und HTML-Verbose-stdout werden zusaetzlich zur Byte-Stabilitaet durch die jeweiligen Validatoren aus AP 1.2 / 1.3 / 1.4 (`validate_json_schema.py`, `validate_dot_reports.py`, `validate_html_reports.py`) gegen ihre Vertraege geprueft; ein versehentliches `verbose:`-Leck in stdout wuerde den Validator brechen.
- **Binary-E2E-Smokes** in `e2e_binary_verbosity` ueber `tests/e2e/run_e2e_verbosity.sh`. Console-Quiet/Verbose-Goldens, Quiet-/Verbose-Artefakt-Byte-Stabilitaet, Verbose-stderr-Sequenzen und `--output`-Datei-Byte-Stabilitaet werden alle ueber die echte `cmake-xray`-Binary inklusive `src/main.cpp` gefahren.

## Zusammenhang mit Releasing

Fuer M3 und spaetere Releases sind mindestens diese Docker-Pfade massgeblich:

- `docker build --target test -t cmake-xray:test .`
- `rg "GCOVR""_EXCL_" | wc -l`
- `docker build --target coverage-check --build-arg XRAY_COVERAGE_THRESHOLD=100 -t cmake-xray:coverage-check .`
- `docker build --target quality-check -t cmake-xray:quality-check .`
- `docker build --target runtime -t cmake-xray .`
- `docker run --rm cmake-xray --help`

Die Release-Checkliste selbst steht in [docs/releasing.md](./releasing.md).

## Hinweise

- Dieses Dokument soll den reproduzierbaren Ist-Stand beschreiben. Veraltete Review-Notizen, Placeholder-Befunde und einmalige Zwischenstaende gehoeren nicht hier hinein.
- Fuer aktuelle Zahlen und Details sind die Artefakte aus `quality` und `coverage` die primaere Quelle.
