# Phasenplan - cmake-xray

## 0. Dokumenteninformationen

| Feld | Wert |
|---|---|
| Dokument | Phasenplan `cmake-xray` |
| Version | `0.5` |
| Stand | `2026-04-22` |
| Status | Entwurf |
| Referenzen | [Lastenheft](./lastenheft.md), [Design](./design.md), [Architektur](./architecture.md) |

### 0.1 Zweck
Dieses Dokument beschreibt eine inkrementelle Lieferplanung fuer **cmake-xray**. Der Phasenplan ist an den Anforderungen des Lastenhefts ausgerichtet und dient als Bruecke zwischen Anforderungen und konkreter Umsetzungsplanung. Er definiert die fachliche Reihenfolge und relative Groesse der Arbeitspakete, enthaelt jedoch bewusst keine kalendarischen Termine.

## 1. Planungsgrundsaetze

- zuerst einen belastbaren MVP liefern
- Risiken frueh durch kleine, pruefbare Inkremente reduzieren
- Folgephasen nur auf bereits stabilisierten Eingabemodellen aufbauen
- jede Phase ueber Lastenheft-Kennungen rueckverfolgbar machen

## 2. Phasenuebersicht

| Phase | Ziel | Ergebnis | Relativer Umfang |
|---|---|---|---|
| Phase 0 | Fundament | Projektgrundgeruest, Build, Tests, Dokumente | klein |
| Phase 1 | MVP-Eingaben und CLI | valide Eingangsdaten, CLI-Struktur, Help, Exit-Codes | klein |
| Phase 2 | Kernanalysen MVP | Translation-Unit-Analyse, Include-Hotspots, Impact fuer Dateien | gross |
| Phase 3 | Berichte und Qualitaet | Markdown, Dokumentation, Referenzdaten, Performance-Baseline | mittel |
| Phase 4 | Erweiterungen | Target-Sicht, HTML, JSON, weitere Plattformen | fortlaufend |

### 2.1 Milestones

| Milestone | Version | Phase | Pruefkriterium |
|---|---|---|---|
| **M0** | `v0.1.0` | Phase 0 | Projekt baut auf Linux, leerer Testlauf laeuft durch, Dokumentenstruktur steht |
| **M1** | `v0.2.0` | Phase 1 | `AK-01`, `AK-02`, `AK-09` erfuellt |
| **M2** | `v0.3.0` | Phase 2 | `AK-03`, `AK-04`, `AK-05` erfuellt; heuristische Ergebnisse gekennzeichnet |
| **M3 (MVP)** | `v1.0.0` | Phase 3 | Alle `AK-01` bis `AK-09` erfuellt; MVP lieferbar |

## 3. Phasen im Detail

### 3.1 Phase 0 - Fundament

Ziel: ein baubares und testbares Projekt mit klaren Dokumentationsartefakten.

| Arbeitspaket | Relevante Kennungen |
|---|---|
| C++/CMake-Projektgrundgeruest mit hexagonaler Grundstruktur (Kern, Ports, Adapter) | `RB-01`, `RB-02`, `RB-03` |
| Linux-Build und Testbasis | `NF-07`, `AK-07` |
| README und Dokumentenstruktur | `NF-16`, `NF-17`, `AK-08` |
| Entscheidung und Einbindung externer Abhaengigkeiten (JSON, CLI-Parsing, Test-Framework) | `RB-10`, `RB-06`, `RB-07` |

**Milestone M0 (`v0.1.0`)**: Das Projekt baut auf Linux, ein leerer Testlauf laeuft durch und die Dokumentenstruktur steht.

### 3.2 Phase 1 - MVP-Eingaben und CLI

Ziel: das Tool akzeptiert gueltige Eingaben, behandelt Fehlerfaelle sauber und ist nutzbar aufrufbar.

| Arbeitspaket | Relevante Kennungen |
|---|---|
| `CompileCommandsJsonAdapter` (`CompileDatabasePort`): Einlesen und Validieren von `compile_commands.json` | `F-01` bis `F-05`, `F-41`, `AK-01`, `AK-02` |
| CLI Adapter: Grundlegende Befehlsstruktur und Verdrahtung der Ports | `F-31`, `F-32`, `F-33`, `F-34`, `AK-09` |
| Konfigurierbare Pfade und Formatauswahl | `F-35`, `F-36` |

**Milestone M1 (`v0.2.0`)**: `AK-01`, `AK-02` und `AK-09` sind erfuellt. Eine gueltige `compile_commands.json` wird eingelesen, ungueltige oder leere Eingaben werden mit klarer Meldung und Exit-Code quittiert, `--help` gibt verstaendliche Hilfe aus.

### 3.3 Phase 2 - Kernanalysen MVP

Ziel: die fachlich zentralen Analysen des ersten Releases liefern nutzbare Ergebnisse.

| Arbeitspaket | Relevante Kennungen |
|---|---|
| Application Core: Translation-Unit-Ranking auf Basis von `arg_count`, `include_path_count` und `define_count` | `F-06` bis `F-09`, `AK-03` |
| `IncludeResolverPort` und MVP-Adapter: Include-Hotspot-Analyse | `F-12` bis `F-15`, `AK-04` |
| Application Core: Dateibasierte Impact-Analyse | `F-21` bis `F-23`, `AK-05` |
| `ConsoleReportAdapter` (`ReportWriterPort`): Konsolenausgabe der Ergebnisse | `F-26`, `AK-03` |
| Reproduzierbarkeit der Analyseergebnisse sicherstellen | `NF-15` |

**Milestone M2 (`v0.3.0`)**: `AK-03`, `AK-04` und `AK-05` sind erfuellt. Das Tool gibt in der Konsole ein TU-Ranking mit Kennzahlen aus, listet Include-Hotspots mit betroffenen TUs, und fuehrt eine Impact-Analyse fuer eine Datei durch. Ergebnisse, die auf der heuristischen Include-Aufloesung basieren, sind als solche gekennzeichnet.

### 3.4 Phase 3 - Berichte und Qualitaet

Ziel: Ergebnisse werden fuer reale Nutzung, Dokumentation und Absicherung stabilisiert.

| Arbeitspaket | Relevante Kennungen |
|---|---|
| `MarkdownReportAdapter` (`ReportWriterPort`): Markdown-Report | `F-27`, `AK-06` |
| Referenzdaten und automatisierte Tests | `NF-10`, `NF-19` |
| Performance-Baseline und Referenzumgebung mit versioniertem Referenzprojekt im Repository | `NF-04`, `NF-05`, `NF-06` |
| Beispielausgaben und Nutzungsdokumentation | `NF-18`, `AK-08` |

**Milestone M3 / MVP (`v1.0.0`)**: Alle Abnahmekriterien `AK-01` bis `AK-09` sind erfuellt. Ein Markdown-Report kann erzeugt werden, die README enthaelt Installations- und Nutzungsbeispiele, automatisierte Tests laufen gegen Referenzdaten, und die Performance-Baseline ist dokumentiert. Das Produkt ist als MVP lieferbar.

### 3.5 Phase 4 - Erweiterungen

Ziel: nicht-MVP-Funktionen kontrolliert aufbauen.

| Arbeitspaket | Relevante Kennungen |
|---|---|
| `CmakeFileApiAdapter` (`TargetMetadataPort`): Target-Metadaten und initiale Target-Sicht; Einstieg mit TU-zu-Target-Zuordnung und targetbezogener Impact-Ausgabe | `F-18` bis `F-20`, `F-24`, `F-25`, `S-02` |
| `HtmlReportAdapter`: HTML-Export | `F-28` |
| `JsonReportAdapter` und `DotReportAdapter`: JSON- und DOT-Ausgaben | `F-29`, `F-30`, `NF-20` |
| Erweiterte Plattformunterstuetzung | `NF-08`, `NF-09` |
| Detail- und Quiet-Modi | `F-39`, `F-40` |

Hinweis: `F-39` (Soll) kann bei Bedarf in Phase 2 oder 3 vorgezogen werden, falls der detailreiche Modus fuer die Diagnose waehrend der MVP-Entwicklung selbst nuetzlich ist.

Phase 4 hat keinen einzelnen Milestone, da sie fortlaufend ist. Einzelne Arbeitspakete koennen bei Bedarf eigene Abschlusskriterien erhalten.

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
| stabiles Compile Database Model im Kern | Translation-Unit- und Include-Analyse |
| belastbarer `IncludeResolverPort`-Adapter | Impact-Analyse und spaetere Target-Sicht |
| klare Ergebnismodelle im Kern | weitere `ReportWriterPort`-Adapter (Markdown, HTML, JSON) |
| Referenzdaten und Tests | Performance- und Stabilitaetsaussagen |

## 6. Risiken fuer die Lieferplanung

| Risiko | Auswirkung auf die Roadmap |
|---|---|
| Include-Datengrundlage ist schwerer als erwartet | Phase 2 verzoegert sich oder wird funktional reduziert |
| optionale Target-Metadaten sind uneinheitlich | Phase 4 wird staerker experimentell |
| Performance reicht fuer Referenzprojekte nicht aus | Phase 3 braucht zusaetzliche Optimierungsschleifen |

## 7. Planungsentscheidungen

- Fuer `NF-04` bis `NF-06` wird zuerst ein **versioniertes, synthetisches CMake-Referenzprojekt im Repository** verwendet. Es soll reproduzierbar mehrere Groessenstufen abdecken, mindestens `250`, `500` und `1.000` Translation Units, und zugleich als Grundlage fuer `NF-19` dienen.
- Fuer das erste Translation-Unit-Ranking reicht als Minimalmenge die Kombination aus `arg_count`, `include_path_count` und `define_count`. Diese Kennzahlen sind direkt aus der Compile-Datenbank ableitbar und entkoppeln das erste Ranking von der heuristischen Include-Aufloesung.
- Fuer den MVP ist der `SourceParsingIncludeAdapter` der risikoaermste `IncludeResolverPort`-Adapter. Er bleibt im MVP die einzige Implementierung; seine Ergebnisse werden als heuristisch gekennzeichnet.
- Der Einstieg in targetbezogene Zusatzdaten lohnt sich erst nach stabilem MVP, also in Phase 4. Der erste Ausbauschritt soll mit `F-19` und `F-24` beginnen, bevor weitergehende Target-Graph-Analysen folgen.
