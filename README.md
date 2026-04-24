# cmake-xray

Build visibility for CMake projects.

`cmake-xray` ist ein Analyse- und Diagnosewerkzeug fuer CMake-basierte C++-Builds. Der aktuelle Stand `v1.1.0` liest `compile_commands.json` und optional CMake-File-API-Reply-Daten, rankt auffaellige Translation Units, leitet heuristische Include-Hotspots ab, analysiert Datei-Impact und zeigt betroffene Targets an. Ergebnisse werden als Konsolen- oder Markdown-Report ausgegeben.

## Status

Der Funktionsumfang umfasst:

- CLI mit `analyze` und `impact`
- Validierung von `compile_commands.json`
- deterministisches Translation-Unit-Ranking auf Basis von `arg_count`, `include_path_count` und `define_count`
- heuristische Include-Hotspots und dateibasierte Impact-Analyse
- CMake File API als optionale zweite Eingabequelle via `--cmake-file-api`
- Target-Zuordnung: Translation Units werden ihren CMake-Targets zugeordnet (`[targets: app, core]`)
- targetbezogene Impact-Ausgabe mit `direct` und `heuristic` Evidenzklassen
- Analyse auch ohne `compile_commands.json`, wenn File-API-Daten ausreichen
- Report-Ausgabe als `console` oder `markdown`
- atomisches Schreiben von Markdown-Reports via `--output`
- versionierte Golden-Files, Referenzdaten und Performance-Baselines
- Docker-basierte Test-, Coverage- und Quality-Gates

Nicht Ziel des aktuellen Stands sind insbesondere:

- Ersatz fuer CMake
- vollstaendige CMake-Interpretation
- transitive Target-Graph-Analyse
- IDE-Integration
- HTML-, JSON- oder DOT-Export

## Voraussetzungen

Referenzplattform ist Linux mit:

- CMake >= 3.20
- C++20-faehigem Compiler
- Git fuer `FetchContent`

Als reproduzierbare Referenzumgebung steht das Multi-Stage-[Dockerfile](./Dockerfile) zur Verfuegung.

## Installation und Ausfuehrung

Fuer normale Nutzung wird die CLI als `cmake-xray` aufgerufen. Releases stellen
dafuer ein versioniertes Linux-CLI-Artefakt und ein OCI-kompatibles
Container-Image bereit, damit Nutzer nicht auf interne Build-Pfade angewiesen
sind.

Release-Artefakt entpacken:

```bash
tar -xzf cmake-xray_X.Y.Z_linux_x86_64.tar.gz
mkdir -p "$HOME/.local/bin"
install -m 0755 cmake-xray "$HOME/.local/bin/cmake-xray"
export PATH="$HOME/.local/bin:$PATH"
cmake-xray --help
```

Container-Image verwenden:

```bash
docker run --rm ghcr.io/pt9912/cmake-xray:vX.Y.Z --help
```

Dabei bezeichnet `X.Y.Z` die Release-Version ohne fuehrendes `v`; Container
werden mit dem Git-Tag wie `vX.Y.Z` markiert.

Lokaler Quellbuild fuer Entwicklung:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Runtime-Image bauen:

```bash
docker build --target runtime -t cmake-xray .
```

Danach steht das Binary im Container als Entrypoint zur Verfuegung:

```bash
docker run --rm cmake-xray --help
```

## Nutzung

### Hilfe

```bash
cmake-xray --help
cmake-xray analyze --help
cmake-xray impact --help
```

### Projektanalyse in der Konsole

```bash
cmake-xray analyze \
  --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json \
  --top 10
```

### Projektanalyse als Markdown auf `stdout`

```bash
cmake-xray analyze \
  --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json \
  --format markdown \
  --top 10
```

### Projektanalyse als Markdown-Datei

```bash
cmake-xray analyze \
  --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json \
  --format markdown \
  --output build/reports/analyze.md \
  --top 10
```

### Impact-Analyse

```bash
cmake-xray impact \
  --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json \
  --changed-file include/common/config.h
```

### Impact-Analyse als Markdown

```bash
cmake-xray impact \
  --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json \
  --changed-file include/common/config.h \
  --format markdown \
  --output build/reports/impact.md
```

### Analyse mit CMake File API (Target-Sicht)

```bash
cmake-xray analyze \
  --cmake-file-api build/.cmake/api/v1/reply
```

Als Wert fuer `--cmake-file-api` wird entweder das Build-Verzeichnis (mit `.cmake/api/v1/reply` darunter) oder direkt das Reply-Verzeichnis akzeptiert. Die Option ist fuer `analyze` und `impact` verfuegbar und kann allein oder zusammen mit `--compile-commands` verwendet werden.

### Analyse mit Compile Database und File API (Mischpfad)

```bash
cmake-xray analyze \
  --compile-commands build/compile_commands.json \
  --cmake-file-api build
```

Im Mischpfad bleibt die Compile Database die autoritative Grundlage fuer Ranking, Hotspots und Impact. Die File API reichert passende Beobachtungen um Target-Kontext an.

### Impact-Analyse mit Target-Ausgabe

```bash
cmake-xray impact \
  --cmake-file-api build \
  --changed-file src/main.cpp
```

Bei geladener Target-Sicht zeigt `impact` zusaetzlich betroffene Targets mit Evidenzklassifikation (`direct` oder `heuristic`). Transitive Target-Abhaengigkeiten werden im aktuellen Stand nicht ausgewertet.

### Pfadaufloesung fuer `--changed-file`

Relative `--changed-file`-Pfade werden relativ zum Verzeichnis der uebergebenen `compile_commands.json` interpretiert. Im File-API-Only-Pfad (ohne `--compile-commands`) werden sie relativ zum im Codemodel angegebenen Top-Level-Source-Verzeichnis interpretiert.

## Heuristik-Hinweis

Include-Hotspots und Header-Impact beruhen auf heuristischer Include-Aufloesung. `cmake-xray` kennzeichnet diese Ergebnisse explizit und gibt Datenluecken als Diagnostics aus. Direkte Treffer auf bekannte Quelldateien werden ohne heuristische Klassifikation ausgewiesen. Target-Aussagen erscheinen nur, wenn File-API-Daten ueber `--cmake-file-api` bereitgestellt werden.

## Beispielausgaben

Kuratierte Beispielartefakte liegen unter [docs/examples](./docs/examples):

Ohne Target-Sicht (nur `compile_commands.json`):

- [docs/examples/analyze-console.txt](./docs/examples/analyze-console.txt)
- [docs/examples/analyze-report.md](./docs/examples/analyze-report.md)
- [docs/examples/impact-console.txt](./docs/examples/impact-console.txt)
- [docs/examples/impact-report.md](./docs/examples/impact-report.md)

Mit Target-Sicht (File API):

- [docs/examples/analyze-console-targets.txt](./docs/examples/analyze-console-targets.txt)
- [docs/examples/analyze-report-targets.md](./docs/examples/analyze-report-targets.md)
- [docs/examples/impact-console-targets.txt](./docs/examples/impact-console-targets.txt)
- [docs/examples/impact-report-targets.md](./docs/examples/impact-report-targets.md)

## Exit-Codes

| Code | Bedeutung                           | Typischer Ausloeser                                                                 |
| ---- | ----------------------------------- | ----------------------------------------------------------------------------------- |
| `0`  | Erfolg                              | gueltige CLI-Verwendung und gueltige Eingabedaten                                   |
| `1`  | Laufzeit- oder Report-Schreibfehler | nicht beschreibbarer `--output`-Pfad, unerwarteter Fehler                           |
| `2`  | CLI-Verwendungsfehler               | unbekanntes Unterkommando, fehlendes Pflichtargument, ungueltige Optionskombination |
| `3`  | Eingabedatei nicht lesbar           | Datei fehlt, kein Zugriff, Pfad ungueltig; gilt auch fuer explizite `--cmake-file-api`-Pfade |
| `4`  | Eingabedaten ungueltig              | JSON fehlerhaft, kein Array, leer, Pflichtfelder fehlen; gilt auch fuer ungueltige oder strukturell unzureichende File-API-Reply-Daten |

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
- [docs/guide.md](./docs/guide.md)
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
