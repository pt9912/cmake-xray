# Performance

## Zweck

Dieses Dokument beschreibt die Referenzumgebung, die versionierten Referenzprojekte und die gemessenen Baselines fuer `cmake-xray`. Die MVP-Baseline (Compile-Database-Pfad) wurde im Stand `v1.0.0` erhoben und gilt unveraendert fuer `v1.1.0` und `v1.2.0`. M5 (`v1.2.0`) ergaenzt die im AP M5-1.8 A.3 versionierten File-API-Reply-Fixtures plus eine getrennte Baseline fuer den CMake-File-API-Pfad; CMake-Configure-Zeit ist nicht Teil der gemessenen Laufzeit, weil die Reply-Daten unter `tests/reference/scale_*/build/.cmake/api/v1/reply/` versioniert sind.

## Referenzdaten

Versionierte Referenzprojekte:

- `tests/reference/scale_250/`
- `tests/reference/scale_500/`
- `tests/reference/scale_1000/`

Jede Groessenstufe enthaelt:

- `compile_commands.json` (Compile-Database-Pfad, MVP-Baseline)
- `CMakeLists.txt` (Quelle fuer den File-API-Pfad, AP M5-1.8 A.3)
- die referenzierten Quelldateien unter `src/`
- die benoetigten Header unter `include/common/`
- versionierte CMake-File-API-Reply-Daten unter `build/.cmake/api/v1/reply/`

Die Referenzdaten werden ueber [tests/reference/generate_reference_projects.py](../../tests/reference/generate_reference_projects.py) erzeugt und zusammen mit dem Generator versioniert. Die File-API-Reply-Verzeichnisse werden ueber [tests/reference/generate_file_api_fixtures.sh](../../tests/reference/generate_file_api_fixtures.sh) regeneriert; das Skript fuehrt `cmake -S ... -B ...` ohne Compile-Schritt aus und schreibt zusaetzlich [tests/reference/file-api-performance-manifest.json](../../tests/reference/file-api-performance-manifest.json) mit Fixture-Liste, CMake-Version, Generator und Reply-Pfad. Plan-M5-1-8.md verlangt explizit, dass Compile-DB-only-Daten nicht als File-API-Baseline missverstanden werden — die Trennung ist sowohl im Verzeichnislayout als auch im Manifest gefuehrt.

Zusatzstichprobe fuer den separaten Impact-Pfad:

- `tests/e2e/testdata/m3/report_impact_header/compile_commands.json`
- `--changed-file include/common/config.h`

## Referenzumgebung

Messdatum: `2026-04-22`

Host-System:

- Linux `6.8.0-110-generic` (`x86_64`)
- CPU: `13th Gen Intel(R) Core(TM) i9-13900H`
- logische CPUs: `20`

Container-/Toolchain-Umgebung:

- Basisimage: `ubuntu:24.04`
- CMake: `3.28.3`
- C++-Compiler: `GNU C++ 13.3.0`
- Messwerkzeug: `/usr/bin/time -v`

## Messmethodik

Vor der Messung wurde das Referenzimage gebaut:

```bash
make docker-test
```

Messpfad:

- Ausgabeformat fuer die NF-04-/NF-05-Baseline: `console` und `markdown`
- fester Analyseaufruf: `analyze --top 10`
- je Szenario ein dokumentierter Messlauf
- warmes Setup: Image und Binary waren bereits gebaut, die Messungen laufen danach in frischen Containern
- Laufzeit und maximaler Speicherverbrauch stammen aus `/usr/bin/time -v`
- Artefakte werden unter `build/reports/performance/` abgelegt

Referenzaufrufe:

```bash
mkdir -p build/reports/performance
docker run --rm -v "$PWD/build/reports/performance:/out" cmake-xray:test \
  /bin/bash -lc '/usr/bin/time -v /workspace/build/cmake-xray analyze --compile-commands /workspace/tests/reference/scale_250/compile_commands.json --top 10 > /out/xray-250-console.stdout.txt 2> /out/xray-250-console.time.txt'
docker run --rm -v "$PWD/build/reports/performance:/out" cmake-xray:test \
  /bin/bash -lc '/usr/bin/time -v /workspace/build/cmake-xray analyze --compile-commands /workspace/tests/reference/scale_500/compile_commands.json --top 10 > /out/xray-500-console.stdout.txt 2> /out/xray-500-console.time.txt'
docker run --rm -v "$PWD/build/reports/performance:/out" cmake-xray:test \
  /bin/bash -lc '/usr/bin/time -v /workspace/build/cmake-xray analyze --compile-commands /workspace/tests/reference/scale_1000/compile_commands.json --top 10 > /out/xray-1000-console.stdout.txt 2> /out/xray-1000-console.time.txt'
docker run --rm -v "$PWD/build/reports/performance:/out" cmake-xray:test \
  /bin/bash -lc '/usr/bin/time -v /workspace/build/cmake-xray analyze --compile-commands /workspace/tests/reference/scale_250/compile_commands.json --top 10 --format markdown > /out/xray-250-markdown.stdout.md 2> /out/xray-250-markdown.time.txt'
docker run --rm -v "$PWD/build/reports/performance:/out" cmake-xray:test \
  /bin/bash -lc '/usr/bin/time -v /workspace/build/cmake-xray analyze --compile-commands /workspace/tests/reference/scale_500/compile_commands.json --top 10 --format markdown > /out/xray-500-markdown.stdout.md 2> /out/xray-500-markdown.time.txt'
docker run --rm -v "$PWD/build/reports/performance:/out" cmake-xray:test \
  /bin/bash -lc '/usr/bin/time -v /workspace/build/cmake-xray analyze --compile-commands /workspace/tests/reference/scale_1000/compile_commands.json --top 10 --format markdown > /out/xray-1000-markdown.stdout.md 2> /out/xray-1000-markdown.time.txt'
docker run --rm -v "$PWD/build/reports/performance:/out" cmake-xray:test \
  /bin/bash -lc '/usr/bin/time -v /workspace/build/cmake-xray impact --compile-commands /workspace/tests/e2e/testdata/m3/report_impact_header/compile_commands.json --changed-file include/common/config.h --format markdown > /out/xray-impact-markdown-sample.stdout.md 2> /out/xray-impact-markdown-sample.time.txt'
```

## Baseline

### `analyze --top 10`

| Szenario | Ausgabeformat | Wall time | Max RSS | Artefakte |
|---|---|---:|---:|---|
| `scale_250` | `console` | `0.10 s` | `5,280 KB` | `xray-250-console.stdout.txt`, `xray-250-console.time.txt` |
| `scale_250` | `markdown` | `0.09 s` | `5,120 KB` | `xray-250-markdown.stdout.md`, `xray-250-markdown.time.txt` |
| `scale_500` | `console` | `0.18 s` | `5,920 KB` | `xray-500-console.stdout.txt`, `xray-500-console.time.txt` |
| `scale_500` | `markdown` | `0.17 s` | `5,760 KB` | `xray-500-markdown.stdout.md`, `xray-500-markdown.time.txt` |
| `scale_1000` | `console` | `0.33 s` | `7,520 KB` | `xray-1000-console.stdout.txt`, `xray-1000-console.time.txt` |
| `scale_1000` | `markdown` | `0.33 s` | `7,680 KB` | `xray-1000-markdown.stdout.md`, `xray-1000-markdown.time.txt` |

### `impact --format markdown` Stichprobe

| Szenario | Wall time | Max RSS | Artefakte |
|---|---:|---:|---|
| `report_impact_header` mit `include/common/config.h` | `0.00 s` | `4,480 KB` | `xray-impact-markdown-sample.stdout.md`, `xray-impact-markdown-sample.time.txt` |

### `analyze --cmake-file-api ... --top 10` (M5-File-API-Baseline)

Messdatum: `2026-04-29`

Vorbedingung: das Test-Image enthaelt die AP-1.8-A.3-Reply-Fixtures
(`make docker-test` re-baut das Image,
nachdem `tests/reference/generate_file_api_fixtures.sh` lokal lief).
Die Messlaeufe greifen auf die im Image gebackenen Reply-Verzeichnisse
und die im Image gebackene Binary zu, sodass weder CMake-Configure noch
ein Volume-Bindmount in die Wall-Time eingeht.

Referenzaufrufe:

```bash
docker run --rm cmake-xray:test bash -lc \
  '/usr/bin/time -v /workspace/build/cmake-xray analyze --cmake-file-api /workspace/tests/reference/scale_250/build --format console --top 10'
docker run --rm cmake-xray:test bash -lc \
  '/usr/bin/time -v /workspace/build/cmake-xray analyze --cmake-file-api /workspace/tests/reference/scale_250/build --format markdown --top 10'
# scale_500 und scale_1000 analog
```

Beobachtungen aus einer warmen Messreihe (Image und Reply-Daten in der
Image-Filesystem-Cache-Schicht):

| Szenario | Ausgabeformat | Wall time | Max RSS |
|---|---|---:|---:|
| `scale_250` (File-API) | `console` | `0.11 s` | `5,760 KB` |
| `scale_250` (File-API) | `markdown` | `0.09 s` | `5,760 KB` |
| `scale_500` (File-API) | `console` | `0.18 s` | `6,880 KB` |
| `scale_500` (File-API) | `markdown` | `0.20 s` | `6,880 KB` |
| `scale_1000` (File-API) | `console` | `0.38 s` | `8,640 KB` |
| `scale_1000` (File-API) | `markdown` | `0.47 s` | `8,480 KB` |

Die Werte liegen in derselben Groessenordnung wie die Compile-Database-
Baseline oben — der File-API-Pfad bringt zusaetzliche
Target-Metadaten-Aufloesung mit, die fuer `scale_1000` rund `0.05–0.15 s`
und `~1 MB` zusaetzliches Maximum-RSS kostet. Die NF-04-Schwelle
(`60 s` fuer `1.000` TUs) und die NF-05-Schwelle (`2 GB`) bleiben
zwei Groessenordnungen entfernt.

## Bewertung gegen NF-04 und NF-05

| Kennung | Soll | Beobachtung | Bewertung |
|---|---|---|---|
| `NF-04` | Projekte bis `1.000` Translation Units in hoechstens `60 s` analysierbar | `scale_1000` liegt bei `0.33 s` in `console` und `markdown` | erfuellt |
| `NF-05` | Speicherverbrauch unter `2 GB` | `scale_1000` liegt bei maximal `7,680 KB` | erfuellt |
| `NF-06` | Referenzumgebung und Referenzprojekt dokumentiert | Generator, versionierte Daten, Messpfad und Artefakte sind dokumentiert | erfuellt |

## Einordnung

- Die dokumentierte Baseline misst den ausgelieferten MVP-Pfad bewusst mit `--top 10`, damit Konsole und Markdown denselben begrenzten Ausgabeumfang verwenden.
- Die Impact-Messung ist fuer M3 eine dokumentierte Stichprobe, keine zweite NF-04-/NF-05-Hauptbaseline.
- Die Zahlen sind als reproduzierbare Referenzwerte fuer diesen Stand zu verstehen. Bei relevanten Aenderungen am Analyse- oder Report-Pfad sollen die Artefakte unter `build/reports/performance/` und dieses Dokument aktualisiert werden.
