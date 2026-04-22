# cmake-xray

Build visibility for CMake projects.

`cmake-xray` ist ein Analyse- und Diagnosewerkzeug fuer CMake-basierte C++-Builds. Der aktuelle Stand `v0.3.0` liest `compile_commands.json`, rankt auffaellige Translation Units, leitet heuristische Include-Hotspots ab und bietet eine dateibasierte Impact-Analyse fuer Translation Units.

## Status

Das Repository enthaelt die umgesetzten Meilensteine M0, M1 und M2 aus [docs/plan-M0.md](./docs/plan-M0.md), [docs/plan-M1.md](./docs/plan-M1.md) und [docs/plan-M2.md](./docs/plan-M2.md).

Der aktuelle Umfang:

- baubares C++20/CMake-Projekt mit hexagonaler Grundstruktur
- CLI mit `analyze`- und `impact`-Unterkommandos
- Einlesen und Validieren von `compile_commands.json`
- Translation-Unit-Ranking auf Basis von `arg_count`, `include_path_count` und `define_count`
- Include-Hotspots mit heuristischer Source-Parsing-Aufloesung
- dateibasierte Impact-Analyse fuer Translation Units
- begrenzbare Konsolenausgabe via `--top`
- definierte Exit-Codes und Fehlermeldungen
- Adapter-, Hexagon- und End-to-End-Tests
- Multi-Stage-`Dockerfile` mit `build`, `test` und `runtime`

## Geplanter Umfang

Fuer das erste Release ist weiterhin vorgesehen:

- Konsolen- und Markdown-Reports
- Nutzung auf Linux in lokalen Umgebungen und in CI

Nicht Ziel des MVP sind insbesondere:

- Ersatz fuer CMake
- vollstaendige CMake-Interpretation
- IDE-Integration
- HTML-, JSON- oder DOT-Export im ersten Release

## Voraussetzungen

Referenzplattform ist Linux mit:

- CMake >= 3.20
- C++20-faehigem Compiler
- Git fuer `FetchContent`

Als Referenzumgebung steht ausserdem das [Dockerfile](./Dockerfile) zur Verfuegung.

## Externe Abhaengigkeiten

| Bereich | Bibliothek | Einbindung | Verwendung |
|---|---|---|---|
| JSON-Parsing | `nlohmann/json` | `FetchContent` | Input-Adapter unter `src/adapters/input/` |
| CLI-Parsing | `CLI11` | `FetchContent` | CLI-Adapter unter `src/adapters/cli/` |
| Test-Framework | `doctest` | `FetchContent` | `tests/` und Target `xray_tests` |

Wichtig: `src/hexagon/` bleibt frei von externen Bibliotheken. Externe Abhaengigkeiten sind auf Adapter, Tests und Composition Root begrenzt.

## Build

Lokaler Quellbuild:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Tests

Lokal:

```bash
ctest --test-dir build --output-on-failure
```

Alternativ gezielt:

```bash
cmake --build build --target xray_tests
./build/xray_tests
```

## Nutzung

### Hilfe

```bash
cmake-xray --help
cmake-xray analyze --help
cmake-xray impact --help
```

### Projektanalyse

```bash
cmake-xray analyze --compile-commands path/to/compile_commands.json --top 10
```

Beispielausgabe:

```text
translation unit ranking
based on metrics: arg_count + include_path_count + define_count
top 3 of 17 translation units
1. src/app/main.cpp [directory: build/app]
   arg_count=8 include_path_count=2 define_count=1

include hotspots [heuristic]
top 2 of 4 include hotspots
- include/common/config.h (affected translation units: 3)
  src/app/main.cpp [directory: build/app]
  src/lib/core.cpp [directory: build/lib]
```

Die Ranking-Basis ist in der Standardausgabe sichtbar. Include-Hotspots sind in M2 bewusst heuristisch und werden inline als solche gekennzeichnet.

### Impact-Analyse

```bash
cmake-xray impact \
  --compile-commands path/to/compile_commands.json \
  --changed-file include/common/config.h
```

Relative `--changed-file`-Pfade werden relativ zur uebergebenen `compile_commands.json` interpretiert.

Beispielausgabe:

```text
impact analysis for include/common/config.h [heuristic]
affected translation units: 3
  src/app/main.cpp [directory: build/app] [heuristic]
  src/lib/core.cpp [directory: build/lib] [heuristic]
note: conditional or generated includes may be missing from this result
```

Impact-Ergebnisse fuer Header beruhen in M2 auf derselben heuristischen Include-Aufloesung wie die Hotspots. Direkte Treffer auf bekannte Quelldateien werden ohne Heuristik-Kennzeichnung ausgegeben.

Bei ungueltiger Eingabe:

```text
error: compile_commands.json is empty: path/to/compile_commands.json
hint: generate the compilation database before running cmake-xray analyze
```

### Exit-Codes

| Code | Bedeutung | Typischer Ausloeser |
|---|---|---|
| `0` | Erfolg | gueltige CLI-Verwendung und gueltige Eingabedaten |
| `1` | (reserviert) | unerwartete Laufzeitfehler |
| `2` | CLI-Verwendungsfehler | unbekanntes Unterkommando, fehlendes Pflichtargument |
| `3` | Eingabedatei nicht lesbar | Datei fehlt, kein Zugriff, Pfad ungueltig |
| `4` | Eingabedaten ungueltig | JSON fehlerhaft, kein Array, leer, Pflichtfelder fehlen |

## Docker

Das Projekt nutzt ein Multi-Stage-`Dockerfile`:

- `build`: konfiguriert und baut das Projekt
- `test`: fuehrt `ctest` in der Referenzumgebung aus
- `coverage`: baut mit Coverage-Instrumentierung, fuehrt `ctest` aus und erzeugt einen `gcovr`-Report
- `coverage-check`: prueft waehrend `docker build`, ob ein konfigurierbarer Coverage-Schwellwert erreicht wurde (Standard: `100`)
- `quality`: fuehrt `clang-tidy` und `lizard` aus und erzeugt einen Qualitaetsreport
- `quality-check`: prueft waehrend `docker build`, ob die Qualitaets-Gates fuer `clang-tidy` und `lizard` eingehalten werden
- `runtime`: enthaelt nur das Binary und noetige Laufzeitpakete

Build und Test der Docker-Stages:

```bash
docker build --target test -t cmake-xray:test .
docker build --target coverage -t cmake-xray:coverage .
docker build --target coverage-check --build-arg XRAY_COVERAGE_THRESHOLD=100 -t cmake-xray:coverage-check .
docker build --target quality -t cmake-xray:quality .
docker build --target quality-check -t cmake-xray:quality-check .
docker build --target runtime -t cmake-xray .
```

Coverage-Zusammenfassung aus dem Docker-Image lesen:

```bash
docker run --rm cmake-xray:coverage
```

Coverage-Gate waehrend des Docker-Builds ausfuehren:

```bash
docker build --target coverage-check --build-arg XRAY_COVERAGE_THRESHOLD=100 -t cmake-xray:coverage-check .
docker build --target coverage-check \
  --build-arg XRAY_COVERAGE_THRESHOLD=101 \
  -t cmake-xray:coverage-check .
```

Der `coverage`-Stage dient nur dem Report. Das eigentliche Fail-fast-Gate fuer die Mindestabdeckung liegt in `coverage-check`, damit ein Report auch dann separat erzeugt werden kann, wenn der Schwellwert unterschritten wird.

Qualitaetsreport aus dem Docker-Image lesen:

```bash
docker run --rm cmake-xray:quality
```

Qualitaets-Gates waehrend des Docker-Builds ausfuehren:

```bash
docker build --target quality-check -t cmake-xray:quality-check .
docker build --target quality-check \
  --build-arg XRAY_LIZARD_MAX_CCN=5 \
  -t cmake-xray:quality-check .
```

Der `quality`-Stage dient dem Report. Das eigentliche Build-Gate fuer statische Analyse und Metriken liegt in `quality-check`.

Konfigurierbare Build-Argumente fuer `quality-check`:

- `XRAY_CLANG_TIDY_MAX_FINDINGS` (Standard: `0`)
- `XRAY_LIZARD_MAX_CCN` (Standard: `10`)
- `XRAY_LIZARD_MAX_LENGTH` (Standard: `50`)
- `XRAY_LIZARD_MAX_PARAMETERS` (Standard: `5`)

Lauf des Runtime-Images:

```bash
docker run --rm cmake-xray --help
docker run --rm cmake-xray analyze --help
docker run --rm cmake-xray impact --help
docker run --rm \
  -v "$PWD/tests/e2e/testdata:/data:ro" \
  cmake-xray analyze --compile-commands /data/m2/analyze/compile_commands.json --top 2
docker run --rm \
  -v "$PWD/tests/e2e/testdata:/data:ro" \
  cmake-xray impact --compile-commands /data/m2/analyze/compile_commands.json --changed-file include/common/config.h
```

## Dokumente

Die fachliche und technische Planung ist in `docs/` abgelegt:

- [docs/lastenheft.md](./docs/lastenheft.md): Anforderungen, Randbedingungen und Abnahmekriterien
- [docs/design.md](./docs/design.md): fachliche und benutzerbezogene Ausgestaltung
- [docs/architecture.md](./docs/architecture.md): geplante Systemstruktur und Datenfluesse
- [docs/roadmap.md](./docs/roadmap.md): inkrementelle Lieferplanung
- [docs/plan-M0.md](./docs/plan-M0.md): Detailplanung fuer den ersten Meilenstein
- [docs/plan-M1.md](./docs/plan-M1.md): Detailplanung fuer den zweiten Meilenstein
- [docs/plan-M2.md](./docs/plan-M2.md): Detailplanung fuer den Kernanalyse-Meilenstein
- [docs/releasing.md](./docs/releasing.md): minimaler Release-Ablauf fuer Build, Test und Tagging

## Lizenz

Das Projekt steht unter der [MIT-Lizenz](./LICENSE).

## Changelog

Aenderungen werden in [CHANGELOG.md](./CHANGELOG.md) dokumentiert.
