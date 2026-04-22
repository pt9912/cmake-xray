# Plan M1 - MVP-Eingaben und CLI (`v0.2.0`)

## 0. Dokumenteninformationen

| Feld       | Wert                                                                                                                                                                   |
| ---------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Dokument   | Plan M1 `cmake-xray`                                                                                                                                                   |
| Version    | `0.6`                                                                                                                                                                  |
| Stand      | `2026-04-22`                                                                                                                                                           |
| Status     | Abgeschlossen                                                                                                                                                          |
| Referenzen | [Lastenheft](./lastenheft.md), [Design](./design.md), [Architektur](./architecture.md), [Phasenplan](./roadmap.md), [Plan M0](./plan-M0.md), [Qualitaet](./quality.md) |

### 0.1 Zweck
Dieser Plan beschreibt die konkreten Schritte fuer Milestone M1 (`v0.2.0`). Ziel ist ein nutzbares CLI-Grundgeruest, das eine `compile_commands.json` kontrolliert einliest, Fehlerfaelle verstaendlich behandelt und den spaeteren Analysekern sauber ueber Ports ansteuert.

### 0.2 Abschlusskriterium
M1 gilt als erreicht, wenn:

- eine gueltige `compile_commands.json` erfolgreich eingelesen und in ein internes Eingabemodell ueberfuehrt wird
- eine ungueltige, leere oder unvollstaendige `compile_commands.json` mit klarer Meldung und definiertem Exit-Code behandelt wird
- `--help` fuer das Hauptkommando und mindestens ein Unterkommando verstaendliche Hilfe ausgibt
- die CLI fuer Erfolg und Fehlerfaelle nachvollziehbare Exit-Codes verwendet
- Adapter-, Hexagon- und End-to-End-Tests die M1-Faelle automatisiert absichern

Relevante Kennungen: `F-01`, `F-02`, `F-03`, `F-04`, `F-31`, `F-32`, `F-33`, `F-34`, `F-35`, `F-36` (vorbereitend), `F-41`, `NF-01`, `NF-02`, `NF-10`, `NF-14`, `AK-01`, `AK-02`, `AK-09`

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
- Diagnosen so modellieren, dass der CLI-Adapter daraus Meldungen und Exit-Codes ableiten kann — das Diagnosemodell wird entweder als eigener Typ (z.B. `compile_database_diagnostic.h`) oder als Erweiterung von `compile_database_status.h` umgesetzt und muss mindestens eine Fehlerkategorie (z.B. Enum), eine optionale Fehlerbeschreibung und bei Eintragsfehler die betroffenen Indizes transportieren koennen

Wichtig:

- JSON-Typen duerfen nicht in oeffentlichen Hexagon-Schnittstellen auftauchen
- das Modell soll fuer spaetere Analysen in Phase 2 weiterverwendbar sein; Modelltypen sollen als Value Objects (immutable nach Konstruktion) entworfen werden, damit sie in spaeteren Milestones ohne Anpassung auch in parallelen Kontexten nutzbar sind
- die Zuordnung von Kerndiagnosen zu konkreten Exit-Codes ist Aufgabe des CLI-Adapters, nicht des Hexagons — das Kernmodell liefert fachliche Fehlerkategorien, der Adapter uebersetzt sie in Exit-Codes
- optionale Felder wie `output` werden in M1 nicht im Modell `compile_entry.h` gefuehrt und koennen in spaeteren Milestones bei Bedarf ergaenzt werden

#### Port-Anpassungen

Die bestehenden Ports genuegen dem M1-Scope nicht und muessen erweitert werden:

**Driven Port** (`CompileDatabasePort`): Die Methode `load_compile_database()` gibt bisher nur `CompileDatabaseStatus{bool}` zurueck. Fuer M1 muss der Rueckgabetyp die geladenen Compile-Eintraege, den Ladestatus und gegebenenfalls Diagnoseinformationen enthalten. Die Pfaduebergabe an den Adapter — per Konstruktor oder Methodenparameter — bleibt der Umsetzung ueberlassen. Der Adapter muss entsprechend angepasst werden; bestehende Tests fuer den `ProjectAnalyzer` aendern sich ebenfalls.

**Driving Port** (`AnalyzeProjectPort`): Die Methode `analyze_project()` nimmt bisher keine Parameter entgegen. Fuer M1 muss der Pfad zur `compile_commands.json` von der CLI durch den Driving Port bis zum Service durchgereicht werden koennen. Die konkrete Loesung — Parameter an `analyze_project()`, Konfigurationsobjekt oder Setter — bleibt der Umsetzung ueberlassen, solange der Pfad nicht fest verdrahtet wird.

Vorgesehene Artefakte:

- neues Modell fuer Compile-Eintraege unter `src/hexagon/model/`
- Erweiterung oder Ersatz von `compile_database_status.h`, damit neben "vorhanden" auch Fehlergrund und Diagnose transportiert werden koennen
- Anpassung von `compile_database_port.h` (Driven Port): fachlich nutzbare Daten statt boolescher Platzhalter
- Anpassung von `analyze_project_port.h` (Driving Port): Pfaddurchreichung ermoeglichen
- Folgeaenderung in `ProjectAnalyzer`: Der Service nimmt kuenftig den Pfad ueber den Driving Port entgegen, reicht ihn an den Driven Port weiter und wertet das Ergebnis (Eintraege, Diagnosen) aus. Die bisherige `analyze_project()`-Implementierung aendert sich substanziell; bestehende Hexagon-Tests muessen auf die neuen Signaturen und Rueckgabetypen angepasst werden

**Ergebnis**: Der Kern enthaelt ein belastbares Eingabemodell fuer Compile-Eintraege und deren Validierungsstatus. Beide Port-Richtungen sind fuer den M1-Durchstich vorbereitet.

### 1.2 `CompileCommandsJsonAdapter` umsetzen

Der bisherige Platzhalter-Adapter in `src/adapters/input/` wird durch einen echten JSON-Adapter ersetzt oder entsprechend weiterentwickelt.

Der Adapter soll mindestens:

- eine Datei `compile_commands.json` von einem konfigurierbaren Pfad laden
- syntaktische JSON-Fehler erkennen
- sicherstellen, dass die Wurzel ein Array ist
- eine leere, aber syntaktisch gueltige Datei gezielt erkennen (`F-41`)
- pro Eintrag die Pflichtinformationen validieren
- unvollstaendige oder unbrauchbare Eintraege mit klarer Rueckmeldung behandeln
- bei gemischt gueltigen und ungueltigen Eintraegen alle Fehler sammeln und zusammen melden, nicht beim ersten Fehler abbrechen — enthaelt die Datei mindestens einen ungueltigen Eintrag, wird die gesamte Datenbasis abgelehnt (kein Teilerfolg)
- ueberschreitet die Anzahl gesammelter Eintragsfehler eine Obergrenze, wird die Ausgabe nach dieser Grenze abgeschnitten und die Gesamtzahl angezeigt, zum Beispiel: `... and 137 more invalid entries` — die Obergrenze wird als benannte Konstante im Adapter festgelegt (Richtwert: 20), ist fuer M1 nicht per CLI konfigurierbar

Fuer M1 genuegt DOM-Parsing mit nlohmann/json ohne Groessenlimit. SAX-Parsing oder Eingabebegrenzung koennen in spaeteren Phasen ergaenzt werden, wenn reale Projekte das erfordern.

Als Mindestvalidierung fuer einen verwertbaren Eintrag gelten:

- Quelldatei vorhanden und nicht leer
- Arbeitsverzeichnis vorhanden und nicht leer
- Compile-Aufruf ueber `command` oder `arguments` vorhanden und nicht leer, gemaess folgender Prioritaetskette:
  1. `arguments` vorhanden und nicht leer → verwenden, `command` ignorieren
  2. `arguments` leer (`[]`) oder fehlend, `command` vorhanden und nicht leer → `command` verwenden
  3. weder verwertbares `arguments` noch verwertbares `command` vorhanden → Eintrag ungueltig
- ein vorhandenes, aber leeres `arguments`-Array (`[]`) gilt als "nicht vorhanden" (siehe Prioritaetskette oben)
- Sonderfall `arguments` mit ausschliesslich leeren Strings (z.B. `[""]`): gilt fuer M1 als nicht leer und wird akzeptiert — eine tiefergehende Inhaltsvalidierung einzelner Argumente ist nicht Bestandteil von M1

Optionale Felder wie `output` werden fuer M1 nicht ausgewertet und bei der Validierung ignoriert.

Nicht Bestandteil der M1-Validierung:

- Splitting oder Parsing des `command`-Strings in einzelne Argumente — der Wert wird als Ganzes gespeichert
- Pfadaufloesung oder Normalisierung von `file` und `directory` — geprueft wird nur, dass die Felder vorhanden und nicht leer sind
- Deduplizierung mehrfach vorkommender Quelldateien (z.B. bei Multi-Config-Builds) — Duplikate werden akzeptiert und unveraendert uebernommen

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
- Anpassung von `src/adapters/CMakeLists.txt` (Quelldateien des Input-Adapters umbenennen)

**Ergebnis**: Eine gueltige `compile_commands.json` wird in ein internes Modell ueberfuehrt; ungueltige Eingaben liefern kontrollierte Fehler.

### 1.3 CLI-Grundstruktur fuer M1 aufbauen

Der CLI-Adapter soll von der Platzhalterausgabe zu einer nachvollziehbaren Befehlsstruktur wechseln. Die Umsetzung nutzt CLI11, das bereits als Abhaengigkeit eingebunden ist. Die Benennung soll sich am Nutzerziel orientieren, nicht an internen Klassen.

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
- CLI11-Parse-Fehler (z.B. unbekannte Option, fehlendes Pflichtargument) abfangen und auf den eigenen Exit-Code `2` ummappen, da CLI11 intern andere Codes verwendet
- Fehlermeldungen auf `stderr`, regulare Ergebnis- oder Hilfetexte auf `stdout`

M1 muss noch keine fachliche Analyse aus Phase 2 liefern. Bei gueltigen Eingaben gibt die CLI eine Bestaetigung mit der Anzahl geladener Eintraege aus, zum Beispiel:

```text
compile database loaded: 42 entries
```

Das Format ist: `compile database loaded: <n> entries` — Plural wird auch bei `1 entries` beibehalten, um die Ausgabe einfach und testbar zu halten. Der Pfad zur Datei wird in der Erfolgsausgabe nicht wiederholt, da er bereits im Aufruf sichtbar ist.

Damit ist der gesamte Pfad von CLI ueber Driving Port bis zum Adapter nachweisbar verdrahtet, ohne dass fachliche Analyseergebnisse vorliegen muessen. Diese Minimalausgabe ist bewusst temporaer und wird in M2 durch echte Analyseergebnisse ersetzt; Tests, die auf diesen exakten String matchen, muessen dann angepasst werden.

Vorgesehene CLI-Semantik fuer M1:

- `cmake-xray --help`: globale Hilfe und Kurzbeschreibung
- `cmake-xray analyze --help`: Hilfe fuer den Analyse-Einstieg
- `cmake-xray analyze --compile-commands <path>`: Validierung und Minimaldurchlauf fuer eine Projektanalyse

Der bisherige `PlaceholderCliAdapter` haengt neben `AnalyzeProjectPort` auch von `GenerateReportPort` ab. Da M1 keine fachliche Report-Erzeugung vorsieht, entfaellt die `GenerateReportPort`-Abhaengigkeit im neuen CLI-Adapter. Der `ReportGenerator`-Service und der `PlaceholderReportAdapter` bleiben im Code bestehen, werden aber vom M1-CLI-Pfad nicht angesteuert und koennen in einem spaeteren Milestone wieder eingebunden werden.

Vorgesehene Artefakte:

- Ersatz oder Umbenennung des Platzhalter-Adapters `src/adapters/cli/placeholder_cli_adapter.*`
- separates Header- oder Konstantenfile fuer Exit-Codes, zum Beispiel `src/adapters/cli/exit_codes.h`
- Anpassung von `src/main.cpp` als Composition Root fuer den M1-CLI-Pfad
- Anpassung von `src/adapters/CMakeLists.txt` (Quelldateien des CLI-Adapters umbenennen)

**Ergebnis**: Die CLI ist fuer echte Eingabedateien nutzbar und dokumentiert die kuenftige Befehlsstruktur.

### 1.4 Exit-Codes und Fehlermeldungsregeln festlegen

M1 soll nicht nur "erfolgreich" oder "fehlgeschlagen" unterscheiden. Der CLI-Adapter benoetigt nachvollziehbare, stabile Regeln fuer Rueckgabecodes.

Die Zuordnung von fachlichen Fehlerkategorien (aus dem Kernmodell) zu konkreten Exit-Codes liegt ausschliesslich im CLI-Adapter. Das Hexagon definiert Diagnosetypen, der Adapter uebersetzt sie in numerische Codes. Damit bleibt die hexagonale Trennung gewahrt und andere Adapter (z.B. ein kuenftiger Language-Server) koennen dieselben Diagnosen anders darstellen.

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

| Code | Bedeutung                 | Typischer Ausloeser                                                                       |
| ---- | ------------------------- | ----------------------------------------------------------------------------------------- |
| `0`  | Erfolg                    | gueltige CLI-Verwendung und gueltige Eingabedaten                                         |
| `1`  | (reserviert)              | unerwartete Laufzeitfehler — wird in M1 nicht aktiv vergeben, bleibt als Auffangcode frei |
| `2`  | CLI-Verwendungsfehler     | unbekanntes Unterkommando, fehlendes Pflichtargument                                      |
| `3`  | Eingabedatei nicht lesbar | Datei fehlt, kein Zugriff, Pfad ungueltig                                                 |
| `4`  | Eingabedaten ungueltig    | JSON fehlerhaft, kein Array, leer, Pflichtfelder fehlen                                   |

Fehlermeldungen sollen nach einem einheitlichen Muster aufgebaut sein:

- Problem klar benennen
- betroffenen Pfad oder Eintrag nennen
- einen naechsten Schritt vorschlagen, sofern sinnvoll

Beispiel fuer einen Dateizugriffsfehler:

```text
error: cannot open compile_commands.json: /nonexistent/compile_commands.json
hint: check the path or generate the compilation database before running cmake-xray analyze
```

Beispiel fuer eine leere Datenbasis:

```text
error: compile_commands.json is empty: /path/to/compile_commands.json
hint: generate the compilation database before running cmake-xray analyze
```

Beispiel fuer mehrere gesammelte Eintragsfehler:

```text
error: compile_commands.json contains 3 invalid entries: /path/to/compile_commands.json
  entry 2: missing "file" field
  entry 5: missing "directory" field
  entry 7: missing "command" and "arguments"
hint: fix or regenerate the compilation database before running cmake-xray analyze
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
- Hinweis: Wird in einem spaeteren Milestone ein Standardpfad eingefuehrt, aendert sich `--compile-commands` von einem Pflichtargument zu einem optionalen Argument. Dieses Breaking Change ist akzeptiert und wird im jeweiligen Milestone-CHANGELOG dokumentiert

**Ergebnis**: Eingabepfade sind explizit steuerbar und die CLI bleibt fuer spaetere Optionen erweiterbar.

### 1.6 Test- und Referenzdaten fuer M1 ausbauen

M1 braucht gezielte Testfaelle fuer Adapter, CLI und Fehlerbehandlung. Neben Unit-Tests sollen erste End-to-End-Tests ueber die CLI dazukommen. Dieses Arbeitspaket gliedert sich in zwei Teilpakete, die unabhaengig voneinander begonnen werden koennen (siehe Reihenfolge in Sektion 3):

#### 1.6a Adapter-Tests

Mindestens benoetigt:

- Adapter-Tests fuer gueltige `compile_commands.json`
- Adapter-Tests fuer syntaktisch fehlerhafte JSON-Dateien
- Adapter-Tests fuer leere Arrays
- Adapter-Tests fuer Wurzel ist kein Array
- Adapter-Tests fuer unvollstaendige Eintraege

#### 1.6b CLI- und End-to-End-Tests

Mindestens benoetigt:

- CLI-Tests fuer `--help`
- CLI-Tests fuer Erfolgspfad und definierte Fehlerpfade
- End-to-End-Tests gegen Referenzdateien

Nicht automatisiert abgedeckt in M1:

- Dateiberechtigungsfehler (Permission Denied) — faellt unter Exit-Code `3`, ist aber in Container-Umgebungen schwer reproduzierbar
- Pfad zeigt auf ein Verzeichnis oder eine andere nicht-regulaere Datei (z.B. `/dev/null`) — faellt ebenfalls unter Exit-Code `3`, wird aber fuer M1 nicht als Pflicht-Testfall gefuehrt

Beide Faelle werden vom Adapter ueber den regulaeren Dateizugriffsfehler behandelt und muessen nicht gesondert erkannt werden.

#### Testdaten

Sinnvolle Testdaten unter `tests/e2e/testdata/`. Jeder Testfall erhaelt ein eigenes Unterverzeichnis, damit die Datei den realen Namen `compile_commands.json` traegt:

- `valid/compile_commands.json`
- `empty/compile_commands.json`
- `invalid_syntax/compile_commands.json`
- `not_an_array/compile_commands.json`
- `missing_fields/compile_commands.json`
- `only_command/compile_commands.json` — Eintrag nur mit `command`, ohne `arguments`
- `command_and_arguments/compile_commands.json` — Eintrag mit beidem, `arguments` wird bevorzugt
- `empty_arguments_with_command/compile_commands.json` — `arguments: []` mit gueltigem `command`
- `mixed_valid_invalid/compile_commands.json` — Mischung aus gueltigen und ungueltigen Eintraegen

Adapter-Tests fuer feinere Randfaelle (z.B. `arguments: [""]`, leeres `arguments` ohne `command`) koennen mit inline konstruiertem JSON arbeiten und benoetigen keine eigenen Testdatenverzeichnisse.

#### Empfohlene Testaufteilung

| Ebene           | Fokus                                        | Beispiel                                                 |
| --------------- | -------------------------------------------- | -------------------------------------------------------- |
| Hexagon-Test    | Modell und Statusauswertung ohne Dateisystem | Validierungsstatus fuer leer oder gueltig                |
| Adapter-Test    | JSON-Datei lesen, validieren, normalisieren  | fehlendes `file`, fehlendes `directory`, nur `arguments` |
| CLI-Test        | Hilfe, Fehlertexte, Exit-Codes               | fehlender Pfad, ungueltige Datei, Erfolg                 |
| End-to-End-Test | Binary mit Referenzdateien                   | `analyze --compile-commands ...` gegen Testdaten         |

#### Testmatrix

Mindestens abzudeckende Testmatrix:

| Fall                                                          | Erwartung                                                         |
| ------------------------------------------------------------- | ----------------------------------------------------------------- |
| gueltiger Compile-Eintrag (Hexagon)                           | Modell enthaelt Quelldatei, Arbeitsverzeichnis und Compile-Aufruf |
| leere Eingabeliste (Hexagon)                                  | Diagnosestatus zeigt leere Datenbasis an                          |
| Eingabeliste mit Fehlern (Hexagon)                            | Diagnosestatus enthaelt Fehlerkategorie und betroffene Eintraege  |
| gueltige Datei mit einem Eintrag                              | Exit `0`, Ausgabe der Anzahl geladener Eintraege                  |
| Datei nicht vorhanden                                         | Exit `3`, klare Fehlermeldung                                     |
| syntaktisch fehlerhafte JSON-Datei                            | Exit `4`, Hinweis auf Syntaxfehler                                |
| JSON-Wurzel ist kein Array                                    | Exit `4`, Hinweis auf erwartetes Array                            |
| leeres Array                                                  | Exit `4`, klare Rueckmeldung zu leerer Datenbasis                 |
| Eintrag ohne `file`                                           | Exit `4`, Eintragsfehler benannt                                  |
| Eintrag ohne `directory`                                      | Exit `4`, Eintragsfehler benannt                                  |
| Eintrag ohne `command` und ohne `arguments`                   | Exit `4`, Eintragsfehler benannt                                  |
| Eintrag nur mit `command`, kein `arguments`                   | Exit `0`, `command` wird verwendet                                |
| Eintrag mit `command` und `arguments`                         | Exit `0`, `arguments` bevorzugt                                   |
| Eintrag mit leerem `arguments` (`[]`) und gueltigem `command` | Exit `0`, Fallback auf `command`                                  |
| Eintrag mit leerem `arguments` (`[]`) und ohne `command`      | Exit `4`, Eintragsfehler benannt                                  |
| gemischt gueltige und ungueltige Eintraege                    | Exit `4`, alle Fehler gesammelt, Datenbasis abgelehnt             |
| `--help` fuer Hauptkommando                                   | Exit `0`, Hilfetext auf `stdout`                                  |
| `analyze --help`                                              | Exit `0`, Hilfetext auf `stdout`                                  |
| fehlendes Pflichtargument `--compile-commands`                | Exit `2`, Nutzungsfehler                                          |

**Ergebnis**: Die M1-Anforderungen sind nicht nur manuell, sondern automatisiert gegen Referenzdaten abgesichert.

### 1.7 README und Nutzungsbeispiele aktualisieren

Mit M1 wird die README vom M0-Platzhalterzustand auf echte CLI-Nutzung erweitert.

Die README soll fuer M1 mindestens enthalten:

- Aufruf des Hauptkommandos mit `--help`
- Aufruf des Analyse-Unterkommandos mit Pfadangabe
- erwartetes Verhalten bei gueltiger Eingabe
- erwartetes Verhalten bei leeren oder ungueltigen Eingaben
- kurze Dokumentation der Exit-Codes

Zusaetzlich ist die Versionsnummer in `application_info.h` und in der Root-`CMakeLists.txt` auf `0.2.0` zu aktualisieren.

**Ergebnis**: README und Beispielaufrufe spiegeln den tatsaechlichen M1-Stand wider.

## 2. Zielartefakte

Folgende Dateien oder Dateigruppen sollen nach M1 voraussichtlich neu entstehen oder substanziell ueberarbeitet sein:

| Bereich             | Zielartefakte                                                                                                  |
| ------------------- | -------------------------------------------------------------------------------------------------------------- |
| Kernmodell          | `src/hexagon/model/compile_entry.h`, erweiterte Status- oder Diagnosemodelle                                   |
| Driven Port         | `src/hexagon/ports/driven/compile_database_port.h`                                                             |
| Driving Port        | `src/hexagon/ports/driving/analyze_project_port.h`                                                             |
| Input-Adapter       | `src/adapters/input/compile_commands_json_adapter.h`, `src/adapters/input/compile_commands_json_adapter.cpp`   |
| CLI-Adapter         | `src/adapters/cli/cli_adapter.h`, `src/adapters/cli/cli_adapter.cpp`, optional `src/adapters/cli/exit_codes.h` |
| Composition Root    | `src/main.cpp`                                                                                                 |
| Version             | `src/hexagon/model/application_info.h` — Versionsnummer auf `v0.2.0` aktualisieren                             |
| Version             | Root-`CMakeLists.txt` — Projektversionsnummer auf `0.2.0` aktualisieren                                        |
| Build-Konfiguration | `src/adapters/CMakeLists.txt` — Quelldateien fuer umbenannte Adapter anpassen                                  |
| Build-Konfiguration | `tests/CMakeLists.txt` — neue Testdateien und E2E-Targets aufnehmen                                            |
| Adapter-Tests       | `tests/adapters/test_compile_commands_json.cpp`                                                                |
| CLI-/E2E-Tests      | neue Tests unter `tests/e2e/` mit Referenzdaten unter `tests/e2e/testdata/`                                    |
| Dokumentation       | `README.md`, gegebenenfalls `CHANGELOG.md` bei Releaseabschluss                                                |

Hinweis: Die konkreten Dateinamen duerfen von dieser Zielstruktur abweichen, wenn die hexagonale Trennung und die Rueckverfolgbarkeit zu `F-01` bis `F-04`, `F-31` bis `F-34` erhalten bleiben.

## 3. Reihenfolge

| Schritt | Arbeitspaket                                              | Abhaengig von |
| ------- | --------------------------------------------------------- | ------------- |
| 1a      | 1.1 Compile-Database-Modell im Kern schaerfen             | M0            |
| 1b      | 1.4 Exit-Codes und Fehlermeldungsregeln festlegen         | M0            |
| 2a      | 1.2 `CompileCommandsJsonAdapter` umsetzen                 | 1a            |
| 2b      | 1.6a Adapter-Tests fuer M1                                | 2a            |
| 3       | 1.3 CLI-Grundstruktur fuer M1 aufbauen                    | 1a, 1b, 2a    |
| 4       | 1.5 Konfigurierbare Eingabepfade vorbereiten              | 3             |
| 5       | 1.6b CLI- und End-to-End-Tests fuer M1                    | 3, 4          |
| 6       | 1.7 README, CHANGELOG und Nutzungsbeispiele aktualisieren | 3, 4, 5       |

Schritte 1a und 1b koennen parallel bearbeitet werden. Die Exit-Code-Regeln (1b) sind eine reine Konventionsentscheidung ohne Code-Abhaengigkeit zum Kernmodell und sollten frueh feststehen, damit Adapter und CLI sie von Anfang an korrekt verwenden. Schritte 2a und 2b koennen ebenfalls parallel bearbeitet werden, sofern der Adapter frueher fertig ist; Adapter-Tests (2b) koennen direkt nach dem Adapter geschrieben werden, ohne auf die CLI zu warten. CLI- und End-to-End-Tests (5) setzen die fertige CLI voraus.

## 4. Pruefung

M1 ist abgeschlossen, wenn folgende Pruefwege erfolgreich durchlaufen:

**Lokal**:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/cmake-xray --help                                                          # exit 0
./build/cmake-xray analyze --help                                                  # exit 0
./build/cmake-xray analyze --compile-commands tests/e2e/testdata/valid/compile_commands.json           # exit 0
./build/cmake-xray analyze --compile-commands tests/e2e/testdata/empty/compile_commands.json           # exit 4
./build/cmake-xray analyze --compile-commands tests/e2e/testdata/invalid_syntax/compile_commands.json  # exit 4
./build/cmake-xray analyze --compile-commands tests/e2e/testdata/not_an_array/compile_commands.json    # exit 4
./build/cmake-xray analyze --compile-commands tests/e2e/testdata/missing_fields/compile_commands.json  # exit 4
./build/cmake-xray analyze --compile-commands /nonexistent/compile_commands.json                       # exit 3
ctest --test-dir build --output-on-failure
```

**Docker**:

```bash
docker build --target test -t cmake-xray:test .
docker build --target runtime -t cmake-xray .
docker run --rm cmake-xray --help; echo "exit: $?"                                  # erwarte 0
docker run --rm cmake-xray analyze --help; echo "exit: $?"                          # erwarte 0
docker run --rm \
  -v "$PWD/tests/e2e/testdata:/data:ro" \
  cmake-xray analyze --compile-commands /data/valid/compile_commands.json; echo "exit: $?"            # erwarte 0
docker run --rm \
  -v "$PWD/tests/e2e/testdata:/data:ro" \
  cmake-xray analyze --compile-commands /data/empty/compile_commands.json; echo "exit: $?"            # erwarte 4
docker run --rm \
  -v "$PWD/tests/e2e/testdata:/data:ro" \
  cmake-xray analyze --compile-commands /data/invalid_syntax/compile_commands.json; echo "exit: $?"   # erwarte 4
docker run --rm \
  -v "$PWD/tests/e2e/testdata:/data:ro" \
  cmake-xray analyze --compile-commands /data/not_an_array/compile_commands.json; echo "exit: $?"     # erwarte 4
docker run --rm \
  -v "$PWD/tests/e2e/testdata:/data:ro" \
  cmake-xray analyze --compile-commands /data/missing_fields/compile_commands.json; echo "exit: $?"   # erwarte 4
docker run --rm cmake-xray analyze --compile-commands /nonexistent/compile_commands.json; echo "exit: $?"  # erwarte 3
```

Hinweis: Falls das Runtime-Image bewusst schlank bleiben soll, werden Referenzdaten fuer den Docker-Pruefpfad per Bind-Mount bereitgestellt statt ins Image kopiert. Der Test gegen `/nonexistent/...` prueft den Fehlerpfad ohne Bind-Mount.

## 5. Rueckverfolgbarkeit

| Planbereich                         | Lastenheft-Kennungen                      |
| ----------------------------------- | ----------------------------------------- |
| Compile-Database-Modell und Adapter | `F-01`, `F-02`, `F-03`, `F-04`, `F-41`    |
| CLI-Struktur und Hilfe              | `F-31`, `F-32`, `AK-09`, `NF-01`          |
| Exit-Codes und Fehlermeldungen      | `F-33`, `F-34`, `AK-02`, `NF-02`, `NF-14` |
| Pfadsteuerung                       | `F-35`, vorbereitend `F-36`               |
| Tests und Referenzdaten             | `AK-01`, `AK-02`, `AK-09`, `NF-10`        |

## 6. Entschiedene und offene Punkte

### 6.1 Entschieden

| Frage                                  | Entscheidung                                                                                                                                                                                                                                                                                                       | Verweis     |
| -------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ----------- |
| Exit-Code-Mapping                      | `0`/`2`/`3`/`4` gemaess Tabelle in AP 1.4                                                                                                                                                                                                                                                                          | AP 1.4      |
| `command` vs. `arguments`              | Beide akzeptiert; `arguments` bevorzugt wenn beide vorhanden, `command` als Fallback — kein Hinweis bei Koexistenz                                                                                                                                                                                                 | AP 1.2      |
| Standardpfad in M1                     | Optional, kein Abschlusskriterium — kuenftiges Breaking Change (`--compile-commands` wird optional) ist akzeptiert                                                                                                                                                                                                 | AP 1.5      |
| Minimalverhalten bei Erfolg            | Ausgabe der Anzahl geladener Eintraege (`entries`, nicht `translation units` — eine TU kann mehrere Eintraege haben, und umgekehrt koennen Duplikate denselben Quellpfad mehrfach beschreiben, siehe Entscheidung "Duplikate in Eingabedaten"); Format `compile database loaded: <n> entries`, Plural auch bei n=1 | AP 1.3      |
| Diagnose-zu-Exit-Code-Zuordnung        | Liegt im CLI-Adapter, nicht im Hexagon                                                                                                                                                                                                                                                                             | AP 1.1, 1.4 |
| JSON-Parsing-Strategie                 | DOM-Parsing mit nlohmann/json, kein Groessenlimit fuer M1                                                                                                                                                                                                                                                          | AP 1.2      |
| Gemischt gueltige/ungueltige Eintraege | Alle Fehler sammeln und zusammen auf `stderr` melden, Exit `4` — gesamte Datenbasis wird abgelehnt (kein Teilerfolg), vermeidet Korrekturschleifen bei systematischen Problemen                                                                                                                                    | AP 1.2, 1.4 |
| Leeres `arguments`-Array               | Gilt als "nicht vorhanden"; ist zusaetzlich kein `command` gesetzt, ist der Eintrag ungueltig                                                                                                                                                                                                                      | AP 1.2      |
| Optionale Felder (`output`)            | Werden fuer M1 weder ausgewertet noch im Modell `compile_entry.h` gefuehrt; koennen in spaeteren Milestones bei Bedarf ergaenzt werden                                                                                                                                                                             | AP 1.1, 1.2 |
| Exit-Code `1`                          | Reserviert fuer unerwartete Laufzeitfehler, wird in M1 nicht aktiv vergeben                                                                                                                                                                                                                                        | AP 1.4      |
| CLI11-Exit-Code-Mapping                | CLI11-Parse-Fehler werden abgefangen und auf Exit-Code `2` gemappt                                                                                                                                                                                                                                                 | AP 1.3      |
| Fehlersammlungs-Obergrenze             | Ab 20 gesammelten Eintragsfehler wird die Ausgabe abgeschnitten, Gesamtzahl angezeigt — als benannte Konstante im Adapter, fuer M1 nicht per CLI konfigurierbar                                                                                                                                                    | AP 1.2      |
| `arguments` mit leeren Strings         | `[""]` gilt als nicht leer und wird akzeptiert; tiefere Inhaltsvalidierung nicht in M1                                                                                                                                                                                                                             | AP 1.2      |
| `GenerateReportPort` in M1             | Nicht vom CLI-Adapter angesteuert; Service und Adapter bleiben im Code, werden spaeter wieder eingebunden                                                                                                                                                                                                          | AP 1.3      |
| Minimalausgabe bei Erfolg              | Bewusst temporaer; wird in M2 durch echte Analyseergebnisse ersetzt, betroffene Tests muessen dann angepasst werden                                                                                                                                                                                                | AP 1.3      |
| `command`-Splitting                    | Der `command`-String wird als Ganzes gespeichert; Splitting in einzelne Argumente ist nicht Bestandteil von M1                                                                                                                                                                                                     | AP 1.2      |
| Pfadnormalisierung                     | `file` und `directory` werden nicht aufgeloest oder normalisiert; geprueft wird nur Vorhandensein und Nicht-Leer-Sein                                                                                                                                                                                              | AP 1.2      |
| Duplikate in Eingabedaten              | Mehrfach vorkommende Quelldateien (z.B. Multi-Config-Builds) werden akzeptiert und unveraendert uebernommen — keine Deduplizierung in M1                                                                                                                                                                           | AP 1.2      |

### 6.2 Offen

Keine offenen Punkte fuer M1.
