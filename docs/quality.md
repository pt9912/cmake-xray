# Qualität

## Zweck

Dieses Dokument beschreibt die verbindlichen Qualitaets- und Metrikpfade fuer `cmake-xray`.
Es dokumentiert keine punktuelle Review-Historie und keine einmaligen KI-Befunde, sondern die aktuell gueltigen, reproduzierbaren Make-Targets fuer die Docker-Pruefwege.

## Statische Analyse und Metriken

Die statische Analyse und die Metriken werden Docker-basiert fuer den Produktcode unter `src/` erhoben:

- `clang-tidy` prueft regelbasierte statische Analysebefunde
- `lizard` prueft Komplexitaet, Funktionslaenge und Parameteranzahl
- Report und Gate sind bewusst getrennt

### Report-Pfad

```bash
make quality-report
```

Der `quality`-Stage erzeugt und zeigt diese Artefakte:

- `/workspace/build-quality/quality/summary.txt`
- `/workspace/build-quality/quality/clang-tidy.txt`
- `/workspace/build-quality/quality/lizard.txt`
- `/workspace/build-quality/quality/lizard-warnings.txt`

### Gate-Pfad

```bash
make quality-gate
```

Optional lassen sich die Grenzwerte per Make-Variable anpassen:

```bash
make quality-gate \
  CLANG_TIDY_MAX_FINDINGS=0 \
  LIZARD_MAX_CCN=10 \
  LIZARD_MAX_LENGTH=50 \
  LIZARD_MAX_PARAMETERS=5
```

Aktuelle Standardgrenzen:

- `clang-tidy`: maximal `0` Findings
- `lizard` CCN: maximal `10`
- `lizard` Funktionslaenge: maximal `50`
- `lizard` Parameteranzahl: maximal `5`

Konfigurierbare Make-Variablen:

- `CLANG_TIDY_MAX_FINDINGS`
- `LIZARD_MAX_CCN`
- `LIZARD_MAX_LENGTH`
- `LIZARD_MAX_PARAMETERS`

## Testabdeckung

Die Coverage wird ebenfalls Docker-basiert fuer `src/` ermittelt:

- `ctest` fuehrt die Tests aus
- `gcovr` erzeugt den Coverage-Report
- die Instrumentierung wird ueber `XRAY_ENABLE_COVERAGE=ON` aktiviert
- das Gate bezieht sich auf Line-Coverage unter `src/`

### Report-Pfad

```bash
make coverage-report
```

Der `coverage`-Stage erzeugt und zeigt diese Artefakte:

- `/workspace/build-coverage/coverage/summary.txt`
- `/workspace/build-coverage/coverage/coverage.txt`

### Gate-Pfad

```bash
make coverage-gate COVERAGE_THRESHOLD=100
```

Der Standard fuer den aktuellen M3-/MVP-Stand ist:

- minimale Line-Coverage unter `src/`: `100%`

Das Gate ist per Make-Variable `COVERAGE_THRESHOLD` konfigurierbar.

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
make runtime
```

## JSON-Reportvertrag

Der maschinenlesbare JSON-Reportvertrag ist in [docs/report-json.md](./report-json.md) dokumentiert und durch [docs/report-json.schema.json](./report-json.schema.json) (JSON Schema Draft 2020-12) formal abgesichert. Zwei CTest-Gates schuetzen den Vertrag in jeder Build-Matrix:

- **Schema-Wohlgeformtheit und Manifest-Paritaet** ueber `tests/validate_json_schema.py`. Der CTest-Eintrag heisst `report_json_schema_validation`. Er prueft die Schema-Datei gegen das Draft-2020-12-Meta-Schema, gleicht das Manifest unter `tests/e2e/testdata/m5/json-reports/manifest.txt` mit dem Verzeichnis ab und validiert jedes gelistete Golden gegen das Schema.
- **Formatversionskonsistenz** im Test `report-json schema format_version const matches kReportFormatVersion` in `xray_tests`. Er parst das Schema und prueft den `$defs/FormatVersion/const`-Wert gegen die C++-Konstante `xray::hexagon::model::kReportFormatVersion` in `src/hexagon/model/report_format_version.h`.
- **JSON-Adapter-Vertrag** in `xray_tests` ueber `tests/adapters/test_json_report_adapter.cpp`. Pflichtfelder, Typen, Enums, Nullability, Sortier-Tie-Breaker, `include_analysis_heuristic`-Beide-Werte und der Schema-Fail-Fall fuer `unresolved_file_api_source_root` sind dort verbindlich abgesichert.
- **JSON-Wiring** in `xray_tests` ueber `tests/adapters/test_port_wiring.cpp`. Tests stellen sicher, dass `--format json` ueber Composition Root und CLI bis zum `JsonReportAdapter` durchverdrahtet ist und nicht in den Console-Fallback faellt.
- **Binary-E2E-Goldens** ueber das CTest-Ziel `e2e_binary_artifacts` und `tests/e2e/run_e2e_artifacts.sh`. Die echte `cmake-xray`-Binary erzeugt JSON-Berichte fuer Compile-Database-only-, File-API-only- (Build-Dir und Reply-Dir), Mixed-Input-, Truncated- und Hotspot-Truncated-Faelle bei `analyze`, sowie fuer `compile_database_directory`-, `file_api_source_root`-, Mixed-Prioritaet- und `cli_absolute`-Faelle bei `impact`. Die Ausgaben werden byte-stabil gegen die Goldens unter `tests/e2e/testdata/m5/json-reports/` verglichen.

Diese Gates sind verpflichtend und werden ueber alle in diesem Dokument aufgefuehrten Make-Targets sowie ueber die nativen Build-Matrizen in `.github/workflows/build.yml` und `.github/workflows/release.yml` ausgefuehrt.

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

## Release-Pipeline-Gates (AP M5-1.6)

Zusaetzlich zu den Docker-Quality-Gates oben fuehrt der Release-Pfad in
[`.github/workflows/release.yml`](../.github/workflows/release.yml) verbindliche
Gates aus, die jeden Tag-Push absichern. Sie sind in
[docs/plan-M5-1-6.md](./plan-M5-1-6.md) festgeschrieben und im Dry-Run-
Orchestrator ([`scripts/release-dry-run.sh`](../scripts/release-dry-run.sh))
identisch verschaltet, sodass Workflow- und Dry-Run-Disposition deckungsgleich
bleiben:

- **Tag-Validator** ueber [`scripts/validate-release-tag.sh`](../scripts/validate-release-tag.sh).
  Akzeptiert ausschliesslich `vMAJOR.MINOR.PATCH` und
  `vMAJOR.MINOR.PATCH-PRERELEASE`. Build-Metadaten (`+...`) werden abgelehnt.
  CTest-Anker: `release_tag_validation` ueber
  `tests/release/test_validate_release_tag.sh` (vier positive plus zwoelf
  negative Faelle).
- **Reproduzierbares Linux-Archiv** ueber [`scripts/build-release-archive.sh`](../scripts/build-release-archive.sh)
  und das `release-archive`-Stage im [Dockerfile](../Dockerfile). Tar wird
  mit `--sort=name --mtime=@SOURCE_DATE_EPOCH --owner=0 --group=0
  --numeric-owner --format=ustar` erzeugt und mit `gzip -n` gepackt;
  `SOURCE_DATE_EPOCH` ist im Workflow auf den Tag-Commit-Zeitstempel
  (`git log -1 --pretty=%ct $TAG`) gepinnt. CTest-Anker:
  `release_archive_reproducibility` ueber
  `tests/release/test_release_archive_repro.sh`.
- **OCI-Idempotenz** ueber [`scripts/oci-image-publish.sh`](../scripts/oci-image-publish.sh).
  Die Subkommandos `build`, `push` und `latest` benutzen
  `docker buildx imagetools inspect --format '{{.Manifest.Digest}}'` als
  kanonische Digest-Quelle (RFC 6920 / OCI Image Spec). Re-Runs ueberschreiben
  einen vorhandenen Tag nur, wenn der lokale und der remote Digest exakt
  uebereinstimmen; Mismatch bricht den Push ab. `:latest` wird ausschliesslich
  fuer regulaere Releases gesetzt (Versions-String enthaelt kein `-`).
  CTest-Anker: `oci_image_smoke` ueber `tests/release/test_oci_image_smoke.sh`.
- **Asset-Allowlist** ueber [`scripts/release-allowlist.sh`](../scripts/release-allowlist.sh).
  Ein bash-Array fixiert die einzigen zulaessigen Asset-Namen
  (`cmake-xray_<version>_linux_x86_64.tar.gz` plus
  `*.sha256`-Sidecar). Jeder Asset-Name ausserhalb der Allowlist bricht
  den finalen Release-Job vor `release_published` ab. CTest-Anker:
  `release_allowlist` ueber `tests/release/test_release_allowlist.sh`
  (positiv plus fuenf Negativfaelle: Praefix-, Versions-, Plattform-,
  Extension- und Sidecar-Drift).
- **Drei-Wege-Versionscheck** verteilt ueber Verify- und Release-Job. Die
  drei kanonischen Quellen sind: (1) Tag (`v$VERSION`), (2) `XRAY_APP_VERSION`
  aus dem CMake-Build (in Linux-Archiv-Binary und OCI-Image-Binary
  eingebrannt) und (3) Release-Asset-/OCI-Publish-Metadaten. Der
  Verify-Job pinnt Quelle (2) gegen (1), indem er den `--version`-Output
  des Linux-Archiv-Binarys und des lokal gebauten OCI-Images gegen den
  Tag vergleicht. Der Release-Job pinnt zusaetzlich Tag-Gleichheit
  (`tag == v${version}`), Asset-Namen-Suffix (`_${version}_`),
  OCI-Tag-Erreichbarkeit ueber `imagetools inspect` und den
  `--version`-Output am tatsaechlich publishten OCI-Image (post-Push,
  aus Registry, nicht aus Local-Cache). Eine Drift in einer der Quellen
  bricht vor `gh release create/edit` ab.
- **Dry-Run-Smoke** ueber [`scripts/release-dry-run.sh`](../scripts/release-dry-run.sh)
  und das `release-dry-run`-Stage im [Dockerfile](../Dockerfile). Nutzt
  fake-`gh` und eine lokale `registry:2` und durchlaeuft die drei Plan-
  Zustandsuebergaenge `draft_release_created -> oci_image_published ->
  release_published`. Refusal-Pfade bei realen Endpunkten (`GH_TOKEN` ohne
  Override) und Manifest-Mismatch-Erkennung sind verbindlich gepinnt.
  CTest-Anker: `release_dry_run` ueber
  `tests/release/test_release_dry_run.sh` (zwei Happy-Path-, sechs Abort-
  und ein AP M5-1.7-Tranche-C.4-Plattform-Negativ-Szenario). Szenario 9
  haengt ueber den `--extra-asset`-Flag des Orchestrators ein synthetisches
  `*_darwin_arm64.tar.gz` in den Allowlist-Aufruf und prueft, dass der
  Dry-Run vor `draft_release_created` mit Bezug auf die "M5 release
  allowlist" abbricht. Ohne `--extra-asset` (real-Release-Pfad) bleibt
  das Verhalten unveraendert.

Diese Gates sind verpflichtend; ein roter Pfad bricht die Veroeffentlichung
ab, bevor irgendein extern sichtbarer Zustand (Draft, OCI-Push, Public-
Release) entsteht.

## Plattformstatus (AP M5-1.7)

Der Plattformstatus-Vertrag aus [docs/plan-M5-1-7.md](./plan-M5-1-7.md)
unterscheidet drei Statusklassen: `supported` (offiziell freigegeben),
`validated_smoke` (Build, Pflicht-Smokes und Plattformgates gruen, aber
ohne Releasefreigabe) und `known_limited` (Pflichtgate fehlt oder ist nicht
vollstaendig gruen, akzeptierte Einschraenkung dokumentiert).

Aktueller Stand nach Abschluss von AP 1.7 (alle Tranchen A–D ausgeliefert):

| Plattform | Status | Required Check | Hinweis |
|---|---|---|---|
| Linux x86_64 | `supported` | `Native (linux-x86_64)` plus die Docker-Gates oben | offizielle M5-Releaseplattform; Atomic-Replace-/CLI-Pflicht-Smokes laufen ueber bestehende E2E-Suites |
| macOS arm64 | `validated_smoke` | `Native (macos-arm64)` | `ctest` faehrt `xray_tests` (inkl. Atomic-Replace-Tests aus B und UNC/Extended-Length-Tests aus C.2) plus die E2E-Suites `e2e_binary_smoke`/`_artifacts`/`_verbosity` (inkl. der `--output`-Pflicht-Smokes aus C.1). Status seit 2026-04-30 `validated_smoke`, weil Branch-Protection den Required Check verankert hat und build.yml run 25153798452 (commit `7be4829`, post-protection) der auditierbare gruene CI-Lauf auf macos-latest ist |
| Windows x86_64 | `validated_smoke` | `Native (windows-x86_64)` plus PowerShell-Smoke (D.1) | `ctest` faehrt dieselben Pflicht-Suites wie macOS; zusaetzlich laeuft `scripts/platform-smoke.ps1` als konvertierungsfreier `native_powershell`-Pfad. Status seit 2026-04-30 `validated_smoke` aus denselben Bedingungen wie macOS (Required Check + Audit ueber run 25153798452) |

Die Required-Check-Namen sind in
[docs/plan-M5-1-7.md](./plan-M5-1-7.md) "Plattformstatus-Vertrag" verbindlich
festgelegt; Branch-Protection-Konfiguration und Workflow-Jobnamen muessen
deckungsgleich bleiben. Die Branch-Protection ist nicht im Workflow
versioniert; ein `gh api repos/<owner>/<repo>/branches/main/protection`-Auszug
beim Setzen oder Aendern der Required Checks ist als Audit-Hinweis im
PR-Beschreibungstext zu hinterlegen.

### Atomic-Replace-Matrix (AP M5-1.7 Tranche B)

`--output` schreibt jeden Report ueber den Atomic-Writer in
[src/adapters/cli/atomic_report_writer.cpp](../src/adapters/cli/atomic_report_writer.cpp).
Der Vertrag pinnt sieben Invarianten plattformverbindlich:

1. neue Zieldatei wird vollstaendig geschrieben,
2. vorhandene Zieldatei wird bei Erfolg ersetzt,
3. vorhandene Zieldatei bleibt bei Renderfehler unveraendert,
4. vorhandene Zieldatei bleibt bei Schreib-/Replace-Fehler unveraendert,
5. temporaere Datei wird im Zielverzeichnis exklusiv reserviert
   (POSIX `O_CREAT | O_EXCL`, Windows `CreateFileW` mit `CREATE_NEW`),
6. Kollisionen bei temporaeren Dateinamen werden bis zu 64 Mal mit
   inkrementiertem Attempt-Counter wiederholt; danach Abbruch mit
   sprechender Fehlermeldung,
7. keine Implementierung loescht den Zielpfad vor dem Replace-Schritt; der
   Swap erfolgt atomar via POSIX `rename`/`renameat` oder Windows
   `ReplaceFileW` mit `REPLACEFILE_WRITE_THROUGH` (Replace) bzw.
   `MoveFileExW` mit `MOVEFILE_WRITE_THROUGH` (Move-New).

Pinning-Tests in `tests/adapters/test_atomic_report_writer.cpp` (CTest-Anker
`xray_tests`) decken alle sieben Invarianten ab und mischen Mock-Pfade
(`RecordingPlatformOps`) mit Host-Pfaden (`DefaultAtomicFilePlatformOps`).
Die Host-Pfade sind verbindlich: auf Linux laeuft der POSIX-Pfad, auf macOS
ebenfalls, auf Windows der `WindowsAtomicFilePlatformOps`-Pfad mit
`ReplaceFileW`/`MoveFileExW`. Renderfehler-Pinning fuer alle Reportformate
liegt in [tests/e2e/test_cli_render_errors.cpp](../tests/e2e/test_cli_render_errors.cpp).

Plattform-Coverage:

| Plattform | Replace-Implementierung | CTest-Lauf |
|---|---|---|
| Linux x86_64 | POSIX `rename` ueber `PosixAtomicFilePlatformOps` | Docker `test`-Stage plus `Native (linux-x86_64)` |
| macOS arm64 | POSIX `rename` ueber `PosixAtomicFilePlatformOps` | `Native (macos-arm64)` |
| Windows x86_64 | `ReplaceFileW` / `MoveFileExW` ueber `WindowsAtomicFilePlatformOps` | `Native (windows-x86_64)` |

Ein roter Atomic-Writer-Test bricht den jeweiligen Required Check und damit
AP 1.7 unabhaengig davon, ob die restlichen CLI-Smokes gruen sind.

#### Synthetische UNC- und Extended-Length-Pfadfaelle (Tranche C.2)

Plan-M5-1-7.md "Windows-Bewertung" verlangt synthetische UNC- und
Extended-Length-Pfadfaelle in mindestens einem Adapter-, CLI- oder
Golden-Pfad. Die Coverage in `xray_tests`:

- **DOT-Adapter** ([tests/adapters/test_dot_report_adapter.cpp](../tests/adapters/test_dot_report_adapter.cpp)):
  Tests "DOT label peels UNC paths down to the basename" und "DOT label
  peels Windows extended-length paths down to the basename" pinnen
  Label-Peeling plus Backslash-Escape im `path`-Attribut.
- **HTML-Adapter** ([tests/adapters/test_html_report_adapter.cpp](../tests/adapters/test_html_report_adapter.cpp)):
  Tests "HTML render_text passes UNC paths through verbatim", "HTML
  render_attribute escapes UNC paths in attributes" und "HTML render_text
  and render_attribute pass extended-length paths through verbatim"
  pinnen Text- und Attribut-Render fuer beide Pfadshapes.
- **JSON-Adapter** ([tests/adapters/test_json_report_adapter.cpp](../tests/adapters/test_json_report_adapter.cpp)):
  Tests "json output preserves UNC paths verbatim through escape and
  round-trip" und "json output preserves extended-length paths verbatim
  through escape and round-trip" pruefen Round-Trip via `nlohmann::json`
  plus Wire-Format-Escape (jeder rohe `\` wird `\\` im JSON-Output).
- **Atomic-Writer**
  ([tests/adapters/test_atomic_report_writer.cpp](../tests/adapters/test_atomic_report_writer.cpp)):
  Windows-Host-conditional Tests "atomic temp path keeps the parent UNC
  root on Windows hosts" und "atomic temp path keeps the extended-length
  parent on Windows hosts" sind via `#ifdef _WIN32` aktiv und decken
  `parent_path()`-/`filename()`-Splitting fuer UNC und Extended-Length
  ab. Auf Linux/macOS-Hosts sind diese Tests dokumentiert geskippt
  (`std::filesystem::path` parst Backslashes auf POSIX nicht als
  Pfadtrenner) — Plan-konform als "dokumentierter Windows-API-Skip mit
  `known_limited`-Folge". Der Skip betrifft nur den Atomic-Writer-Splitting-
  Pfadtest; die Adapter-Display-Tests (DOT/HTML/JSON oben) laufen
  unabhaengig vom Host.

Echte UNC-Dateisystem-Smokes (`\\localhost\...`-Share) und echte
Extended-Length-Dateiziele bleiben optionale Folgehaertung; die
Required-Pflichtfaelle sind ueber die synthetischen Tests oben abgedeckt.

#### Optionale Plattform-Haertungen (Tranche D)

Vier optionale Tranche-D-Schritte ergaenzen die A-C-Pflichtbasis ohne den
offiziellen Plattformstatus zu aendern:

- **D.1 — PowerShell-Smoke ([scripts/platform-smoke.ps1](../scripts/platform-smoke.ps1))**:
  konvertierungsfreier Pflichtmodus `native_powershell` neben dem
  MSYS-Bash-Pfad. Der Smoke laeuft im `Native (windows-x86_64)`-Job nach
  `ctest` und treibt alle Pflichtkommandos plus `--output`-Smokes mit
  nativen Windows-Pfaden, ohne dass MSYS dazwischensteht.
- **D.2 — zurueckgezogen**: ein zweiter Ninja-/clang-cl-Build-Pfad neben
  dem Visual-Studio-Default war als optionale Haertung vorgesehen.
  Erstkontakt auf GHA-windows-latest scheiterte an `CMAKE_MT-NOTFOUND`
  (mt.exe ohne vcvars-Setup), und ein Workaround haette entweder eine
  Drittanbieter-Action (`ilammy/msvc-dev-cmd`) oder eine brittle
  vcvars-Pfad-Hardcoding gebraucht. Da der VS-Multi-Config-Default-Build
  im selben Job durchlaeuft und die PowerShell-Smoke (D.1) eine
  generatorunabhaengige Binary-Verifikation liefert, wurde D.2
  plan-konform ausgelassen.
- **D.3 — zurueckgezogen**: ein zusaetzlicher Intel-`macos-13`-Smoke war
  in der Plan-Klausel "falls arm64 nicht repraesentativ genug ist"
  konditional vorgesehen; das Repraesentativitaetsproblem ist heute
  nicht erkennbar, deshalb verzichten wir auf den zweiten macOS-Job
  (Plan-konformes Auslassen der optionalen Tranche).
- **D.4 — Manuelle Smoke-Checkliste**:
  [docs/platform-smoke-checklist.md](./platform-smoke-checklist.md)
  spiegelt die Required-Check-Schritte fuer lokale Plattformnachweise.
  Ein erfolgreicher manueller Lauf begruendet *keine* Statushaerung;
  `validated_smoke` braucht den automatisierten, Branch-Protection-
  verankerten Nachweis.

### Mindestversionen fuer CMake und Compiler

Mindestversionen sind in [tests/platform/toolchain-minimums.json](../tests/platform/toolchain-minimums.json)
versioniert und werden durch [cmake/ToolchainMinimums.cmake](../cmake/ToolchainMinimums.cmake)
beim Configure jeder Plattform fail-fast geprueft (`FATAL_ERROR` bei
Unterschreitung). Aktueller Stand:

| Quelle | Mindestversion |
|---|---|
| CMake | `3.20` |
| GCC (`GNU`) | `10` |
| Clang | `12` |
| AppleClang | `13` |
| MSVC | `19.29` |

Eine Anhebung dieser Werte erfolgt ausschliesslich ueber
`tests/platform/toolchain-minimums.json`; CMakeLists.txt
(`cmake_minimum_required`), README, `docs/guide.md` und dieser Abschnitt
muessen synchron nachgezogen werden. Das Modul querchecked
`CMAKE_MINIMUM_REQUIRED_VERSION` gegen die JSON-Datei und bricht ab, wenn
beide auseinanderlaufen.

ClangCL meldet sich gegenueber CMake je nach Setup als `Clang` oder `MSVC`.
Der Fail-Fast-Check liest `CMAKE_CXX_COMPILER_ID` und vergleicht gegen den
dazu passenden Eintrag in `compiler_minimums`; eine separate `ClangCL`-
Schluesselung wird nicht eingefuehrt.

Drei CTest-Smokes pinnen das Gate-Verhalten unabhaengig davon, ob die
echte Host-Toolchain je unter das Minimum faellt:

- `toolchain_minimums_accepts_current_gnu` (Positivpfad, GNU 13.3.0).
- `toolchain_minimums_rejects_old_gnu` (Negativpfad, GNU 9 < Minimum 10;
  `WILL_FAIL` invertiert).
- `toolchain_minimums_rejects_unknown_compiler_id` (Negativpfad, ID `Foo`
  fehlt in `compiler_minimums`; `WILL_FAIL` invertiert).

Die Smokes laufen `cmake -P` gegen Wrapper-Skripte unter
[tests/platform/cmake/](../tests/platform/cmake/) und decken Pfad-Resolve,
JSON-Lookup und FATAL_ERROR-Pfad ab. Ein stiller Skip oder eine
Pfad-Regression im Modul faellt damit in CI auf statt nur dann, wenn ein
realer Host das Minimum unterschreitet.

### Explizite CMake-Plattform-Pin

Die Plan-Pflicht "Mindestens ein Matrixjob pro Hostfamilie nutzt eine
explizit eingerichtete aktuelle CMake-/Compiler-Kombination statt nur
implizit vorinstallierter Runner-Defaults"
([plan-M5-1-7.md](./plan-M5-1-7.md) "CMake-/Compiler-Kompatibilitaet")
ist in `.github/workflows/build.yml` ueber den Step
`Install pinned CMake` umgesetzt: `python -m pip install cmake==3.30.5`
schiebt eine versionsgepinnte CMake-Installation aus PyPI vor den
Runner-Default auf `PATH`. Aktueller Pin: **CMake 3.30.5**. Anhebungen
folgen dem gleichen Reviewpfad wie Mindestversions-Bumps in
`tests/platform/toolchain-minimums.json`. Compiler-Versionen bleiben
Runner-Default; ihre konkreten Werte loggt der Step `Show toolchain
versions` plus der Configure-Status `AP M5-1.7 toolchain check: ...`.

## Zusammenhang mit Releasing

Fuer M3 und spaetere Releases sind mindestens diese Make-Targets massgeblich:

- `make docker-test`
- `rg "GCOVR""_EXCL_" | wc -l`
- `make coverage-gate COVERAGE_THRESHOLD=100`
- `make quality-gate`
- `make runtime`

Die Release-Checkliste selbst steht in [docs/releasing.md](./releasing.md).

## Hinweise

- Dieses Dokument soll den reproduzierbaren Ist-Stand beschreiben. Veraltete Review-Notizen, Placeholder-Befunde und einmalige Zwischenstaende gehoeren nicht hier hinein.
- Fuer aktuelle Zahlen und Details sind die Artefakte aus `quality` und `coverage` die primaere Quelle.
