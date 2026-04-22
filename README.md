# cmake-xray

Build visibility for CMake projects.

`cmake-xray` ist ein Analyse- und Diagnosewerkzeug fuer CMake-basierte C++-Builds. Der aktuelle Stand `v0.2.0` liest `compile_commands.json` ein, validiert die Eingabedaten und meldet Fehler mit klaren Diagnosen und definierten Exit-Codes.

## Status

Das Repository enthaelt die umgesetzten Meilensteine M0 und M1 aus [docs/plan-M0.md](./docs/plan-M0.md) und [docs/plan-M1.md](./docs/plan-M1.md).

Der aktuelle Umfang:

- baubares C++20/CMake-Projekt mit hexagonaler Grundstruktur
- CLI mit `analyze`-Unterkommando und `--compile-commands`-Option
- Einlesen und Validieren von `compile_commands.json`
- definierte Exit-Codes und Fehlermeldungen
- Adapter-, Hexagon- und End-to-End-Tests
- Multi-Stage-`Dockerfile` mit `build`, `test` und `runtime`

## Geplanter Umfang

Fuer das erste Release ist vorgesehen:

- Einlesen und Validieren von `compile_commands.json`
- Analyse auffaelliger Translation Units
- Include-Hotspot-Analyse
- dateibasierte Rebuild-Impact-Analyse
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
```

### Projektanalyse

```bash
cmake-xray analyze --compile-commands path/to/compile_commands.json
```

Bei gueltiger Eingabe:

```text
compile database loaded: 42 entries
```

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
- `coverage-check`: prueft waehrend `docker build`, ob ein konfigurierbarer Coverage-Schwellwert erreicht wurde
- `runtime`: enthaelt nur das Binary und noetige Laufzeitpakete

Build und Test der Docker-Stages:

```bash
docker build --target test -t cmake-xray:test .
docker build --target coverage -t cmake-xray:coverage .
docker build --target coverage-check -t cmake-xray:coverage-check .
docker build --target runtime -t cmake-xray .
```

Coverage-Zusammenfassung aus dem Docker-Image lesen:

```bash
docker run --rm cmake-xray:coverage
```

Coverage-Gate waehrend des Docker-Builds ausfuehren:

```bash
docker build --target coverage-check -t cmake-xray:coverage-check .
docker build --target coverage-check \
  --build-arg XRAY_COVERAGE_THRESHOLD=96 \
  -t cmake-xray:coverage-check .
```

Der `coverage`-Stage dient nur dem Report. Das eigentliche Fail-fast-Gate fuer die Mindestabdeckung liegt in `coverage-check`, damit ein Report auch dann separat erzeugt werden kann, wenn der Schwellwert unterschritten wird.

Lauf des Runtime-Images:

```bash
docker run --rm cmake-xray --help
docker run --rm cmake-xray analyze --help
docker run --rm \
  -v "$PWD/tests/e2e/testdata:/data:ro" \
  cmake-xray analyze --compile-commands /data/valid/compile_commands.json
```

## Dokumente

Die fachliche und technische Planung ist in `docs/` abgelegt:

- [docs/lastenheft.md](./docs/lastenheft.md): Anforderungen, Randbedingungen und Abnahmekriterien
- [docs/design.md](./docs/design.md): fachliche und benutzerbezogene Ausgestaltung
- [docs/architecture.md](./docs/architecture.md): geplante Systemstruktur und Datenfluesse
- [docs/roadmap.md](./docs/roadmap.md): inkrementelle Lieferplanung
- [docs/plan-M0.md](./docs/plan-M0.md): Detailplanung fuer den ersten Meilenstein
- [docs/plan-M1.md](./docs/plan-M1.md): Detailplanung fuer den zweiten Meilenstein
- [docs/releasing.md](./docs/releasing.md): minimaler Release-Ablauf fuer Build, Test und Tagging

## Lizenz

Das Projekt steht unter der [MIT-Lizenz](./LICENSE).

## Changelog

Aenderungen werden in [CHANGELOG.md](./CHANGELOG.md) dokumentiert.
