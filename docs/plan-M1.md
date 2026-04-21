# Plan M1 - MVP-Eingaben und CLI (`v0.2.0`)

## 0. Dokumenteninformationen

| Feld | Wert |
|---|---|
| Dokument | Plan M1 `cmake-xray` |
| Version | `0.1` |
| Stand | `2026-04-21` |
| Status | Entwurf |
| Referenzen | [Lastenheft](./lastenheft.md), [Design](./design.md), [Architektur](./architecture.md), [Phasenplan](./roadmap.md), [Plan M0](./plan-M0.md) |

### 0.1 Zweck
Dieser Plan beschreibt die konkreten Schritte fuer Milestone M1 (`v0.2.0`). Ziel ist ein nutzbares CLI-Grundgeruest, das eine `compile_commands.json` kontrolliert einliest, Fehlerfaelle verstaendlich behandelt und den spaeteren Analysekern sauber ueber Ports ansteuert.

### 0.2 Abschlusskriterium
M1 gilt als erreicht, wenn:

- eine gueltige `compile_commands.json` erfolgreich eingelesen und in ein internes Eingabemodell ueberfuehrt wird
- eine ungueltige, leere oder unvollstaendige `compile_commands.json` mit klarer Meldung und definiertem Exit-Code behandelt wird
- `--help` fuer das Hauptkommando und mindestens ein Unterkommando verstaendliche Hilfe ausgibt
- die CLI fuer Erfolg und Fehlerfaelle nachvollziehbare Exit-Codes verwendet
- Adapter-, Hexagon- und End-to-End-Tests die M1-Faelle automatisiert absichern

Relevante Kennungen: `F-01`, `F-02`, `F-03`, `F-04`, `F-31`, `F-32`, `F-33`, `F-34`, `F-41`, `NF-01`, `NF-02`, `NF-14`, `AK-01`, `AK-02`, `AK-09`

### 0.3 Abgrenzung
Bestandteil von M1 sind:

- Einlesen und Validieren von `compile_commands.json`
- CLI-Bedienung fuer Hauptkommando, Analyse-Unterkommando und Hilfe
- definierte Exit-Codes und konkrete Fehlermeldungen
- Testdaten und automatisierte Absicherung fuer Erfolg und Fehlerfaelle

Nicht Bestandteil von M1 sind:

- fachliche Ranking-Logik fuer Translation Units (`F-06` bis `F-09`)
- Include-Hotspot-Analyse (`F-12` bis `F-17`)
- Impact-Analyse (`F-21` bis `F-25`)
- Markdown-, HTML-, JSON- oder DOT-Ausgaben (`F-27` bis `F-30`)
- detailreicher oder reduzierter Modus (`F-39`, `F-40`), sofern dies M1 unnoetig verzoegert

## 1. Arbeitspakete

### 1.1 Compile-Database-Modell im Kern schaerfen

Der Platzhalterzustand aus M0 reicht fuer M1 nicht mehr aus. Der Kern benoetigt ein stabiles, formatunabhaengiges Modell fuer Eingabedaten aus `compile_commands.json`.

Mindestens benoetigt:

- ein Modell fuer einzelne Compile-Eintraege
- Extraktion mindestens von Quelldateipfad, Arbeitsverzeichnis und Compile-Aufruf
- ein Ergebnisobjekt, das zwischen erfolgreichem Laden, leerer Datenbasis und Fehlerfaellen unterscheiden kann
- Diagnosen oder Fehltexte so modellieren, dass der CLI-Adapter daraus Meldungen und Exit-Codes ableiten kann

Wichtig:

- JSON-Typen duerfen nicht in oeffentlichen Hexagon-Schnittstellen auftauchen
- das Modell soll fuer spaetere Analysen in Phase 2 weiterverwendbar sein

Vorgesehene Artefakte:

- neues Modell fuer Compile-Eintraege unter `src/hexagon/model/`
- Erweiterung oder Ersatz von `compile_database_status.h`, damit neben "vorhanden" auch Fehlergrund und Diagnose transportiert werden koennen
- Anpassung des `CompileDatabasePort`, damit nicht nur ein boolescher Platzhalter, sondern fachlich nutzbare Daten geliefert werden

**Ergebnis**: Der Kern enthaelt ein belastbares Eingabemodell fuer Compile-Eintraege und deren Validierungsstatus.

### 1.2 `CompileCommandsJsonAdapter` umsetzen

Der bisherige Platzhalter-Adapter in `src/adapters/input/` wird durch einen echten JSON-Adapter ersetzt oder entsprechend weiterentwickelt.

Der Adapter soll mindestens:

- eine Datei `compile_commands.json` von einem konfigurierbaren Pfad laden
- syntaktische JSON-Fehler erkennen
- sicherstellen, dass die Wurzel ein Array ist
- eine leere, aber syntaktisch gueltige Datei gezielt erkennen (`F-41`)
- pro Eintrag die Pflichtinformationen validieren
- unvollstaendige oder unbrauchbare Eintraege mit klarer Rueckmeldung behandeln

Als Mindestvalidierung fuer einen verwertbaren Eintrag gelten:

- Quelldatei vorhanden und nicht leer
- Arbeitsverzeichnis vorhanden und nicht leer
- Compile-Aufruf ueber `command` oder `arguments` vorhanden und nicht leer

Die Rueckmeldung soll nicht nur sagen, dass die Datei ungueltig ist, sondern moeglichst auch warum, zum Beispiel:

- Datei nicht gefunden
- JSON syntaktisch fehlerhaft
- oberste Struktur ist kein Array
- Datei ist leer
- Eintrag `n` enthaelt kein `file`
- Eintrag `n` enthaelt weder `command` noch `arguments`

Vorgesehene Artefakte:

- Ersatz oder Umbenennung des Platzhalter-Adapters `src/adapters/input/json_dependency_probe.*`
- Dateiname gemaess Architekturziel vorzugsweise `compile_commands_json_adapter.*`
- Tests unter `tests/adapters/` fuer alle relevanten Validierungsfaelle

**Ergebnis**: Eine gueltige `compile_commands.json` wird in ein internes Modell ueberfuehrt; ungueltige Eingaben liefern kontrollierte Fehler.

### 1.3 CLI-Grundstruktur fuer M1 aufbauen

Der CLI-Adapter soll von der Platzhalterausgabe zu einer nachvollziehbaren Befehlsstruktur wechseln. Die Benennung soll sich am Nutzerziel orientieren, nicht an internen Klassen.

Fuer M1 reicht eine kleine, aber belastbare Struktur, zum Beispiel:

```text
cmake-xray analyze --compile-commands <path>
cmake-xray analyze --help
cmake-xray --help
```

Anforderungen fuer die erste CLI-Struktur:

- Hauptkommando mit Projektbeschreibung und globaler Hilfe
- mindestens ein Unterkommando fuer die Projektanalyse
- Pfadangabe fuer `compile_commands.json`
- Ausgabe definierter Exit-Codes fuer Erfolg und Fehler
- Fehlermeldungen auf `stderr`, regulare Ergebnis- oder Hilfetexte auf `stdout`

M1 muss noch keine fachliche Analyse aus Phase 2 liefern. Bei gueltigen Eingaben darf weiterhin ein Platzhalter- oder Minimalergebnis ausgegeben werden, solange die Eingabe sauber geladen und der Aufrufpfad korrekt verdrahtet ist.

Vorgesehene CLI-Semantik fuer M1:

- `cmake-xray --help`: globale Hilfe und Kurzbeschreibung
- `cmake-xray analyze --help`: Hilfe fuer den Analyse-Einstieg
- `cmake-xray analyze --compile-commands <path>`: Validierung und Minimaldurchlauf fuer eine Projektanalyse

Vorgesehene Artefakte:

- Ersatz oder Umbenennung des Platzhalter-Adapters `src/adapters/cli/placeholder_cli_adapter.*`
- separates Header- oder Konstantenfile fuer Exit-Codes, zum Beispiel `src/adapters/cli/exit_codes.h`
- Anpassung von `src/main.cpp` als Composition Root fuer den M1-CLI-Pfad

**Ergebnis**: Die CLI ist fuer echte Eingabedateien nutzbar und dokumentiert die kuenftige Befehlsstruktur.

### 1.4 Exit-Codes und Fehlermeldungsregeln festlegen

M1 soll nicht nur "erfolgreich" oder "fehlgeschlagen" unterscheiden. Der CLI-Adapter benoetigt nachvollziehbare, stabile Regeln fuer Rueckgabecodes.

Mindestens zu unterscheiden:

- Erfolg
- ungueltige CLI-Verwendung oder fehlende Pflichtargumente
- Eingabedatei fehlt oder ist nicht lesbar
- Eingabedaten sind syntaktisch oder fachlich ungueltig

Fuer M1 reicht eine kleine feste Menge dokumentierter Codes. Wichtiger als die konkrete Nummernwahl ist, dass:

- dieselben Fehlerfaelle immer denselben Code liefern
- die Meldung den Nutzer zum naechsten Schritt fuehrt
- Tests die Exit-Codes absichern

Vorgeschlagene M1-Exit-Codes:

| Code | Bedeutung | Typischer Ausloeser |
|---|---|---|
| `0` | Erfolg | gueltige CLI-Verwendung und gueltige Eingabedaten |
| `2` | CLI-Verwendungsfehler | unbekanntes Unterkommando, fehlendes Pflichtargument |
| `3` | Eingabedatei nicht lesbar | Datei fehlt, kein Zugriff, Pfad ungueltig |
| `4` | Eingabedaten ungueltig | JSON fehlerhaft, kein Array, leer, Pflichtfelder fehlen |

Fehlermeldungen sollen nach einem einheitlichen Muster aufgebaut sein:

- Problem klar benennen
- betroffenen Pfad oder Eintrag nennen
- einen naechsten Schritt vorschlagen, sofern sinnvoll

Beispiel:

```text
error: compile_commands.json is empty: /path/to/compile_commands.json
hint: generate the compilation database before running cmake-xray analyze
```

**Ergebnis**: Exit-Codes und Fehlermeldungen sind konsistent, automatisiert pruefbar und in README oder Hilfe dokumentiert.

### 1.5 Konfigurierbare Eingabepfade vorbereiten

Phase 1 enthaelt auch konfigurierbare Pfade und Formatauswahl (`F-35`, `F-36`). Fuer M1 muss nicht das gesamte Konfigurationsziel abgeschlossen sein, aber die Struktur soll spaetere Erweiterungen nicht blockieren.

Fuer M1 sinnvoll:

- Pfad zur `compile_commands.json` explizit per CLI angeben koennen
- Standardpfad nur dann verwenden, wenn das Verhalten in Hilfe und Tests klar beschrieben ist
- CLI-Parsing so aufbauen, dass spaetere Optionen fuer Ausgabeformate erweiterbar bleiben

Nicht erforderlich fuer M1:

- Markdown-, HTML- oder JSON-Ausgaben
- umfassende Mehrfachkonfiguration

Entscheidung fuer M1:

- der explizite CLI-Pfad ist der Primaerpfad
- ein impliziter Standardpfad ist optional, aber kein Abschlusskriterium fuer M1
- falls ein Standardpfad unterstuetzt wird, muss `--help` das Verhalten eindeutig benennen

**Ergebnis**: Eingabepfade sind explizit steuerbar und die CLI bleibt fuer spaetere Optionen erweiterbar.

### 1.6 Test- und Referenzdaten fuer M1 ausbauen

M1 braucht gezielte Testfaelle fuer Adapter, CLI und Fehlerbehandlung. Neben Unit-Tests sollen erste End-to-End-Tests ueber die CLI dazukommen.

Mindestens benoetigt:

- Adapter-Tests fuer gueltige `compile_commands.json`
- Adapter-Tests fuer syntaktisch fehlerhafte JSON-Dateien
- Adapter-Tests fuer leere Arrays
- Adapter-Tests fuer unvollstaendige Eintraege
- CLI-Tests fuer `--help`
- CLI-Tests fuer Erfolgspfad und definierte Fehlerpfade

Sinnvolle Testdaten unter `tests/e2e/testdata/`:

- `compile_commands.valid.json`
- `compile_commands.empty.json`
- `compile_commands.invalid_syntax.json`
- `compile_commands.missing_fields.json`

Empfohlene Testaufteilung:

| Ebene | Fokus | Beispiel |
|---|---|---|
| Hexagon-Test | Modell und Statusauswertung ohne Dateisystem | Validierungsstatus fuer leer oder gueltig |
| Adapter-Test | JSON-Datei lesen, validieren, normalisieren | fehlendes `file`, fehlendes `directory`, nur `arguments` |
| CLI-Test | Hilfe, Fehlertexte, Exit-Codes | fehlender Pfad, ungueltige Datei, Erfolg |
| End-to-End-Test | Binary mit Referenzdateien | `analyze --compile-commands ...` gegen Testdaten |

Mindestens abzudeckende Testmatrix:

| Fall | Erwartung |
|---|---|
| gueltige Datei mit einem Eintrag | Exit `0`, Minimalergebnis oder Bestaetigung |
| Datei nicht vorhanden | Exit `3`, klare Fehlermeldung |
| syntaktisch fehlerhafte JSON-Datei | Exit `4`, Hinweis auf Syntaxfehler |
| JSON-Wurzel ist kein Array | Exit `4`, Hinweis auf erwartetes Array |
| leeres Array | Exit `4`, klare Rueckmeldung zu leerer Datenbasis |
| Eintrag ohne `file` | Exit `4`, Eintragsfehler benannt |
| Eintrag ohne `directory` | Exit `4`, Eintragsfehler benannt |
| Eintrag ohne `command` und ohne `arguments` | Exit `4`, Eintragsfehler benannt |
| `--help` fuer Hauptkommando | Exit `0`, Hilfetext auf `stdout` |
| `analyze --help` | Exit `0`, Hilfetext auf `stdout` |
| fehlendes Pflichtargument `--compile-commands` | Exit `2`, Nutzungsfehler |

**Ergebnis**: Die M1-Anforderungen sind nicht nur manuell, sondern automatisiert gegen Referenzdaten abgesichert.

### 1.7 README und Nutzungsbeispiele aktualisieren

Mit M1 wird die README vom M0-Platzhalterzustand auf echte CLI-Nutzung erweitert.

Die README soll fuer M1 mindestens enthalten:

- Aufruf des Hauptkommandos mit `--help`
- Aufruf des Analyse-Unterkommandos mit Pfadangabe
- erwartetes Verhalten bei gueltiger Eingabe
- erwartetes Verhalten bei leeren oder ungueltigen Eingaben
- kurze Dokumentation der Exit-Codes

**Ergebnis**: README und Beispielaufrufe spiegeln den tatsaechlichen M1-Stand wider.

## 2. Zielartefakte

Folgende Dateien oder Dateigruppen sollen nach M1 voraussichtlich neu entstehen oder substanziell ueberarbeitet sein:

| Bereich | Zielartefakte |
|---|---|
| Kernmodell | `src/hexagon/model/compile_entry.h`, erweiterte Status- oder Diagnosemodelle |
| Driven Port | `src/hexagon/ports/driven/compile_database_port.h` |
| Input-Adapter | `src/adapters/input/compile_commands_json_adapter.h`, `src/adapters/input/compile_commands_json_adapter.cpp` |
| CLI-Adapter | `src/adapters/cli/cli_adapter.h`, `src/adapters/cli/cli_adapter.cpp`, optional `src/adapters/cli/exit_codes.h` |
| Composition Root | `src/main.cpp` |
| Adapter-Tests | `tests/adapters/test_compile_commands_json.cpp` |
| CLI-/E2E-Tests | neue Tests unter `tests/e2e/` mit Referenzdaten unter `tests/e2e/testdata/` |
| Dokumentation | `README.md`, gegebenenfalls `CHANGELOG.md` bei Releaseabschluss |

Hinweis: Die konkreten Dateinamen duerfen von dieser Zielstruktur abweichen, wenn die hexagonale Trennung und die Rueckverfolgbarkeit zu `F-01` bis `F-04`, `F-31` bis `F-34` erhalten bleiben.

## 3. Reihenfolge

| Schritt | Arbeitspaket | Abhaengig von |
|---|---|---|
| 1 | 1.1 Compile-Database-Modell im Kern schaerfen | M0 |
| 2 | 1.2 `CompileCommandsJsonAdapter` umsetzen | 1 |
| 3 | 1.4 Exit-Codes und Fehlermeldungsregeln festlegen | 2 |
| 4 | 1.3 CLI-Grundstruktur fuer M1 aufbauen | 2, 3 |
| 5 | 1.5 Konfigurierbare Eingabepfade vorbereiten | 4 |
| 6 | 1.6 Test- und Referenzdaten fuer M1 ausbauen | 2, 3, 4, 5 |
| 7 | 1.7 README und Nutzungsbeispiele aktualisieren | 4, 5, 6 |

Schritte 3 und 4 koennen teilweise parallel vorbereitet werden, solange die Exit-Code-Regeln vor dem finalen CLI-Verhalten festgezogen werden.

## 4. Pruefung

M1 ist abgeschlossen, wenn folgende Pruefwege erfolgreich durchlaufen:

**Lokal**:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/cmake-xray --help
./build/cmake-xray analyze --help
./build/cmake-xray analyze --compile-commands tests/e2e/testdata/compile_commands.valid.json
./build/cmake-xray analyze --compile-commands tests/e2e/testdata/compile_commands.empty.json
cd build && ctest --output-on-failure
```

**Docker**:

```bash
docker build --target test -t cmake-xray:test .
docker build --target runtime -t cmake-xray .
docker run --rm cmake-xray --help
docker run --rm cmake-xray analyze --help
docker run --rm \
  -v "$PWD/tests/e2e/testdata:/data:ro" \
  cmake-xray analyze --compile-commands /data/compile_commands.valid.json
```

Hinweis: Falls das Runtime-Image bewusst schlank bleiben soll, werden Referenzdaten fuer den Docker-Pruefpfad per Bind-Mount bereitgestellt statt ins Image kopiert.

## 5. Rueckverfolgbarkeit

| Planbereich | Lastenheft-Kennungen |
|---|---|
| Compile-Database-Modell und Adapter | `F-01`, `F-02`, `F-03`, `F-04`, `F-41` |
| CLI-Struktur und Hilfe | `F-31`, `F-32`, `AK-09`, `NF-01` |
| Exit-Codes und Fehlermeldungen | `F-33`, `F-34`, `AK-02`, `NF-02`, `NF-14` |
| Pfadsteuerung | `F-35`, vorbereitend `F-36` |
| Tests und Referenzdaten | `AK-01`, `AK-02`, `AK-09`, `NF-10` |

## 6. Offene Punkte fuer die Umsetzung

- Welche konkrete Exit-Code-Mapping-Tabelle wird fuer M1 festgelegt?
- Soll `command` und `arguments` gleichwertig unterstuetzt werden, oder wird eines intern bevorzugt normalisiert?
- Soll ein Standardpfad fuer `compile_commands.json` bereits in M1 aktiv sein oder erst spaeter?
- Welches minimale Erfolgsverhalten ist fuer gueltige Eingaben sinnvoll, solange die eigentliche Analyse erst in M2 folgt?
