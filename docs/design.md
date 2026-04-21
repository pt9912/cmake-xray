# Design - cmake-xray

## 0. Dokumenteninformationen

| Feld | Wert |
|---|---|
| Dokument | Design `cmake-xray` |
| Version | `0.1` |
| Stand | `2026-04-21` |
| Status | Entwurf |
| Referenz | [Lastenheft](./Lastenheft.md) |

### 0.1 Zweck
Dieses Dokument konkretisiert die fachliche und benutzerbezogene Ausgestaltung des Produkts auf Basis der Anforderungen im [Lastenheft](./Lastenheft.md). Es beschreibt beabsichtigtes Verhalten, Ausgabemodelle und Interaktionsmuster, ohne die technische Implementierungstiefe des Architekturdokuments vorwegzunehmen.

### 0.2 Nicht-Ziel
Dieses Dokument definiert keine abschliessenden Klassen-, Modul- oder Build-Strukturen. Diese werden in [architecture.md](./architecture.md) beschrieben.

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
| CLI-Hilfe und Diagnose | Bedienbarkeit, Fehlererklaerung und Nachvollziehbarkeit sicherstellen | `F-31` bis `F-40`, `NF-01`, `NF-02` |

### 2.2 Erwartete CLI-Eigenschaften

- Unterkommandos sollen nach Nutzerziel statt nach interner Verarbeitung benannt werden.
- Konsolenausgaben sollen standardmaessig knapp, aber interpretierbar sein.
- Ein detailreicher Modus soll zusaetzliche Diagnoseinformationen liefern.
- Fehlermeldungen sollen den konkreten Eingabefehler benennen und einen naechsten Schritt vorschlagen.

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
- zentrale Ergebnisse je Analyseart
- Hinweise auf fehlende oder unvollstaendige Daten

### 4.3 Spaetere Ausgabeformate

HTML, JSON und DOT sind Erweiterungen ausserhalb des MVP. Fuer diese Formate muessen spaeter Stabilitaets- und Versionsfragen gesondert beschrieben werden.

## 5. Verhalten bei fehlenden oder schwachen Daten

Dieses Produkt darf Unsicherheit nicht verstecken. Wenn Eingaben oder Ableitungen nicht ausreichen, soll das Design folgende Regeln einhalten:

- Analyse abbrechen, wenn Pflichtdaten ungueltig sind
- Analyse leer oder teilweise ausgeben, wenn Daten formal gueltig, aber inhaltlich unzureichend sind
- fehlende Datengrundlagen explizit benennen
- keine nicht belegbaren Target- oder Include-Aussagen vortaeuschen

## 6. Rueckverfolgbarkeit

| Designbereich | Lastenheft-Kennungen |
|---|---|
| Eingabeverhalten | `F-01` bis `F-05`, `F-41`, `S-01` bis `S-03` |
| Translation-Unit-Analyse | `F-06` bis `F-11` |
| Include-Analyse | `F-12` bis `F-17` |
| Target- und Impact-Sicht | `F-18` bis `F-25` |
| Berichtsausgaben | `F-26` bis `F-30`, `NF-20` |
| CLI-Nutzung | `F-31` bis `F-40`, `NF-01` bis `NF-03` |

## 7. Offene Designfragen

- Welche Standard-Kennzahlen sollen fuer die erste Einstufung auffaelliger Translation Units verwendet werden?
- Welche Detailtiefe soll der `--verbose`-Modus liefern, ohne die Standardausgabe zu ueberladen?
- Wie sollen teilweise fehlende Include- oder Target-Daten visuell und sprachlich markiert werden?
- Welche Struktur soll ein Markdown-Bericht fuer spaetere Automatisierung und Diffbarkeit bekommen?
