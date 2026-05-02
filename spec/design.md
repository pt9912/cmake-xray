# Design - cmake-xray

## 0. Dokumenteninformationen

| Feld | Wert |
|---|---|
| Dokument | Design `cmake-xray` |
| Version | `0.4` |
| Stand | `2026-04-22` |
| Status | Entwurf |
| Referenzen | [Lastenheft](./lastenheft.md), [Architektur](./architecture.md), [Phasenplan](../docs/planning/roadmap.md) |

### 0.1 Zweck
Dieses Dokument konkretisiert die fachliche und benutzerbezogene Ausgestaltung des Produkts auf Basis der Anforderungen im [Lastenheft](./lastenheft.md). Es beschreibt beabsichtigtes Verhalten, Ausgabemodelle und Interaktionsmuster, ohne die technische Implementierungstiefe des Architekturdokuments vorwegzunehmen.

### 0.2 Nicht-Ziel
Dieses Dokument definiert keine abschliessenden Klassen-, Modul- oder Build-Strukturen. Die hexagonale Zerlegung in Ports und Adapter wird in [architecture.md](./architecture.md) beschrieben. Organisatorische Randbedingungen wie Repository-Hosting, Lizenzwahl und formale Abnahmekriterien werden hier nur insoweit beruehrt, wie sie sichtbares Produktverhalten beeinflussen; ihre primaere Heimat bleiben Lastenheft, Roadmap und Projektdokumentation.

## 1. Designprinzipien

- Ergebnisse muessen fuer Nutzer mit wenig Spezialwissen interpretierbar sein.
- Jeder Bericht soll eine klare Verbindung zwischen Beobachtung und Datengrundlage herstellen.
- Fehlende Daten sollen sichtbar gemacht werden, statt scheinbare Praezision vorzutaeuschen.
- Das CLI-Verhalten soll fuer lokale Nutzung und CI gleichartig bleiben.
- Jede spaetere Designentscheidung soll auf Lastenheft-Kennungen rueckfuehrbar sein.

## 2. Nutzerinteraktion

### 2.1 Primaere Nutzungsmodi

| Modus | Ziel | Relevante Anforderungen |
|---|---|---|
| Projektanalyse | Auffaellige Translation Units und Include-Hotspots sichtbar machen | `F-06` bis `F-15`, `F-26`, `F-27` |
| Impact-Analyse | Auswirkungen einer Datei auf Translation Units und spaeter Targets abschaetzen | `F-21` bis `F-25` |
| Berichtserzeugung | Ergebnisse fuer Menschen und spaeter fuer Maschinen exportieren | `F-26` bis `F-30` |
| CLI-Hilfe und Diagnose | Bedienbarkeit, Fehlererklaerung und Nachvollziehbarkeit sicherstellen | `F-31` bis `F-34`, `F-39`, `F-40`, `NF-01`, `NF-02` |

### 2.2 Erwartete CLI-Eigenschaften

- Unterkommandos sollen nach Nutzerziel statt nach interner Verarbeitung benannt werden.
- `--help` soll fuer Hauptkommando und Unterkommandos eine knappe, aber vollstaendige Orientierung geben.
- Konsolenausgaben sollen standardmaessig knapp, aber interpretierbar sein.
- Ein detailreicher Modus soll zusaetzliche Diagnoseinformationen liefern.
- Exit-Codes sollen zwischen Verwendungsfehlern, nicht lesbaren Eingaben, ungueltigen Eingaben und erfolgreicher Analyse unterscheiden.
- Fehlermeldungen sollen den konkreten Eingabefehler benennen und einen naechsten Schritt vorschlagen.
- Alle Eingaben und Steueroptionen sollen ueber CLI-Parameter uebergeben werden; das Produkt soll keine interaktive Laufzeitumgebung voraussetzen.

### 2.3 Eingabegrundlagen

Fuer die Kernanalysen werden langfristig zwei primaere CMake-Eingabequellen unterstuetzt:

- `compile_commands.json` als compilernahe und nach Moeglichkeit exakte Datengrundlage
- die CMake File API als zweite CMake-native Datengrundlage fuer Generatoren, bei denen keine `compile_commands.json` bereitsteht

Beide Quellen sollen dieselben fachlichen Analysearten tragen koennen. Die Datengrundlage bleibt jedoch Teil des Ergebniskontexts:

- Berichte sollen sichtbar machen, ob Kennzahlen und Ableitungen auf `exact` oder `derived` Beobachtungen beruhen.
- Ergebnisse aus abgeleiteten Daten duerfen nicht stillschweigend denselben Praezisionsanspruch tragen wie compilernahe Eingaben.
- Fehlt `compile_commands.json`, soll eine ausreichend vollstaendige CMake-File-API-Lage fuer die Kernanalysen genutzt werden, statt allein wegen der fehlenden Compilation Database abzubrechen.
- Bereits beim Einlesen soll sichtbar unterscheidbar bleiben, ob Eingaben nicht lesbar, syntaktisch ungueltig, strukturell unzureichend oder nur fachlich partiell verwertbar sind.
- Jede nutzbare Beobachtung muss mindestens Quelldatei, Arbeitskontext und einen fuer die Analyse nutzbaren Compile-Kontext transportieren, damit spaetere Auswertungen dieselbe fachliche Grundlage teilen.

## 3. Design der Analyseergebnisse

### 3.1 Translation-Unit-Auswertung

Die Auswertung soll fuer jede auffaellige Translation Unit mindestens folgende Fragen beantworten:

- Warum ist diese Datei auffaellig?
- Welche Kennzahlen fuehren zur Einstufung?
- Wie verhaelt sich die Datei relativ zu anderen Translation Units?

Geplante Ergebnisbausteine:

| Baustein | Beschreibung | Relevante Anforderungen |
|---|---|---|
| Rang | Position innerhalb der auffaelligen Translation Units | `F-07` |
| Kennzahlen | Werte, die zur Einstufung herangezogen wurden | `F-08`, `F-09` |
| Kontext | Einordnung relativ zu anderen Dateien oder Grenzwerten | `F-09`, `F-10` |

Fuer das erste Ranking werden standardmaessig drei compile-kontext-basierte Kennzahlen verwendet:

- `arg_count`: Anzahl der Argumente des Compile-Aufrufs
- `include_path_count`: Anzahl erkannter Include-Suchpfade aus `-I`, `-isystem` und `-iquote`
- `define_count`: Anzahl gesetzter Praeprozessor-Defines aus `-D`

Diese Minimalmenge ist fuer den MVP ausreichend, weil sie ohne weitere Quelltext- oder Include-Heuristiken aus einer compilernahen oder abgeleiteten Buildbeschreibung verfuegbar ist. Include-basierte Kennzahlen wie Anzahl aufgeloester Includes oder Include-Tiefe koennen spaeter als zusaetzlicher Kontext ergaenzt werden, sollen aber die erste Rangfolge nicht voraussetzen.

### 3.2 Include-Hotspots

Ein Include-Hotspot soll nicht nur als Headername erscheinen, sondern als beobachtbares Problemobjekt mit Kontext:

- Header-Bezeichnung
- Anzahl betroffener Translation Units
- Liste oder Auswahl der betroffenen Translation Units
- optional Kennzeichnung intern/extern

Die Ergebnisdarstellung soll ausserdem offen fuer spaetere Verfeinerungen bleiben:

- Projekt-Header und externe Header sollen unterscheidbar gemacht werden, sobald die zugrunde liegenden Daten dies belastbar erlauben.
- Wo moeglich, soll kenntlich sein, ob ein Hotspot auf direkten Includes, indirekten Includes oder einer gemischten Sicht beruht.

### 3.3 Target-Sicht

Sobald geeignete Build-Metadaten vorliegen, soll das Design neben Translation Units auch Targets als sichtbare Kontexteinheit fuehren:

- Translation Units koennen null, einem oder mehreren Targets zugeordnet sein.
- Die erste Target-Sicht bleibt kontextgebunden an Projektanalyse und Impact-Analyse; sie ist kein eigener Analysebaum.
- Spaetere Ausbaustufen duerfen direkte Target-Abhaengigkeiten textuell ausgeben und auffaellige stark vernetzte Targets hervorheben, muessen diese Zusatzsicht aber klar von der initialen Target-Zuordnung trennen.

### 3.4 Impact-Analyse

Die Impact-Analyse soll einen Dateipfad als Eingabe nehmen und drei Ergebnisklassen unterscheiden. Diese Grundklassifikation beschreibt die Herkunft und Belastbarkeit der Evidenz und nicht die Tiefe eines spaeteren Target-Graphen:

| Ergebnistyp | Bedeutung |
|---|---|
| `direct` | mindestens eine betroffene Translation Unit oder ein betroffenes Target wurde ohne include-basierte Heuristik direkt aus bekannten Quelldatei- oder Zuordnungsdaten abgeleitet |
| `heuristic` | Betroffenheit ergibt sich aus include-basierten oder sonst abgeleiteten Beobachtungen; direkte und heuristische Treffer koennen gemeinsam vorkommen, die Gesamtsicht bleibt dann `heuristic` |
| `uncertain` | es wurden keine belastbaren Treffer gefunden und die verfuegbare Datengrundlage ist heuristisch oder partiell |

Die ausgegebene Sicht soll mindestens enthalten:

- den geprueften Dateipfad
- die voraussichtlich betroffenen Translation Units
- bei vorhandener Target-Sicht die voraussichtlich betroffenen Targets
- einen sichtbaren Hinweis, wenn Teile der Ableitung nur heuristisch oder nur partiell moeglich waren
- spaetere Erweiterungen duerfen Auswirkungen zusaetzlich nach direkter oder transitiver Target-Beziehung priorisieren, muessen diese zweite Achse aber klar von der Grundklassifikation `direct|heuristic|uncertain` trennen

### 3.5 Sortierung und Begrenzung von Ergebnislisten

Analyse-Ergebnisse wie das Translation-Unit-Ranking und die Include-Hotspot-Liste koennen bei grossen Projekten umfangreich werden. Das Design soll folgende Regeln einhalten:

- Standardmaessig werden nur die auffaelligsten Ergebnisse angezeigt (Top-N), damit die Ausgabe ohne Konfiguration nutzbar bleibt (`NF-03`).
- Die Anzahl der angezeigten Ergebnisse soll vom Nutzer steuerbar sein, zum Beispiel ueber einen CLI-Parameter.
- Wird die Ausgabe begrenzt, soll ein Hinweis die Gesamtanzahl der analysierten Eintraege nennen, damit der Nutzer die Einordnung nicht verliert.
- Die konkrete Standardanzahl (zum Beispiel Top 10 oder Top 20) wird im Pflichtenheft oder bei der Implementierung festgelegt.

Relevante Anforderungen: `F-42`, `F-10`, `F-13`, `NF-03`

### 3.6 Konfigurations- und Vergleichserweiterungen

Nicht jede Konfiguration gehoert in den MVP, aber das Design soll die Richtung spaeterer Erweiterungen vorgeben:

- Schwellenwerte fuer Einstufungen sollen konfigurierbar ergaenzt werden koennen, ohne die Grundlogik des Rankings auszutauschen.
- Einzelne Analysearten sollen spaeter gezielt aktivier- oder deaktivierbar sein, damit Nutzer Berichte auf ihr Ziel reduzieren koennen.
- Ein spaeterer Vergleich zwischen zwei Analysezeitpunkten soll auf denselben fachlichen Ergebnisobjekten aufsetzen, statt einen getrennten Berichtstyp mit eigener Semantik einzufuehren.

## 4. Berichtsausgaben

### 4.1 Konsolenausgabe

Die Konsole ist das primaere Medium des ersten Meilensteins. Die Ausgabe soll:

- ohne weitere Tools lesbar sein
- fuer CI-Logs geeignet sein
- Ergebnisse in klaren Abschnitten ausgeben
- Fehler und Warnungen deutlich vom Analyseergebnis trennen

### 4.2 Markdown-Bericht

Markdown ist das erste persistente Berichtsziel. Ein Bericht soll mindestens enthalten:

- Kontext der Analyse
- Eingabedaten oder deren Herkunft
- sichtbare Kennzeichnung der Datengrundlage (`exact` oder `derived`)
- zentrale Ergebnisse je Analyseart
- Hinweise auf fehlende oder unvollstaendige Daten

### 4.3 Spaetere Ausgabeformate

HTML, JSON und DOT sind Erweiterungen ausserhalb des MVP. Fuer diese Formate muessen spaeter Stabilitaets- und Versionsfragen gesondert beschrieben werden.

### 4.4 Nutzung in Shell und CI

Die ersten Berichtsformate muessen nicht nur lokal lesbar sein, sondern sich auch fuer Automatisierung eignen:

- Konsolen- und Markdown-Ausgaben sollen in Shell-Skripten, GitHub Actions und anderen CI-Pipelines ohne interaktive Nacharbeit nutzbar sein.
- Standardausgaben sollen diff- und logfreundlich bleiben; Zusatzdetails duerfen nur ueber explizite Optionen dazukommen.
- Nutzungsbeispiele fuer die wichtigsten Aufrufe sollen so formuliert sein, dass sie direkt in lokale Skripte oder CI-Schritte uebernommen werden koennen.

## 5. Verhalten bei fehlenden oder schwachen Daten

Dieses Produkt darf Unsicherheit nicht verstecken. Wenn Eingaben oder Ableitungen nicht ausreichen, soll das Design folgende Regeln einhalten:

- Analyse abbrechen, wenn Pflichtdaten ungueltig sind
- Analyse leer oder teilweise ausgeben, wenn Daten formal gueltig, aber inhaltlich unzureichend sind
- Analyse nicht allein deshalb abbrechen, weil keine `compile_commands.json` vorliegt, wenn eine alternative CMake-native Datengrundlage fuer dieselben Kernanalysen ausreicht
- fehlende Datengrundlagen explizit benennen
- keine nicht belegbaren Target- oder Include-Aussagen vortaeuschen

## 6. Darstellung von Unsicherheit und Datenluecken

Abschnitt 5 beschreibt, wann Unsicherheit kommuniziert wird. Dieser Abschnitt beschreibt, wie sie dem Nutzer dargestellt werden soll:

- Warnungen und Datenluecken sollen **inline beim betroffenen Ergebnis** erscheinen, nicht in einem separaten Abschnitt am Ende. So bleibt der Zusammenhang zwischen Beobachtung und Einschraenkung erhalten.
- Ein einheitliches Kennzeichnungsmuster soll verwendet werden, damit Nutzer Unsicherheiten auf einen Blick erkennen. Die konkrete Darstellung (zum Beispiel Praefix, Farbgebung oder Klammernotation) wird bei der Implementierung festgelegt.
- Im Markdown-Bericht sollen Datenluecken als eigene Zeilen oder Hinweisbloecke erscheinen, damit sie bei Diffs und Reviews sichtbar bleiben.
- Im `--verbose`-Modus sollen zusaetzlich die Gruende fuer fehlende Daten oder eingeschraenkte Ableitungen ausgegeben werden.

Relevante Anforderungen: `F-09`, `F-23`, `NF-02`, `NF-14`

## 7. Nichtfunktionale Leitplanken fuer das Design

Die fachliche Ausgestaltung soll folgende nichtfunktionalen Leitplanken respektieren:

- Der erste nutzbare Stand bleibt Linux-zentriert; spaetere Plattformen duerfen keine abweichende Fachsemantik erzwingen.
- Standardausgaben sollen ohne Zusatzkonfiguration fuer lokale Nutzung und CI sinnvoll einsetzbar sein.
- Erweiterungen fuer neue Analysearten und Ausgabeformate sollen auf denselben fachlichen Ergebnisobjekten aufsetzen.
- Performance- und Referenzumgebungsziele werden nicht im Design berechnet, aber das Design soll keine Interaktion oder Datenpflichten vorsehen, die diese Ziele unnoetig gefaehrden.
- Dokumentation und Beispielausgaben gehoeren zum Produktverhalten und muessen die wichtigsten Nutzungsmodi nachvollziehbar machen.

## 8. Rueckverfolgbarkeit

| Designbereich | Lastenheft-Kennungen |
|---|---|
| Eingabeverhalten | `F-01` bis `F-05`, `F-41`, `S-01` bis `S-03`, `RB-04` |
| Translation-Unit-Analyse | `F-06` bis `F-11` |
| Include-Analyse | `F-12` bis `F-17` |
| Target- und Impact-Sicht | `F-18` bis `F-25` |
| Ergebnisbegrenzung | `F-42`, `F-10`, `F-13`, `NF-03` |
| Berichtsausgaben | `F-26` bis `F-30`, `S-04` bis `S-08`, `NF-13`, `NF-20` |
| Unsicherheit und Datenluecken | `F-09`, `F-23`, `NF-02`, `NF-14` |
| CLI-Nutzung | `F-31` bis `F-34`, `F-39`, `F-40`, `NF-01` bis `NF-03` |
| Konfiguration | `F-35` bis `F-38` |
| Nutzung in Shell und CI | `S-09` bis `S-11`, `NF-16` bis `NF-18` |
| Nichtfunktionale Leitplanken | `NF-04` bis `NF-13` |

## 9. Offene Designfragen

- Welche Detailtiefe soll der `--verbose`-Modus liefern, ohne die Standardausgabe zu ueberladen?
- Wie sollen teilweise fehlende Include- oder Target-Daten visuell und sprachlich markiert werden?
- Welche Struktur soll ein Markdown-Bericht fuer spaetere Automatisierung und Diffbarkeit bekommen?
