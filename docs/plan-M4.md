# Plan M4 - CMake File API als zweite Eingabequelle und initiale Target-Sicht (`v1.1.0`)

## 0. Dokumenteninformationen

| Feld | Wert |
|---|---|
| Dokument | Plan M4 `cmake-xray` |
| Version | `0.1` |
| Stand | `2026-04-22` |
| Status | Entwurf |
| Referenzen | [Lastenheft](./lastenheft.md), [Design](./design.md), [Architektur](./architecture.md), [Phasenplan](./roadmap.md), [Plan M3](./plan-M3.md), [Qualitaet](./quality.md), [Releasing](./releasing.md) |

### 0.1 Zweck
Dieser Plan beschreibt die konkreten Schritte fuer Milestone M4 (`v1.1.0`). Ziel ist der erste Ausbau nach dem MVP: `cmake-xray` soll die CMake File API als zweite echte Eingabequelle fuer die Kernanalysen nutzen koennen, daraus Translation-Unit-Beobachtungen und Target-Zuordnungen ableiten und diese Zusatzsicht in `analyze` und `impact` fuer Konsole und Markdown nutzbar machen.

M4 ist bewusst kein Sammel-Meilenstein fuer alle Phase-4-Erweiterungen. Der Fokus liegt auf `F-05`, `F-19` und `F-24`, also auf einer zweiten CMake-nativen Eingabequelle, TU-zu-Target-Zuordnung und targetbezogener Impact-Ausgabe. Bestehende M3-Aufrufe ohne File-API-Daten sollen fachlich und in ihrer Ausgabeform rueckwaertskompatibel bleiben.

### 0.2 Abschlusskriterium
M4 gilt als erreicht, wenn:

- `cmake-xray analyze` und `cmake-xray impact` optional eine CMake-File-API-Eingabe entgegennehmen und gueltige Reply-Daten reproduzierbar verarbeiten koennen
- Translation-Unit-Beobachtungen ueber ihre eindeutige Beobachtungsidentitaet und nicht nur ueber rohe Dateipfade null, einem oder mehreren Targets zugeordnet werden koennen
- `analyze` bei geladener File-API-Lage die bestehenden TU- und Hotspot-Ausgaben um sichtbare Target-Zuordnungen erweitert, ohne die M3-Standardausgabe ohne Zusatzdaten zu veraendern
- `impact` bei geladener File-API- und Target-Sicht zusaetzlich betroffene Targets ausgibt; direkte Evidenz aus direkt betroffenen Translation Units und heuristische Evidenz aus include-basiert betroffenen Translation Units bleiben unterscheidbar
- `analyze` und `impact` bei ausreichender File-API-Lage auch ohne `compile_commands.json` Kernanalysen ausfuehren koennen; die Datengrundlage wird dabei als `derived` sichtbar gemacht
- bei gemeinsamer Angabe von `compile_commands.json` und File API die Compile Database die autoritative `exact`-Grundlage fuer Ranking, Hotspots und TU-basierte Impact-Ableitung bleibt; die File API reichert passende Beobachtungen um Target-Kontext an und fuehrt in M4 keine zusaetzlichen nur-File-API-Beobachtungen in diese Kernergebnisse ein
- `impact` relative `--changed-file`-Pfade im Mischpfad relativ zum Verzeichnis der `compile_commands.json` und im File-API-Only-Pfad relativ zum im Codemodel angegebenen Top-Level-Source-Verzeichnis aufloest
- unvollstaendige oder nur teilweise verwertbare File-API-Daten als Diagnostics sichtbar werden, ohne die Kernanalyse zu maskieren
- explizit angegebene, aber nicht lesbare oder syntaktisch ungueltige File-API-Daten mit klarer Fehlermeldung und konsistenter Exit-Code-Semantik behandelt werden
- Konsole und Markdown die Beobachtungsherkunft (`exact` oder `derived`) und den Ladezustand der Target-Metadaten explizit sichtbar machen, sobald File-API-Daten beteiligt sind
- Adapter-, Kern-, Reporter- und End-to-End-Tests die Pfade mit und ohne File-API-Daten sowie regressionskritische Faelle fuer Mehrfachzuordnungen, partielle Metadaten und stabile Sortierung absichern

Relevante Kennungen: `F-05`, `F-19`, `F-24`, `F-26`, `F-27`, `F-31`, `F-32`, `F-33`, `F-34`, `F-35`, `F-36`, `NF-10`, `NF-14`, `NF-15`, `NF-18`, `NF-19`, `S-02`

### 0.3 Abgrenzung
Bestandteil von M4 sind:

- ein allgemeiner `BuildModelPort` statt der bisherigen Trennung zwischen primaerer Compile-Datenbank und spaeterem Metadaten-Nebenpfad
- ein `CmakeFileApiAdapter` als erste zweite konkrete `BuildModelPort`-Implementierung neben `CompileCommandsJsonAdapter`
- Translation-Unit-Beobachtungen und TU-zu-Target-Zuordnung aus der CMake File API, auch wenn keine `compile_commands.json` vorliegt
- targetbezogene Erweiterungen der bestehenden `analyze`- und `impact`-Ergebnisse fuer Konsole und Markdown
- Diagnostics fuer fehlende, partielle oder mehrdeutige File-API-Daten
- Referenzdaten und automatisierte Tests fuer die neuen Pfade
- README-, Beispiel- und Release-Aktualisierungen fuer `v1.1.0`

Nicht Bestandteil von M4 sind:

- transitive Target-Graph-Analysen und textuelle Darstellung direkter Target-Abhaengigkeiten (`F-18`)
- Hervorhebung von Targets mit vielen ein- oder ausgehenden Abhaengigkeiten (`F-20`)
- Priorisierung betroffener Targets ueber den Target-Graphen hinweg (`F-25`)
- HTML-, JSON- und DOT-Ausgaben (`F-28` bis `F-30`)
- eigenstaendige Schema- oder Formatversionierung fuer maschinenlesbare Formate (`NF-20`)
- ein voll ausgebauter `--verbose`- oder `--quiet`-Modus (`F-39`, `F-40`)
- automatische Ausfuehrung von CMake oder Erzeugung von File-API-Query-Dateien; M4 liest nur bereits vorhandene Reply-Daten
- formale Plattformfreigaben fuer macOS oder Windows (`NF-08`, `NF-09`)

Diese Abgrenzung setzt die Architekturentscheidung aus [architecture.md](./architecture.md) um: Phase 4 beginnt mit `F-05`, `F-19` und `F-24`; weitergehende Target-Graph-Auswertungen und weitere Report-Formate folgen erst danach.

## 1. Arbeitspakete

### 1.1 Build-Model-Vertraege und Kernmodelle schaerfen

Die bisherige Trennung zwischen `CompileDatabasePort` fuer die Kernanalyse und `TargetMetadataPort` fuer spaetere Zusatzsicht ist fuer M4 zu schmal. Fuer die CMake File API als zweite primaere Eingabequelle wird ein strukturierter Vertrag benoetigt, der zwischen vorhandenen, fehlenden und nur teilweise verwertbaren Build-Daten unterscheiden kann.

Fuer M4 benoetigt der Kern mindestens:

- ein quellenunabhaengiges Modell fuer Beobachtungsidentitaet und Match-Key, das von `CompileCommandsJsonAdapter` und `CmakeFileApiAdapter` nach denselben Normalisierungsregeln befuellt wird
- ein Modell fuer Target-Identitaet mit stabilem Vergleichsschluessel, menschenlesbarem Namen und technischem Typ
- ein Modell fuer die Zuordnung einer konkreten Translation-Unit-Beobachtung zu null, einem oder mehreren Targets
- ein Ergebnisobjekt fuer geladene Build-Beobachtungen inklusive Herkunft (`exact` oder `derived`) und Diagnostics
- Erweiterungen an `AnalysisResult` und `ImpactResult`, damit targetbezogene Sicht und reportweite Hinweise getrennt transportiert werden koennen

Wichtig:

- die Beobachtungsidentitaet darf nicht nur auf `source_path` basieren; Doppelbeobachtungen mit identischem Quelldateipfad, aber unterschiedlichem Build-Kontext, muessen ueber denselben adapteruebergreifend normalisierten Match-Key getrennt bleiben
- ein und dieselbe Quelldatei kann in mehreren Targets vorkommen; M4 darf daher keine implizite 1:1-Annahme zwischen Translation Unit und Target treffen
- fehlende Target-Zuordnungen sind in M4 kein Ladefehler, sondern fachliche Unvollstaendigkeit und werden als Diagnostics behandelt
- eine File-API-basierte Beobachtung darf dieselben Ergebnisarten speisen wie eine Compile-Database-Beobachtung, muss ihre Herkunft aber explizit tragen
- wenn sowohl `compile_commands.json` als auch File API vorliegen, bleibt die Compile Database in M4 die autoritative Menge fuer Ranking, Hotspots und TU-basierte Impact-Ableitung; die File API reichert nur passende Beobachtungen an, statt die Kernergebnis-Menge stillschweigend zu erweitern
- direkte Target-Abhaengigkeiten muessen in M4 noch nicht modelliert werden; fuer den ersten Ausbau reichen Target-Beschreibung und Source-Zuordnung
- Report-Adapter duerfen keine eigenen Target-Matches berechnen; die Aufloesung bleibt Aufgabe von Port, Adapter und Kern

Vorgesehene Artefakte:

- neue Modelle unter `src/hexagon/model/` fuer Target-Beschreibung, Target-Zuordnung und targetbezogene Impact-Sicht
- Anpassung von `src/hexagon/model/analysis_result.h`, `src/hexagon/model/impact_result.h` und gegebenenfalls `src/hexagon/model/translation_unit.h`
- Ersetzung von `src/hexagon/ports/driven/compile_database_port.h` und `src/hexagon/ports/driven/target_metadata_port.h` durch einen gemeinsamen Vertrag wie `build_model_port.h`

**Ergebnis**: Der Kern besitzt einen belastbaren, diagnostikfaehigen Vertrag fuer quellenunabhaengige Build-Beobachtungen, ohne bereits einen vollstaendigen Target-Graphen einzufuehren.

### 1.2 `CmakeFileApiAdapter` fuer CMake-Reply-Daten umsetzen

Als erster M4-Adapter fuer `S-02` wird die CMake File API angebunden. Der Adapter soll Reply-Daten lesen, daraus Translation-Unit-Beobachtungen, Compile-Kontexte und Target-Zuordnungen extrahieren und diese Informationen in Kernmodelle ueberfuehren.

Entscheidung fuer M4:

- gelesen werden ausschliesslich bereits vorhandene File-API-Reply-Daten
- der Nutzer uebergibt explizit einen Pfad; M4 fuehrt kein implizites Auffinden, kein Query-Writing und keinen CMake-Lauf durch
- als Eingabe werden entweder das Build-Verzeichnis mit `.cmake/api/v1/reply` oder direkt das Reply-Verzeichnis akzeptiert
- relevant fuer M4 sind vor allem Source-Dateien, Compile-Gruppen, Include-Verzeichnisse, Compile-Command-Fragmente, Toolchain-Informationen sowie Target-Name und Target-Typ; tiefergehende Graph-Informationen duerfen intern gelesen, muessen aber noch nicht ausgewertet werden

Der Adapter muss mindestens folgende Faelle robust behandeln:

- fehlender oder nicht lesbarer Reply-Pfad
- syntaktisch ungueltige JSON-Dateien
- strukturell gueltige, aber fuer M4 unzureichende Reply-Daten, zum Beispiel ohne auswertbare Codemodel-Information
- Quellen, die in der File API vorhanden sind, aber keinen fuer die Kernanalyse nutzbaren Compile-Kontext liefern
- Dateien, die in mehreren Targets auftauchen
- Reply-Daten, aus denen sich kein belastbarer Top-Level-Source-Root fuer den File-API-Only-Pfad von `impact` ableiten laesst

Fuer die Zuordnung gelten in M4 folgende Regeln:

- verglichen wird auf lexikalisch normalisierten Pfadschluesseln, nicht auf rohen Anzeige-Strings
- die Zuordnung laeuft immer von einer konkreten normalisierten Beobachtung zur Target-Menge; doppelte Beobachtungen bleiben getrennt
- die File API liefert fuer jede nutzbare Beobachtung denselben quellenunabhaengigen Match-Key wie die Compile Database; fuer M4 wird dafuer mindestens normalisierter Quellpfad plus normalisierter Kontextschluessel verwendet
- Mehrfachtreffer werden deterministisch gespeichert und spaeter sortiert ausgegeben

Vorgesehene Artefakte:

- neue Dateien `src/adapters/input/cmake_file_api_adapter.h` und `src/adapters/input/cmake_file_api_adapter.cpp`
- Anpassung von `src/adapters/CMakeLists.txt`
- neue Adapter-Tests fuer den File-API-Pfad

**Ergebnis**: `cmake-xray` kann eine reale, versionierbare zweite Quelle fuer Kernanalyse und Target-Sicht lesen, ohne den Kern an CMake-spezifische JSON-Strukturen zu koppeln.

### 1.3 Projekt- und Impact-Analyse um Target-Sicht erweitern

Die neue Eingabequelle ist erst nutzbar, wenn Projekt- und Impact-Analyse sie in ihre Ergebnisobjekte integrieren. M4 erweitert deshalb die bestehenden Analysekern-Pfade um eine optionale zweite primaere Datengrundlage.

Fuer `analyze` bedeutet das:

- die bestehende Ranking- und Hotspot-Logik aus M2/M3 bleibt unveraendert die fachliche Basis
- wenn `compile_commands.json` und File API gemeinsam vorliegen, bildet die Compile Database die autoritative `exact`-Beobachtungsmenge; Ranking und Hotspots arbeiten in M4 auf dieser Menge weiter, waehrend File-API-Daten nur passende Beobachtungen um Target-Kontext und Diagnostics anreichern
- Beobachtungen, die nur in der File API vorkommen und keinen Match zur exakten Beobachtungsmenge haben, werden im Mischpfad in M4 nicht in Ranking oder Hotspots aufgenommen, sondern als reportweite Diagnostics sichtbar gemacht
- vorhandene Target-Zuordnungen werden den betroffenen Translation Units als Zusatzkontext beigefuegt
- fehlende oder partielle Zuordnungen werden als Diagnostics sichtbar, ohne Ranking oder Hotspot-Analyse ungueltig zu machen

Fuer `impact` bedeutet das:

- wenn `compile_commands.json` und File API gemeinsam vorliegen, wird die Menge direkt und heuristisch betroffener Translation Units weiterhin aus der `exact`-Beobachtungsmenge der Compile Database abgeleitet; die File API liefert dazu nur Target-Zuordnungen und Zusatzdiagnostics
- wenn nur die File API vorliegt, wird dieselbe Impact-Logik auf der `derived`-Beobachtungsmenge ausgefuehrt
- betroffene Targets werden als Vereinigungsmenge aller Targets gebildet, die zu betroffenen Translation Units gehoeren
- die Herkunft der Evidenz bleibt sichtbar: Targets, die ueber mindestens eine direkt betroffene Translation Unit erreicht werden, gelten in M4 als `direct`; Targets, die ausschliesslich ueber heuristisch betroffene Translation Units erreicht werden, gelten als `heuristic`
- erscheint dasselbe Target in beiden Evidenzarten, gewinnt `direct`; das Target wird genau einmal in der staerkeren Klasse gefuehrt
- transitive Ausbreitung ueber Target-Abhaengigkeiten ist ausdruecklich nicht Bestandteil von M4

Wichtig:

- ohne geladene File-API-Daten bleiben die Ergebnisobjekte fachlich auf M3-Niveau; neue Felder koennen leer sein, duerfen aber keine geaenderte Standardausgabe erzwingen
- mit ausschliesslich File-API-basierter Eingabe bleiben Ranking, Hotspots und Impact fachlich verfuegbar; Ergebnisobjekte kennzeichnen die Beobachtungen als `derived`
- der Mischpfad kennzeichnet seine Kernergebnisse weiterhin als `exact`; die zusaetzliche File-API-Nutzung wird getrennt davon als geladene oder partielle Target-Metadatenlage sichtbar gemacht
- Diagnostics fuer fehlende Target-Abdeckung bleiben reportweit; einzelne Translation Units sollen nicht mit redundanten Standardhinweisen ueberfrachtet werden
- Sortierung und Priorisierung muessen reproduzierbar bleiben; neue Target-Listen werden deterministisch nach `(classification, display_name, type)` erzeugt

Vorgesehene Artefakte:

- Anpassung von `src/hexagon/services/project_analyzer.*`
- Anpassung von `src/hexagon/services/impact_analyzer.*`
- gegebenenfalls Anpassung der Driving Ports, falls fuer optionale Metadaten-Eingaben Request-Objekte oder zusaetzliche Parameter noetig werden

**Ergebnis**: Die bestehende Analyse bleibt auf dem M3-Pfad rueckwaertskompatibel und gewinnt zusaetzlich einen zweiten, klar gekennzeichneten Eingabepfad mit targetbezogenem Mehrwert.

### 1.4 Konsolen- und Markdown-Reports fuer Targets erweitern

M4 fuegt keine neuen Ausgabeformate hinzu, erweitert aber die bestehenden M3-Reporter um eine klar definierte Target-Sicht.

Fuer die Report-Vertraege gilt:

- ohne File-API-Daten bleibt die M3-Ausgabe fuer identische Eingaben bytegleich
- sobald File-API-Daten beteiligt sind, machen Konsole und Markdown in der Uebersicht die Beobachtungsherkunft (`exact` oder `derived`) und den Ladezustand der Target-Metadaten explizit sichtbar
- mit File-API-Daten werden bestehende TU-Zeilen in Konsole und Markdown um eine deterministisch sortierte Target-Anzeige erweitert, zum Beispiel als Suffix ` [targets: app, core]`
- `impact` erhaelt zusaetzlich eigene Target-Abschnitte fuer `direct` und `heuristic`
- `analyze` fuehrt in M4 keine eigenstaendige Target-Rangliste ein; die erste Target-Sicht bleibt absichtlich kontextgebunden an Ranking und Hotspots

Fuer Markdown bedeutet das konkret:

- der bestehende M3-Vertrag bleibt fuer Aufrufe ohne File-API-Daten unveraendert
- `analyze` ergaenzt in der Uebersicht genau dann Target-Zeilen, wenn File-API-Daten geladen wurden, mindestens:
  - `Observation source: exact|derived`
  - `Target metadata: loaded|partial`
  - `Translation units with target mapping: <mapped> of <total>`
- TU-Zeilen in `## Translation Unit Ranking` und in `## Include Hotspots` koennen bei vorhandener Zuordnung den Target-Suffix tragen
- `impact` ergaenzt in der Uebersicht bei geladener Target-Sicht mindestens:
  - `Observation source: exact|derived`
  - `Target metadata: loaded|partial`
  - `Affected targets: <count>`
- `impact` fuegt vor `## Diagnostics` die neuen Abschnitte `## Directly Affected Targets` und `## Heuristically Affected Targets` ein; fuer leere Abschnitte gelten explizite Leersaetze analog zum M3-Stil

Fuer die Konsolenausgabe gilt dieselbe fachliche Semantik:

- keine neuen Farben oder volatilen Metadaten
- klare, diffbare Textstruktur
- sichtbare Uebersichtszeilen fuer `observation source: exact|derived` und `target metadata: loaded|partial`, sobald File-API-Daten beteiligt sind
- deterministische Sortierung innerhalb der Target-Listen

Vorgesehene Artefakte:

- Anpassung von `src/adapters/output/console_report_adapter.*`
- Anpassung von `src/adapters/output/markdown_report_adapter.*`
- Erweiterung der Reporter-Tests und Golden-Outputs

**Ergebnis**: Die zweite Eingabequelle und ihre Target-Sicht werden in den bestehenden Report-Pfaden sichtbar, ohne fuer M4 einen dritten Analysemodus oder ein neues Ausgabeformat einzufuehren.

### 1.5 CLI, Eingabepfad und Fehlersemantik fuer die zweite Eingabequelle festlegen

Der neue Metadatenpfad muss fuer Nutzer klar und fuer bestehende Automation stabil sein. M4 erweitert deshalb die vorhandenen Unterkommandos, statt einen separaten Target-Unterbaum oder implizite Suchheuristiken einzufuehren.

Entscheidung fuer M4:

- `analyze` und `impact` erhalten die optionale Angabe `--cmake-file-api <path>`
- `analyze` und `impact` verlangen mindestens eine primaere Eingabequelle; erlaubt sind `--compile-commands`, `--cmake-file-api` oder beide gemeinsam
- ohne diese Option bleibt das Verhalten fachlich und textuell auf M3-Niveau
- die Option ist explizit; es gibt keine automatische Ableitung aus `--compile-commands`
- die Option muss auch ohne `--compile-commands` nutzbar sein, wenn die File-API-Lage fuer die Kernanalyse ausreicht
- fuer `impact` gilt: relative `--changed-file`-Pfade werden mit `--compile-commands` wie bisher relativ zum Verzeichnis der Compile Database interpretiert; im File-API-Only-Pfad werden sie relativ zum im Codemodel angegebenen Top-Level-Source-Verzeichnis interpretiert
- `--format console|markdown` bleibt unveraendert; M4 fuehrt kein neues Formatflag ein

Fehlerregeln fuer M4:

- ein nicht lesbarer expliziter `--cmake-file-api`-Pfad wird wie andere nicht lesbare Eingaben als Exit-Code `3` behandelt
- syntaktisch ungueltige Reply-Daten sowie strukturell unlesbare Reply-Sets werden wie andere ungueltige Eingaben mit Exit-Code `4` behandelt, auch wenn zusaetzlich eine `compile_commands.json` angegeben wurde
- im File-API-Only-Pfad werden strukturell gueltige, aber fuer die Kernanalyse unzureichende Reply-Daten ebenfalls als Exit-Code `4` behandelt; dazu zaehlen insbesondere fehlende nutzbare Beobachtungen oder ein fehlender Top-Level-Source-Root fuer relative `impact`-Pfade
- partielle, aber parsebare File-API-Daten liefern genau dann Exit-Code `0`, wenn sie wenigstens einen fachlich nutzbaren Teilbeitrag fuer den gewaehlten Aufruf liefern; im Mischpfad reichen dafuer zum Beispiel teilweise vorhandene Target-Zuordnungen, im File-API-Only-Pfad mindestens eine fuer die Kernanalyse nutzbare Beobachtung
- bestehende M3-Codes fuer CLI-Verwendungsfehler (`2`), Report-Schreibfehler (`1`) und Compile-Database-Probleme (`3`/`4`) bleiben erhalten

Die Hilfe und Fehlermeldungen sollen mindestens klaeren:

- was `--cmake-file-api` erwartet
- dass als Wert entweder das Build-Verzeichnis oder direkt das Reply-Verzeichnis akzeptiert wird
- dass der Pfad optional ist
- dass mindestens eine primaere Eingabequelle angegeben werden muss
- dass ohne diese Option keine Target-Sicht berechnet wird
- wie relative `--changed-file`-Pfade im Compile-Database- und im File-API-Only-Pfad aufgeloest werden
- welcher naechste Schritt bei fehlenden oder ungueltigen Reply-Daten sinnvoll ist

Vorgesehene Artefakte:

- Anpassung von `src/adapters/cli/cli_adapter.*`
- gegebenenfalls punktuelle Anpassung von `src/adapters/cli/exit_codes.h`
- Anpassung von `src/main.cpp`
- Erweiterung der CLI-/E2E-Tests fuer neue Hilfetexte und Fehlerpfade

**Ergebnis**: Die neue Eingabequelle ist explizit, rueckwaertskompatibel und mit den bestehenden Fehlerklassen vereinbar.

### 1.6 Referenzdaten, Tests und Golden-Outputs fuer M4 ausbauen

M4 fuehrt mit der CMake File API eine zweite reale Datengrundlage ein. Damit steigen sowohl die Anzahl der Randfaelle als auch das Risiko, dass Beobachtungen, Herkunftsklassen und Target-Zuordnungen durch Pfad- oder Sortierdetails instabil werden.

Mindestens benoetigt:

- Adapter-Tests fuer `CmakeFileApiAdapter`, insbesondere fuer valide Codemodel-Reply-Daten, nicht lesbare Pfade, ungueltiges JSON und strukturell unzureichende Replies
- Adapter- und Kern-Tests fuer den adapteruebergreifend identischen Beobachtungs-Match-Key, damit Compile Database und File API dieselbe TU-Identitaet verwenden
- Kern-Tests fuer null, eine und mehrere Target-Zuordnungen pro Translation-Unit-Beobachtung
- Kern-Tests fuer den Mischpfad, die bestaetigen, dass die Compile Database die autoritative `exact`-Grundlage bleibt und File-API-Daten nur passende Beobachtungen anreichern
- Kern-Tests fuer targetbezogene Impact-Ableitung, insbesondere fuer gemischte direkte und heuristische Evidenz
- CLI- und Kern-Tests fuer die Pfadauflosung von `impact`: relativ zum Compile-Database-Verzeichnis im Mischpfad und relativ zum File-API-Source-Root im File-API-Only-Pfad
- Reporter-Tests fuer Target-Suffixe in Ranking- und Hotspot-Eintraegen sowie fuer neue Target-Abschnitte im Impact-Report
- Reporter-Tests fuer sichtbare Uebersichtsangaben zu `Observation source: exact|derived` und `Target metadata: loaded|partial`
- End-to-End-Tests fuer `analyze` und `impact` mit und ohne `--cmake-file-api`, jeweils mit Build-Verzeichnis und direktem Reply-Verzeichnis
- Regressionstests, die bestaetigen, dass M3-Ausgaben ohne File-API-Daten unveraendert bleiben

Fuer Referenzdaten gilt in M4:

- neue Fixtures unter `tests/e2e/testdata/m4/` enthalten je nach Fall entweder nur eine versionierte minimale File-API-Reply-Struktur oder eine Kombination aus `compile_commands.json`, relevanten Quelldateien und File-API-Reply-Daten
- mindestens ein Fixture muss partielle Target-Abdeckung enthalten
- mindestens ein Fixture muss Mehrfachzuordnungen derselben Quelldatei zu mehreren Targets abdecken
- mindestens ein Fixture muss eine permutierte oder anders sortierte Reply-Struktur abdecken, damit die Sortierung der Target-Listen nicht von Dateireihenfolgen abhaengt
- mindestens ein Fixture muss den File-API-Only-Pfad mit relativem `--changed-file` und eindeutigem Top-Level-Source-Root absichern

**Ergebnis**: Die neuen M4-Pfade sind ueber reproduzierbare Fixtures und bytegenaue Reporter-Erwartungen abgesichert.

### 1.7 README, Beispiele und Release-Artefakte fuer `v1.1.0` aktualisieren

Mit M4 wird das Produkt nicht breiter im Formatangebot, aber fachlich tiefer in der Build-Sicht. Diese neue Bedienung muss dokumentiert werden, ohne den M3-Standardpfad zu verwischen.

Die README soll fuer M4 mindestens enthalten:

- den aktualisierten Projektstatus mit Verweis auf den MVP-Stand und den neuen M4-Ausbau
- Beispiele fuer `analyze` und `impact` mit `--cmake-file-api`
- klaren Hinweis, dass Target-Aussagen optional sind und nur bei bereitgestellten Metadaten erscheinen
- kurze Erklaerung, dass M4 noch keine transitive Target-Graph-Analyse durchfuehrt
- Verweis auf Beispielausgaben fuer Konsole und Markdown

Zusaetzlich sind bei Abschluss von M4 zu aktualisieren:

- `CHANGELOG.md` fuer `v1.1.0`
- `src/hexagon/model/application_info.h`
- Root-`CMakeLists.txt`
- `docs/releasing.md`, sodass der Release-Ablauf die Pruefung des optionalen File-API-Pfads einschliesst
- gegebenenfalls `docs/roadmap.md`, falls der M4-Meilenstein dort vom Entwurf in einen konkreteren Stand ueberfuehrt wird

Sinnvolle Beispielartefakte unter `docs/examples/`:

- `analyze-console-targets.txt`
- `analyze-report-targets.md`
- `impact-console-targets.txt`
- `impact-report-targets.md`

**Ergebnis**: Nutzungsdokumentation, Beispielausgaben und Release-Hinweise bilden den ersten post-MVP-Ausbau fuer die zweite Eingabequelle und die erste Target-Sicht konsistent ab.

## 2. Zielartefakte

Folgende Dateien oder Dateigruppen sollen nach M4 voraussichtlich neu entstehen oder substanziell ueberarbeitet sein:

| Bereich | Zielartefakte |
|---|---|
| Kernmodelle | neue oder erweiterte Dateien unter `src/hexagon/model/` fuer Targets, Zuordnungen und targetbezogenen Impact; Anpassungen von `analysis_result.h`, `impact_result.h`, gegebenenfalls `translation_unit.h` |
| Driven Port | neuer oder umgebauter gemeinsamer Vertrag unter `src/hexagon/ports/driven/build_model_port.h` |
| Input-Adapter | neue Dateien `src/adapters/input/cmake_file_api_adapter.*` |
| Services | `src/hexagon/services/project_analyzer.*`, `src/hexagon/services/impact_analyzer.*` |
| Output-Adapter | `src/adapters/output/console_report_adapter.*`, `src/adapters/output/markdown_report_adapter.*` |
| CLI | `src/adapters/cli/cli_adapter.*`, gegebenenfalls `src/adapters/cli/exit_codes.h` |
| Composition Root | `src/main.cpp` |
| Build | `src/adapters/CMakeLists.txt`, `tests/CMakeLists.txt` |
| Adapter-Tests | neue oder erweiterte Tests unter `tests/adapters/` fuer File API und Reporter-Erweiterungen |
| Kern-Tests | neue oder erweiterte Tests unter `tests/hexagon/` fuer Target-Zuordnung und targetbezogenen Impact |
| E2E-Tests | neue oder erweiterte Tests unter `tests/e2e/` fuer Aufrufe mit und ohne `--cmake-file-api` |
| Referenzdaten | neue Fixtures unter `tests/e2e/testdata/m4/` mit versionierter minimaler File-API-Reply-Struktur |
| Beispielausgaben | neue Dateien unter `docs/examples/` |
| Produktdokumentation | `README.md`, `CHANGELOG.md`, `docs/releasing.md`, gegebenenfalls `docs/roadmap.md` |

Hinweis: Die konkreten Dateinamen duerfen von dieser Zielstruktur abweichen, solange die Trennung zwischen Kern, optionaler Metadatenquelle, Report-Adaptern und CLI erhalten bleibt.

## 3. Reihenfolge

| Schritt | Arbeitspaket | Abhaengig von |
|---|---|---|
| 1a | 1.1 Build-Model-Vertraege und Kernmodelle schaerfen | M3 |
| 1b | 1.5 CLI- und Fehlersemantik festlegen | M3 |
| 2 | 1.2 `CmakeFileApiAdapter` umsetzen | 1a, 1b |
| 3 | 1.3 Projekt- und Impact-Analyse um Target-Sicht erweitern | 1a, 1b, 2 |
| 4 | 1.4 Konsolen- und Markdown-Reports fuer Targets erweitern | 3 |
| 5 | 1.6 Referenzdaten, Tests und Golden-Outputs ausbauen | 2, 3, 4 |
| 6 | 1.7 README, Beispiele und Release-Artefakte aktualisieren | 4, 5 |

Schritte 1a und 1b koennen parallel bearbeitet werden. Die Kernvertraege muessen frueh feststehen, damit Adapter und Reporter auf denselben Target-Begriff arbeiten. Die CLI- und Fehlersemantik sollte ebenfalls frueh entschieden sein, weil sie den Rueckwaertskompatibilitaetsrahmen fuer den gesamten M4-Ausbau definiert. Test- und Fixture-Arbeit aus 1.6 kann nach Fertigstellung des Adapters teilweise parallel zum Reporter-Ausbau beginnen, sollte aber die finalen Report-Vertraege aus 1.4 respektieren.

## 4. Pruefung

M4 ist abgeschlossen, wenn folgende Pruefwege erfolgreich durchlaufen:

**Lokal**:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/cmake-xray --help
./build/cmake-xray analyze --help
./build/cmake-xray impact --help
./build/cmake-xray analyze --compile-commands tests/e2e/testdata/m2/basic_project/compile_commands.json
./build/cmake-xray analyze --cmake-file-api tests/e2e/testdata/m4/with_targets/build
./build/cmake-xray analyze --cmake-file-api tests/e2e/testdata/m4/with_targets/build/.cmake/api/v1/reply
./build/cmake-xray analyze --compile-commands tests/e2e/testdata/m4/with_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/with_targets/build
./build/cmake-xray analyze --compile-commands tests/e2e/testdata/m4/with_targets/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/with_targets/build --format markdown
./build/cmake-xray impact --cmake-file-api tests/e2e/testdata/m4/with_targets/build --changed-file include/common/config.h
./build/cmake-xray impact --cmake-file-api tests/e2e/testdata/m4/with_targets/build/.cmake/api/v1/reply --changed-file include/common/config.h
./build/cmake-xray impact --compile-commands tests/e2e/testdata/m4/with_targets/compile_commands.json --changed-file include/common/config.h --cmake-file-api tests/e2e/testdata/m4/with_targets/build
./build/cmake-xray impact --compile-commands tests/e2e/testdata/m4/partial_targets/compile_commands.json --changed-file include/common/config.h --cmake-file-api tests/e2e/testdata/m4/partial_targets/build --format markdown
./build/cmake-xray analyze --compile-commands tests/e2e/testdata/m4/with_targets/compile_commands.json --cmake-file-api /nonexistent/reply
./build/cmake-xray analyze --compile-commands tests/e2e/testdata/m4/invalid_file_api/compile_commands.json --cmake-file-api tests/e2e/testdata/m4/invalid_file_api/build
ctest --test-dir build --output-on-failure
```

Die Pruefung soll insbesondere bestaetigen:

- Aufrufe ohne `--cmake-file-api` bleiben fachlich und textuell auf M3-Niveau
- bei gemeinsamem Einsatz von `compile_commands.json` und File API bleibt die Compile Database die autoritative `exact`-Grundlage fuer Ranking, Hotspots und TU-basierte Impact-Ableitung
- valide File-API-Reply-Daten werden fuer `analyze` und `impact` geladen und deterministisch ausgewertet
- Build-Verzeichnis und direktes Reply-Verzeichnis werden als Werte fuer `--cmake-file-api` gleichwertig akzeptiert
- Mehrfachzuordnungen einer Quelldatei zu mehreren Targets bleiben sichtbar und reproduzierbar sortiert
- Target-Ausgaben unterscheiden direkte und heuristische Evidenz, ohne transitive Zielgraph-Ableitungen vorzutaeuschen
- nicht lesbare explizite Metadatenpfade liefern Exit-Code `3`
- syntaktisch oder strukturell ungueltige explizite Metadaten liefern Exit-Code `4`
- partielle, aber parsebare Metadaten liefern Exit-Code `0` und reportweite Diagnostics
- File-API-Only-`impact` loest relative `--changed-file`-Pfade ueber den im Codemodel angegebenen Top-Level-Source-Root auf
- Konsole und Markdown machen die Beobachtungsherkunft (`exact` oder `derived`) und den Status der Target-Metadaten sichtbar
- Konsole und Markdown geben dieselbe targetbezogene Fachinformation wieder
- Sortierung bleibt stabil, auch wenn Compile-Database-Eintraege oder File-API-Dateien anders angeordnet sind

## 5. Rueckverfolgbarkeit

| Planbereich | Lastenheft-Kennungen |
|---|---|
| Build-Model-Vertrag und CMake-File-API-Eingabe | `F-05`, `F-19`, `F-35`, `S-02`, `NF-14` |
| Erweiterung von `analyze` und `impact` um Target-Sicht | `F-19`, `F-24`, `F-21`, `F-22`, `F-23` |
| Konsolen- und Markdown-Reports | `F-26`, `F-27`, `F-36`, `NF-13` |
| CLI, Hilfe und Fehlermeldungen | `F-31`, `F-32`, `F-33`, `F-34`, `NF-02`, `NF-14` |
| Reproduzierbarkeit und Testbarkeit | `NF-10`, `NF-15`, `NF-19` |
| README und Beispielausgaben | `NF-16`, `NF-17`, `NF-18` |

## 6. Entschiedene und offene Punkte

### 6.1 Entschieden

| Frage | Entscheidung | Verweis |
|---|---|---|
| Fokus von M4 | M4 bleibt auf `F-05`, `F-19` und `F-24` fokussiert; weitergehende Target-Graph-Analysen und weitere Ausgabeformate bleiben ausserhalb dieses Milestones | 0.3 |
| Quelle fuer die zweite Eingabequelle | Erster Adapter ist die CMake File API; gelesen werden nur vorhandene Reply-Daten, ohne CMake-Lauf oder Query-Erzeugung | AP 1.2 |
| CLI fuer die zweite Eingabequelle | `analyze` und `impact` erhalten die optionale Angabe `--cmake-file-api <path>`; es gibt keine implizite Auto-Erkennung aus `--compile-commands` | AP 1.5 |
| Rueckwaertskompatibilitaet | Ohne `--cmake-file-api` bleiben die bestehenden M3-Ausgaben unveraendert | 0.1, AP 1.4, AP 1.5 |
| Granularitaet der Zuordnung | Target-Zuordnung erfolgt pro Translation-Unit-Beobachtung und darf null, ein oder mehrere Targets liefern | AP 1.1, 1.2 |
| Merge-Policy fuer beide Eingabequellen | Im Mischpfad bleibt `compile_commands.json` die autoritative `exact`-Grundlage fuer Kernergebnisse; die File API reichert nur passende Beobachtungen mit Target-Kontext an | AP 1.1, 1.3 |
| Semantik targetbezogener Impact-Ausgabe | `direct` und `heuristic` beschreiben in M4 die Herkunft der Evidenz ueber betroffene Translation Units, nicht transitive Abhaengigkeiten im Target-Graph | AP 1.3, 1.4 |
| Konfliktfall direkte plus heuristische Evidenz | Erreicht dasselbe Target beide Evidenzarten, wird es genau einmal als `direct` ausgegeben | AP 1.3 |
| Aufloesung relativer `impact`-Pfade | Mit `--compile-commands` relativ zum Verzeichnis der Compile Database, im File-API-Only-Pfad relativ zum Top-Level-Source-Root aus dem Codemodel | AP 1.5 |
| Sichtbarkeit der Datenherkunft | Sobald File-API-Daten beteiligt sind, machen Konsole und Markdown die Beobachtungsherkunft (`exact` oder `derived`) und den Ladezustand der Target-Metadaten sichtbar | AP 1.3, 1.4 |
| Fehlerbehandlung fuer explizite Metadaten | Nicht lesbare Pfade liefern Exit-Code `3`; syntaktisch ungueltige oder strukturell unlesbare Reply-Sets liefern `4`; partielle, aber fachlich nutzbare Daten bleiben bei `0` und werden ueber Diagnostics erklaert | AP 1.5 |
| Analyze-Report in M4 | Keine eigenstaendige Target-Rangliste; die erste Target-Sicht bleibt an Ranking- und Hotspot-Kontext gekoppelt | AP 1.4 |

### 6.2 Offen

- Soll ein spaeterer Phase-4-Meilenstein fuer transitive Target-Analysen ein eigenes Unterkommando einfuehren oder weiter auf `analyze` und `impact` aufbauen?
- Soll fuer spaetere weitere Metadatenquellen neben der CMake File API ein generischerer CLI-Name als `--cmake-file-api` eingefuehrt werden, oder bleibt diese Option dauerhaft die primaere Benutzeroberflaeche?
- Wie sollen HTML-, JSON- und DOT-Reporter spaeter auf der M4-Target-Sicht aufsetzen, ohne ihre jeweiligen Formatvertraege vorzeitig an M4-Textdarstellungen zu koppeln?
