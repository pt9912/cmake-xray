# Architecture - cmake-xray

## 0. Dokumenteninformationen

| Feld | Wert |
|---|---|
| Dokument | Architecture `cmake-xray` |
| Version | `0.3` |
| Stand | `2026-04-21` |
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
                  ┌─────────────────────┐
                  │   Primary Adapter    │
                  │       CLI            │
                  └────────┬────────────┘
                           │
                  ╔════════▼════════════╗
                  ║   Primary Ports     ║
                  ║  (Driving)          ║
                  ╠═════════════════════╣
                  ║                     ║
                  ║   Application Core  ║
                  ║   (Hexagon)         ║
                  ║                     ║
                  ║  - TU Metrics       ║
                  ║  - Include Analysis  ║
                  ║  - Impact Engine    ║
                  ║  - Target Model     ║
                  ║  - Diagnostics      ║
                  ║                     ║
                  ╠═════════════════════╣
                  ║   Secondary Ports   ║
                  ║  (Driven)           ║
                  ╚══╤═══════╤═══════╤══╝
                     │       │       │
              ┌──────▼──┐ ┌──▼───┐ ┌─▼──────────┐
              │ Input    │ │Report│ │ Include     │
              │ Adapters │ │Adapt.│ │ Adapters    │
              └─────────┘ └──────┘ └──────��──────┘
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

| Adapter | Implementiert | Beschreibung | MVP |
|---|---|---|---|
| CompileCommandsJsonAdapter | `CompileDatabasePort` | Liest und validiert `compile_commands.json` | ja |
| SourceParsingIncludeAdapter | `IncludeResolverPort` | Parst Quelldateien entlang der `-I`-Pfade aus dem Compile-Aufruf | ja (Kandidat) |
| CompilerDepsIncludeAdapter | `IncludeResolverPort` | Wertet `.d`-Dependency-Dateien oder compilergestuetzte `-M`-Ausgaben aus | spaeter |
| ConsoleReportAdapter | `ReportWriterPort` | Schreibt Ergebnisse auf die Konsole | ja |
| MarkdownReportAdapter | `ReportWriterPort` | Erzeugt einen Markdown-Bericht | ja |
| HtmlReportAdapter | `ReportWriterPort` | Erzeugt einen HTML-Bericht | spaeter |
| JsonReportAdapter | `ReportWriterPort` | Erzeugt JSON-Ausgabe mit Schema-Version | spaeter |
| DotReportAdapter | `ReportWriterPort` | Erzeugt DOT/Graphviz-Ausgabe | spaeter |
| CmakeFileApiAdapter | `TargetMetadataPort` | Liest Target-Informationen aus der CMake File API | spaeter |

## 4. Datenfluss

1. Der **CLI Adapter** nimmt Kommando, Eingabepfade und Optionen entgegen und ruft den passenden Primary Port auf.
2. Der Kern ruft ueber den `CompileDatabasePort` die Compile-Eintraege ab. Der Adapter validiert und normalisiert die Daten.
3. Der Kern ruft ueber den `IncludeResolverPort` die Include-Beziehungen ab. Welcher Adapter dahinter steht, ist dem Kern nicht bekannt.
4. Der Kern fuehrt die angeforderte Analyse durch (TU-Ranking, Hotspots, Impact) und sammelt dabei Diagnostics-Informationen.
5. Der Kern reicht das Analyseergebnis (einschliesslich Diagnostics) an den `ReportWriterPort`. Der aktive Adapter erzeugt die Ausgabe im gewaehlten Format.
6. Der **CLI Adapter** setzt den Exit-Code basierend auf dem Ergebnis.

### 4.1 Abhaengigkeitsrichtung

Alle Abhaengigkeiten zeigen nach innen:

```
CLI Adapter  ──►  Primary Ports  ──►  Application Core
                                            │
                                            ▼
                                      Secondary Ports
                                            │
CompileCommandsJsonAdapter  ◄───────────────┘
SourceParsingIncludeAdapter ◄──────���────────┘
ConsoleReportAdapter        ◄─────���─────────┘
MarkdownReportAdapter       ◄───────────────┘
```

Kein Adapter kennt einen anderen Adapter. Der Kern kennt keine konkreten Adapter, nur die Port-Schnittstellen.

## 5. Architekturelle Entscheidungen

### 5.1 Include-Datenstrategie als austauschbarer Adapter

Die offene Frage der Include-Datenherkunft wird durch den `IncludeResolverPort` architekturell geloest. Der Kern arbeitet gegen die Port-Abstraktion; der konkrete Adapter kann ausgetauscht werden, ohne den Kern zu aendern.

Fuer den MVP wird ein einzelner Adapter implementiert. Die bevorzugte Strategie ist das **Parsen von Quelldateien entlang der Include-Pfade** (`SourceParsingIncludeAdapter`), da diese Variante:

- keinen vorherigen Build erfordert
- keinen Compiler auf dem Analysesystem voraussetzt
- in CI-Umgebungen ohne Build-Artefakte funktioniert

Alternative Adapter (compilergestuetzte Dependency-Informationen, vorhandene `.d`-Dateien) koennen spaeter als weitere `IncludeResolverPort`-Implementierungen ergaenzt werden.

### 5.2 Stabilitaet der internen Modelle

Interne Datenmodelle sollen:

- Eingabedaten klar von abgeleiteten Daten trennen
- Unsicherheit und Datenluecken ausdruecken koennen
- fuer mehrere Reporter wiederverwendbar sein
- reproduzierbare Sortierung und Auswertung ermoeglichen

Die Modelle gehoeren zum Kern und sind frei von Adapter-spezifischen Details. Die Port-Schnittstellen verwenden ausschliesslich Kernmodelle, keine adapterspezifischen Typen.

### 5.3 Erweiterbarkeit

Neue Analysearten erweitern den Kern und fuegen bei Bedarf neue Secondary Ports hinzu. Neue Ausgabeformate erfordern nur einen neuen `ReportWriterPort`-Adapter. Neue Eingabequellen erfordern nur einen neuen Adapter fuer den jeweiligen Secondary Port. In keinem Fall muss bestehender Adapter-Code geaendert werden.

### 5.4 Externe Abhaengigkeiten

Externe Abhaengigkeiten sollen minimal gehalten werden, um die Einstiegshuerde fuer Beitragende niedrig zu halten (`RB-06`, `RB-07`). Wo bewaehrte Bibliotheken einen klaren Vorteil gegenueber Eigenimplementierungen bieten, sollen sie bevorzugt werden. Fuer den MVP werden mindestens Entscheidungen zu folgenden Bereichen benoetigt:

- JSON-Parsing (fuer `compile_commands.json` und spaetere JSON-Ausgaben)
- CLI-Argument-Parsing (im CLI Adapter)
- Test-Framework

Externe Abhaengigkeiten duerfen nur in Adaptern oder in der Adapter-Verdrahtung auftreten, nicht im Kern.

### 5.5 Testbarkeitsstrategie

Die hexagonale Architektur unterstuetzt Testbarkeit direkt: Secondary Ports koennen durch Test-Doubles ersetzt werden, ohne Dateisystem oder CLI.

| Testebene | Was wird getestet | Wie |
|---|---|---|
| Unit-Tests | Einzelne Kernbausteine (Metriken, Include Analysis, Impact Engine) | Test-Doubles fuer Secondary Ports; definierte Eingaben, erwartete Ergebnisse |
| Integrationstests | Datenfluss durch den gesamten Kern | Test-Doubles fuer alle Secondary Ports; Pruefung der Ergebnisstruktur |
| Adapter-Tests | Einzelne Adapter isoliert (z.B. JSON-Parsing, Markdown-Erzeugung) | Echte Dateien oder Strings als Eingabe; Ausgabe gegen Erwartung pruefen |
| End-to-End-Tests | CLI-Werkzeug als Ganzes | Referenzprojekte (`NF-19`); Ausgaben gegen erwartete Ergebnisse vergleichen |

Die Referenzumgebung und Referenzprojekte aus `NF-04` bis `NF-06` und `NF-19` sollen auch als Grundlage fuer die Testinfrastruktur dienen.

### 5.6 Diagnostics als Querschnittsaspekt

Diagnostics ist kein eigener Port oder Adapter, sondern ein **Protokoll innerhalb des Kerns**. Analyseschritte fuegen Warnungen, Hinweise und Datenluecken an das Analyseergebnis an. Reporter-Adapter geben diese Informationen formatgerecht aus. Der CLI Adapter leitet daraus Exit-Codes ab.

## 6. Risiken fuer die Architektur

| Risiko | Auswirkung | Minderung durch Hexagonal |
|---|---|---|
| Include-Ableitung ist teuer oder unzuverlaessig | Performance- und Zuverlaessigkeitsprobleme | Adapter austauschbar; alternative Strategie ohne Kernaenderung moeglich |
| Zusatzdaten fuer Targets fehlen oft | eingeschraenkte Target- und Impact-Sicht | `TargetMetadataPort` liefert leere Ergebnisse; Kern behandelt Fehlen explizit |
| Reporter kennen zu viele Analyse-Interna | geringe Erweiterbarkeit | Reporter arbeiten nur gegen Kernmodelle ueber `ReportWriterPort` |
| CLI und Analysekern sind zu eng gekoppelt | schlechte Testbarkeit | CLI ist nur ein Primary Adapter; Kern testbar ueber Ports ohne CLI |
| Port-Abstraktionen werden zu frueh zu komplex | Over-Engineering | MVP startet mit je einer Implementierung pro Port; Abstraktion waechst mit den Anforderungen |

## 7. Rueckverfolgbarkeit

| Architekturthema | Lastenheft-Kennungen |
|---|---|
| Application Core | `F-06` bis `F-25`, `NF-15` |
| CompileDatabasePort / Adapter | `F-01` bis `F-05`, `F-41`, `S-01` |
| IncludeResolverPort / Adapter | `F-12` bis `F-17`, `S-02` |
| TargetMetadataPort / Adapter | `F-18` bis `F-20`, `S-02` |
| ReportWriterPort / Adapter | `F-26` bis `F-30`, `NF-20` |
| CLI Adapter | `F-31` bis `F-40`, `NF-01`, `NF-02` |
| Diagnostics | `F-03`, `F-09`, `F-23`, `NF-02`, `NF-14`, `NF-15` |
| Externe Abhaengigkeiten | `RB-06`, `RB-07` |
| Testbarkeit | `NF-10`, `NF-19`, `NF-04` bis `NF-06` |
| Plattform- und Laufzeitrahmen | `NF-07`, `RB-01` bis `RB-05` |

## 8. Offene Architekturfragen

- Welche interne Repraesentation eignet sich fuer reproduzierbare Rankings und sortierte Berichte?
- Soll der `IncludeResolverPort` im MVP bereits mehrere Adapter unterstuetzen oder reicht einer?
- Wie wird der `SourceParsingIncludeAdapter` mit Compiler-spezifischen Include-Konventionen umgehen (z.B. `__has_include`, bedingte Includes)?
- Welche Formatversionen werden fuer spaetere JSON-Ausgaben benoetigt?
- Wie werden Adapter in der Anwendung verdrahtet (manuelle Konstruktion, einfache Factory, Konfiguration)?
