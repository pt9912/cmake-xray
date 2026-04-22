# Architecture - cmake-xray

## 0. Dokumenteninformationen

| Feld | Wert |
|---|---|
| Dokument | Architecture `cmake-xray` |
| Version | `0.6` |
| Stand | `2026-04-22` |
| Status | Entwurf |
| Referenzen | [Lastenheft](./lastenheft.md), [Design](./design.md), [Phasenplan](./roadmap.md) |

### 0.1 Zweck
Dieses Dokument beschreibt die geplante Systemstruktur von **cmake-xray**. Es leitet aus den Anforderungen und dem Design eine technische Zerlegung in Verantwortlichkeiten, Datenfluesse und Integrationspunkte ab.

### 0.2 Architekturstil
Das System folgt einer **hexagonalen Architektur** (Ports & Adapters). Der fachliche Analysekern definiert Ports als abstrakte Schnittstellen. Konkrete Adapter binden externe Quellen, Ausgabekanaele und Steuerungswege an.

Dieser Stil wurde gewaehlt, weil:

- die Include-Datenstrategie bewusst offen gehalten ist und austauschbar bleiben muss
- mehrere Ausgabeformate geplant sind, die denselben Analysekern nutzen
- Testbarkeit durch austauschbare Adapter ohne Dateisystem oder CLI moeglich wird
- spaetere Eingabequellen (CMake File API, Dependency-Artefakte) ohne Kernumbau anbindbar sein sollen

## 1. Architekturziele

- fachliche Analysearten sauber voneinander trennen
- Abhaengigkeiten zeigen immer nach innen: Adapter haengen vom Kern ab, nie umgekehrt
- alle externen Quellen und Senken ueber Ports anbinden
- fehlende oder optionale Eingabedaten kontrolliert behandeln
- Erweiterungen fuer weitere Ausgabeformate und spaetere Analysen ohne Kernaenderungen ermoeglichen

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
| automatisierte Testbarkeit | `NF-10`, `NF-19` |

## 3. Hexagonale Zerlegung

### 3.1 Uebersicht

```
              +------------------------+
              |    Primary Adapter     |
              |         CLI            |
              +-----------+------------+
                          |
              +-----------v------------+
              |    Primary Ports       |
              |    (Driving)           |
              +========================+
              |                        |
              |    Application Core    |
              |    (Hexagon)           |
              |                        |
              |  - TU Metrics          |
              |  - Include Analysis    |
              |  - Impact Engine       |
              |  - Target Model        |
              |  - Diagnostics         |
              |                        |
              +========================+
              |    Secondary Ports     |
              |    (Driven)            |
              +--+--------+--------+--+
                 |        |        |
           +-----v--+ +--v----+ +-v-----------+
           | Input   | |Report| | Include     |
           | Adapt.  | |Adapt.| | Adapters    |
           +---------+ +------+ +-------------+
```

### 3.2 Application Core (Hexagon)

Der Kern enthaelt die gesamte Fachlogik. Er hat keine Abhaengigkeiten auf Dateisystem, CLI-Frameworks oder konkrete Datenformate. Alle Kommunikation nach aussen laeuft ueber Ports.

| Kernbaustein | Verantwortung | Relevante Kennungen |
|---|---|---|
| Compile Database Model | Normalisierte, formatunabhaengige Sicht auf Compile-Eintraege | `F-04`, `F-06` |
| Translation Unit Metrics | Berechnung und Ranking von Kennzahlen fuer auffaellige TUs | `F-07`, `F-08`, `F-09`, `F-10` |
| Include Analysis | Auswertung von Include-Beziehungen und Hotspot-Ermittlung | `F-12` bis `F-17` |
| Impact Engine | Ableitung betroffener TUs und spaeter Targets bei Dateiaenderungen | `F-21` bis `F-25` |
| Target Model | Abbildung optionaler targetbezogener Metadaten | `F-18` bis `F-20` |
| Diagnostics | Querschnittsaspekt: sammelt Warnungen, Datenluecken und Unsicherheiten waehrend der Analyse und reicht sie an die Ergebnisse weiter | `F-03`, `F-09`, `F-23`, `NF-14`, `NF-15` |

### 3.3 Ports

Ports sind abstrakte Schnittstellen, die der Kern definiert. Sie beschreiben **was** der Kern braucht oder anbietet, nicht **wie** es technisch umgesetzt wird.

#### Primary Ports (Driving — die Aussenwelt steuert den Kern)

| Port | Verantwortung | Relevante Kennungen |
|---|---|---|
| `AnalyzeProject` | Projektanalyse ausloesen: TU-Ranking und Include-Hotspots erzeugen | `F-06` bis `F-15` |
| `AnalyzeImpact` | Impact-Analyse fuer einen Dateipfad ausloesen | `F-21` bis `F-25` |
| `GenerateReport` | Analyseergebnisse in ein Ausgabeformat uebersetzen | `F-26` bis `F-30` |

#### Secondary Ports (Driven — der Kern steuert die Aussenwelt)

| Port | Verantwortung | Relevante Kennungen |
|---|---|---|
| `CompileDatabasePort` | Compile-Eintraege laden und validieren | `F-01` bis `F-05`, `F-41`, `S-01` |
| `IncludeResolverPort` | Include-Beziehungen fuer Translation Units ermitteln | `F-12` bis `F-17` |
| `TargetMetadataPort` | Optionale Target-Metadaten laden | `F-18` bis `F-20`, `S-02` |
| `ReportWriterPort` | Analyseergebnisse in ein konkretes Format schreiben | `F-26` bis `F-30`, `NF-20` |

### 3.4 Adapter

Adapter sind konkrete Implementierungen der Ports. Sie enthalten die technischen Details und Abhaengigkeiten auf externe Formate, Bibliotheken und Systeme.

#### Primary Adapter (Driving)

| Adapter | Implementiert | Beschreibung | Relevante Kennungen |
|---|---|---|---|
| CLI Adapter | `AnalyzeProject`, `AnalyzeImpact`, `GenerateReport` | Uebersetzt Kommandozeilenargumente in Port-Aufrufe; verantwortet Help, Exit-Codes, Fehlerausgabe | `F-31` bis `F-40` |

Spaetere Primary Adapter (nicht MVP): IDE-Integration, programmatische API.

#### Secondary Adapter (Driven)

Die folgende Tabelle beschreibt den Zielzustand fuer den **Gesamt-MVP** gemaess Phasenplan, also den bis `M3 / v1.0.0` lieferbaren Stand. Zwischenstaende einzelner Milestones wie `M2` koennen bewusst nur eine Teilmenge dieser Adapter enthalten.

| Adapter | Implementiert | Beschreibung | Gesamt-MVP (bis M3) |
|---|---|---|---|
| CompileCommandsJsonAdapter | `CompileDatabasePort` | Liest und validiert `compile_commands.json` | ja (ab Phase 1 / M1) |
| SourceParsingIncludeAdapter | `IncludeResolverPort` | Parst Quelldateien entlang der `-I`-Pfade aus dem Compile-Aufruf; **experimentelle MVP-Heuristik** mit bekannten Einschraenkungen (siehe 6.1) und einzige Include-Adapter-Implementierung im MVP | ja (ab Phase 2 / M2; einziger Include-Adapter im Gesamt-MVP, experimentell) |
| CompilerDepsIncludeAdapter | `IncludeResolverPort` | Wertet `.d`-Dependency-Dateien oder compilergestuetzte `-M`-Ausgaben aus | spaeter |
| ConsoleReportAdapter | `ReportWriterPort` | Schreibt Ergebnisse auf die Konsole | ja (ab Phase 2 / M2) |
| MarkdownReportAdapter | `ReportWriterPort` | Erzeugt einen Markdown-Bericht | ja (ab Phase 3 / M3) |
| HtmlReportAdapter | `ReportWriterPort` | Erzeugt einen HTML-Bericht | spaeter |
| JsonReportAdapter | `ReportWriterPort` | Erzeugt JSON-Ausgabe mit Schema-Version | spaeter |
| DotReportAdapter | `ReportWriterPort` | Erzeugt DOT/Graphviz-Ausgabe | spaeter |
| CmakeFileApiAdapter | `TargetMetadataPort` | Liest Target-Informationen aus der CMake File API | spaeter |

Im Gesamt-MVP gibt es genau eine Implementierung des `IncludeResolverPort`; sie wird ab Phase 2 / M2 eingefuehrt. Weitere Include-Adapter werden erst nach Stabilisierung des MVP eingefuehrt, damit Portverhalten, Diagnostics und Ergebniskennzeichnung zunaechst an einer einzigen Datenstrategie geschaerft werden koennen.

## 4. Verzeichnis- und Dateistruktur

Die Verzeichnisstruktur bildet die hexagonale Zerlegung direkt im Dateisystem ab. Der Kern (`hexagon/`) und die Adapter (`adapters/`) sind auf oberster Ebene getrennt.

```
cmake-xray/
├── CMakeLists.txt
├── src/
│   ├── CMakeLists.txt
│   ├── main.cpp                                    # Composition Root: verdrahtet Ports mit Adaptern
│   │
│   ├── hexagon/                                    # Application Core
│   │   ├── CMakeLists.txt
│   │   ├── model/                                  # Domaenmodelle und Datenstrukturen
│   │   │   ├── compile_entry.h                     # Einzelner Eintrag aus der Compile-Datenbank
│   │   │   ├── translation_unit.h                  # TU mit zugeordneten Kennzahlen
│   │   │   ├── include_graph.h                     # Include-Beziehungen zwischen Dateien
│   │   │   ├── impact_result.h                     # Ergebnis einer Impact-Analyse
│   │   │   ├── analysis_result.h                   # Gesamtergebnis einer Projektanalyse
│   │   │   ├── target_info.h                       # Optionale Target-Metadaten
│   │   │   └── diagnostic.h                        # Warnungen, Hinweise, Datenluecken
│   │   │
│   │   ├── ports/                                  # Port-Schnittstellen
│   │   │   ├── driving/                            # Primary Ports (Aussenwelt → Kern)
│   │   │   │   ├── analyze_project_port.h
│   │   │   │   ├── analyze_impact_port.h
│   │   │   │   └── generate_report_port.h
│   │   │   └── driven/                             # Secondary Ports (Kern → Aussenwelt)
│   │   │       ├── compile_database_port.h
│   │   │       ├── include_resolver_port.h
│   │   │       ├── target_metadata_port.h
│   │   │       └── report_writer_port.h
│   │   │
│   │   └── services/                               # Anwendungslogik (implementiert Primary Ports)
│   │       ├── project_analyzer.h
│   │       ├── project_analyzer.cpp
│   │       ├── impact_analyzer.h
│   │       ├── impact_analyzer.cpp
│   │       ├── report_generator.h
│   │       └── report_generator.cpp
│   │
│   └── adapters/                                   # Alle Adapter
│       ├── CMakeLists.txt
│       ├── cli/                                    # Primary Adapter
│       │   ├── cli_adapter.h
│       │   ├── cli_adapter.cpp
│       │   └── exit_codes.h
│       ├── input/                                  # Driven Adapter: Eingabequellen
│       │   ├── compile_commands_json_adapter.h
│       │   ├── compile_commands_json_adapter.cpp
│       │   ├── source_parsing_include_adapter.h
│       │   └── source_parsing_include_adapter.cpp
│       └── output/                                 # Driven Adapter: Ausgabekanaele
│           ├── console_report_adapter.h
│           ├── console_report_adapter.cpp
│           ├── markdown_report_adapter.h
│           └── markdown_report_adapter.cpp
│
├── tests/
│   ├── CMakeLists.txt
│   ├── hexagon/                                    # Unit- und Integrationstests fuer den Kern
│   │   ├── test_tu_metrics.cpp
│   │   ├── test_include_analysis.cpp
│   │   └── test_impact_engine.cpp
│   ├── adapters/                                   # Adapter-Tests
│   │   ├── test_compile_commands_json.cpp
│   │   ├── test_source_parsing_include.cpp
│   │   ├── test_console_report.cpp
│   │   └── test_markdown_report.cpp
│   └── e2e/                                        # End-to-End-Tests ueber CLI
│       ├── test_cli_analyze.cpp
│       └── testdata/                               # Referenzprojekte und erwartete Ausgaben
│
└── docs/
    ├── lastenheft.md
    ├── design.md
    ├── architecture.md
    └── roadmap.md
```

### 4.1 Zuordnung zur hexagonalen Zerlegung

| Verzeichnis | Architekturrolle | Abhaengigkeiten |
|---|---|---|
| `src/hexagon/model/` | Domaenmodelle | keine |
| `src/hexagon/ports/driving/` | Primary Ports | nur `model/` |
| `src/hexagon/ports/driven/` | Secondary Ports | nur `model/` |
| `src/hexagon/services/` | Anwendungslogik | `model/`, `ports/` |
| `src/adapters/cli/` | Primary Adapter | `hexagon/ports/driving/`, `hexagon/model/` |
| `src/adapters/input/` | Driven Adapter (Eingabe) | `hexagon/ports/driven/`, `hexagon/model/` |
| `src/adapters/output/` | Driven Adapter (Ausgabe) | `hexagon/ports/driven/`, `hexagon/model/` |
| `src/main.cpp` | Composition Root | alles (verdrahtet Ports mit konkreten Adaptern) |

### 4.2 CMake-Targets

| CMake-Target | Verzeichnis | Abhaengigkeiten |
|---|---|---|
| `xray_hexagon` (library) | `src/hexagon/` | keine externen |
| `xray_adapters` (library) | `src/adapters/` | `xray_hexagon`, externe Bibliotheken (JSON, CLI-Parsing) |
| `cmake-xray` (executable) | `src/main.cpp` | `xray_hexagon`, `xray_adapters` |
| `xray_tests` (executable) | `tests/` | `xray_hexagon`, `xray_adapters`, Test-Framework |

Die Trennung in zwei Libraries stellt sicher, dass `xray_hexagon` **keine** externen Abhaengigkeiten hat. Externe Bibliotheken werden ausschliesslich ueber `xray_adapters` eingebunden.

### 4.3 Konventionen

- Header und Implementierung liegen im selben Verzeichnis.
- Dateinamen verwenden `snake_case`.
- Jeder Port ist ein einzelner Header mit einer abstrakten Klasse.
- Spaetere Adapter (HTML, JSON, DOT, CMake File API) werden in `adapters/output/` bzw. `adapters/input/` ergaenzt, ohne bestehende Dateien zu aendern.

## 5. Datenfluss

1. Der **CLI Adapter** nimmt Kommando, Eingabepfade und Optionen entgegen und ruft den passenden Primary Port auf.
2. Der Kern ruft ueber den `CompileDatabasePort` die Compile-Eintraege ab. Der Adapter validiert und normalisiert die Daten.
3. Der Kern ruft ueber den `IncludeResolverPort` die Include-Beziehungen ab. Welcher Adapter dahinter steht, ist dem Kern nicht bekannt.
4. Der Kern fuehrt die angeforderte Analyse durch (TU-Ranking, Hotspots, Impact) und sammelt dabei Diagnostics-Informationen.
5. Der Kern reicht das Analyseergebnis (einschliesslich Diagnostics) an den `ReportWriterPort`. Der aktive Adapter erzeugt die Ausgabe im gewaehlten Format.
6. Der **CLI Adapter** setzt den Exit-Code basierend auf dem Ergebnis.

### 5.1 Abhaengigkeitsrichtung

Alle Abhaengigkeiten zeigen nach innen:

```
CLI Adapter  ------->  Primary Ports  ------->  Application Core
                                                      |
                                                      v
                                                Secondary Ports
                                                      |
CompileCommandsJsonAdapter  <-------------------------+
SourceParsingIncludeAdapter <-------------------------+
ConsoleReportAdapter        <-------------------------+
MarkdownReportAdapter       <-------------------------+
```

Kein Adapter kennt einen anderen Adapter. Der Kern kennt keine konkreten Adapter, nur die Port-Schnittstellen.

## 6. Architekturelle Entscheidungen

### 6.1 Include-Datenstrategie als austauschbarer Adapter

Die offene Frage der Include-Datenherkunft wird durch den `IncludeResolverPort` architekturell geloest. Der Kern arbeitet gegen die Port-Abstraktion; der konkrete Adapter kann ausgetauscht werden, ohne den Kern zu aendern.

Fuer den Gesamt-MVP wird, beginnend in Phase 2 / M2, genau ein Adapter implementiert. Der erste Kandidat ist das **Parsen von Quelldateien entlang der Include-Pfade** (`SourceParsingIncludeAdapter`). Diese Variante wird als **experimentelle MVP-Heuristik** eingestuft, nicht als verlaessliche compilernahe Aufloesung.

Vorteile:

- keinen vorherigen Build erfordert
- keinen Compiler auf dem Analysesystem voraussetzt
- in CI-Umgebungen ohne Build-Artefakte funktioniert

Bekannte Einschraenkungen:

- **Bedingte Includes** (`#ifdef`, `#if`) werden nicht ausgewertet; der Adapter sieht alle Zweige, der Compiler nur den aktiven.
- **Compilerdefinierte Makros** (`-D`, eingebaute Defines) beeinflussen Include-Pfade, werden aber nicht interpretiert.
- **`-include`-Flags** (forced includes) und generierte Header sind im Quelltext nicht sichtbar.
- **Systemabhaengige Aufloesung** (Compiler-interne Suchpfade, Frameworks unter macOS) wird nicht abgebildet.

Daraus folgt: Ab Phase 2 / M2 und damit auch im spaeter lieferbaren Gesamt-MVP koennen die Ergebnisse von `F-12` bis `F-17` und `F-21` bis `F-25` unvollstaendig oder ueberzaehlig sein. Die Architektur muss sicherstellen, dass Analyseergebnisse, die auf diesem Adapter basieren, im Bericht als **heuristisch** gekennzeichnet werden (`F-09`, `F-23`, `NF-15`). Die Diagnostics-Infrastruktur (6.6) transportiert diese Einschraenkung an den Nutzer.

Alternative Adapter (compilergestuetzte Dependency-Informationen ueber `-M`-Flags, vorhandene `.d`-Dateien) sollen als weitere `IncludeResolverPort`-Implementierungen folgen und wuerden eine compilernahe Sicht liefern. Die Port-Abstraktion stellt sicher, dass der Wechsel ohne Kernaenderung moeglich ist.

### 6.2 Stabilitaet der internen Modelle

Interne Datenmodelle sollen:

- Eingabedaten klar von abgeleiteten Daten trennen
- Unsicherheit und Datenluecken ausdruecken koennen
- fuer mehrere Reporter wiederverwendbar sein
- reproduzierbare Sortierung und Auswertung ermoeglichen

Die Modelle gehoeren zum Kern und sind frei von Adapter-spezifischen Details. Die Port-Schnittstellen verwenden ausschliesslich Kernmodelle, keine adapterspezifischen Typen.

### 6.3 Erweiterbarkeit

Neue Analysearten erweitern den Kern und fuegen bei Bedarf neue Secondary Ports hinzu. Neue Ausgabeformate erfordern nur einen neuen `ReportWriterPort`-Adapter. Neue Eingabequellen erfordern nur einen neuen Adapter fuer den jeweiligen Secondary Port. In keinem Fall muss bestehender Adapter-Code geaendert werden.

Fuer targetbezogene Zusatzdaten bleibt der `TargetMetadataPort` bereits im MVP Teil der Architektur, darf dort aber leer bleiben. Eine konkrete Implementierung lohnt sich erst nach stabilem MVP; der erste Ausbauschritt soll mit der TU-zu-Target-Zuordnung (`F-19`) und der targetbezogenen Ausgabe in der Impact-Analyse (`F-24`) beginnen. Weitergehende Target-Graph-Analysen (`F-18`, `F-20`, `F-25`) folgen danach.

### 6.4 Externe Abhaengigkeiten

Externe Abhaengigkeiten sollen minimal gehalten werden (`RB-10`), um Build-Komplexitaet und Einstiegshuerde fuer Beitragende gering zu halten (`RB-06`, `RB-07`). Wo bewaehrte Bibliotheken einen klaren Vorteil gegenueber Eigenimplementierungen bieten, sollen sie bevorzugt werden. Fuer den MVP werden mindestens Entscheidungen zu folgenden Bereichen benoetigt:

- JSON-Parsing (fuer `compile_commands.json` und spaetere JSON-Ausgaben)
- CLI-Argument-Parsing (im CLI Adapter)
- Test-Framework

Externe Abhaengigkeiten duerfen nur in Adaptern oder in der Adapter-Verdrahtung auftreten, nicht im Kern.

### 6.5 Testbarkeitsstrategie

Die hexagonale Architektur unterstuetzt Testbarkeit direkt: Secondary Ports koennen durch Test-Doubles ersetzt werden, ohne Dateisystem oder CLI.

| Testebene | Was wird getestet | Wie |
|---|---|---|
| Unit-Tests | Einzelne Kernbausteine (Metriken, Include Analysis, Impact Engine) | Test-Doubles fuer Secondary Ports; definierte Eingaben, erwartete Ergebnisse |
| Integrationstests | Datenfluss durch den gesamten Kern | Test-Doubles fuer alle Secondary Ports; Pruefung der Ergebnisstruktur |
| Adapter-Tests | Einzelne Adapter isoliert (z.B. JSON-Parsing, Markdown-Erzeugung) | Echte Dateien oder Strings als Eingabe; Ausgabe gegen Erwartung pruefen |
| End-to-End-Tests | CLI-Werkzeug als Ganzes | Referenzprojekte (`NF-19`); Ausgaben gegen erwartete Ergebnisse vergleichen |

Die Referenzumgebung und Referenzprojekte aus `NF-04` bis `NF-06` und `NF-19` sollen auch als Grundlage fuer die Testinfrastruktur dienen.

Als erste gemeinsame Grundlage fuer Performance- und Testaussagen wird ein versioniertes synthetisches CMake-Referenzprojekt im Repository vorgesehen. Es soll reproduzierbar mehrere Groessenstufen, mindestens `250`, `500` und `1.000` Translation Units, sowie kontrollierte Include-Hotspots bereitstellen.

### 6.6 Diagnostics als Querschnittsaspekt

Diagnostics ist kein eigener Port oder Adapter, sondern ein **Protokoll innerhalb des Kerns**. Analyseschritte fuegen Warnungen, Hinweise und Datenluecken an das Analyseergebnis an. Reporter-Adapter geben diese Informationen formatgerecht aus. Der CLI Adapter leitet daraus Exit-Codes ab.

## 7. Risiken fuer die Architektur

| Risiko | Auswirkung | Minderung durch Hexagonal |
|---|---|---|
| Include-Ableitung ist teuer oder unzuverlaessig | Performance- und Zuverlaessigkeitsprobleme | Adapter austauschbar; alternative Strategie ohne Kernaenderung moeglich |
| Zusatzdaten fuer Targets fehlen oft | eingeschraenkte Target- und Impact-Sicht | `TargetMetadataPort` liefert leere Ergebnisse; Kern behandelt Fehlen explizit |
| Reporter kennen zu viele Analyse-Interna | geringe Erweiterbarkeit | Reporter arbeiten nur gegen Kernmodelle ueber `ReportWriterPort` |
| CLI und Analysekern sind zu eng gekoppelt | schlechte Testbarkeit | CLI ist nur ein Primary Adapter; Kern testbar ueber Ports ohne CLI |
| Port-Abstraktionen werden zu frueh zu komplex | Over-Engineering | MVP startet mit je einer Implementierung pro Port; Abstraktion waechst mit den Anforderungen |

## 8. Rueckverfolgbarkeit

| Architekturthema | Lastenheft-Kennungen |
|---|---|
| Application Core | `F-06` bis `F-25`, `NF-15` |
| CompileDatabasePort / Adapter | `F-01` bis `F-05`, `F-41`, `S-01` |
| IncludeResolverPort / Adapter | `F-12` bis `F-17`, `S-02` |
| TargetMetadataPort / Adapter | `F-18` bis `F-20`, `S-02` |
| ReportWriterPort / Adapter | `F-26` bis `F-30`, `NF-20` |
| CLI Adapter | `F-31` bis `F-40`, `NF-01`, `NF-02` |
| Diagnostics | `F-03`, `F-09`, `F-23`, `NF-02`, `NF-14`, `NF-15` |
| Verzeichnisstruktur und CMake-Targets | `RB-01`, `RB-02`, `RB-10` |
| Externe Abhaengigkeiten | `RB-10`, `RB-06`, `RB-07` |
| Testbarkeit | `NF-10`, `NF-19`, `NF-04` bis `NF-06` |
| Plattform- und Laufzeitrahmen | `NF-07`, `RB-01` bis `RB-05` |

## 9. Offene Architekturfragen

- Welche interne Repraesentation eignet sich fuer reproduzierbare Rankings und sortierte Berichte?
- Wie wird der `SourceParsingIncludeAdapter` mit Compiler-spezifischen Include-Konventionen umgehen (z.B. `__has_include`, bedingte Includes)?
- Welche Formatversionen werden fuer spaetere JSON-Ausgaben benoetigt?
- Soll `main.cpp` die Verdrahtung direkt vornehmen oder eine Factory verwenden?
