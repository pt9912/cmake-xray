# Design - cmake-xray

## 0. Dokumenteninformationen

| Feld | Wert |
|---|---|
| Dokument | Design `cmake-xray` |
| Version | `0.3` |
| Stand | `2026-04-22` |
| Status | Entwurf |
| Referenzen | [Lastenheft](./lastenheft.md), [Architektur](./architecture.md), [Phasenplan](./roadmap.md) |

### 0.1 Zweck
Dieses Dokument konkretisiert die fachliche und benutzerbezogene Ausgestaltung des Produkts auf Basis der Anforderungen im [Lastenheft](./lastenheft.md). Es beschreibt beabsichtigtes Verhalten, Ausgabemodelle und Interaktionsmuster, ohne die technische Implementierungstiefe des Architekturdokuments vorwegzunehmen.

### 0.2 Nicht-Ziel
Dieses Dokument definiert keine abschliessenden Klassen-, Modul- oder Build-Strukturen. Die hexagonale Zerlegung in Ports und Adapter wird in [architecture.md](./architecture.md) beschrieben.

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
- Konsolenausgaben sollen standardmaessig knapp, aber interpretierbar sein.
- Ein detailreicher Modus soll zusaetzliche Diagnoseinformationen liefern.
- Fehlermeldungen sollen den konkreten Eingabefehler benennen und einen naechsten Schritt vorschlagen.

### 2.3 Eingabegrundlagen

Fuer die Kernanalysen werden langfristig zwei primaere CMake-Eingabequellen unterstuetzt:

- `compile_commands.json` als compilernahe und nach Moeglichkeit exakte Datengrundlage
- die CMake File API als zweite CMake-native Datengrundlage fuer Generatoren, bei denen keine `compile_commands.json` bereitsteht

Beide Quellen sollen dieselben fachlichen Analysearten tragen koennen. Die Datengrundlage bleibt jedoch Teil des Ergebniskontexts:

- Berichte sollen sichtbar machen, ob Kennzahlen und Ableitungen auf `exact` oder `derived` Beobachtungen beruhen.
- Ergebnisse aus abgeleiteten Daten duerfen nicht stillschweigend denselben Praezisionsanspruch tragen wie compilernahe Eingaben.
- Fehlt `compile_commands.json`, soll eine ausreichend vollstaendige CMake-File-API-Lage fuer die Kernanalysen genutzt werden, statt allein wegen der fehlenden Compilation Database abzubrechen.

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

### 3.3 Impact-Analyse

Die Impact-Analyse soll einen Dateipfad als Eingabe nehmen und drei Ergebnisklassen unterscheiden:

| Ergebnistyp | Bedeutung |
|---|---|
| direkte Betroffenheit | Translation Units oder Targets koennen unmittelbar betroffen sein |
| indirekte Betroffenheit | Betroffenheit ergibt sich aus weiteren Beziehungen oder Zusatzdaten |
| unklare Betroffenheit | fuer Teile der Analyse fehlen Daten oder belastbare Ableitungen |

### 3.4 Sortierung und Begrenzung von Ergebnislisten

Analyse-Ergebnisse wie das Translation-Unit-Ranking und die Include-Hotspot-Liste koennen bei grossen Projekten umfangreich werden. Das Design soll folgende Regeln einhalten:

- Standardmaessig werden nur die auffaelligsten Ergebnisse angezeigt (Top-N), damit die Ausgabe ohne Konfiguration nutzbar bleibt (`NF-03`).
- Die Anzahl der angezeigten Ergebnisse soll vom Nutzer steuerbar sein, zum Beispiel ueber einen CLI-Parameter.
- Wird die Ausgabe begrenzt, soll ein Hinweis die Gesamtanzahl der analysierten Eintraege nennen, damit der Nutzer die Einordnung nicht verliert.
- Die konkrete Standardanzahl (zum Beispiel Top 10 oder Top 20) wird im Pflichtenheft oder bei der Implementierung festgelegt.

Relevante Anforderungen: `F-42`, `F-10`, `F-13`, `NF-03`

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

## 7. Rueckverfolgbarkeit

| Designbereich | Lastenheft-Kennungen |
|---|---|
| Eingabeverhalten | `F-01` bis `F-05`, `F-41`, `S-01` bis `S-03` |
| Translation-Unit-Analyse | `F-06` bis `F-11` |
| Include-Analyse | `F-12` bis `F-17` |
| Target- und Impact-Sicht | `F-18` bis `F-25` |
| Ergebnisbegrenzung | `F-42`, `F-10`, `F-13`, `NF-03` |
| Berichtsausgaben | `F-26` bis `F-30`, `NF-20` |
| Unsicherheit und Datenluecken | `F-09`, `F-23`, `NF-02`, `NF-14` |
| CLI-Nutzung | `F-31` bis `F-34`, `F-39`, `F-40`, `NF-01` bis `NF-03` |
| Konfiguration | `F-35` bis `F-38` |

## 8. Offene Designfragen

- Welche Detailtiefe soll der `--verbose`-Modus liefern, ohne die Standardausgabe zu ueberladen?
- Wie sollen teilweise fehlende Include- oder Target-Daten visuell und sprachlich markiert werden?
- Welche Struktur soll ein Markdown-Bericht fuer spaetere Automatisierung und Diffbarkeit bekommen?
