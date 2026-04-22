# Plan M3 - Markdown-Reports, Referenzdaten und MVP-Abnahme (`v1.0.0`)

## 0. Dokumenteninformationen

| Feld | Wert |
|---|---|
| Dokument | Plan M3 `cmake-xray` |
| Version | `0.1` |
| Stand | `2026-04-22` |
| Status | Entwurf |
| Referenzen | [Lastenheft](./lastenheft.md), [Design](./design.md), [Architektur](./architecture.md), [Phasenplan](./roadmap.md), [Plan M2](./plan-M2.md), [Qualitaet](./quality.md), [Releasing](./releasing.md) |

### 0.1 Zweck
Dieser Plan beschreibt die konkreten Schritte fuer Milestone M3 (`v1.0.0`). Ziel ist der lieferfaehige MVP-Stand: Die in M2 eingefuehrten fachlichen Analysen sollen als stabile Markdown-Berichte exportierbar sein, ueber versionierte Referenzdaten und erwartete Ausgaben abgesichert werden und in einer dokumentierten Referenzumgebung pruefbar bleiben.

### 0.2 Abschlusskriterium
M3 gilt als erreicht, wenn:

- `cmake-xray analyze` und `cmake-xray impact` neben der Konsolenausgabe auch einen deterministischen Markdown-Report erzeugen koennen
- das Ausgabeformat und ein optionaler Ausgabepfad per CLI steuerbar sind, ohne die bestehende Exit-Code-Semantik fuer Eingabefehler zu verwischen
- Markdown-Berichte die M2-Ergebnisse vollstaendig und reproduzierbar abbilden: Analysekontext, TU-Ranking, Include-Hotspots, Impact-Ergebnis sowie Diagnostics und Heuristik-Hinweise
- versionierte Referenzdaten und erwartete Konsolen- bzw. Markdown-Ausgaben fuer die zentralen Analysepfade vorliegen und automatisiert geprueft werden
- README, Beispielausgaben, CHANGELOG und Release-Dokumentation den M3-/MVP-Stand widerspiegeln
- eine dokumentierte Referenzumgebung sowie Messwerte fuer `250`, `500` und `1.000` Translation Units vorliegen und die Soll-Ziele aus `NF-04` und `NF-05` explizit bewertet werden
- die Docker-Pruefpfade fuer Test, Coverage-Gate und Quality-Gate mit dem M3-Stand erfolgreich durchlaufen

Relevante Kennungen: `F-26`, `F-27`, `F-35`, `F-36`, `F-42`, `NF-04`, `NF-05`, `NF-06`, `NF-10`, `NF-18`, `NF-19`, `AK-03`, `AK-04`, `AK-05`, `AK-06`, `AK-08`, `AK-09`

### 0.3 Abgrenzung
Bestandteil von M3 sind:

- Markdown-Report-Erzeugung fuer Projektanalyse und Impact-Analyse
- CLI-Steuerung fuer Ausgabeformat und optionalen Ausgabepfad
- stabile, diffbare Report-Struktur fuer Menschen und CI-Artefakte
- versionierte Referenzdaten und Golden-Outputs fuer Kernfaelle
- dokumentierte Referenzumgebung und reproduzierbare Performance-Messung
- Beispielausgaben, Nutzungsdokumentation und Release-Vorbereitung fuer `v1.0.0`

Nicht Bestandteil von M3 sind:

- neue fachliche Analysearten jenseits von M2
- targetbezogene Zusatzdaten und Target-Impact (`F-18` bis `F-25`)
- HTML-, JSON- oder DOT-Ausgaben (`F-28` bis `F-30`)
- `--quiet` sowie ein umfassender `--verbose`-Ausbau (`F-39`, `F-40`), sofern dies nicht zur M3-Dokumentation oder Fehlersichtbarkeit zwingend noetig ist
- Schema- oder Formatversionierung fuer maschinenlesbare Ausgabeformate (`NF-20`)

M3 baut auf den Ergebnissen aus M2 auf. Der Milestone dient der Stabilisierung, Exportierbarkeit und Lieferfaehigkeit des MVP und nicht der Einfuehrung eines zweiten Analysekerns.

## 1. Arbeitspakete

### 1.1 Report-Vertraege, Formatmodell und Verantwortungsgrenzen schaerfen

Die fuer M2 ausreichende Report-Strecke ist fuer M3 zu schmal. Neben Konsolenausgabe muessen jetzt auch persistierbare Markdown-Berichte erzeugt werden, ohne dass CLI, Dateisystem und Kern erneut eng gekoppelt werden.

Fuer M3 benoetigt der Report-Pfad mindestens:

- eine explizite Repraesentation des gewuenschten Ausgabeformats, zum Beispiel `console` und `markdown`
- eine einheitliche Repraesentation des generierten Reports, mindestens mit Textinhalt und Formatkennung
- getrennte, fachlich klare Eintrittspunkte fuer Projektanalyse und Impact-Analyse; bestehende M2-Entscheidungen dazu bleiben erhalten
- eine stabile Verantwortungsgrenze: Der Report-Adapter erzeugt Text, die Entscheidung ueber `stdout` oder Dateipfad bleibt ausserhalb des Hexagons
- dieselben Kernmodelle und Diagnostics fuer Konsole und Markdown; Reporter duerfen keine eigene Fachlogik oder abweichende Ergebnisaggregation einfuehren

Wichtig:

- das Hexagon bleibt frei von Dateisystem- und CLI-Typen
- ein Port-Rename ist fuer M3 nicht erforderlich; wichtiger als die Benennung ist die saubere Verantwortungstrennung
- Ausgabe- oder Schreibfehler duerfen nicht als Eingabefehler maskiert werden; der bisher reservierte Exit-Code `1` wird in M3 fuer unerwartete Laufzeit- bzw. Report-Schreibfehler aktiviert
- die Reihenfolge von Abschnitten, Listen und Diagnostics muss explizit dokumentiert und nicht vom Zufall der Eingabereihenfolge abhaengig sein

Vorgesehene Artefakte:

- neues oder erweitertes Modell fuer Report-Format und gerenderten Report unter `src/hexagon/model/`
- Anpassung von `src/hexagon/ports/driving/generate_report_port.h`
- Anpassung von `src/hexagon/ports/driven/report_writer_port.h`
- Anpassung von `src/hexagon/services/report_generator.*`

**Ergebnis**: Der Kern besitzt einen belastbaren, formatfaehigen Report-Vertrag, ohne fuer M3 einen zusaetzlichen Dateisystem-Port einzufuehren.

### 1.2 `MarkdownReportAdapter` fuer Projekt- und Impact-Berichte umsetzen

M3 fuehrt den im Architekturzielbild vorgesehenen `MarkdownReportAdapter` ein. Der Adapter muss dieselben fachlichen Ergebnisse wie der Konsolenreport darstellen, aber in einer diffbaren, gut verlinkbaren und fuer CI-Artefakte geeigneten Struktur.

Ein Markdown-Bericht fuer `analyze` soll mindestens enthalten:

- Titel und Report-Typ
- Tool-Version
- Pfad zur verwendeten `compile_commands.json`
- Kurzuebersicht zur Analyse, einschliesslich Gesamtanzahl und ggf. Begrenzung via `--top`
- Translation-Unit-Ranking mit Rang und Kennzahlen
- Include-Hotspots mit Header-Bezeichnung, Anzahl betroffener Translation Units und vollstaendiger TU-Zuordnung
- sichtbare Kennzeichnung heuristischer Ergebnisse
- sichtbare Diagnostics und Datenluecken

Ein Markdown-Bericht fuer `impact` soll mindestens enthalten:

- Titel und Report-Typ
- Tool-Version
- Pfad zur verwendeten `compile_commands.json`
- angefragter `--changed-file`
- betroffene Translation Units, mindestens getrennt nach direkter und heuristisch abgeleiteter Betroffenheit, sofern diese Unterscheidung im Kern verfuegbar ist
- klaren Hinweis bei `0` betroffenen Translation Units
- sichtbare Diagnostics und Datenluecken

Formatregeln fuer M3:

- Ausgabe als einfaches, breit kompatibles Markdown ohne HTML-Einbettung
- keine ANSI-Farben, keine Zeitstempel, keine Hostnamen und keine anderen volatilen Metadaten
- deterministische Abschnitts- und Listensortierung
- genau ein abschliessender Zeilenumbruch
- Heuristik-Hinweise und Datenluecken entweder inline am Ergebnis oder in einem unmittelbar folgenden Hinweisblock, nicht nur versteckt in einer Fussnote

Ein moeglicher M3-Zielaufruf ist:

```text
cmake-xray analyze --compile-commands <path> --format markdown
cmake-xray analyze --compile-commands <path> --format markdown --output report.md
cmake-xray impact --compile-commands <path> --changed-file <path> --format markdown --output impact.md
```

Vorgesehene Artefakte:

- neue Dateien `src/adapters/output/markdown_report_adapter.h` und `src/adapters/output/markdown_report_adapter.cpp`
- Anpassung des bestehenden Konsolenreport-Adapters, falls gemeinsame Hilfsfunktionen fuer Layout, Diagnostics oder Pfaddarstellung sinnvoll sind
- Anpassung von `src/adapters/CMakeLists.txt`

**Ergebnis**: Projekt- und Impact-Ergebnisse koennen als stabiler Markdown-Text erzeugt werden, ohne fachliche Informationen gegenueber der Konsole zu verlieren.

### 1.3 CLI fuer `--format` und `--output` erweitern

Die Berichtserzeugung muss fuer Nutzer uebersichtlich bleiben. M3 erweitert deshalb die bestehenden Unterkommandos `analyze` und `impact`, statt einen separaten Parallelpfad einzufuehren.

Entscheidung fuer M3:

- Standard bleibt `--format console`
- unterstuetzte Werte fuer `--format` sind `console` und `markdown`
- `--output <path>` ist nur in Verbindung mit `--format markdown` zulaessig
- wenn `--format markdown` ohne `--output` verwendet wird, wird der Markdown-Text auf `stdout` geschrieben
- wenn `--output <path>` verwendet wird, schreibt die CLI den Report dorthin und gibt hoechstens eine knappe Erfolgsbestaetigung auf `stdout` aus
- ein ungueltiges Format oder eine unzulaessige Optionskombination bleibt ein CLI-Verwendungsfehler mit Exit-Code `2`
- ein nicht beschreibbarer Report-Pfad oder ein Schreibfehler liefert Exit-Code `1`
- bestehende Eingabefehler fuer `compile_commands.json` behalten ihre M1-/M2-Codes `3` und `4`

Anforderungen an die Hilfe und Fehlermeldungen:

- `analyze --help` und `impact --help` muessen die neue Format- und Output-Semantik sichtbar erklaeren
- Fehlermeldungen bei ungueltigen Kombinationen sollen den naechsten Schritt nennen, zum Beispiel `hint: use --format markdown when writing a report file`
- `--top` muss fuer Markdown dieselbe fachliche Semantik haben wie fuer die Konsole

Nicht Ziel von M3:

- mehrere gleichzeitige Report-Sinks in einem Aufruf
- ein eigenstaendiges `report`-Unterkommando neben `analyze` und `impact`
- implizite Dateinamensfindung oder automatische Report-Ablage ohne explizite Nutzerentscheidung

Vorgesehene Artefakte:

- Anpassung von `src/adapters/cli/cli_adapter.*`
- Anpassung von `src/adapters/cli/exit_codes.h`
- Anpassung von `src/main.cpp`
- gegebenenfalls kleine Hilfstypen fuer CLI-Konfiguration und Report-Ausgabe

**Ergebnis**: Markdown-Berichte sind ueber dieselben Kommandowege wie die Konsolenausgabe nutzbar, mit klarer Hilfe und stabiler Exit-Code-Semantik.

### 1.4 Referenzdaten, Golden Files und Reporter-Tests fuer M3 ausbauen

Mit M3 steigt die Regressionsempfindlichkeit: Schon kleine Aenderungen an Sortierung, Abschnittstiteln oder Diagnostics koennen Markdown-Berichte und dokumentierte Beispielausgaben ungewollt aendern. Deshalb braucht M3 nicht nur mehr Tests, sondern auch explizite Golden-Outputs.

Mindestens benoetigt:

- Adapter-Tests fuer `MarkdownReportAdapter`, insbesondere fuer Abschnittsreihenfolge, Tabellen-/Listendarstellung, Heuristik-Kennzeichnung und Diagnostics
- CLI-Tests fuer `--format markdown`, `--output`, ungueltige Kombinationen und Report-Schreibfehler
- End-to-End-Tests, die Markdown-Ausgaben gegen erwartete Dateien vergleichen
- Golden-Outputs fuer zentrale Erfolgsfaelle von `analyze` und `impact`
- Tests dafuer, dass dieselben fachlichen Daten in Konsole und Markdown konsistent wiedergegeben werden

Sinnvolle Referenzfaelle unter `tests/e2e/testdata/m3/` sind:

- `report_project/` fuer einen typischen `analyze`-Durchlauf
- `report_impact_header/` fuer heuristische Header-Betroffenheit
- `report_impact_source/` fuer direkte TU-Betroffenheit
- `report_empty_impact/` fuer einen gueltigen Fall ohne Treffer
- `report_diagnostics/` fuer sichtbare Datenluecken und Heuristik-Hinweise
- `report_top_limit/` fuer `--top` in Konsole und Markdown

Golden-Output-Regeln fuer M3:

- erwartete Markdown-Dateien liegen versioniert im Repository
- erwartete Ausgaben enthalten keine volatilen Werte wie Zeitstempel oder temporaere Verzeichnisse
- Vergleiche sollen byte-stabil sein oder hoechstens zeilenendungsbezogen auf LF normalisieren; fachliche Vergleiche "ungefaehr gleich" reichen fuer M3 nicht aus

Fuer die Nutzerdokumentation sinnvoll:

- die kuratierten Beispielausgaben in `docs/examples/` werden aus denselben Testfaellen abgeleitet oder zumindest daran gespiegelt, damit Dokumentation und Golden-Tests nicht auseinanderlaufen

**Ergebnis**: Markdown- und Konsolenberichte sind gegen unbeabsichtigte Layout- und Inhaltsregressionen abgesichert.

### 1.5 Versioniertes Referenzprojekt und Performance-Baseline aufbauen

Phase 3 enthaelt nicht nur Report-Export, sondern auch Qualitaets- und Stabilisierungsaussagen fuer reale Nutzung. M3 fuehrt deshalb eine dokumentierte Referenzumgebung und ein versioniertes Referenzprojekt fuer Performance- und Testaussagen ein.

Das Referenzprojekt soll:

- im Repository versioniert sein; externe Downloads oder lokale Ad-hoc-Projekte reichen nicht aus
- mehrere Groessenstufen abdecken, mindestens `250`, `500` und `1.000` Translation Units
- kontrollierte Include-Hotspots und reproduzierbare Compile-Daten enthalten
- fuer Performance-Messung und fuer Teile der End-to-End-Teststrategie wiederverwendbar sein

Die Messmethodik fuer M3 soll mindestens dokumentieren:

- Betriebssystem, Compiler, CMake-Version und relevante Hardware-/Containerdaten
- exakten Build- und Analyseaufruf
- ob warm oder kalt gemessen wird
- wie viele Messlaeufe in die Baseline eingehen
- wie Laufzeit und Speicher erhoben werden
- ob und wie `--top` die gemessene Ausgabe begrenzt

Pragmatische Entscheidung fuer M3:

- die Performance-Baseline ist reproduzierbar zu messen, aber nicht Teil jedes Standard-`ctest`-Laufs
- sie wird als eigener dokumentierter Pruefpfad behandelt, lokal oder in einem separaten CI-Workflow
- die Ergebnisse werden gegen `NF-04` und `NF-05` bewertet; Nichterreichen darf nicht stillschweigend uebergangen werden, sondern muss in der Dokumentation sichtbar sein

Vorgesehene Artefakte:

- versioniertes Referenzprojekt oder Generator unter `tests/reference/`
- neue Dokumentation `docs/performance.md`
- gegebenenfalls Hilfsskripte fuer reproduzierbare Messlaeufe unter `tests/reference/` oder `scripts/`

**Ergebnis**: `cmake-xray` besitzt eine nachvollziehbare Referenzbasis fuer Performance- und Stabilitaetsaussagen des MVP.

### 1.6 README, Beispielausgaben, Release-Dokumentation und Versionshebung abschliessen

M3 ist der MVP-Abschluss. Dokumentation und Release-Artefakte muessen deshalb nicht mehr nur den Entwicklungsstand beschreiben, sondern den lieferbaren Produktstand.

Die README soll fuer M3 mindestens enthalten:

- aktualisierten Projektstatus mit Verweis auf M2 und M3
- Beispiele fuer `analyze` und `impact` in der Konsole
- Beispiele fuer Markdown-Ausgabe via `--format markdown`
- Beispiel fuer `--output <path>`
- klaren Hinweis auf heuristische Include-/Impact-Aussagen
- Verweis auf Referenz- und Beispielausgaben
- Verweis auf `docs/performance.md`, sofern diese Datei eingefuehrt wird

Zusaetzlich sind bei Abschluss von M3 zu aktualisieren:

- `CHANGELOG.md` fuer `v1.0.0`
- `src/hexagon/model/application_info.h`
- Root-`CMakeLists.txt`
- `docs/releasing.md`, falls der Release-Ablauf um Markdown-Artefakte oder weitere Pruefschritte ergaenzt wird
- gegebenenfalls `docs/roadmap.md`, falls Status oder Formulierungen vom Entwurf in den erreichten MVP-Stand uebergehen

Sinnvolle Beispielartefakte unter `docs/examples/`:

- `analyze-console.txt`
- `analyze-report.md`
- `impact-console.txt`
- `impact-report.md`

**Ergebnis**: Nutzerdokumentation, Beispielausgaben und Release-Artefakte entsprechen dem auslieferbaren MVP-Stand `v1.0.0`.

## 2. Zielartefakte

Folgende Dateien oder Dateigruppen sollen nach M3 voraussichtlich neu entstehen oder substanziell ueberarbeitet sein:

| Bereich | Zielartefakte |
|---|---|
| Reportmodelle | neue/erweiterte Modelle unter `src/hexagon/model/` fuer Report-Format und gerenderten Report |
| Driving Port | `src/hexagon/ports/driving/generate_report_port.h` |
| Driven Port | `src/hexagon/ports/driven/report_writer_port.h` |
| Service | `src/hexagon/services/report_generator.*` |
| Output-Adapter | `src/adapters/output/console_report_adapter.*`, neue Dateien `src/adapters/output/markdown_report_adapter.*` |
| CLI | `src/adapters/cli/cli_adapter.*`, `src/adapters/cli/exit_codes.h` |
| Composition Root | `src/main.cpp` |
| Build | `src/adapters/CMakeLists.txt`, `tests/CMakeLists.txt` |
| Adapter-Tests | neue oder erweiterte Tests unter `tests/adapters/` fuer Markdown-Reporting und Output-Fehlerfaelle |
| E2E-Tests | neue oder erweiterte Tests unter `tests/e2e/` fuer Markdown und Golden-Outputs |
| Referenzdaten | neue Fixtures unter `tests/e2e/testdata/m3/` sowie versionierte Referenzprojekte oder Generatoren unter `tests/reference/` |
| Beispielausgaben | neue Dateien unter `docs/examples/` |
| Performance-Dokumentation | neue Datei `docs/performance.md` oder ein klar abgegrenzter Abschnitt in bestehender Dokumentation |
| Produktdokumentation | `README.md`, `CHANGELOG.md`, `docs/releasing.md`, gegebenenfalls `docs/roadmap.md` |

Hinweis: Die konkreten Dateinamen duerfen von dieser Zielstruktur abweichen, solange die Trennung zwischen Kern, Report-Adaptern, CLI und Referenzdaten erhalten bleibt.

## 3. Reihenfolge

| Schritt | Arbeitspaket | Abhaengig von |
|---|---|---|
| 1a | 1.1 Report-Vertraege und Formatmodell schaerfen | M2 |
| 1b | 1.5 Referenzprojekt und Messmethodik festlegen | M2 |
| 2a | 1.2 `MarkdownReportAdapter` umsetzen | 1a |
| 2b | 1.3 CLI fuer `--format` und `--output` erweitern | 1a |
| 3 | 1.4 Golden-Outputs und Reporter-Tests ausbauen | 2a, 2b |
| 4 | 1.5 Performance-Baseline messen und dokumentieren | 1b, 3 |
| 5 | 1.6 README, Beispielausgaben und Release-Artefakte abschliessen | 2b, 3, 4 |

Schritte 1a und 1b koennen parallel bearbeitet werden. Das Referenzprojekt fuer Performance und Regressionstests sollte frueh feststehen, damit Reporter-Tests, Beispielausgaben und spaetere Baseline-Messungen auf derselben Datengrundlage aufbauen. Die eigentlichen Messungen aus 1.5 sollten jedoch erst gegen einen funktional stabilen M3-Stand erfolgen, damit nicht auf vorlaeufigen Report- oder Ausgabewegen gemessen wird.

## 4. Pruefung

M3 ist abgeschlossen, wenn folgende Pruefwege erfolgreich durchlaufen:

**Lokal**:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
mkdir -p build/reports
./build/cmake-xray --help
./build/cmake-xray analyze --help
./build/cmake-xray impact --help
./build/cmake-xray analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json
./build/cmake-xray analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json --format markdown > build/reports/analyze.stdout.md
./build/cmake-xray analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json --format markdown --output build/reports/analyze.md
./build/cmake-xray impact --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json --changed-file include/common/config.h --format markdown --output build/reports/impact.md
ctest --test-dir build --output-on-failure
```

**Docker / Quality Gates**:

```bash
docker build --target test -t cmake-xray:test .
docker build --target coverage-check --build-arg XRAY_COVERAGE_THRESHOLD=100 -t cmake-xray:coverage-check .
docker build --target quality-check -t cmake-xray:quality-check .
docker build --target runtime -t cmake-xray .
docker run --rm cmake-xray --help
docker run --rm \
  -v "$PWD/tests/e2e/testdata/m3:/data:ro" \
  -v "$PWD/build/reports:/out" \
  cmake-xray analyze --compile-commands /data/report_project/compile_commands.json --format markdown --output /out/analyze.md
docker run --rm \
  -v "$PWD/tests/e2e/testdata/m3:/data:ro" \
  -v "$PWD/build/reports:/out" \
  cmake-xray impact --compile-commands /data/report_impact_header/compile_commands.json --changed-file include/common/config.h --format markdown --output /out/impact.md
```

**Performance-Baseline**:

```bash
/usr/bin/time -v ./build/cmake-xray analyze --compile-commands tests/reference/scale_250/compile_commands.json --top 10 > /tmp/xray-250.txt
/usr/bin/time -v ./build/cmake-xray analyze --compile-commands tests/reference/scale_500/compile_commands.json --top 10 > /tmp/xray-500.txt
/usr/bin/time -v ./build/cmake-xray analyze --compile-commands tests/reference/scale_1000/compile_commands.json --top 10 > /tmp/xray-1000.txt
```

Die Pruefung soll insbesondere bestaetigen:

- Konsole und Markdown verwenden dieselbe fachliche Datengrundlage
- `--format markdown` funktioniert fuer `analyze` und `impact`
- `--output` schreibt Reports reproduzierbar und mit passender Fehlerbehandlung
- ungueltige Format-/Output-Kombinationen liefern Exit-Code `2`
- Schreibfehler fuer Report-Dateien liefern Exit-Code `1`
- Eingabefehler behalten ihre M1-/M2-Codes `3` und `4`
- Golden-Outputs bleiben stabil
- Coverage- und Quality-Gates bleiben trotz Report-Ausbau gruen
- Referenzumgebung, Messwerte und Bewertung gegen `NF-04` und `NF-05` sind dokumentiert

## 5. Rueckverfolgbarkeit

| Planbereich | Lastenheft-Kennungen |
|---|---|
| Markdown-Report und Report-Vertraege | `F-27`, `AK-06`, `NF-13` |
| Konsolen-/Markdown-Paritaet und Ergebnisbegrenzung | `F-26`, `F-42`, `AK-03`, `AK-04`, `AK-05`, `NF-15` |
| CLI fuer Format und Ausgabepfad | `F-31`, `F-32`, `F-33`, `F-34`, `F-35`, `F-36`, `AK-09`, `NF-02`, `NF-14` |
| Referenzdaten und Golden-Outputs | `NF-10`, `NF-18`, `NF-19`, `AK-08` |
| Referenzumgebung und Performance-Baseline | `NF-04`, `NF-05`, `NF-06` |
| Release- und Produktdokumentation | `NF-16`, `NF-17`, `NF-18`, `AK-08` |

## 6. Entschiedene und offene Punkte

### 6.1 Entschieden

| Frage | Entscheidung | Verweis |
|---|---|---|
| Formatwahl in M3 | `analyze` und `impact` bleiben die einzigen Nutzerpfade; Report-Ausgabe wird ueber `--format console|markdown` erweitert | AP 1.3 |
| Default-Ausgabe | Standard bleibt die Konsolenausgabe | AP 1.3 |
| Dateiausgabe | `--output <path>` ist nur fuer Markdown zulaessig; ohne `--output` geht Markdown auf `stdout` | AP 1.3 |
| Schreibfehler | nicht beschreibbare Report-Pfade nutzen Exit-Code `1`; Eingabefehler behalten `3` und `4` | AP 1.1, 1.3 |
| Verantwortung der Report-Strecke | Report-Adapter erzeugen Text; `stdout`/Dateisystem-Entscheidung bleibt ausserhalb des Hexagons | AP 1.1 |
| Markdown-Stil | einfaches, diffbares Markdown ohne HTML, Zeitstempel oder sonstige volatile Metadaten | AP 1.2 |
| Golden-Outputs | versioniert im Repository und gegen stabile, nichtvolatile Ausgaben geprueft | AP 1.4 |
| Referenzprojekt | versioniert im Repository unter `tests/reference/`; keine externen Benchmark-Abhaengigkeiten | AP 1.5 |
| Performance-Pfad | reproduzierbar dokumentiert, aber kein Pflichtbestandteil jedes `ctest`-Laufs | AP 1.5 |
| Nicht-MVP-Formate | HTML, JSON und DOT bleiben ausserhalb von M3 | AP 0.3 |

### 6.2 Offen

Keine offenen Punkte fuer M3.
