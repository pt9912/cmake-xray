# Guide - cmake-xray

## Zweck

Dieser Guide beschreibt den praktischen Einstieg in `cmake-xray`: Build erzeugen,
Eingabedaten bereitstellen, Projektanalyse ausfuehren, Impact abschaetzen und
Reports lesen.

Fuer Architektur, Design, Release- und Qualitaetsdetails bleiben die
spezialisierten Dokumente massgeblich:

- [spec/architecture.md](../../spec/architecture.md)
- [spec/design.md](../../spec/design.md)
- [docs/user/releasing.md](./releasing.md)
- [docs/user/quality.md](./quality.md)
- [docs/user/performance.md](./performance.md)

## Voraussetzungen

`cmake-xray` ist fuer CMake-basierte C++-Projekte gedacht. Fuer den lokalen
Quellbuild werden benoetigt:

- CMake >= 3.20
- ein C++20-faehiger Compiler
- Git fuer `FetchContent`

CMake- und Compiler-Mindestversionen sind in
[../tests/platform/toolchain-minimums.json](../../tests/platform/toolchain-minimums.json)
versioniert; das Modul [../cmake/ToolchainMinimums.cmake](../../cmake/ToolchainMinimums.cmake)
prueft sie beim Configure jeder Plattform fail-fast. Anhebungen laufen
synchron ueber JSON, `cmake_minimum_required`, [README](../../README.md) und
diesen Abschnitt.

Als reproduzierbare Umgebung kann alternativ das Dockerfile im Repository
verwendet werden.

### Plattformstatus

Linux ist die offizielle Releaseplattform fuer M5; macOS arm64 und
Windows x86_64 sind seit 2026-04-30 als `validated_smoke` freigegeben,
weil Branch-Protection auf `main` die `Native (...)`-Required-Checks
verankert hat und ein gruener Post-Protection-CI-Lauf auf den
Plattform-Runnern auditiert ist. Die Beispiele in diesem Guide setzen
Linux voraus, laufen mit Anpassungen aber auch auf macOS und Windows
lokal. Eine offizielle macOS-/Windows-Releasefreigabe ist aus diesem
Guide ausdruecklich *nicht* abgeleitet — die normative Aussage steht in
[releasing.md](./releasing.md) "Plattformartefakte macOS und Windows";
Required-Check-Namen, Atomic-Replace-Matrix und Smoke-Report-Vertrag in
[quality.md](./quality.md) "Plattformstatus (AP M5-1.7)".

## Ausfuehrungswege

Fuer normale Nutzung wird die CLI als `cmake-xray` aufgerufen. Auf Linux x86_64
geschieht das ueber ein versioniertes Release-Artefakt oder ueber ein
OCI-kompatibles Container-Image. Auf macOS arm64 und Windows x86_64
(`validated_smoke`) gibt es kein offizielles Release-Artefakt; Nutzer dort
bauen das Binary aus dem Source (Sektion "Lokaler Quellbuild" weiter unten)
und legen es in ihren `PATH`. Beispiele in diesem Guide setzen den so
installierten `cmake-xray` voraus, nicht den internen Entwicklerpfad
`./build/cmake-xray`.

### Release-Artefakt

Ein Release stellt ein Archiv nach diesem Schema bereit:

```text
cmake-xray_X.Y.Z_linux_x86_64.tar.gz
```

Das Archiv enthaelt die ausfuehrbare Datei `cmake-xray` sowie
Begleitdokumentation. Nach dem Entpacken kann die Datei in ein Verzeichnis im
`PATH` gelegt werden:

```bash
tar -xzf cmake-xray_X.Y.Z_linux_x86_64.tar.gz
mkdir -p "$HOME/.local/bin"
install -m 0755 cmake-xray "$HOME/.local/bin/cmake-xray"
export PATH="$HOME/.local/bin:$PATH"
cmake-xray --help
```

Wenn `cmake-xray` im `PATH` liegt, sind alle weiteren Beispiele direkt
uebertragbar.

### Container

Das Runtime-Image fuehrt `cmake-xray` als Entrypoint aus. Fuer lokale Daten wird
das Projektverzeichnis oder ein Teil davon in den Container gemountet:

```bash
docker run --rm ghcr.io/pt9912/cmake-xray:X.Y.Z --help
```

Dabei bezeichnet `X.Y.Z` die Release-Version ohne fuehrendes `v`; der OCI-Tag
spiegelt die App-Version, der Git-Tag den Release-Anker (`vX.Y.Z`). Beide
Werte werden ueber den AP-1.6-Drei-Wege-Versionscheck gegeneinander
abgesichert (siehe `docs/user/releasing.md`).

Mit lokal gebautem Runtime-Image:

```bash
make runtime
```

### Lokaler Quellbuild

Der lokale Build deckt zwei Faelle ab: Entwicklung, Tests und Debugging auf
jeder Plattform sowie den unterstuetzten Nutzungsweg auf macOS arm64 und
Windows x86_64, fuer die kein vorgefertigtes Release-Artefakt erzeugt wird:

```bash
make dev
```

Resultat:

- Linux/macOS: `./build/cmake-xray`
- Windows (Visual-Studio-Generator): `./build/Release/cmake-xray.exe`

Auf macOS und Windows wird die so gebaute Binary anschliessend wie ein
installiertes `cmake-xray` in den `PATH` gelegt; auf Linux ist der Quellbuild
fuer Endnutzer-Aufrufe nicht empfohlen — dort liefert das versionierte
Release-Artefakt das supported Binary.

## Schnellstart

Die folgenden Schnellstart-Beispiele setzen ein lokal geklontes
`cmake-xray`-Repository sowie ein installiertes `cmake-xray` im `PATH` voraus.
Wer von einem Release-Artefakt aus arbeitet, ersetzt die Pfade unter
`tests/e2e/testdata/` durch eigene Eingabedaten.

Eine erste Projektanalyse:

```bash
cmake-xray analyze \
  --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json \
  --top 10
```

Eine erste Impact-Analyse:

```bash
cmake-xray impact \
  --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json \
  --changed-file include/common/config.h
```

Dieselbe Projektanalyse ueber das Runtime-Image:

```bash
docker run --rm \
  -v "$PWD/tests/e2e/testdata/m3:/data:ro" \
  ghcr.io/pt9912/cmake-xray:X.Y.Z \
  analyze --compile-commands /data/report_project/compile_commands.json --top 10
```

## Eingabedaten vorbereiten

`cmake-xray` liest `compile_commands.json` und optional Daten der
CMake File API. Beide Eingaben koennen einzeln oder kombiniert verwendet
werden.

### Compilation Database

In einem CMake-Projekt wird die Datei typischerweise so erzeugt:

```bash
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

Danach liegt die Compilation Database normalerweise unter:

```text
build/compile_commands.json
```

Wichtig:

- Die Datei muss ein gueltiges JSON-Array sein.
- Jeder Eintrag muss die erwarteten Pflichtfelder enthalten.
- Leere, nicht lesbare oder syntaktisch ungueltige Dateien werden mit
  definierten Exit-Codes abgewiesen.

### CMake File API

Fuer die Target-Sicht legt CMake auf Anfrage strukturierte Reply-Daten ab.
Die Anfrage wird vor dem ersten `cmake -B build` als leere Query-Datei
hinterlegt:

```bash
mkdir -p build/.cmake/api/v1/query
touch build/.cmake/api/v1/query/codemodel-v2
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

Die Reply-Daten landen unter `build/.cmake/api/v1/reply/`. Als Wert fuer
`--cmake-file-api` wird entweder das Build-Verzeichnis oder direkt das
Reply-Verzeichnis akzeptiert.

## Projektanalyse

Die Projektanalyse rankt auffaellige Translation Units und zeigt
Include-Hotspots:

```bash
cmake-xray analyze \
  --compile-commands build/compile_commands.json \
  --top 20
```

Typische Einsatzfaelle:

- grobe Orientierung in einem gewachsenen CMake-Projekt
- Erkennen von Translation Units mit vielen Compilerargumenten
- Erkennen von Headern, die viele Translation Units beeinflussen koennen
- Erzeugen eines Markdown-Artefakts fuer Reviews oder CI

Markdown-Ausgabe auf `stdout`:

```bash
cmake-xray analyze \
  --compile-commands build/compile_commands.json \
  --format markdown \
  --top 20
```

Markdown-Ausgabe als Datei:

```bash
cmake-xray analyze \
  --compile-commands build/compile_commands.json \
  --format markdown \
  --output build/reports/analyze.md \
  --top 20
```

JSON-Ausgabe fuer Tooling und CI:

```bash
cmake-xray analyze \
  --compile-commands build/compile_commands.json \
  --format json \
  --top 20
```

DOT-Ausgabe fuer Graphviz-Visualisierung:

```bash
cmake-xray analyze \
  --compile-commands build/compile_commands.json \
  --format dot \
  --top 20 > build/reports/analyze.dot
dot -Tsvg build/reports/analyze.dot > build/reports/analyze.svg
```

DOT ist Graphviz-Quelltext, kein gerendertes Bild. Format, Knoten- und
Kantenarten, Attributlexik, Sortier- und Budgetregeln sind in
[spec/report-dot.md](../../spec/report-dot.md) verbindlich dokumentiert. Mit
`--output <path>` schreibt der Adapter den Bericht atomar:

```bash
cmake-xray analyze \
  --compile-commands build/compile_commands.json \
  --format dot \
  --output build/reports/analyze.dot \
  --top 20
```

JSON ist der maschinenlesbare Reportvertrag von `cmake-xray`. Format,
Pflichtfelder, Enum-Werte und Sortierregeln sind in
[spec/report-json.md](../../spec/report-json.md) verbindlich dokumentiert; das JSON
Schema in [spec/report-json.schema.json](../../spec/report-json.schema.json)
unterstuetzt automatisierte Validierung. Erfolgreiche `--format json`-Aufrufe
schreiben ausschliesslich gueltiges JSON nach `stdout`; Fehler bleiben
Textmeldungen auf `stderr`. Mit `--output <path>` schreibt der Adapter den
Bericht atomar in eine Datei und laesst `stdout` und `stderr` leer:

```bash
cmake-xray analyze \
  --compile-commands build/compile_commands.json \
  --format json \
  --output build/reports/analyze.json \
  --top 20
```

HTML-Ausgabe fuer Reviews und CI-Artefakte:

```bash
cmake-xray analyze \
  --compile-commands build/compile_commands.json \
  --format html \
  --top 20
```

HTML ist ein eigenstaendiges HTML5-Dokument mit inline CSS, ohne externe
Ressourcen, ohne JavaScript und ohne HTML-Kommentare. Format, Dokumentstruktur,
Pflichtsektionen, CSS-Regeln und Escape-Vertrag sind in
[spec/report-html.md](../../spec/report-html.md) verbindlich dokumentiert.
Erfolgreiche `--format html`-Aufrufe schreiben ausschliesslich gueltiges HTML
nach `stdout`; Fehler bleiben Textmeldungen auf `stderr` ohne HTML-Fehlerdokument.
Mit `--output <path>` schreibt der Adapter den Bericht atomar in eine Datei und
laesst `stdout` und `stderr` leer:

```bash
cmake-xray analyze \
  --compile-commands build/compile_commands.json \
  --format html \
  --output build/reports/analyze.html \
  --top 20
```

Die erzeugte Datei kann ohne weitere Build- oder Asset-Schritte direkt im
Browser geoeffnet oder als CI-Artefakt veroeffentlicht werden.

## Impact-Analyse

Die Impact-Analyse schaetzt ab, welche Translation Units von einer geaenderten
Datei betroffen sind:

```bash
cmake-xray impact \
  --compile-commands build/compile_commands.json \
  --changed-file include/common/config.h
```

Relative `--changed-file`-Pfade werden relativ zum Verzeichnis der uebergebenen
`compile_commands.json` interpretiert.

Markdown-Ausgabe als Datei:

```bash
cmake-xray impact \
  --compile-commands build/compile_commands.json \
  --changed-file include/common/config.h \
  --format markdown \
  --output build/reports/impact.md
```

JSON-Ausgabe fuer Tooling und CI:

```bash
cmake-xray impact \
  --compile-commands build/compile_commands.json \
  --changed-file include/common/config.h \
  --format json \
  --output build/reports/impact.json
```

DOT-Ausgabe fuer Impact-Visualisierung:

```bash
cmake-xray impact \
  --compile-commands build/compile_commands.json \
  --changed-file include/common/config.h \
  --format dot \
  --output build/reports/impact.dot
dot -Tsvg build/reports/impact.dot > build/reports/impact.svg
```

Direkte Impact-Kanten erscheinen mit `style="solid"`, heuristische mit
`style="dashed"`, sodass Direktheit und Heuristik im gerenderten Bild
visuell unterscheidbar bleiben.

HTML-Ausgabe fuer Reviews und CI-Artefakte:

```bash
cmake-xray impact \
  --compile-commands build/compile_commands.json \
  --changed-file include/common/config.h \
  --format html \
  --output build/reports/impact.html
```

Der HTML-Impact-Bericht enthaelt getrennte Sektionen fuer direkt und
heuristisch betroffene Translation Units sowie Targets mit sichtbaren
`direct`/`heuristic`-Badges, sodass Direktheit und Heuristik auch ohne
Farbinformation unterscheidbar bleiben. Wird `--changed-file` weggelassen
oder als nicht aufloesbarer File-API-Quellpfad uebergeben, lehnt der HTML-Pfad
das Rendern als Textfehler auf `stderr` mit Exit-Code ungleich `0` ab und
erzeugt kein HTML-Fehlerdokument.

Der Impact-JSON-Bericht enthaelt zusaetzlich `inputs.changed_file` und
`inputs.changed_file_source`. Erlaubte Werte fuer `changed_file_source` sind
`compile_database_directory`, `file_api_source_root` und `cli_absolute`. Der
M5-Vertrag begrenzt Impact-Listen nicht ueber `--top`; alle betroffenen
Translation Units und Targets aus dem Modell werden ausgegeben.

## Reports lesen

`analyze`-Reports enthalten:

- Metadaten zur Eingabe und Ergebnisgroesse
- Ranking auffaelliger Translation Units
- Include-Hotspots
- Diagnostics zu Datenluecken oder unsicheren Befunden

`impact`-Reports enthalten:

- die untersuchte Datei
- direkt betroffene Translation Units
- heuristisch betroffene Translation Units
- Diagnostics zur Einordnung des Ergebnisses

Kuratierte Beispielausgaben liegen unter [docs/examples](../examples):

Ohne Target-Sicht (nur `compile_commands.json`):

- [docs/examples/analyze-console.txt](../examples/analyze-console.txt)
- [docs/examples/analyze-report.md](../examples/analyze-report.md)
- [docs/examples/analyze-report.html](../examples/analyze-report.html)
- [docs/examples/analyze-report.json](../examples/analyze-report.json)
- [docs/examples/analyze-report.dot](../examples/analyze-report.dot)
- [docs/examples/impact-console.txt](../examples/impact-console.txt)
- [docs/examples/impact-report.md](../examples/impact-report.md)
- [docs/examples/impact-report.html](../examples/impact-report.html)
- [docs/examples/impact-report.json](../examples/impact-report.json)
- [docs/examples/impact-report.dot](../examples/impact-report.dot)

Mit Target-Sicht (File API):

- [docs/examples/analyze-console-targets.txt](../examples/analyze-console-targets.txt)
- [docs/examples/analyze-report-targets.md](../examples/analyze-report-targets.md)
- [docs/examples/analyze-report-targets.html](../examples/analyze-report-targets.html)
- [docs/examples/analyze-report-targets.json](../examples/analyze-report-targets.json)
- [docs/examples/analyze-report-targets.dot](../examples/analyze-report-targets.dot)
- [docs/examples/impact-console-targets.txt](../examples/impact-console-targets.txt)
- [docs/examples/impact-report-targets.md](../examples/impact-report-targets.md)
- [docs/examples/impact-report-targets.html](../examples/impact-report-targets.html)
- [docs/examples/impact-report-targets.json](../examples/impact-report-targets.json)
- [docs/examples/impact-report-targets.dot](../examples/impact-report-targets.dot)

## CLI-Modi: `--quiet` und `--verbose`

`analyze` und `impact` akzeptieren je `--quiet` oder `--verbose` als
command-lokales Flag. Beide Flags sind gegenseitig exklusiv und muessen
hinter dem Subcommand stehen; eine globale Position vor dem Subcommand
wird abgelehnt. Der vollstaendige Vertrag steht in
[plan-M5-1-5.md](../planning/plan-M5-1-5.md); `quality.md` listet die zugehoerigen
Quality-Gates.

### Anwendungsfall Quiet: knappe CI-Logs

```bash
cmake-xray analyze \
  --compile-commands build/compile_commands.json \
  --quiet
```

Quiet reduziert den Console-Report auf wenige Pflichtzeilen (Status,
Anzahl analysierter Translation Units, Top-Hotspot, optional `target
metadata`). Fuer Markdown, JSON, DOT und HTML bleibt der Reportinhalt
unveraendert: stdout ist byte-stabil zum Normalmodus, `--output`-Dateien
sind byte-identisch, stderr bleibt im Erfolgsfall leer. Quiet eignet sich
fuer CI-Logs, in denen ein reines "ist die Analyse durchgelaufen"-Signal
genuegt.

Wichtig: Quiet unterdrueckt keine Fehler. Eingabe-, Render- und
Schreibfehler erscheinen weiter auf stderr und liefern denselben
nonzero Exit-Code wie ohne Quiet.

### Anwendungsfall Verbose: lokale Diagnose

```bash
cmake-xray analyze \
  --compile-commands build/compile_commands.json \
  --verbose
```

Verbose schreibt im Console-Modus zusaetzliche Diagnose-Sections nach
stdout (`inputs`, `targets`, `observations`, `notes`). Fuer
Artefaktformate bleibt stdout byte-stabil zum Normalmodus, und ein
zusaetzlicher `verbose: ...`-Block geht ausschliesslich nach stderr; er
nennt `report_type`, `format`, `output`, die Eingabequellen und
Beobachtungs-/Targetstatus. Verbose veraendert keine Exit-Codes.

### Verbose-Fehlerkontext

Bei Render- oder Schreibfehlern ergaenzt Verbose die normale
Fehlermeldung um vier `verbose:`-Zeilen mit `command`, `format`,
`output` und `validation_stage`. Beispiel fuer einen Renderfehler:

```text
error: cannot render report: <message>
verbose: command=analyze
verbose: format=json
verbose: output=stdout
verbose: validation_stage=render
```

`validation_stage` nimmt die Werte `input`, `analysis`, `render` oder
`write` an und wird aus der CLI-Schicht gesetzt; Stacktraces oder
C++-Typnamen werden nie ausgegeben.

## Heuristiken einordnen

Include-Hotspots und Header-Impact beruhen im aktuellen Stand auf
heuristischer Include-Aufloesung. Das bedeutet:

- direkte Treffer auf bekannte Quelldateien sind belastbarer als
  heuristische Include-Treffer
- bedingte Includes koennen fehlen
- generierte Header koennen fehlen, wenn sie nicht aus den vorhandenen
  Eingabedaten ableitbar sind
- relevante Unsicherheiten erscheinen als Diagnostics im Report

Die Ergebnisse sind deshalb als Orientierung und Review-Hilfe gedacht, nicht als
vollstaendiger Ersatz fuer Build-System- oder Compilerwissen.

## Exit-Codes

| Code | Bedeutung                           |
| ---- | ----------------------------------- |
| `0`  | Erfolg                              |
| `1`  | Laufzeit- oder Report-Schreibfehler |
| `2`  | CLI-Verwendungsfehler               |
| `3`  | Eingabedatei nicht lesbar           |
| `4`  | Eingabedaten ungueltig              |

## Quality Gate in einem Anwender-Projekt

`cmake-xray` hat im aktuellen Stand keine konfigurierbaren Analyseschwellen.
Die einzigen harten Fehlersignale sind die Exit-Codes `1`, `3` und `4` aus dem
vorigen Abschnitt. Ein Quality Gate in einem Anwender-Projekt wird deshalb aus
drei Bausteinen aufgebaut:

1. Eingabedaten reproduzierbar erzeugen
2. `cmake-xray` aus Build oder CI heraus aufrufen
3. den Markdown-Report als Artefakt sichern

Schwellen auf Reportinhalte sind nicht Bestandteil von `cmake-xray` und werden
bei Bedarf ausserhalb des Tools gepflegt.

### Eingabedaten im Anwender-Projekt erzeugen

In der `CMakeLists.txt` des zu analysierenden Projekts wird die
Compilation Database aktiviert:

```cmake
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)
```

Fuer die Target-Sicht ueber die CMake File API wird vor dem ersten
`cmake -B build` zusaetzlich eine Query-Datei abgelegt:

```bash
mkdir -p build/.cmake/api/v1/query
touch build/.cmake/api/v1/query/codemodel-v2
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Analyse als CMake-Target

Optional kann der Aufruf als `add_custom_target` in das Anwender-Projekt
eingebunden werden. `find_program` wird ohne `REQUIRED` aufgerufen, damit das
Configure des Anwender-Projekts nicht fehlschlaegt, falls `cmake-xray` lokal
nicht installiert ist:

```cmake
find_program(CMAKE_XRAY cmake-xray)

if(CMAKE_XRAY)
  add_custom_target(xray
    COMMAND ${CMAKE_XRAY} analyze
            --compile-commands ${CMAKE_BINARY_DIR}/compile_commands.json
            --cmake-file-api ${CMAKE_BINARY_DIR}
            --format markdown
            --output ${CMAKE_BINARY_DIR}/reports/xray-analyze.md
            --top 20
    VERBATIM)
endif()
```

Aufruf: `cmake --build build --target xray`.

### CI-Integration

Skizze fuer GitHub Actions, wenn `cmake-xray` im `PATH` des Runners liegt:

```yaml
- run: cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
- run: cmake --build build
- run: |
    cmake-xray analyze \
      --compile-commands build/compile_commands.json \
      --cmake-file-api build \
      --format markdown --output build/xray.md --top 20
- uses: actions/upload-artifact@v4
  with: { name: cmake-xray, path: build/xray.md }
```

Der CI-Schritt schlaegt automatisch fehl, wenn `cmake-xray` einen Exit-Code
ungleich `0` zurueckgibt.

### Container-Variante

Ohne lokale Installation kann das Runtime-Image verwendet werden:

```bash
docker run --rm -v "$PWD/build:/data:ro" \
  ghcr.io/pt9912/cmake-xray:X.Y.Z \
  analyze --compile-commands /data/compile_commands.json --cmake-file-api /data
```

## Weitere Anwendungsfaelle

Neben dem Quality-Gate-Pfad gibt es weitere Einsatzszenarien, die mit dem
aktuellen Funktionsumfang abgedeckt sind. Sie nutzen dieselben Eingabedaten
und unterliegen denselben Heuristik-Hinweisen wie die regulaere Analyse.

### PR-Review-Hilfe

Fuer jede in einem Pull Request geaenderte Datei laesst sich abschaetzen,
welche Translation Units und Targets davon plausibel betroffen sind. Reviewer
sehen so die Reichweite einer Aenderung, bevor sie sich durch den Diff
arbeiten:

```bash
cmake-xray impact \
  --compile-commands build/compile_commands.json \
  --cmake-file-api build \
  --changed-file include/common/config.h \
  --format markdown
```

### Refactoring-Kandidaten finden

Das TU-Ranking zeigt Translation Units mit besonders vielen
Compilerargumenten, Include-Pfaden oder Defines. Solche TUs sind oft erste
Kandidaten fuer Forward-Decls, IWYU oder das Schneiden von Header-Modulen:

```bash
cmake-xray analyze \
  --compile-commands build/compile_commands.json \
  --top 30
```

### Header-Hotspots stabilisieren

Die Hotspot-Liste markiert Header, die von vielen TUs gezogen werden.
Aenderungen an solchen Headern verursachen weitreichende Neukompilierung. Die
Liste eignet sich als Eingangspunkt fuer Stabilitaetsmassnahmen wie PIMPL,
kleinere Header oder Forward-Decls:

```bash
cmake-xray analyze \
  --compile-commands build/compile_commands.json \
  --format markdown \
  --output build/reports/hotspots.md
```

### Test-Selection als Beschleuniger

`impact` kann als Hinweis fuer eine engere Testauswahl genutzt werden. Die
Auswertung beruht auf heuristischer Include-Aufloesung und darf nur als
Beschleuniger eingesetzt werden, nicht als Ersatz fuer einen vollstaendigen
Testlauf vor Release:

```bash
cmake-xray impact \
  --cmake-file-api build \
  --changed-file src/foo/bar.cpp
```

### Onboarding und Codebase-Karte

Eine Erstanalyse mit Target-Sicht gibt neuen Entwicklern einen schnellen
Ueberblick ueber Targets, zentrale Header und Schwerpunktdateien:

```bash
cmake-xray analyze \
  --cmake-file-api build \
  --top 20
```

### Vorher/Nachher-Vergleich

Zwei Markdown-Reports vor und nach einem Refactoring koennen mit
Standardwerkzeugen verglichen werden, um die Wirkung sichtbar zu machen.
Einen eingebauten Vergleichsmodus gibt es im aktuellen Stand nicht:

```bash
cmake-xray analyze \
  --compile-commands build/compile_commands.json \
  --format markdown --output build/reports/xray-before.md
# Refactoring durchfuehren und neu bauen
cmake-xray analyze \
  --compile-commands build/compile_commands.json \
  --format markdown --output build/reports/xray-after.md
diff -u build/reports/xray-before.md build/reports/xray-after.md
```

## Verifikation

Lokale Tests:

```bash
make test BUILD_TYPE=Debug
```

Reproduzierbare Docker-Gates:

```bash
make docker-gates
make runtime
```

Details stehen in [docs/user/quality.md](./quality.md) und
[docs/user/releasing.md](./releasing.md).
