# Plan M2 - Kernanalysen MVP (`v0.3.0`)

## 0. Dokumenteninformationen

| Feld       | Wert                                                                                                                                                                   |
| ---------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Dokument   | Plan M2 `cmake-xray`                                                                                                                                                   |
| Version    | `0.3`                                                                                                                                                                  |
| Stand      | `2026-04-22`                                                                                                                                                           |
| Status     | Abgeschlossen                                                                                                                                                          |
| Referenzen | [Lastenheft](./lastenheft.md), [Design](./design.md), [Architektur](./architecture.md), [Phasenplan](./roadmap.md), [Plan M1](./plan-M1.md), [Qualitaet](./quality.md) |

### 0.1 Zweck
Dieser Plan beschreibt die konkreten Schritte fuer Milestone M2 (`v0.3.0`). Ziel ist der erste fachlich nutzbare Analysepfad: `cmake-xray` soll auffaellige Translation Units rangfolgebasiert ausgeben, Include-Hotspots aus einer dokumentierten MVP-Heuristik ableiten und eine dateibasierte Impact-Analyse fuer Translation Units bereitstellen.

### 0.2 Abschlusskriterium
M2 gilt als erreicht, wenn:

- `cmake-xray analyze --compile-commands <path>` eine nachvollziehbare Rangfolge auffaelliger Translation Units inklusive der verwendeten Kennzahlen ausgibt
- dieselbe Projektanalyse Include-Hotspots mit Header-Bezeichnung, Anzahl betroffener Translation Units und einer sichtbaren Zuordnung zu betroffenen Translation Units ausgibt
- `cmake-xray impact --compile-commands <path> --changed-file <path>` fuer eine Datei betroffene Translation Units oder einen klaren Hinweis auf fehlende bzw. unzureichende Daten ausgibt
- Ergebnisse, die auf der Source-Parsing-Include-Heuristik beruhen, inline als heuristisch gekennzeichnet werden
- die Sortierung und Begrenzung der Ergebnislisten reproduzierbar und automatisiert abgesichert ist
- Adapter-, Kern-, Reporter- und End-to-End-Tests die M2-Pfade automatisiert absichern

Relevante Kennungen: `F-06`, `F-07`, `F-08`, `F-09`, `F-12`, `F-13`, `F-14`, `F-15`, `F-21`, `F-22`, `F-23`, `F-26`, `F-31`, `F-32`, `F-42`, `NF-03`, `NF-10`, `NF-14`, `NF-15`, `AK-03`, `AK-04`, `AK-05`; zusaetzlich wird der Eingabepfad-Teilaspekt aus `F-35` fuer `--compile-commands` und `--changed-file` mit abgedeckt

### 0.3 Abgrenzung
Bestandteil von M2 sind:

- Ergebnismodelle fuer Translation-Unit-Ranking, Include-Hotspots, Impact-Ergebnisse und Diagnostics
- Berechnung der Kennzahlen `arg_count`, `include_path_count` und `define_count`
- reproduzierbare Rangbildung und konfigurierbare Begrenzung der Konsolenausgabe
- ein MVP-`IncludeResolverPort` mit `SourceParsingIncludeAdapter`
- heuristische Include-Hotspot-Analyse und dateibasierte Impact-Analyse auf TU-Ebene
- ein echter `ConsoleReportAdapter` fuer Projekt- und Impact-Ausgaben
- CLI-Erweiterung um den Impact-Pfad sowie aktualisierte Hilfe- und Nutzungsbeispiele
- Eingabepfade fuer `--compile-commands` und `--changed-file` als Teilaspekt von `F-35`
- Referenzdaten und automatisierte Tests fuer Ranking, Hotspots, Impact und Heuristik-Kennzeichnung

Nicht Bestandteil von M2 sind:

- targetbezogene Ausgabe (`F-24`, `F-25`) und konkrete `TargetMetadataPort`-Implementierungen
- Markdown-, HTML-, JSON- oder DOT-Ausgaben (`F-27` bis `F-30`)
- konfigurierbare Ausgabepfade aus `F-35` und Steuerung des Ausgabeformats (`F-36`)
- Vergleich zwischen zwei Analysezeitpunkten (`F-11`)
- alternative Include-Adapter auf Basis von `.d`-Dateien oder Compiler-Flags
- Unterscheidung zwischen Projekt-Headern und externen Headern (`F-16`), sofern dies die M2-Lieferung verzoegert
- direkte/indirekte Include-Umschaltung (`F-17`) als vom Nutzer steuerbare Option
- `--verbose` und `--quiet` als voll ausgearbeitete Modi (`F-39`, `F-40`), sofern die Standardausgabe die M2-Abnahmekriterien bereits erfuellt

Diese Abgrenzung konkretisiert den Phasenplan fuer M2: `F-27` bleibt in Phase 3 bzw. M3. Architekturstellen, die spaetere Report-Adapter bereits als MVP-Zielbild beschreiben, sind fuer diesen Milestone daher als Zielstruktur ueber M2 hinaus zu lesen.

## 1. Arbeitspakete

### 1.1 Ergebnismodelle, Ports und Diagnostics fuer Phase 2 schaerfen

Der M1-Kern kennt bisher nur `CompileDatabaseResult` und eine minimale `AnalysisResult`-Huelse. Fuer M2 reicht das nicht mehr aus. Der Kern benoetigt belastbare Ergebnisobjekte, die sowohl Projektanalyse als auch Impact-Analyse inklusive Datenluecken tragen koennen.

Mindestens benoetigt:

- ein erweitertes Eingabemodell fuer Compile-Eintraege bzw. Compile-Aufrufe, das die Herkunft des Aufrufs (`arguments` vs. `command`) fuer M2 sichtbar haelt
- ein Modell fuer eine analysierte Translation Unit mit Pfad, Rang, Kennzahlen und optionalen Hinweisen
- ein Modell fuer Include-Hotspots mit Header-Bezeichnung, Anzahl betroffener Translation Units und vollstaendiger, disambiguierbarer sowie stabil sortierbarer Liste der betroffenen Translation Units
- ein Modell fuer Impact-Ergebnisse mit mindestens direkter Betroffenheit, heuristisch abgeleiteter Betroffenheit und Hinweisen auf fehlende Daten
- ein wiederverwendbares Diagnosemodell fuer Warnungen, Datenluecken und Heuristik-Hinweise
- Erweiterung von `AnalysisResult`, damit neben `compile_database` auch TU-Ranking, Include-Hotspots und Analyse-Diagnostics transportiert werden

Wichtig:

- bestehende M1-Fehlermodelle fuer ungueltige Eingaben bleiben erhalten; M2-Diagnostics behandeln nur fachliche Unsicherheit nach erfolgreichem Laden der Eingabedaten
- Diagnostics gehoeren in den Kern und werden von Report-Adaptern dargestellt, nicht im CLI-Adapter zusammengesetzt
- Sortierreihenfolgen duerfen nicht implizit von der Reihenfolge in `compile_commands.json` abhaengen; Modelle sollen die fuer reproduzierbare Sortierung noetigen Schluessel explizit tragen
- M2 darf die Herkunft des Compile-Aufrufs nicht verlieren: Eintraege aus `arguments` und `command` muessen im Kern unterscheidbar bleiben, damit Tokenisierung, Diagnostics und Tests fachlich korrekt moeglich sind
- Pfadvergleiche fuer TU-Dateien, Header und `--changed-file` duerfen nicht gegen rohe Anzeige-Strings laufen; der Kern benoetigt dafuer einen expliziten, dokumentierten Vergleichsschluessel
- M2 analysiert jede geladene Compile-Database-Zeile als eigenstaendige TU-Beobachtung; bei identischem Quelldateipfad duerfen daher mehrere Ergebniszeilen entstehen

#### Port-Anpassungen

Die bestehenden Port-Signaturen sind fuer M2 zu schmal und muessen konkretisiert werden:

**Driving Port** (`AnalyzeProjectPort`): bleibt der Einstieg fuer die Projektanalyse, liefert aber kuenftig ein vollstaendiges `AnalysisResult` mit Ranking, Hotspots und Diagnostics.

**Driving Port** (`AnalyzeImpactPort`): die aktuelle Signatur `std::string analyze_impact(std::string_view changed_path)` ist nur Platzhalter. Fuer M2 muss der Port mindestens den Pfad zur `compile_commands.json` und den geaenderten Dateipfad entgegennehmen sowie ein fachliches `ImpactResult` zurueckgeben. Eine Signatur mit zwei Parametern ist fuer M2 ausreichend; ein eigenes Request-Objekt ist optional.

**Driven Port** (`IncludeResolverPort`): die aktuelle Boole-Methode `can_resolve_includes()` reicht nicht aus. Fuer M2 muss der Port Include-Beziehungen fuer eine Menge von Compile-Eintraegen liefern koennen, inklusive Heuristik-Hinweisen und partiellen Fehlern. Der Rueckgabetyp soll sowohl erfolgreich aufgeloeste Beziehungen als auch unresolved Includes bzw. Warnungen transportieren. Dabei muss jede Beziehung der konkreten TU-Beobachtung bzw. Compile-Database-Zeile zuordenbar bleiben; Beobachtungen mit identischem Quelldateipfad duerfen auf Port-Ebene nicht zusammengefaltet werden.

**Driven Port** (`ReportWriterPort`) und **Driving Port** (`GenerateReportPort`): beide sind aktuell nur auf `AnalysisResult` ausgelegt. Fuer M2 muss der Report-Pfad sowohl Projektanalyse- als auch Impact-Ergebnisse ausgeben koennen. Fuer den aktuellen Scope genuegen zwei explizite Methoden statt eines generischen Variant-Modells, zum Beispiel eine fuer Projektanalyse und eine fuer Impact-Ergebnisse.

Vorgesehene Artefakte:

- neue oder erweiterte Modelle unter `src/hexagon/model/` fuer Compile-Aufruf, Translation Units, Hotspots, Impact und Diagnostics
- Anpassung von `compile_entry.h` oder Einfuehrung eines dedizierten Modells fuer den urspruenglichen Compile-Aufruf
- Anpassung von `analysis_result.h`
- Ersatz des Platzhalters in `analyze_impact_port.h`
- substanzielle Erweiterung von `include_resolver_port.h`
- Anpassung von `report_writer_port.h` und `generate_report_port.h`
- Folgeanpassung im Compile-Database-Ladepfad (`compile_database_port.h`, `compile_commands_json_adapter.*`), damit die Compile-Aufruf-Provenienz erhalten bleibt

**Ergebnis**: Der Kern kann M2-Ergebnisse fachlich modellieren, ohne dass Reporter oder CLI Fachzustand als unstrukturierte Strings tragen muessen.

### 1.2 Translation-Unit-Kennzahlen und reproduzierbares Ranking umsetzen

Die erste M2-Analyse stutzt sich bewusst auf Kennzahlen, die direkt aus der Compile-Datenbank ableitbar sind. Damit bleibt das Ranking auch ohne Include-Heuristik verfuegbar und transparent.

Fuer jede analysierte Translation Unit werden mindestens berechnet:

- `arg_count`: Anzahl der erkannten Compile-Argumente
- `include_path_count`: Anzahl erkannter Include-Suchpfade aus `-I`, `-isystem` und `-iquote`
- `define_count`: Anzahl erkannter Praeprozessor-Defines aus `-D`

Fuer Eintraege mit `arguments` kann direkt auf dem Vektor gearbeitet werden. Fuer Eintraege mit `command` muss M2 erstmals eine begrenzte Tokenisierung einfuehren.

Voraussetzung fuer M2 ist, dass das Eingabemodell zwischen originalem `arguments`-Vektor und rohem `command`-String unterscheiden kann. Das blosse Speichern eines einteiligen Pseudo-`arguments`-Vektors aus dem `command`-String reicht fuer Tokenisierung, Diagnostics und Testbarkeit nicht aus.

Entscheidung fuer M2:

- enthaelt ein Eintrag sowohl `arguments` als auch `command`, bleibt die M1-Prioritaet erhalten: `arguments` werden verwendet, `command` wird ignoriert
- enthaelt ein Eintrag nur `command`, wird der rohe String bis zur Tokenisierung erhalten
- `arguments` werden unveraendert ausgewertet
- `command` wird mit einem dokumentierten, lokalen Tokenizer in Argumente zerlegt
- volle POSIX-Shell-Semantik, Umgebungsvariablen, Command-Substitution oder Backtick-Auswertung sind nicht Bestandteil des MVP
- der Tokenizer muss mindestens Whitespace-Trennung sowie einfache und doppelte Quotes in gaengigen Compile-Commands verarbeiten koennen
- fuehrt die Tokenisierung zu einem unklaren oder unvollstaendigen Ergebnis, soll dies nicht zum Abbruch fuehren; stattdessen wird eine Diagnostic erzeugt und die Metrikbestimmung best effort fortgesetzt

Die Rangbildung muss fuer Nutzer nachvollziehbar und fuer Tests stabil sein. Fuer M2 wird daher ein einfacher, dokumentierter Sortierschluessel festgelegt:

1. absteigend nach `score = arg_count + include_path_count + define_count`
2. bei Gleichstand absteigend nach `arg_count`
3. danach absteigend nach `include_path_count`
4. danach absteigend nach `define_count`
5. danach aufsteigend nach disambiguierendem TU-Schluessel (z.B. Quelldateipfad plus Arbeitsverzeichnis oder Command-Fingerprint)

Die Konsolenausgabe muss die drei Einzelkennzahlen immer sichtbar machen. Der interne `score` darf als technischer Sortierschluessel verwendet werden, muss aber nicht gesondert ausgegeben werden.

Nicht Bestandteil von M2:

- konfigurierbare Schwellenwerte (`F-10`, `F-38`)
- zeitlicher Vergleich (`F-11`)
- buildzeitnahe Metriken, etwa echte Compiler-Laufzeiten

Vorgesehene Artefakte:

- Kernlogik fuer Metrikextraktion und Rangbildung unter `src/hexagon/services/` oder `src/hexagon/model/`
- Anpassung des Eingabemodells und des Ladepfads, damit `arguments` und `command` fuer M2 getrennt erhalten bleiben
- Tests fuer `arguments`-, `command`- und Mischfaelle
- Erweiterung von `ProjectAnalyzer`, damit die Metriken nach dem Laden der Compile-Datenbank berechnet werden

**Ergebnis**: Die Projektanalyse liefert auch ohne Include-Daten ein nutzbares, reproduzierbares TU-Ranking gemaess `AK-03`.

### 1.3 `IncludeResolverPort` und `SourceParsingIncludeAdapter` umsetzen

M2 benoetigt erstmals Include-Beziehungen. Architektur und Roadmap haben dafuer bereits den `IncludeResolverPort` und einen `SourceParsingIncludeAdapter` vorgesehen. Dieser Pfad wird in M2 als bewusste MVP-Heuristik umgesetzt.

Der Adapter soll mindestens:

- von jeder Translation Unit ausgehend Quelldateien lesen und `#include`-Direktiven erkennen
- direkte Includes rekursiv verfolgen, um einen fuer Hotspots und Impact nutzbaren Include-Graphen zu erhalten
- Includes gegen die Compile-Eintrag-spezifischen Suchpfade aus `-I`, `-isystem` und `-iquote` sowie gegen den lokalen Quelldateikontext aufloesen
- relative Suchpfade aus `-I`, `-isystem` und `-iquote` vor der Verwendung reproduzierbar gegen das `directory` des jeweiligen Compile-Eintrags aufloesen
- Zyklen erkennen und Traversierung dagegen absichern
- fehlende oder nicht aufloesbare Includes als Diagnostic erfassen, nicht als fatalen Analysefehler
- die Heuristik-Natur des gesamten Ergebnisses explizit markieren

Bekannte und fuer M2 akzeptierte Einschraenkungen:

- bedingte Includes (`#if`, `#ifdef`) werden nicht semantisch ausgewertet
- forced includes ueber Compiler-Flags (z.B. `-include`) werden nicht beruecksichtigt
- generierte Header, die zur Analysezeit nicht im Dateisystem vorliegen, koennen fehlen
- compilerinterne Standardsuchpfade und plattformspezifische Besonderheiten werden nur erfasst, wenn sie explizit in den Compile-Argumenten sichtbar sind
- Makroexpansion in Include-Pfaden wird nicht unterstuetzt

Fuer M2 reicht ein pragmatischer Parser:

- Zeilen werden textuell untersucht; relevant sind Include-Direktiven mit optionalem fuehrenden Whitespace sowie optionalem Leerraum zwischen `#` und `include`
- sowohl `#include "..."`, `# include "..."`, `#include <...>` als auch `# include <...>` werden erkannt
- Kommentare und offensichtliche Nicht-Include-Zeilen werden ignoriert
- tiefere Praeprozessor-Semantik bleibt ausserhalb des Scopes

#### Dokumentierte Suchreihenfolge fuer Includes

Damit die Include-Aufloesung in M2 bei gleichnamigen Headern reproduzierbar bleibt, wird eine feste MVP-Suchreihenfolge definiert:

- fuer `#include "header.h"` wird zuerst relativ zur aktuell analysierten Datei gesucht, danach in `-iquote`-Pfaden in ihrer Auftretensreihenfolge im Compile-Aufruf, danach in `-I`-Pfaden in ihrer Auftretensreihenfolge und zuletzt in `-isystem`-Pfaden in ihrer Auftretensreihenfolge
- fuer `#include <header.h>` wird nicht relativ zur aktuell analysierten Datei gesucht; stattdessen werden `-I`-Pfad, dann `-isystem`-Pfade jeweils in ihrer Auftretensreihenfolge im Compile-Aufruf durchsucht
- pro Include-Direktive gewinnt der erste erfolgreich aufgeloeste Pfad; nur dieser Pfad geht in Include-Graph, Hotspots und Impact-Analyse ein
- diese Suchreihenfolge ist bewusst als dokumentierte MVP-Heuristik zu verstehen; Abweichungen von compilergenauer Aufloesung sind fuer M2 akzeptiert, muessen aber nichtdeterministisches Verhalten ausschliessen

#### Pfadkanonisierung und Vergleich

Damit Ranking, Hotspots und Impact bei realen Daten reproduzierbar funktionieren, legt M2 eine einheitliche Vergleichssemantik fuer Pfade fest:

- Compile-Database-Quelldateien werden fuer Vergleiche gegen das jeweilige `directory` des Eintrags aufgeloest, falls `file` relativ ist
- ein aufgeloester Header wird fuer Vergleiche als normalisierter Pfadschluessel gespeichert; Grundlage ist der tatsaechlich erfolgreiche Aufloesungspfad ueber Dateikontext und Include-Suchpfade
- relative Include-Suchpfade aus `-I`, `-iquote` und `-isystem` werden vor der Traversierung gegen das jeweilige `directory` des Compile-Eintrags aufgeloest und anschliessend in dieselbe lexikalische Normalform ueberfuehrt wie TU- und Header-Pfade
- `--changed-file` wird, falls relativ angegeben, relativ zum Verzeichnis der uebergebenen `compile_commands.json` interpretiert
- vor Vergleichen werden mindestens `.`- und `..`-Segmente sowie triviale Separatorunterschiede lexikalisch normalisiert
- eine Aufloesung ueber Symlinks oder Dateisystem-Kanonisierung ist fuer M2 nicht erforderlich; M2 arbeitet mit einer stabilen lexikalischen Normalform
- fuer die Anzeige darf weiterhin die nutzernahe Originalschreibweise oder eine bewusst kompakte Darstellung verwendet werden; Vergleichsschluessel und Anzeigeformat sind getrennt zu behandeln

Nicht Bestandteil von M2:

- alternative Include-Resolver auf Basis von `.d`-Dateien
- Ermittlung direkter vs. indirekter Includes als getrennt konfigurierbare Sicht
- Klassifikation intern/external als Pflichtmerkmal jedes Hotspots

Vorgesehene Artefakte:

- `src/adapters/input/source_parsing_include_adapter.h`
- `src/adapters/input/source_parsing_include_adapter.cpp`
- Anpassung von `src/adapters/CMakeLists.txt`
- Adapter-Tests fuer einfache, transitive, zyklische und teilweise unaufloesbare Include-Faelle

**Ergebnis**: Der Kern erhaelt eine belastbare, aber klar als heuristisch gekennzeichnete Include-Datengrundlage fuer Hotspots und Impact.

### 1.4 Include-Hotspot-Analyse im Kern ableiten

Sobald der Include-Graph verfuegbar ist, muss der Kern daraus nachvollziehbare Include-Hotspots ableiten.

Fuer M2 gilt:

- ein Hotspot ist ein Header, der in mindestens zwei analysierten Translation Units vorkommt
- die Zaehlung basiert auf betroffenen TU-Beobachtungen aus der Compile-Datenbank, nicht auf der absoluten Anzahl einzelner Include-Kanten
- dieselbe TU darf einen Header nur einmal zum Hotspot-Zaehler beitragen, auch wenn der Header mehrfach entlang verschiedener Include-Pfade erreicht wird
- existieren mehrere Compile-Database-Zeilen mit identischem Quelldateipfad, zaehlen diese Beobachtungen fuer Hotspots getrennt, sofern der Header fuer jede Beobachtung nachweisbar ist
- die Hotspot-Liste wird absteigend nach Anzahl betroffener Translation Units sortiert; Gleichstaende werden ueber den Header-Pfad stabil aufgeloest

Die Konsolenausgabe muss mindestens enthalten:

- Header-Bezeichnung
- Anzahl betroffener Translation Units
- eine sichtbare Zuordnung zu betroffenen Translation Units; bei Pfadkollisionen muss die Ausgabe die TU-Beobachtungen disambiguieren, zum Beispiel ueber Arbeitsverzeichnis oder einen stabilen TU-Schluessel

Damit `F-15` und `AK-04` in M2 vollstaendig erfuellt werden, gilt fuer die Konsole:

- fuer jeden ausgegebenen Hotspot wird die vollstaendige Menge betroffener Translation Units ausgegeben; M2 darf die Zuordnung nicht auf eine Vorschau oder Restanzahl verkuerzen
- die Darstellung darf kompakt formatiert sein, zum Beispiel eine TU pro Zeile oder als stabil gruppierte Liste, solange keine betroffenen TU-Beobachtungen ausgelassen werden
- `--top` begrenzt in M2 nur die Anzahl ausgegebener Ranking- bzw. Hotspot-Eintraege, nicht die TU-Zuordnung innerhalb eines bereits ausgegebenen Hotspots

Die gesamte Hotspot-Sektion muss als heuristisch gekennzeichnet werden, solange sie auf dem `SourceParsingIncludeAdapter` beruht. Die Kennzeichnung soll im Abschnitt selbst sichtbar sein, nicht nur in `--help`.

Vorgesehene Artefakte:

- Kernlogik fuer Hotspot-Aggregation im Umfeld von `ProjectAnalyzer`
- Hotspot-Modelle unter `src/hexagon/model/`
- Reporter-Unterstuetzung fuer vollstaendige, disambiguierte TU-Zuordnungen je Hotspot

**Ergebnis**: `AK-04` wird ueber eine reproduzierbare, heuristisch gekennzeichnete Hotspot-Liste erfuellt.

### 1.5 Dateibasierte Impact-Analyse einfuehren

M2 fuehrt mit `impact` einen zweiten fachlichen Einstieg ein. Die Analyse soll beantworten, welche Translation Units von einer Aenderung an einer Datei voraussichtlich betroffen sind.

Mindestens zu unterscheiden:

- direkte Betroffenheit: der geaenderte Pfad ist selbst eine analysierte Translation Unit
- heuristisch abgeleitete Betroffenheit: der geaenderte Pfad taucht als Header im aufgeloesten Include-Graphen auf und wird auf betroffene Translation Units rueckprojiziert
- unklare oder leere Betroffenheit: die Datei kommt in den verfuegbaren Daten nicht vor oder die Datengrundlage ist unvollstaendig

Der neue Service `ImpactAnalyzer` soll mindestens:

- die Compile-Datenbank laden
- denselben Include-Graphen bzw. dieselbe Include-Resolver-Logik wie die Projektanalyse nutzen, um doppelte Analysepfade zu vermeiden
- den uebergebenen Dateipfad ueber denselben kanonischen Vergleichsschluessel wie die Projektanalyse gegen bekannte TU-Quelldateien und gegen aufgeloeste Header vergleichen
- betroffene Translation Units deterministisch sortieren
- Diagnostics erzeugen, wenn die Aussage nur partiell moeglich ist

Fuer direkte Treffer gilt in M2:

- trifft der geaenderte Pfad auf mehrere Compile-Database-Zeilen mit identischem Quelldateipfad, sind alle passenden TU-Beobachtungen als direkt betroffen auszugeben
- die Impact-Ausgabe darf dabei nicht stillschweigend auf eindeutige Quelldateipfade deduplizieren; sie muss dieselbe TU-Semantik wie Ranking und Hotspots verwenden
- bei Pfadkollisionen muss die Konsolenausgabe die einzelnen TU-Beobachtungen sichtbar unterscheiden

Fuer M2 wird keine targetbezogene Ausgabe verlangt. Fehlen Target-Metadaten, ist dies kein Fehler und erfordert keinen eigenen Exit-Code.

CLI-Semantik fuer M2:

```text
cmake-xray impact --compile-commands <path> --changed-file <path>
cmake-xray impact --help
```

Vorgesehene Ergebnisregeln:

- ist die Eingabe formal gueltig, liefert der Befehl Exit-Code `0`, auch wenn keine betroffenen Translation Units gefunden werden
- findet die Analyse keine Betroffenen, soll die Ausgabe dies explizit und nachvollziehbar sagen
- beruht die Aussage auf dem heuristischen Include-Graphen, muss dies inline sichtbar sein
- relative `--changed-file`-Angaben muessen gemaess der oben definierten Pfadsemantik reproduzierbar zum selben Vergleichsschluessel fuehren wie die intern aufgeloesten TU- und Header-Pfade

Beispiel fuer einen positiven Fall:

```text
impact analysis for include/common/config.h [heuristic]
affected translation units: 3
  src/app/main.cpp
  src/lib/core.cpp
  tests/core/test_core.cpp
note: conditional or generated includes may be missing from this result
```

Beispiel fuer fehlende Datengrundlage:

```text
impact analysis for include/generated/version.h [heuristic]
affected translation units: 0
note: the file was not present in the loaded compile database or resolved include graph
```

Vorgesehene Artefakte:

- `src/hexagon/services/impact_analyzer.h`
- `src/hexagon/services/impact_analyzer.cpp`
- Erweiterung von `src/main.cpp` und CLI-Verdrahtung
- neue Tests unter `tests/hexagon/` und `tests/e2e/`

**Ergebnis**: `AK-05` ist mit einem fachlichen, testbaren Impact-Pfad auf TU-Ebene erfuellt.

### 1.6 Konsolenreport und CLI fuer M2 erweitern

Der M1-Erfolgstext `compile database loaded: <n> entries` ist fuer M2 nicht mehr ausreichend. Die CLI soll echte Ergebnisobjekte ueber einen Reporter ausgeben.

M2 fuehrt daher einen `ConsoleReportAdapter` ein. Der CLI-Adapter bleibt fuer:

- Argument-Parsing
- Aufruf der passenden Driving Ports
- Mappen von Eingabefehlern auf Exit-Codes
- Schreiben auf `stdout` bzw. `stderr`

verantwortlich, waehrend die eigentliche Darstellung der Analyseergebnisse ueber `GenerateReportPort` und `ReportWriterPort` erfolgt.

Fuer M2 sinnvoll:

- Unterkommando `analyze` fuer TU-Ranking und Include-Hotspots
- Unterkommando `impact` fuer die dateibasierte Impact-Analyse
- Option `--compile-commands <path>` fuer beide Unterkommandos
- Option `--top <n>` mindestens fuer `analyze`, damit Ranking- und Hotspot-Listen begrenzt werden koennen (`F-42`)

Entscheidung fuer M2:

- Standardwert fuer `--top` ist `10`
- die Ausgabe nennt bei Begrenzung immer die Gesamtanzahl, zum Beispiel `top 10 of 37 translation units`
- die Begrenzung betrifft nur die Hauptlisten fuer TU-Ranking und Hotspots; die TU-Zuordnung innerhalb eines ausgegebenen Hotspots sowie Impact-Ergebnisse werden in M2 voll ausgegeben
- Analyse-Diagnostics mit gueltiger Datengrundlage fuehren nicht zu neuen Exit-Codes; sie erscheinen inline im Report
- die Hilfe fuer `impact` nennt explizit, dass relative `--changed-file`-Pfade relativ zur uebergebenen `compile_commands.json` interpretiert werden

Die CLI-Hilfe und/oder die Konsolenausgabe muessen die Bewertungsgrundlage fuer das TU-Ranking benennen (`F-09`), zum Beispiel:

- Ranking basiert auf `arg_count`, `include_path_count` und `define_count`
- Hotspot- und Header-basierte Impact-Aussagen beruhen auf heuristischer Include-Aufloesung

Vorgesehene Artefakte:

- Ersatz von `src/adapters/output/placeholder_report_adapter.*`
- neue Dateien `src/adapters/output/console_report_adapter.h` und `src/adapters/output/console_report_adapter.cpp`
- Erweiterung von `src/adapters/cli/cli_adapter.cpp`
- Anpassung von `src/main.cpp`, `src/adapters/CMakeLists.txt`, `src/hexagon/services/report_generator.*`

**Ergebnis**: Die Nutzeroberflaeche zeigt echte M2-Fachergebnisse an und bleibt dabei in Verantwortung und Exit-Code-Semantik sauber getrennt.

### 1.7 Test- und Referenzdaten fuer M2 ausbauen

M2 fuehrt neue Risiken ein: heuristische Include-Aufloesung, nichttriviale Sortierung, Tokenisierung von `command`-Strings und Impact-Ausgaben. Diese Faelle muessen gezielt automatisiert werden.

#### 1.7a Kern- und Service-Tests

Mindestens benoetigt:

- Tests fuer Metrikextraktion aus `arguments`
- Tests fuer Metrikextraktion aus `command`
- Tests fuer reproduzierbare Rangfolge bei verschiedenen Eingabereihenfolgen
- Tests dafuer, dass `command`-basierte Eintraege ihre Provenienz bis zur Tokenisierung behalten
- Tests fuer unklare oder unvollstaendige `command`-Tokenisierung, bei denen die Analyse best effort fortgesetzt und eine Diagnostic erzeugt wird
- Tests fuer Hotspot-Aggregation, eindeutige TU-Zaehlung und stabile Sortierung
- Tests fuer Pfadkanonisierung bei relativen, absoluten und lexikalisch aequivalenten Pfaden
- Tests fuer Impact-Analyse bei direkter TU-Betroffenheit
- Tests fuer Impact-Analyse bei Header-Betroffenheit ueber transitive Includes
- Tests fuer direkte Betroffenheit bei mehreren Compile-Database-Zeilen mit identischem Quelldateipfad

#### 1.7b Adapter-Tests

Mindestens benoetigt:

- Include-Adapter-Tests fuer direkte Includes
- Include-Adapter-Tests fuer transitive Includes
- Include-Adapter-Tests fuer relative Include-Suchpfade aus `-I`, `-iquote` und `-isystem`
- Include-Adapter-Tests fuer dokumentierte Suchreihenfolge bei gleichnamigen Headern
- Include-Adapter-Tests fuer Include-Zyklen ohne Endlosschleife und mit stabiler Ergebnisbildung
- Include-Adapter-Tests fuer unaufloesbare Includes mit Diagnostics
- Reporter-Tests fuer begrenzte Listen, Heuristik-Kennzeichnung und vollstaendige TU-Zuordnung je ausgegebenem Hotspot

#### 1.7c CLI- und End-to-End-Tests

Mindestens benoetigt:

- `analyze --help` und `impact --help`
- Erfolgspfad fuer `analyze`
- Erfolgspfad fuer `impact` bei geaenderter Quelldatei
- Erfolgspfad fuer `impact` bei geaendertem Header
- Analyse mit leerem Ergebnis, aber gueltiger Datengrundlage
- Begrenzung via `--top`
- stabile Ausgabe bei permutierter Reihenfolge in `compile_commands.json`
- Erfolgspfad fuer `impact` mit relativem `--changed-file`, das relativ zur `compile_commands.json` interpretiert wird
- Ausgabe mit disambiguierten TU-Beobachtungen bei mehrfach vorkommendem Quelldateipfad

#### Referenzdaten

Im Repo hinterlegt und fuer die M2-Abnahme genutzt werden kleine, versionierte Referenzprojekte unter `tests/e2e/testdata/`, insbesondere:

- `m2/basic_project/` mit wenigen TUs, klaren Compile-Argumenten, transitiven Includes, relativen `--changed-file`-Pfaden und einem bewusst unaufloesbaren Include
- `m2/duplicate_tu_entries/` fuer mehrfach vorkommende Quelldateipfade bei unterschiedlichen TU-Beobachtungen
- `m2/permuted_compile_commands/` als inhaltlich identische, anders sortierte Datenbasis zur Reproduzierbarkeitspruefung

Ergaenzend werden Include-Suchpfade, Suchreihenfolge, Zyklen und Tokenizer-Edgecases ueber gezielte Unit- und Adapter-Tests mit temporaren Testdaten abgedeckt.

Nicht automatisiert abgedeckt in M2:

- Performance-Aussagen fuer `250`/`500`/`1.000` TUs aus Phase 3
- plattformspezifische Compiler-Sonderfaelle, die nur mit realen Toolchains ausserhalb des Repos nachstellbar sind

#### Testmatrix

Mindestens abzudeckende Testmatrix:

| Fall                                                         | Erwartung                                                                 |
| ------------------------------------------------------------ | ------------------------------------------------------------------------- |
| `arguments` mit `-I`, `-isystem`, `-iquote`, `-D`            | korrekte Kennzahlen                                                       |
| `command` mit Quotes und Leerzeichen                         | Tokenizer liefert reproduzierbar nutzbare Argumente                       |
| unklare `command`-Tokenisierung                              | Analyse laeuft best effort weiter und erzeugt eine Diagnostic             |
| `command`-basierter Eintrag                                  | Herkunft des Compile-Aufrufs bleibt bis zur Tokenisierung erhalten        |
| identische Datenbasis in anderer Reihenfolge                 | identisches Ranking und identische Hotspot-Reihenfolge                    |
| zwei Compile-Eintraege mit gleichem Quelldateipfad           | beide TU-Beobachtungen bleiben erhalten, Ausgabe bleibt eindeutig         |
| direkter Include-Hotspot in mehreren TUs                     | Header wird mit korrekter TU-Anzahl ausgewiesen                           |
| Hotspot mit vielen betroffenen TUs                           | jeder ausgegebene Hotspot zeigt die vollstaendige TU-Zuordnung            |
| zwei TUs mit gleichem Quelldateipfad und gemeinsamem Header  | Hotspot zaehlt beide TU-Beobachtungen, Ausgabe bleibt disambiguiert       |
| relative Include-Suchpfade in Compile-Argumenten             | Aufloesung erfolgt reproduzierbar relativ zum Compile-`directory`         |
| gleichnamiger Header in lokalem Kontext und Suchpfaden       | dokumentierte Suchreihenfolge liefert stabil denselben Treffer            |
| zyklische Includes                                           | Analyse terminiert reproduzierbar ohne Endlosschleife                     |
| transitive Includes ueber mehrere Header-Stufen              | Impact-Analyse findet betroffene TUs                                      |
| `impact` mit relativem `--changed-file`                      | Pfad wird relativ zur `compile_commands.json` korrekt aufgeloest          |
| `impact` mit lexikalisch aequivalentem Pfad (`./`, `..`)     | gleiches fachliches Ergebnis wie mit kanonischer Schreibweise             |
| unaufloesbarer Header                                        | Analyse bleibt erfolgreich, Diagnostic und Heuristik-Hinweis erscheinen   |
| `analyze --top 3`                                            | Ausgabe zeigt Top-3 und Gesamtanzahl                                      |
| `impact` fuer Quelldatei aus Compile-Datenbank               | direkte Betroffenheit, Exit `0`                                           |
| `impact` fuer Quelldatei mit mehrfacher TU-Beobachtung       | alle passenden TU-Beobachtungen werden separat ausgewiesen                |
| `impact` fuer Header aus Include-Graph                       | heuristisch markierte Betroffenheit, Exit `0`                             |
| `impact` fuer Datei ausserhalb aller bekannten Daten         | Exit `0`, klarer Hinweis auf fehlende Daten                               |
| ungueltige `compile_commands.json` im `impact`-Befehl        | bestehende M1-Fehlermeldung und Exit-Code `4` bzw. `3` bleiben erhalten   |

**Ergebnis**: Die M2-Funktionen sind fachlich und technisch gegen Regressionen abgesichert, insbesondere fuer Heuristik und Reproduzierbarkeit.

### 1.8 README, CHANGELOG und Nutzungsbeispiele aktualisieren

Mit M2 muss die Dokumentation vom Eingabe-MVP auf ein fachlich nutzbares Tool umgestellt werden.

Die README soll fuer M2 mindestens enthalten:

- Beispiel fuer `analyze --compile-commands ...`
- Beispiel fuer `impact --compile-commands ... --changed-file ...`
- kurze Erlaeuterung der TU-Kennzahlen
- sichtbaren Hinweis auf heuristische Include- und Impact-Ergebnisse
- Hinweis auf die Pfadsemantik von `--changed-file` (relative Pfade gelten relativ zur `compile_commands.json`)
- Erklaerung von `--top`

Zusatzlich sind bei Abschluss der Implementierung:

- die Versionsnummern in `application_info.h` und Root-`CMakeLists.txt` auf `0.3.0` anzuheben
- `CHANGELOG.md` um die neuen M2-Funktionen zu erweitern

**Ergebnis**: Dokumentation und Beispielaufrufe spiegeln den tatsaechlichen M2-Stand wider.

## 2. Zielartefakte

Folgende Dateien oder Dateigruppen sollen nach M2 voraussichtlich neu entstehen oder substanziell ueberarbeitet sein:

| Bereich             | Zielartefakte                                                                                                                       |
| ------------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| Kernmodelle         | neue/erweiterte Modelle unter `src/hexagon/model/` fuer Compile-Aufruf, Translation Units, Hotspots, Impact und Diagnostics       |
| Compile Input Model | `src/hexagon/model/compile_entry.h` oder separates Modell fuer `arguments`/`command`-Provenienz                                   |
| Analysis Result     | `src/hexagon/model/analysis_result.h`                                                                                               |
| Impact Result       | neues Modell unter `src/hexagon/model/impact_result.h`                                                                              |
| Driving Ports       | `src/hexagon/ports/driving/analyze_project_port.h`, `src/hexagon/ports/driving/analyze_impact_port.h`                              |
| Driven Ports        | `src/hexagon/ports/driven/include_resolver_port.h`, `src/hexagon/ports/driven/report_writer_port.h`                                |
| Services            | `src/hexagon/services/project_analyzer.*`, neue Dateien `src/hexagon/services/impact_analyzer.*`, `src/hexagon/services/report_generator.*` |
| Input-Adapter       | Anpassung von `src/adapters/input/compile_commands_json_adapter.*`, neue Dateien `src/adapters/input/source_parsing_include_adapter.*` |
| Output-Adapter      | neue Dateien `src/adapters/output/console_report_adapter.*`, Entfernung oder Ersatz von `placeholder_report_adapter.*`             |
| CLI                 | `src/adapters/cli/cli_adapter.*`, optional `src/adapters/cli/exit_codes.h`                                                         |
| Composition Root    | `src/main.cpp`                                                                                                                      |
| Build-Konfiguration | `src/adapters/CMakeLists.txt`, `src/hexagon/CMakeLists.txt`, `tests/CMakeLists.txt`                                                |
| Tests               | neue oder erweiterte Tests unter `tests/hexagon/`, `tests/adapters/`, `tests/e2e/`                                                 |
| Referenzdaten       | neue Fixtures unter `tests/e2e/testdata/m2/`                                                                                        |
| Dokumentation       | `README.md`, `CHANGELOG.md`, gegebenenfalls `docs/roadmap.md` bei Statusfortschreibung                                              |

Hinweis: Die konkreten Dateinamen duerfen von dieser Zielstruktur abweichen, solange die hexagonale Trennung, die Rueckverfolgbarkeit zu `AK-03` bis `AK-05` und die klare Trennung zwischen Kernlogik und Reportern erhalten bleiben.

## 3. Reihenfolge

| Schritt | Arbeitspaket                                             | Abhaengig von |
| ------- | -------------------------------------------------------- | ------------- |
| 1a      | 1.1 Ergebnismodelle, Ports und Diagnostics schaerfen     | M1            |
| 1b      | 1.6 Konsolenreport- und CLI-Vertrag fuer M2 festlegen    | M1            |
| 2a      | 1.2 TU-Kennzahlen und Ranking umsetzen                   | 1a            |
| 2b      | 1.3 `IncludeResolverPort` und `SourceParsingIncludeAdapter` umsetzen | 1a     |
| 3       | 1.4 Include-Hotspots im Kern ableiten                    | 2b            |
| 4       | 1.5 Impact-Analyse einfuehren                            | 2a, 2b        |
| 5       | 1.6 Konsolenreport und CLI integrieren                   | 1b, 2a, 3, 4  |
| 6       | 1.7 Tests und Referenzdaten fuer M2 ausbauen             | 2a, 2b, 3, 4, 5 |
| 7       | 1.8 README, CHANGELOG und Nutzungsbeispiele aktualisieren | 5, 6         |

Schritte 1a und 1b koennen parallel bearbeitet werden. Die Reporter- und CLI-Vertraege sollten frueh festgelegt werden, damit Projekt- und Impact-Ergebnisse nicht spaeter auf unpassende String-Schnittstellen zurechtgebogen werden muessen. Die fachlichen Kernelemente 2a und 2b sind weitgehend unabhaengig und koennen nach den Modellentscheidungen parallel entwickelt werden. Hotspots (3) bauen auf dem Include-Graphen auf; Impact (4) benoetigt sowohl Ranking-/TU-Modelle als auch Include-Daten. Tests (6) sollen abschnittsweise mitlaufen, werden hier aber als eigener Integrationsschritt zusammengefasst.

## 4. Pruefung

M2 ist abgeschlossen, wenn folgende Pruefwege erfolgreich durchlaufen:

**Lokal**:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/cmake-xray --help
./build/cmake-xray analyze --help
./build/cmake-xray impact --help
./build/cmake-xray analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json
./build/cmake-xray analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --top 3
./build/cmake-xray analyze --compile-commands tests/e2e/testdata/m2/permuted_compile_commands/compile_commands.json --top 3
./build/cmake-xray analyze --compile-commands tests/e2e/testdata/m2/duplicate_tu_entries/compile_commands.json --top 3
./build/cmake-xray impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file src/app/main.cpp
./build/cmake-xray impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/common/shared.h
./build/cmake-xray impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file ./include/common/../common/config.h
./build/cmake-xray impact --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json --changed-file include/generated/version.h
./build/cmake-xray impact --compile-commands tests/e2e/testdata/m2/duplicate_tu_entries/compile_commands.json --changed-file src/app/main.cpp
ctest --test-dir build --output-on-failure
```

**Docker**:

```bash
docker build --target test -t cmake-xray:test .
docker build --target runtime -t cmake-xray .
docker run --rm cmake-xray --help
docker run --rm cmake-xray analyze --help
docker run --rm cmake-xray impact --help
docker run --rm \
  -v "$PWD/tests/e2e/testdata/m2:/data:ro" \
  cmake-xray analyze --compile-commands /data/basic_project/compile_commands.json --top 3
docker run --rm \
  -v "$PWD/tests/e2e/testdata/m2:/data:ro" \
  cmake-xray analyze --compile-commands /data/permuted_compile_commands/compile_commands.json --top 3
docker run --rm \
  -v "$PWD/tests/e2e/testdata/m2:/data:ro" \
  cmake-xray analyze --compile-commands /data/duplicate_tu_entries/compile_commands.json --top 3
docker run --rm \
  -v "$PWD/tests/e2e/testdata/m2:/data:ro" \
  cmake-xray impact --compile-commands /data/basic_project/compile_commands.json --changed-file src/app/main.cpp
docker run --rm \
  -v "$PWD/tests/e2e/testdata/m2:/data:ro" \
  cmake-xray impact --compile-commands /data/basic_project/compile_commands.json --changed-file include/common/shared.h
docker run --rm \
  -v "$PWD/tests/e2e/testdata/m2:/data:ro" \
  cmake-xray impact --compile-commands /data/basic_project/compile_commands.json --changed-file ./include/common/../common/config.h
docker run --rm \
  -v "$PWD/tests/e2e/testdata/m2:/data:ro" \
  cmake-xray impact --compile-commands /data/basic_project/compile_commands.json --changed-file include/generated/version.h
docker run --rm \
  -v "$PWD/tests/e2e/testdata/m2:/data:ro" \
  cmake-xray impact --compile-commands /data/duplicate_tu_entries/compile_commands.json --changed-file src/app/main.cpp
```

Die lokale und Docker-Pruefung sollen insbesondere bestaetigen:

- Exit-Code `0` fuer erfolgreiche `analyze`- und `impact`-Aufrufe mit gueltiger Datengrundlage
- unveraenderte M1-Fehlercodes fuer ungueltige oder unlesbare Eingaben
- sichtbare Heuristik-Kennzeichnung in Hotspot- und Header-basierten Impact-Ausgaben
- vollstaendige TU-Zuordnung fuer jeden ausgegebenen Include-Hotspot
- dokumentierte Suchreihenfolge und relative Include-Suchpfade fuehren zu reproduzierbaren Header-Treffern
- reproduzierbare Pfadvergleiche fuer relative und lexikalisch aequivalente `--changed-file`-Angaben
- reproduzierbare Ausgabe unabhaengig von der Reihenfolge in `compile_commands.json`

## 5. Rueckverfolgbarkeit

| Planbereich                              | Lastenheft-Kennungen                                       |
| ---------------------------------------- | ---------------------------------------------------------- |
| TU-Kennzahlen und Ranking                | `F-06`, `F-07`, `F-08`, `F-09`, `AK-03`, `NF-15`           |
| Include-Hotspots                         | `F-12`, `F-13`, `F-14`, `F-15`, `AK-04`, `NF-15`           |
| Impact-Analyse                           | `F-21`, `F-22`, `F-23`, `AK-05`, `NF-14`                   |
| Konsolenausgabe und Ergebnisbegrenzung   | `F-26`, `F-31`, `F-32`, `F-42`, `NF-03`                    |
| Eingabepfade (`--compile-commands`, `--changed-file`; Teilaspekt aus `F-35`) | `F-35`                                |
| Heuristik-Kennzeichnung und Diagnostics  | `F-09`, `F-23`, `NF-14`, `NF-15`                           |
| Tests und Referenzdaten                  | `AK-03`, `AK-04`, `AK-05`, `NF-10`                         |

## 6. Entschiedene und offene Punkte

### 6.1 Entschieden

| Frage                                  | Entscheidung                                                                                                                                                                    | Verweis     |
| -------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------- |
| Ranking-Basis                          | `score = arg_count + include_path_count + define_count`; Tie-Break gemaess AP 1.2                                                                                              | AP 1.2      |
| `command`-Tokenisierung                | lokaler, dokumentierter Tokenizer; keine volle Shell-Semantik, kein Shell-Out                                                                                                  | AP 1.2      |
| Compile-Aufruf-Provenienz              | M2 behaelt sichtbar, ob ein Eintrag aus `arguments` oder `command` stammt; `command` bleibt bis zur Tokenisierung als roher String verfuegbar                                | AP 1.1, 1.2 |
| TU-Identitaet in M2                    | jede Compile-Database-Zeile ist eine eigenstaendige TU-Beobachtung; Ranking, Hotspots und Impact arbeiten auf dieser Beobachtungsebene, Ausgabe muss Pfadkollisionen eindeutig darstellen | AP 1.1, 1.2, 1.4, 1.5 |
| IncludeResolver-Portvertrag            | aufgeloeste Include-Beziehungen bleiben bis in Kernmodelle und Reporter der konkreten TU-Beobachtung zuordenbar; identische Quelldateipfade duerfen nicht implizit dedupliziert werden | AP 1.1, 1.3 |
| Include-Strategie im MVP               | rekursives Source-Parsing entlang `#include` und Compile-Suchpfaden; Ergebnis ist heuristisch                                                                                  | AP 1.3      |
| Pfadvergleich in M2                    | Vergleiche laufen ueber lexikalisch normalisierte Vergleichsschluessel; relative TU-Pfade gegen `directory`, relative `--changed-file`-Pfade gegen das Verzeichnis der `compile_commands.json` | AP 1.3, 1.5 |
| Unaufloesbare Includes                 | fuehren zu Diagnostics, nicht zum Analyseabbruch                                                                                                                                | AP 1.3      |
| Hotspot-Zaehlung                       | pro Header Anzahl betroffener TU-Beobachtungen, nicht Anzahl einzelner Kanten; jede TU zaehlt hoechstens einmal pro Header                                                    | AP 1.4      |
| Hotspot-Modell in M2                   | speichert fuer jeden Hotspot die vollstaendige, disambiguierbare und stabil sortierbare TU-Zuordnung; kompakte Vorschauen sind hoechstens eine spaetere Reporterableitung       | AP 1.1, 1.4 |
| Heuristik-Kennzeichnung                | Hotspots immer heuristisch; Impact nur dann heuristisch, wenn Header-/Include-Graph-Daten beteiligt sind                                                                       | AP 1.4, 1.5 |
| Impact-CLI                             | neues Unterkommando `impact` mit `--compile-commands` und `--changed-file`                                                                                                     | AP 1.5      |
| Exit-Codes bei Datenluecken            | gueltige Analyse mit fehlenden oder partiellen Daten bleibt Exit `0`; Unsicherheit wird im Report statt ueber neuen Exit-Code transportiert                                   | AP 1.5, 1.6 |
| Ergebnisbegrenzung                     | `--top` fuer `analyze`, Standardwert `10`, Gesamtanzahl bleibt sichtbar                                                                                                        | AP 1.6      |
| Impact-Ausgabelaenge                   | Impact-Ergebnisse werden in M2 voll ausgegeben; `--top` gilt nicht fuer `impact`                                                                                               | AP 1.6      |
| Report-Port fuer M2                    | Projektanalyse und Impact erhalten getrennte Report-Methoden statt eines generischen Variant-Payloads                                                                          | AP 1.1, 1.6 |
| Markdown-Report in M2                  | nicht Bestandteil dieses Milestones; fuer die M2-Planung gilt der Phasenplan mit Einordnung von `F-27` in Phase 3 bzw. M3                                                     | AP 1.6, Phasenplan |
| Target-Metadaten in M2                 | nicht Bestandteil des Milestones; fehlende Targets sind kein Fehler                                                                                                             | AP 1.5      |
| Projekt-/Externe-Header-Unterscheidung | kein Pflichtumfang fuer M2; wird nur umgesetzt, wenn sie ohne Risiko fuer `AK-04` erreichbar ist                                                                              | AP 1.3, 1.4 |

### 6.2 Offen

Keine offenen Punkte fuer M2.
