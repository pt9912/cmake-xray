# Roadmap - cmake-xray

## 0. Dokumenteninformationen

| Feld | Wert |
|---|---|
| Dokument | Roadmap `cmake-xray` |
| Version | `0.1` |
| Stand | `2026-04-21` |
| Status | Entwurf |
| Referenzen | [Lastenheft](./Lastenheft.md), [Design](./design.md), [Architecture](./architecture.md) |

### 0.1 Zweck
Dieses Dokument beschreibt eine inkrementelle Lieferplanung fuer **cmake-xray**. Die Roadmap ist an den Anforderungen des Lastenhefts ausgerichtet und dient als Bruecke zwischen Anforderungen und konkreter Umsetzungsplanung.

## 1. Planungsgrundsaetze

- zuerst einen belastbaren MVP liefern
- Risiken frueh durch kleine, pruefbare Inkremente reduzieren
- Folgephasen nur auf bereits stabilisierten Eingabemodellen aufbauen
- jede Phase ueber Lastenheft-Kennungen rueckverfolgbar machen

## 2. Phasenuebersicht

| Phase | Ziel | Ergebnis |
|---|---|---|
| Phase 0 | Fundament | Projektgrundgeruest, Build, Tests, Dokumente |
| Phase 1 | MVP-Eingaben und CLI | valide Eingangsdaten, CLI-Struktur, Help, Exit-Codes |
| Phase 2 | Kernanalysen MVP | Translation-Unit-Analyse, Include-Hotspots, Impact fuer Dateien |
| Phase 3 | Berichte und Qualitaet | Markdown, Dokumentation, Referenzdaten, Performance-Baseline |
| Phase 4 | Erweiterungen | Target-Sicht, HTML, JSON, weitere Plattformen |

## 3. Phasen im Detail

### 3.1 Phase 0 - Fundament

Ziel: ein baubares und testbares Projekt mit klaren Dokumentationsartefakten.

| Arbeitspaket | Relevante Kennungen |
|---|---|
| C++/CMake-Projektgrundgeruest | `RB-01`, `RB-02`, `RB-03` |
| Linux-Build und Testbasis | `NF-07`, `AK-07` |
| README und Dokumentenstruktur | `NF-16`, `NF-17`, `AK-08` |

### 3.2 Phase 1 - MVP-Eingaben und CLI

Ziel: das Tool akzeptiert gueltige Eingaben, behandelt Fehlerfaelle sauber und ist nutzbar aufrufbar.

| Arbeitspaket | Relevante Kennungen |
|---|---|
| Einlesen und Validieren von `compile_commands.json` | `F-01` bis `F-05`, `F-41`, `AK-01`, `AK-02` |
| Grundlegende CLI-Struktur | `F-31`, `F-32`, `F-33`, `F-34`, `AK-09` |
| Konfigurierbare Pfade und Formatauswahl | `F-35`, `F-36` |

### 3.3 Phase 2 - Kernanalysen MVP

Ziel: die fachlich zentralen Analysen des ersten Releases liefern nutzbare Ergebnisse.

| Arbeitspaket | Relevante Kennungen |
|---|---|
| Translation-Unit-Ranking | `F-06` bis `F-09`, `AK-03` |
| Include-Hotspot-Analyse | `F-12` bis `F-15`, `AK-04` |
| Dateibasierte Impact-Analyse | `F-21` bis `F-23`, `AK-05` |
| Konsolenausgabe der Ergebnisse | `F-26`, `AK-03` |

### 3.4 Phase 3 - Berichte und Qualitaet

Ziel: Ergebnisse werden fuer reale Nutzung, Dokumentation und Absicherung stabilisiert.

| Arbeitspaket | Relevante Kennungen |
|---|---|
| Markdown-Report | `F-27`, `AK-06` |
| Referenzdaten und automatisierte Tests | `NF-10`, `NF-19` |
| Performance-Baseline und Referenzumgebung | `NF-04`, `NF-05`, `NF-06` |
| Beispielausgaben und Nutzungsdokumentation | `NF-18`, `AK-08` |

### 3.5 Phase 4 - Erweiterungen

Ziel: nicht-MVP-Funktionen kontrolliert aufbauen.

| Arbeitspaket | Relevante Kennungen |
|---|---|
| Target-Metadaten und Target-Analyse | `F-18` bis `F-20`, `F-24`, `F-25`, `S-02` |
| HTML-Export | `F-28` |
| JSON- und DOT-Ausgaben | `F-29`, `F-30`, `NF-20` |
| Erweiterte Plattformunterstuetzung | `NF-08`, `NF-09` |
| Detail- und Quiet-Modi | `F-39`, `F-40` |

## 4. MVP-Abgrenzung

Der MVP umfasst Phase 0 bis Phase 3, soweit die Ergebnisse direkt auf die Abnahmekriterien `AK-01` bis `AK-09` einzahlen.

Nicht Bestandteil des MVP sind insbesondere:

- targetbezogene Zusatzanalysen mit optionalen Metadaten
- HTML-Export
- JSON- und DOT-Ausgaben
- weitergehende Plattformziele ausser Linux

## 5. Abhaengigkeiten zwischen Phasen

| Vorbedingung | Folgtphase |
|---|---|
| stabiles Eingabemodell | Translation-Unit- und Include-Analyse |
| belastbare Include-Datengrundlage | Impact-Analyse und spaetere Target-Sicht |
| klare interne Ergebnisstruktur | Markdown, HTML und JSON |
| Referenzdaten und Tests | Performance- und Stabilitaetsaussagen |

## 6. Risiken fuer die Lieferplanung

| Risiko | Auswirkung auf die Roadmap |
|---|---|
| Include-Datengrundlage ist schwerer als erwartet | Phase 2 verzoegert sich oder wird funktional reduziert |
| optionale Target-Metadaten sind uneinheitlich | Phase 4 wird staerker experimentell |
| Performance reicht fuer Referenzprojekte nicht aus | Phase 3 braucht zusaetzliche Optimierungsschleifen |

## 7. Offene Planungsfragen

- Welches Referenzprojekt soll fuer `NF-04` bis `NF-06` zuerst verwendet werden?
- Welche minimale Kennzahlenmenge reicht fuer das erste Translation-Unit-Ranking?
- Welche Include-Datenstrategie ist fuer den MVP am risikoaermsten?
- Wann lohnt sich der Einstieg in targetbezogene Zusatzdaten?
