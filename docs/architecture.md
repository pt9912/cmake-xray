# Architecture - cmake-xray

## 0. Dokumenteninformationen

| Feld | Wert |
|---|---|
| Dokument | Architecture `cmake-xray` |
| Version | `0.1` |
| Stand | `2026-04-21` |
| Status | Entwurf |
| Referenzen | [Lastenheft](./Lastenheft.md), [Design](./design.md) |

### 0.1 Zweck
Dieses Dokument beschreibt die geplante Systemstruktur von **cmake-xray**. Es leitet aus den Anforderungen und dem Design eine technische Zerlegung in Verantwortlichkeiten, Datenfluesse und Integrationspunkte ab.

## 1. Architekturzielen

- fachliche Analysearten sauber voneinander trennen
- unterschiedliche Datenquellen austauschbar anbinden
- CLI, Analysekern und Reporter entkoppeln
- fehlende oder optionale Eingabedaten kontrolliert behandeln
- Erweiterungen fuer weitere Ausgabeformate und spaetere Analysen vorbereiten

## 2. Kontext

### 2.1 Systemkontext

| Externe Quelle oder Senke | Rolle |
|---|---|
| `compile_commands.json` | primaere Eingabequelle fuer Translation Units und Compile-Aufrufe |
| optionale Build-Metadaten | Zusatzquelle fuer targetbezogene Analysen |
| Quelldateien / Header | Grundlage fuer Include- oder Abhaengigkeitsableitungen |
| CLI-Nutzer | startet Analysen lokal |
| CI-Systeme | fuehren Analysen automatisiert aus |
| Konsole / Markdown-Datei | erste Ausgabekanaele |

### 2.2 Architekturtreiber

| Treiber | Herkunft |
|---|---|
| Linux als primaere Zielplattform | `NF-07`, `RB-01`, `RB-02`, `RB-03` |
| reproduzierbare Analyseergebnisse | `NF-15` |
| austauschbare Datengrundlagen fuer Include- und Target-Sicht | `F-05`, `F-12` bis `F-25`, `S-02` |
| klare Fehlermeldungen und Exit-Codes | `F-03`, `F-33`, `F-34`, `NF-02` |

## 3. Logische Zerlegung

### 3.1 Geplante Hauptbausteine

| Baustein | Verantwortung | Relevante Kennungen |
|---|---|---|
| CLI Layer | Argumente, Unterkommandos, Help, Exit-Codes | `F-21`, `F-31` bis `F-40` |
| Input Layer | Einlesen und Validieren von Eingabedaten | `F-01` bis `F-05`, `F-41`, `S-01` bis `S-03` |
| Analysis Core | Translation-Unit-, Include-, Target- und Impact-Analyse | `F-06` bis `F-25` |
| Reporting Layer | Konsolen- und Dateiausgaben erzeugen | `F-26` bis `F-30`, `NF-20` |
| Diagnostics Layer | Warnungen, Fehler, Datenluecken und Nachvollziehbarkeit | `F-03`, `F-09`, `F-23`, `NF-14`, `NF-15` |

### 3.2 Empfohlene Unterteilung des Analysis Core

| Teilkomponente | Verantwortung |
|---|---|
| Compile Database Model | Normalisierte Sicht auf `compile_commands.json` |
| Include Resolver | Herleitung oder Einlesen von Include-Beziehungen |
| Translation Unit Metrics | Berechnung und Ranking von Kennzahlen |
| Impact Engine | Ableitung betroffener Translation Units und spaeter Targets |
| Target Model | Abbildung optionaler targetbezogener Metadaten |

## 4. Datenfluss

1. CLI nimmt Kommando, Eingabepfade und Optionen entgegen.
2. Input Layer validiert und normalisiert die Eingangsdaten.
3. Analysis Core baut interne Modelle fuer Translation Units und weitere Build-Beziehungen auf.
4. Reporter transformieren die Analyseergebnisse in Konsole oder Markdown.
5. Diagnostics Layer liefert Fehler, Warnungen und Unsicherheiten an CLI und Reporter zurueck.

## 5. Architekturelle Entscheidungen

### 5.1 Datenquellenstrategie

Die Architektur muss mehrere Quellen fuer Abhaengigkeiten und Zusatzinformationen zulassen:

- primaer `compile_commands.json`
- optional zusaetzliche Build-Metadaten
- optional vorhandene Dependency-Artefakte
- optional durch Analyse aus Quelltext oder Compilerinformationen gewonnene Beziehungen

Die konkrete Reihenfolge und Priorisierung dieser Quellen ist eine umzusetzende Architekturentscheidung und spaeter zu fixieren.

### 5.2 Stabilitaet der internen Modelle

Interne Datenmodelle sollen:

- Eingabedaten klar von abgeleiteten Daten trennen
- Unsicherheit und Datenluecken ausdruecken koennen
- fuer mehrere Reporter wiederverwendbar sein
- reproduzierbare Sortierung und Auswertung ermoeglichen

### 5.3 Erweiterbarkeit

Neue Analysearten und Ausgabeformate sollen moeglich sein, ohne den CLI-Einstieg oder bestehende Reporter grundlegend zu brechen.

## 6. Risiken fuer die Architektur

| Risiko | Auswirkung |
|---|---|
| Include-Ableitung ist teuer oder unzuverlaessig | Performance- und Zuverlaessigkeitsprobleme |
| Zusatzdaten fuer Targets fehlen oft | eingeschraenkte Target- und Impact-Sicht |
| Reporter kennen zu viele Analyse-Interna | geringe Erweiterbarkeit |
| CLI und Analysekern sind zu eng gekoppelt | schlechte Testbarkeit und schwerere Automatisierung |

## 7. Rueckverfolgbarkeit

| Architekturthema | Lastenheft-Kennungen |
|---|---|
| Input Layer | `F-01` bis `F-05`, `F-41`, `S-01` bis `S-03` |
| Include-Datengrundlage | `F-12` bis `F-17`, `S-02`, `NF-15` |
| Impact und Targets | `F-18` bis `F-25` |
| CLI-Verhalten | `F-31` bis `F-40`, `NF-01`, `NF-02` |
| Reporting | `F-26` bis `F-30`, `NF-20` |
| Plattform- und Laufzeitrahmen | `NF-07`, `RB-01` bis `RB-05` |

## 8. Offene Architekturfragen

- Welche interne Repraesentation eignet sich fuer reproduzierbare Rankings und sortierte Berichte?
- Welche Datenquelle soll fuer Includes im MVP priorisiert werden?
- Wie wird der Diagnostics Layer Unsicherheit und fehlende Daten zwischen Analyse und Report transportieren?
- Welche Formatversionen werden fuer spaetere JSON-Ausgaben benoetigt?
