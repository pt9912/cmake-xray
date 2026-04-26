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
- **Binary-E2E-Goldens** ueber das CTest-Ziel `e2e_binary` und `tests/e2e/run_e2e.sh`. Die echte `cmake-xray`-Binary erzeugt JSON-Berichte fuer Compile-Database-only-, File-API-only- (Build-Dir und Reply-Dir), Mixed-Input-, Truncated- und Hotspot-Truncated-Faelle bei `analyze`, sowie fuer `compile_database_directory`-, `file_api_source_root`-, Mixed-Prioritaet- und `cli_absolute`-Faelle bei `impact`. Die Ausgaben werden byte-stabil gegen die Goldens unter `tests/e2e/testdata/m5/json-reports/` verglichen.

Diese Gates sind verpflichtend und werden ueber alle in diesem Dokument aufgefuehrten Docker-Pfade sowie ueber die nativen Build-Matrizen in `.github/workflows/build.yml` und `.github/workflows/release.yml` ausgefuehrt.

### Validator-Abhaengigkeit

Das Validatorskript benoetigt das Python-Paket `jsonschema` (Version `>=4.10,<5`) inklusive `Draft202012Validator`. Die Bootstrap-Pfade sind:

- **Docker**: das `toolchain`-Stage in [Dockerfile](../Dockerfile) installiert `python3-jsonschema` ueber `apt`. Alle abgeleiteten Stages, die `ctest` ausfuehren (`test`, `coverage`, `coverage-check`), erben die Abhaengigkeit.
- **Native CI** (Linux/macOS/Windows-Matrizen): die GitHub-Workflows installieren das Paket vor `ctest` ueber `python -m pip install -r tests/requirements-json-schema.txt`.
- **Lokale Entwickler**: derselbe `pip install`-Befehl. Auf Debian/Ubuntu reicht alternativ `apt-get install python3-jsonschema`.

CTest schlaegt mit konkreter Installationsanweisung fehl, wenn das Paket nicht installiert ist. Der Test wird nicht still uebersprungen und greift nicht auf das Netzwerk zu.

## DOT-Reportvertrag

Der visualisierungsorientierte DOT-Reportvertrag ist in [docs/report-dot.md](./report-dot.md) dokumentiert. Drei CTest-Gates sichern den Vertrag:

- **DOT-Python-Syntax-Gate** ueber `tests/validate_dot_reports.py`. Der CTest-Eintrag heisst `dot_report_python_validation`. Er prueft Pflichtattribute (`xray_report_type`, `format_version`, `graph_node_limit`, `graph_edge_limit`, `graph_truncated`), digraph-Namensvertrag, balancierte Klammern und Quoted-String-Escape-Regeln. Der Validator nutzt nur die Python-Standardbibliothek und laeuft auf jedem Host mit `python3` auf dem PATH.
- **DOT-Graphviz-Syntax-Gate** ueber `dot -Tsvg`. Der CTest-Eintrag heisst `dot_report_graphviz_validation` und ist nur registriert, wenn `dot` zur Build-Zeit auffindbar ist. Im Docker-Stage ist `graphviz` per apt installiert; native CI-Matrizen ohne Graphviz fuehren nur den Python-Pfad aus.
- **Negativtests** `dot_report_python_validation_rejects_broken_dot` und `dot_report_graphviz_validation_rejects_broken_dot` (Letzterer nur mit Graphviz vorhanden) verifizieren via `WILL_FAIL`, dass beide Gates ein bewusst kaputtes DOT-Fixture unter `tests/adapters/fixtures/dot_broken_sample.dot` ablehnen. So bleibt nachweisbar, dass die Gates echte Verstoesse erkennen und nicht still passen.

### Bootstrap-Matrix

| Umgebung | Aktiver Pfad | Installation |
|---|---|---|
| Docker (`test`, `coverage`, `coverage-check`) | beide Gates | `graphviz` und `python3-jsonschema` via apt im `toolchain`-Stage |
| Native CI Linux/macOS/Windows (`build.yml`, `release.yml`) | Python-Fallback | reine Standardbibliothek; `python3` durch `setup-python` |
| Lokale Entwicklung | beide Gates, falls Graphviz installiert | Graphviz via System-Paketmanager |

CTest selbst installiert keine Systempakete und greift nicht auf das Netzwerk zu. Installation von Graphviz oder Python-Abhaengigkeiten erfolgt ausschliesslich im Bootstrap-Schritt der jeweiligen Umgebung.

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
