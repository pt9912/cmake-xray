# cmake-xray

Build visibility for CMake projects.

`cmake-xray` ist ein Analyse- und Diagnosewerkzeug fuer CMake-basierte C++-Builds. Der aktuelle Entwicklungsstand auf `main` implementiert den geplanten M3-/MVP-Umfang fuer das kommende `v1.0.0`-Release: `compile_commands.json` lesen, auffaellige Translation Units ranken, heuristische Include-Hotspots ableiten, Datei-Impact analysieren und dieselben Ergebnisse sowohl als Konsolen- als auch als Markdown-Report exportieren.

## Status

Das Repository enthaelt die umgesetzten Meilensteine M0 bis M3 aus [docs/plan-M0.md](./docs/plan-M0.md), [docs/plan-M1.md](./docs/plan-M1.md), [docs/plan-M2.md](./docs/plan-M2.md) und [docs/plan-M3.md](./docs/plan-M3.md).

Der umgesetzte M3-/MVP-Umfang umfasst:

- CLI mit `analyze` und `impact`
- Validierung von `compile_commands.json`
- deterministisches Translation-Unit-Ranking auf Basis von `arg_count`, `include_path_count` und `define_count`
- heuristische Include-Hotspots und dateibasierte Impact-Analyse
- Report-Ausgabe als `console` oder `markdown`
- atomisches Schreiben von Markdown-Reports via `--output`
- versionierte Golden-Files, Referenzdaten und Performance-Baselines
- Docker-basierte Test-, Coverage- und Quality-Gates

Die Release-Artefakte fuer `v1.0.0` werden erst beim eigentlichen Releasing finalisiert; bis dahin bleiben die Aenderungen im [CHANGELOG](./CHANGELOG.md) unter `Unreleased`.

Nicht Ziel des MVP sind insbesondere:

- Ersatz fuer CMake
- vollstaendige CMake-Interpretation
- IDE-Integration
- HTML-, JSON- oder DOT-Export

## Voraussetzungen

Referenzplattform ist Linux mit:

- CMake >= 3.20
- C++20-faehigem Compiler
- Git fuer `FetchContent`

Als reproduzierbare Referenzumgebung steht das Multi-Stage-[Dockerfile](./Dockerfile) zur Verfuegung.

## Build und Installation

Lokaler Quellbuild:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Runtime-Image bauen:

```bash
docker build --target runtime -t cmake-xray .
```

Danach steht das Binary lokal unter `./build/cmake-xray` bzw. im Container als Entrypoint zur Verfuegung.

## Nutzung

### Hilfe

```bash
./build/cmake-xray --help
./build/cmake-xray analyze --help
./build/cmake-xray impact --help
```

### Projektanalyse in der Konsole

```bash
./build/cmake-xray analyze \
  --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json \
  --top 10
```

### Projektanalyse als Markdown auf `stdout`

```bash
./build/cmake-xray analyze \
  --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json \
  --format markdown \
  --top 10
```

### Projektanalyse als Markdown-Datei

```bash
./build/cmake-xray analyze \
  --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json \
  --format markdown \
  --output build/reports/analyze.md \
  --top 10
```

### Impact-Analyse

```bash
./build/cmake-xray impact \
  --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json \
  --changed-file include/common/config.h
```

### Impact-Analyse als Markdown

```bash
./build/cmake-xray impact \
  --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json \
  --changed-file include/common/config.h \
  --format markdown \
  --output build/reports/impact.md
```

Relative `--changed-file`-Pfade werden relativ zum Verzeichnis der uebergebenen `compile_commands.json` interpretiert.

## Heuristik-Hinweis

Include-Hotspots und Header-Impact beruhen im MVP auf heuristischer Include-Aufloesung. `cmake-xray` kennzeichnet diese Ergebnisse explizit und gibt Datenluecken als Diagnostics aus. Direkte Treffer auf bekannte Quelldateien werden ohne heuristische Klassifikation ausgewiesen.

## Beispielausgaben

Kuratierte Beispielartefakte liegen unter [docs/examples](./docs/examples):

- [docs/examples/analyze-console.txt](./docs/examples/analyze-console.txt)
- [docs/examples/analyze-report.md](./docs/examples/analyze-report.md)
- [docs/examples/impact-console.txt](./docs/examples/impact-console.txt)
- [docs/examples/impact-report.md](./docs/examples/impact-report.md)

Die Beispiele stammen aus denselben kanonischen M3-Fixtures wie die E2E-Golden-Files.

## Exit-Codes

| Code | Bedeutung | Typischer Ausloeser |
|---|---|---|
| `0` | Erfolg | gueltige CLI-Verwendung und gueltige Eingabedaten |
| `1` | Laufzeit- oder Report-Schreibfehler | nicht beschreibbarer `--output`-Pfad, unerwarteter Fehler |
| `2` | CLI-Verwendungsfehler | unbekanntes Unterkommando, fehlendes Pflichtargument, ungueltige Optionskombination |
| `3` | Eingabedatei nicht lesbar | Datei fehlt, kein Zugriff, Pfad ungueltig |
| `4` | Eingabedaten ungueltig | JSON fehlerhaft, kein Array, leer, Pflichtfelder fehlen |

## Tests und Quality Gates

Docker-basierte Referenzpfade:

```bash
docker build --target test -t cmake-xray:test .
docker build --target coverage-check --build-arg XRAY_COVERAGE_THRESHOLD=100 -t cmake-xray:coverage-check .
docker build --target quality-check -t cmake-xray:quality-check .
docker build --target runtime -t cmake-xray .
docker run --rm cmake-xray --help
```

Separat verfuegbare Reports:

```bash
docker build --target coverage -t cmake-xray:coverage .
docker run --rm cmake-xray:coverage
docker build --target quality -t cmake-xray:quality .
docker run --rm cmake-xray:quality
```

## Performance-Baseline

Die versionierten Referenzprojekte liegen unter [tests/reference](./tests/reference). Die dokumentierte Baseline fuer `scale_250`, `scale_500` und `scale_1000` sowie die Impact-Stichprobe steht in [docs/performance.md](./docs/performance.md). Die zugehoerigen Messartefakte werden unter `build/reports/performance/` erzeugt.

## Dokumente

Weitere Dokumentation:

- [docs/lastenheft.md](./docs/lastenheft.md)
- [docs/design.md](./docs/design.md)
- [docs/architecture.md](./docs/architecture.md)
- [docs/roadmap.md](./docs/roadmap.md)
- [docs/releasing.md](./docs/releasing.md)
- [docs/quality.md](./docs/quality.md)
- [docs/performance.md](./docs/performance.md)

## Lizenz

Das Projekt steht unter der [MIT-Lizenz](./LICENSE).

## Changelog

Aenderungen werden in [CHANGELOG.md](./CHANGELOG.md) dokumentiert.
