# cmake-xray

Build visibility for CMake projects.

`cmake-xray` ist ein Analyse- und Diagnosewerkzeug fuer CMake-basierte C++-Builds. Der aktuelle Stand `v0.1.0` liefert das technische Fundament fuer spaetere Analysen: Projektstruktur, Build, Tests, Docker-Referenzumgebung und ein Platzhalter-Binary.

## Status

Das Repository enthaelt den umgesetzten Meilenstein M0 aus [docs/plan-M0.md](./docs/plan-M0.md).

Der aktuelle Umfang ist bewusst klein:

- baubares C++20/CMake-Projekt mit hexagonaler Grundstruktur
- Platzhalter-Binary `cmake-xray`
- Platzhalter-Testlauf ueber `ctest`
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

Fuer M0 sind folgende Bibliotheken festgelegt:

| Bereich | Bibliothek | Einbindung | Verwendung |
|---|---|---|---|
| JSON-Parsing | `nlohmann/json` | `FetchContent` | spaetere Adapter unter `src/adapters/input/` |
| CLI-Parsing | `CLI11` | `FetchContent` | spaeterer CLI-Adapter unter `src/adapters/cli/` |
| Test-Framework | `doctest` | `FetchContent` | `tests/` und Target `xray_tests` |

Wichtig: `src/hexagon/` bleibt frei von externen Bibliotheken. Externe Abhaengigkeiten sind auf Adapter, Tests und Composition Root begrenzt.

## Build

Lokaler Quellbuild:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Danach liegt das Platzhalter-Binary unter `./build/cmake-xray`.

## Tests

Lokal:

```bash
cd build && ctest --output-on-failure
```

Alternativ gezielt:

```bash
cmake --build build --target xray_tests
./build/xray_tests
```

## Nutzung

Der aktuelle M0-Stand liefert ein Platzhalter-Binary ohne echte Analysefunktion:

```bash
./build/cmake-xray
```

Erwartet wird eine kurze Textausgabe mit Exit-Code `0`.

## Docker

Das Projekt nutzt ein Multi-Stage-`Dockerfile`:

- `build`: konfiguriert und baut das Projekt
- `test`: fuehrt `ctest` in der Referenzumgebung aus
- `runtime`: enthaelt nur das Platzhalter-Binary und noetige Laufzeitpakete

Build und Test der Docker-Stages:

```bash
docker build --target test -t cmake-xray:test .
docker build --target runtime -t cmake-xray .
```

Lauf des Runtime-Images:

```bash
docker run --rm cmake-xray
```

Empfohlene gehaertete Laufzeit:

```bash
docker run --rm --read-only --tmpfs /tmp:rw,noexec,nosuid,size=64m cmake-xray
```

Das Runtime-Image laeuft als unprivilegierter Benutzer `cmake-xray`. Die `--read-only`-Option ist fuer den aktuellen Platzhalter-Stand geeignet; das zusaetzliche `tmpfs` haelt eine temporaere Schreibflaeche bereit, falls spaetere Laufzeitbibliotheken oder kuenftige Funktionen sie benoetigen.

## Dokumente

Die fachliche und technische Planung ist in `docs/` abgelegt:

- [docs/lastenheft.md](./docs/lastenheft.md): Anforderungen, Randbedingungen und Abnahmekriterien
- [docs/design.md](./docs/design.md): fachliche und benutzerbezogene Ausgestaltung
- [docs/architecture.md](./docs/architecture.md): geplante Systemstruktur und Datenfluesse
- [docs/roadmap.md](./docs/roadmap.md): inkrementelle Lieferplanung
- [docs/plan-M0.md](./docs/plan-M0.md): Detailplanung fuer den ersten Meilenstein
- [docs/releasing.md](./docs/releasing.md): minimaler Release-Ablauf fuer Build, Test und Tagging

## Lizenz

Das Projekt steht unter der [MIT-Lizenz](./LICENSE).

## Changelog

Aenderungen werden in [CHANGELOG.md](./CHANGELOG.md) dokumentiert.
