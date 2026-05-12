# Plan M6 - Target-Graph, Analysekonfiguration und Vergleichssichten (`v1.3.0`)

## 0. Dokumenteninformationen

| Feld | Wert |
|---|---|
| Dokument | Plan M6 `cmake-xray` |
| Dokumentrevision | `0.2` |
| Zielrelease | `v1.3.0` |
| Stand | `2026-05-12` |
| Status | in Arbeit |
| Referenzen | [Lastenheft](../../../spec/lastenheft.md), [Design](../../../spec/design.md), [Architektur](../../../spec/architecture.md), [Phasenplan](./roadmap.md), [Plan M5](../done/plan-M5.md), [Qualitaet](../../user/quality.md), [Releasing](../../user/releasing.md) |

### 0.1 Zweck
Dieser Plan beschreibt die konkreten Schritte fuer Milestone M6 (`v1.3.0`). Ziel ist der Ausbau nach M5: `cmake-xray` soll direkte Target-Abhaengigkeiten als eigene Analysegrundlage nutzen, betroffene Targets ueber den Target-Graphen priorisieren, Include-Sichten verfeinern, Analyseumfang und Schwellenwerte gezielter konfigurierbar machen und Projektanalysen zwischen zwei Zeitpunkten vergleichbar ausgeben.

M6 erweitert damit erstmals wieder den fachlichen Analyseumfang. Die in M5 eingefuehrten Ausgabeformate bleiben die Auslieferungswege; M6 darf sie um neue fachliche Felder und Abschnitte erweitern, muss dabei aber bestehende Format- und Kompatibilitaetsvertraege respektieren.

### 0.2 Abschlusskriterium
M6 gilt als erreicht, wenn:

- `cmake-xray analyze` bei vorhandenen CMake-File-API-Daten direkte Target-zu-Target-Abhaengigkeiten ausgibt und den Ladezustand des Target-Graphen sichtbar macht
- Targets mit vielen ein- oder ausgehenden direkten Abhaengigkeiten reproduzierbar hervorgehoben werden
- `cmake-xray impact` betroffene Targets nicht nur aus TU-Zuordnungen ableitet, sondern bei vorhandenem Target-Graphen nach direkter und transitiver Betroffenheit priorisiert und angeforderte sowie wirksame Traversaltiefe sichtbar macht
- Include-Hotspots zwischen Projekt-Headern und externen Headern unterscheiden koennen, sofern die Datenlage dies belastbar erlaubt
- Include-Auswertungen auf direkte Includes, indirekte Includes oder beide Sichten begrenzt werden koennen, ohne die Heuristik-Hinweise zu verlieren
- einzelne Analyseabschnitte fuer `analyze` gezielt aktiviert oder deaktiviert werden koennen
- Schwellenwerte fuer TU-Ranking, Include-Hotspots und Target-Hub-Hervorhebung ueber CLI-Optionen steuerbar sind und in Reports sichtbar werden
- ein Vergleich zweier versionierter Analysezeitpunkte aus stabilen `analyze`-JSON-Berichten erzeugt werden kann
- Console, Markdown, HTML, JSON und DOT die neuen M6-Ergebnisdaten fuer `analyze` und `impact` deterministisch, dokumentiert und regressionsgesichert ausgeben
- `compare` seine M6-Ergebnisdaten deterministisch in Console, Markdown und JSON ausgibt; DOT und HTML sind fuer `compare` in M6 ausdruecklich nicht unterstuetzt
- JSON-Vertraege, DOT-Vertraege, Beispiele, README, Guide, Qualitaetsdokumentation und Changelog auf den M6-Umfang aktualisiert sind
- automatisierte Kern-, Adapter-, CLI-, Golden- und Schema-/Syntax-Tests die neuen Analysepfade und Konfigurationsoptionen absichern

Relevante Kennungen: `F-10`, `F-11`, `F-16`, `F-17`, `F-18`, `F-20`, `F-25`, `F-37`, `F-38`

### 0.3 Abgrenzung
Bestandteil von M6 sind:

- direkte Target-Graph-Modelle im Kern auf Basis vorhandener CMake-File-API-Daten
- textuelle Darstellung direkter Target-Abhaengigkeiten in Console und Markdown
- Target-Hub-Auswertung fuer Targets mit vielen eingehenden oder ausgehenden direkten Abhaengigkeiten
- target-graph-basierte Impact-Priorisierung fuer betroffene Targets
- Erweiterung von HTML, JSON und DOT um M6-Target-Graph- und Priorisierungsdaten fuer `analyze` und `impact`
- Include-Klassifikation `project` vs. `external`, soweit aus Projektwurzel, Include-Pfaden und Quellpfaden belastbar ableitbar
- steuerbare Include-Sicht `direct`, `indirect` oder `all`, soweit der aktive Include-Resolver diese Unterscheidung liefern kann
- CLI-Konfiguration fuer Analyseauswahl, fachliche Schwellenwerte und Impact-Traversaltiefe
- Vergleich zweier `analyze`-JSON-Berichte als stabiler Analysezeitpunkt-Vergleich mit Console-, Markdown- und JSON-Ausgabe
- Referenzdaten, Goldens, Schema-/Formatdokumentation und Beispiele fuer die neuen Sichten

Nicht Bestandteil von M6 sind:

- automatische Erzeugung von CMake-File-API-Query-Dateien oder implizite CMake-Ausfuehrung
- eine GUI oder interaktive HTML-Anwendung
- ein neuer Build-Graph fuer Nicht-CMake-Buildsysteme
- automatische Refactoring- oder Optimierungsvorschlaege
- eine vollstaendige semantische C++-Include-Auswertung mit Praeprozessor-Interpretation
- eine neue allgemeine Konfigurationsdatei; M6 nutzt explizite CLI-Optionen
- Vergleich beliebiger HTML-, Markdown-, DOT- oder Console-Berichte; der Vergleich basiert auf versionierten JSON-Analyseberichten
- Vergleich von `impact`-JSON-Berichten oder Impact-spezifischen M6-Feldern; M6-Compare ist ausdruecklich `analyze`-only
- DOT-Ausgabe fuer `compare`; Differenzgraphen brauchen einen eigenen spaeteren Darstellungsvertrag
- stillschweigende Breaking Changes an JSON-`format_version: 1`; inkompatible oder verpflichtende neue JSON-Felder erfordern eine dokumentierte neue Formatversion
- vollstaendige Plattform-Hochstufung ueber den in M5 dokumentierten Status hinaus, sofern dafuer keine eigenen Release- und CI-Gates entstehen

M6 baut auf M4 und M5 auf. Ohne CMake-File-API-Daten bleiben Target-Graph-Abschnitte leer, entfallen nach dokumentierter Formatregel oder melden `not_loaded`; bestehende Analysepfade ueber `compile_commands.json` muessen weiter nutzbar bleiben.

### 0.4 Aktueller Stand der Arbeitspakete

| AP                       | Status      | Plan                                                | Lead-Commit |
| ------------------------ | ----------- | --------------------------------------------------- | ----------- |
| 1.1 Target-Graph-Vertraege   | `erledigt`  | [`done/plan-M6-1-1.md`](../done/plan-M6-1-1.md)     | siehe Liefer-Stand-Block im Plan |
| 1.2 Target-Graph-Ausgaben    | `erledigt`  | [`done/plan-M6-1-2.md`](../done/plan-M6-1-2.md)     | siehe Liefer-Stand-Block im Plan |
| 1.3 Impact-Priorisierung     | `erledigt`  | [`done/plan-M6-1-3.md`](../done/plan-M6-1-3.md)     | siehe Liefer-Stand-Block im Plan |
| 1.4 Include-Sicht v4         | `erledigt`  | [`done/plan-M6-1-4.md`](../done/plan-M6-1-4.md)     | `2f83d7e` (Lead A.5 step 25c), Liefer-Stand-Block listet alle Tranche-Hashes |
| 1.5 Analyseauswahl + Budgets | `erledigt`  | [`done/plan-M6-1-5.md`](../done/plan-M6-1-5.md)     | `4b03c0d` (Lead A.6 Audit-Pass), Liefer-Stand-Block listet alle Tranche-Hashes |
| 1.6 Compare-Sicht            | `in Arbeit` | [`in-progress/plan-M6-1-6.md`](./plan-M6-1-6.md)    | — (A.1 lokal umgesetzt, Lead-Commit folgt) |
| 1.7 Referenzdaten + Doku     | `offen`     | [`open/plan-M6-1-7.md`](../open/plan-M6-1-7.md)     | —           |
| 1.8 Praeprozessor-Include    | `offen`     | [`open/plan-M6-1-8.md`](../open/plan-M6-1-8.md)     | —           |

Roadmap §0.2 spiegelt dieselbe Status-Sicht milestone-uebergreifend. Per-AP-Liefer-Stand-Bloecke leben in den jeweiligen Sub-Plaenen; dieses Dokument bleibt der M6-Master mit Arbeitspaket-Beschreibungen und Reihenfolge.

## 1. Arbeitspakete

### 1.1 Target-Graph-Vertraege im Kern schaerfen

M4 fuehrte Target-Zuordnungen fuer Translation Units ein. M6 erweitert diese Sicht um direkte Target-zu-Target-Abhaengigkeiten, ohne die bestehenden TU- und Include-Modelle mit CMake-spezifischen JSON-Strukturen zu koppeln.

Fuer M6 benoetigt der Kern mindestens:

- ein `TargetDependency`-Modell mit stabilem Quell-Target, Ziel-Target, Abhaengigkeitsart und Provenienz
- eine eindeutige Richtungsregel: `from` ist das konsumierende Target, `to` ist das Target, von dem es direkt abhaengt
- einen `TargetGraph` oder gleichwertigen Ergebnisbaustein mit Target-Knoten, direkten Kanten, Diagnostics und Ladezustand
- deterministische Sortierung von Targets und Kanten nach stabilem Target-Schluessel, Anzeige-Name, Typ und Kantenart
- explizite Behandlung von Zyklen, fehlenden Target-Definitionen und externen bzw. nicht aufloesbaren Abhaengigkeitszielen
- getrennte Zaehler fuer eingehende und ausgehende direkte Abhaengigkeiten
- Schwellwertlogik fuer Hub-Erkennung, die keine Report-Adapter-eigene Fachlogik erfordert

Wichtig:

- direkte Target-Abhaengigkeiten duerfen nur aus belastbaren Build-Metadaten entstehen; fehlende oder unvollstaendige File-API-Daten fuehren zu Diagnostics, nicht zu erratenen Kanten
- Target-Identitaet bleibt stabiler als Anzeige-Name; gleichnamige Targets in unterschiedlichen Kontexten duerfen nicht versehentlich zusammenfallen
- Target-Graph-Daten muessen optional sein, damit Compile-Database-only-Laeufe nicht fehlschlagen
- Zyklen sind fuer Darstellung und Impact-Priorisierung zulaessig, muessen aber beim Traversieren mit visited-Sets begrenzt werden
- die Fachlogik fuer Hub-Erkennung und Impact-Priorisierung liegt im Hexagon, nicht in Console-, Markdown-, HTML-, JSON- oder DOT-Adaptern

Vorgesehene Artefakte:

- Erweiterung von `src/hexagon/model/target.*` oder neuen Target-Graph-Modellen unter `src/hexagon/model/`
- Erweiterung von `src/hexagon/model/build_model_result.*` um direkte Target-Kanten und Graph-Diagnostics
- Anpassung von `src/adapters/input/cmake_file_api_adapter.*`
- Anpassung von `src/hexagon/services/project_analyzer.*`
- Kern- und Adapter-Tests fuer direkte Abhaengigkeiten, Zyklen, fehlende Ziele, Mehrfachkanten und stabile Sortierung

**Ergebnis**: Der Kern besitzt eine belastbare Target-Graph-Sicht, die direkt aus File-API-Daten gespeist wird und fuer Reports sowie Impact-Priorisierung wiederverwendbar ist.

### 1.2 Target-Graph-Ausgaben und Hub-Hervorhebung umsetzen

`F-18` verlangt eine textuelle Ausgabe direkter Target-Abhaengigkeiten. M6 muss diese Sicht in Console und Markdown liefern und die bestehenden M5-Formate konsistent erweitern.

Fuer `analyze` gilt:

- Console und Markdown erhalten einen eigenen Abschnitt fuer direkte Target-Abhaengigkeiten, sobald Target-Graph-Daten geladen wurden
- jede Kante wird in der Form `<from> -> <to>` oder einer dokumentierten gleichwertigen Textform ausgegeben
- `<from>` und `<to>` verwenden im Normalfall den eindeutigen Target-Anzeigenamen; wenn ein Anzeigename nicht eindeutig ist, wird die textuelle Form zu `<display_name> [key: <stable_target_key>]` erweitert
- JSON und DOT geben fuer jede Target-Kante immer stabile Target-Keys und Anzeige-Namen aus; Console, Markdown und HTML muessen bei Namenskollisionen dieselbe Disambiguierungsregel sichtbar machen
- Targets mit vielen eingehenden oder ausgehenden direkten Abhaengigkeiten werden in einem eigenen Hub-Abschnitt hervorgehoben
- die wirksamen Hub-Schwellenwerte werden in der Uebersicht oder im Abschnittskopf sichtbar gemacht
- bei fehlendem Target-Graphen wird kein scheinbar leerer Erfolg suggeriert; der Ladezustand bleibt sichtbar

Fuer JSON, HTML und DOT gilt:

- JSON erhaelt strukturierte Target-Graph- und Hub-Daten nur nach dokumentierter Formatentscheidung; inkompatible Pflichtfelder erhoehen `format_version`
- HTML stellt Target-Graph und Hubs menschenlesbar dar, ohne externe Ressourcen oder JavaScript-Pflicht einzufuehren
- DOT darf erstmals Target-zu-Target-Kanten ausgeben; diese Kanten muessen klar von M5-TU-, Include- und Impact-Kanten unterscheidbar sein
- alle Formate behalten stabile Sortierung, Escaping-Regeln und keine volatilen Metadaten bei

Wichtig:

- `--top` begrenzt weiterhin praesentierte Listen, nicht die intern geladene Target-Graph-Menge
- Hub-Schwellenwerte duerfen Kanten nicht aus dem Graphen entfernen; sie steuern nur Hervorhebung und Abschnittsinhalte
- DOT bleibt ein Graphviz-Quelltext, keine gerenderte SVG-/PNG-Datei
- Console-/Markdown-Goldens ohne File-API-Daten duerfen sich durch M6 nicht unnoetig aendern

Uebergreifender Sortiervertrag fuer M6:

- Alle M6-Ausgaben verwenden dieselben fachlichen Sortierschluessel wie die Kernmodelle; Formatadapter duerfen keine eigene Container-, Map- oder Locale-Reihenfolge einfuehren.
- Strings werden fuer Sortierschluessel nach den dokumentierten Normalisierungsregeln verglichen und danach byteweise lexikografisch in UTF-8 sortiert; Locale-, Plattform- oder Dateisystem-Reihenfolgen sind nicht Teil des Vertrags.
- Enum-aehnliche Felder werden nur ueber explizit dokumentierte Rangtabellen sortiert, nicht ueber Stringwerte oder Implementierungs-Ordinalwerte.
- Target-Knoten sortieren nach `(stable_target_key, display_name, target_type)`, Target-Kanten nach `(from_stable_target_key, to_stable_target_key, edge_kind, from_display_name, to_display_name)`, Translation-Unit-Eintraege bei gleicher Metrik nach `(normalized_source_path, normalized_build_context_key)`, und Include-Hotspots bei gleicher Zaehler-/Rankingmetrik nach `(normalized_header_display_path, include_origin, include_depth_kind)`.
- Compare-Diff-Gruppen verwenden zusaetzlich ihre dokumentierte Gruppenreihenfolge `added`, `removed`, `changed`; innerhalb einer Gruppe gilt der stabile Vergleichsschluessel der jeweiligen Entitaet und danach der Anzeige-Name als Tie-Breaker.

Vorgesehene Artefakte:

- Anpassung von `src/adapters/output/console_report_adapter.*`
- Anpassung von `src/adapters/output/markdown_report_adapter.*`
- Anpassung von `src/adapters/output/html_report_adapter.*`
- Anpassung von `src/adapters/output/json_report_adapter.*`
- Anpassung von `src/adapters/output/dot_report_adapter.*`
- Aktualisierung von `spec/report-json.md`, `spec/report-json.schema.json`, `spec/report-dot.md` und `spec/report-html.md`
- Golden-Outputs fuer Console, Markdown, HTML, JSON und DOT

**Ergebnis**: Nutzer koennen direkte Target-Abhaengigkeiten und auffaellige Target-Hubs in allen etablierten M5-Ausgabewegen nachvollziehen.

### 1.3 Impact-Priorisierung ueber den Target-Graphen einfuehren

M4 und M5 koennen betroffene Targets aus betroffenen Translation Units ableiten. M6 priorisiert diese Targets zusaetzlich ueber direkte und transitive Target-Abhaengigkeiten.

Fuer `impact` benoetigt M6 mindestens:

- eine Zielmenge direkt betroffener Owner-Targets aus betroffenen Translation Units
- eine Traversierung ueber reverse Target-Abhaengigkeiten, damit Targets sichtbar werden, die von einem betroffenen Target abhaengen
- eine Prioritaetsklasse fuer direkt betroffene Targets
- eine Prioritaetsklasse fuer direkt nachgelagerte Targets
- eine Prioritaetsklasse fuer transitiv nachgelagerte Targets
- eine getrennte Evidenzachse fuer `direct` und `heuristic`, damit include-basierte Unsicherheit nicht mit Target-Graph-Tiefe vermischt wird
- eine maximale Traversaltiefe als deterministisches Graph-Budget, damit grosse oder zyklische Graphen beherrschbar bleiben
- CLI-Steuerung `--impact-target-depth <n>` mit Default `2`: `0` beschraenkt die Sicht auf direkt betroffene Owner-Targets, positive Werte erlauben entsprechend viele reverse Target-Graph-Schritte, Werte groesser `32` und negative Werte sind CLI-Verwendungsfehler
- CLI-Steuerung `--require-target-graph` fuer Nutzer, die fehlende Target-Graph-Daten als harten Fehler behandeln wollen
- Diagnostics, wenn der Target-Graph fehlt, partiell ist oder wegen Budget/Zyklus begrenzt wurde

Wichtig:

- die bestehende Impact-Klassifikation bleibt erhalten; M6 ergaenzt Prioritaet und Target-Graph-Tiefe, ersetzt aber nicht `direct|heuristic|uncertain`
- direkte TU-Treffer gewinnen gegen heuristische Treffer, wenn dasselbe Target aus beiden Wegen erreicht wird
- eine kuerzere Target-Graph-Distanz gewinnt gegen eine laengere Distanz; bei gleicher Distanz bleibt die staerkere Evidenz sichtbar
- `evidence_strength` verwendet fuer M6 ausschliesslich die Werte `direct`, `heuristic` und `uncertain` mit der festen Sortierstaerke `direct` vor `heuristic` vor `uncertain`; neue Werte brauchen eine dokumentierte Vertrags- und Golden-Aenderung
- `priority_class` verwendet fuer M6 ausschliesslich die Werte `direct`, `direct_dependent` und `transitive_dependent` mit der festen Sortierstaerke `direct` vor `direct_dependent` vor `transitive_dependent`; die Sortierung darf nicht aus String- oder Enum-Ordinalwerten abgeleitet werden
- `direct` bedeutet `graph_distance=0`, `direct_dependent` bedeutet `graph_distance=1`, und `transitive_dependent` bedeutet `graph_distance>=2`; falls mehrere Wege dasselbe Target erreichen, bestimmt die minimale erreichte Distanz die Prioritaetsklasse
- reverse Target-Graph-Traversierung verwendet eine deterministische BFS: Start-Targets, Frontier-Knoten und ausgehende reverse Kanten werden vor jedem Schritt nach `(stable_target_key, edge_kind, display_name, target_type)` sortiert; die erste erreichte minimale Distanz gewinnt, weitere gleich lange Pfade ergaenzen nur Diagnostics oder Provenienz, aber aendern nicht `graph_distance` oder `priority_class`
- die vollstaendige Sortierung priorisierter Impact-Targets erfolgt deterministisch nach `(priority_class, graph_distance, evidence_strength, stable_target_key, display_name, target_type)`; fuer `evidence_strength` sortiert `direct` vor `heuristic`, und alle Felder muessen im Ergebnis oder ueber das Target-Modell stabil verfuegbar sein
- Traversierung und Sortierung muessen reproduzierbar sein; erreichte Tiefen- oder Kantenbudgets muessen im Ergebnis und in Reports sichtbar werden
- ohne Target-Graph bleibt `impact` im Default-Fall auf dem M5-Verhalten, dokumentiert die fehlende Graph-Priorisierung und setzt `impact_target_depth_requested` auf den CLI-/Default-Wert sowie `impact_target_depth_effective` auf `0`
- nur `--require-target-graph` macht fehlende oder nicht nutzbare Target-Graph-Daten zum CLI-/Eingabefehler; das Setzen von `--impact-target-depth` allein darf Compile-Database-only-Laeufe nicht brechen

Vorgesehene Artefakte:

- Erweiterung von `src/hexagon/model/impact_result.*`
- Erweiterung von `AnalyzeImpactRequest` um die wirksame Target-Graph-Traversaltiefe
- Anpassung von `src/hexagon/services/impact_analyzer.*`
- Anpassung von Console-, Markdown-, HTML-, JSON- und DOT-Reportadaptern fuer priorisierte Impact-Targets, angeforderte und wirksame Traversaltiefe, Graph-Priorisierungsstatus und Budget-/Fallback-Diagnostics
- Dokumentation der Impact-M6-Felder in `spec/report-json.md`, `spec/report-json.schema.json`, `spec/report-dot.md` und `spec/report-html.md`
- Tests fuer direkte, heuristische, gemischte, zyklische und tiefenbegrenzte Target-Graph-Impact-Faelle
- Report- und Golden-Tests fuer priorisierte Impact-Target-Abschnitte in Console, Markdown, HTML, JSON und DOT

**Ergebnis**: Impact-Berichte koennen nicht nur sagen, welche Targets betroffen sind, sondern auch, wie nah diese Betroffenheit am geaenderten Artefakt liegt.

### 1.4 Include-Sicht nach Herkunft und Tiefe verfeinern

M2 bis M5 behandeln Include-Hotspots bewusst heuristisch. M6 ergaenzt die sichtbaren Include-Daten um Herkunft und Tiefe, soweit der aktive Include-Resolver diese Informationen liefern kann.

Fuer Include-Hotspots benoetigt M6 mindestens:

- Klassifikation `project`, `external` oder `unknown`
- Unterscheidung `direct`, `indirect` oder `mixed`
- CLI-Steuerung fuer Include-Herkunft als Single-Value-Enum `--include-scope <all|project|external|unknown>`, Default `all`
- CLI-Steuerung fuer Include-Tiefe als Single-Value-Enum `--include-depth <all|direct|indirect>`, Default `all`
- sichtbare Diagnostics, wenn eine gewuenschte Sicht mit der aktuellen Datenlage nicht belastbar getrennt werden kann
- stabile Zaehler fuer Gesamtmenge, zurueckgegebene Menge und Filter-/Top-Begrenzung

Wichtig:

- Projekt-Header duerfen nicht allein anhand einfacher String-Praefixe geraten werden; die Regel muss auf dokumentierten Projektwurzeln, Source-Roots, Include-Pfaden und System-Include-Hinweisen beruhen
- bei unsicherer Klassifikation ist `unknown` ehrlicher als `project` oder `external`
- `--include-scope all` enthaelt `project`, `external` und `unknown`; `--include-scope project` und `--include-scope external` schliessen `unknown` aus und muessen in der Uebersicht ausweisen, wie viele unbekannte Hotspots dadurch nicht gezeigt wurden
- `--include-scope unknown` ist fuer Diagnose- und Datenqualitaetslaeufe zulaessig und zeigt nur nicht belastbar klassifizierte Include-Hotspots
- Scope- und Depth-Filter werden als Schnittmenge angewendet: Ein Hotspot erscheint nur, wenn seine Herkunftsklasse zur gewaehlten `--include-scope` passt und seine Tiefenklasse zur gewaehlten `--include-depth` passt
- `--include-scope` und `--include-depth` akzeptieren genau einen Wert; Kommas, leere Werte, mehrfaches Setzen derselben Option und unbekannte Werte sind CLI-Verwendungsfehler ohne stdout-Report und ohne Zieldatei
- direkte Includes sind aus der unmittelbar geparsten Datei ableitbar; indirekte Includes setzen eine kontrollierte, zyklusfeste Transitivitaetsberechnung voraus
- indirekte Include-Transitivitaet verwendet fuer M6 `include_depth_limit = 32` und `include_node_budget = 10000` pro Analyze-Lauf; erreichte Grenzen kuerzen die indirekte Sicht deterministisch und erzeugen Diagnostics sowie strukturierte Reportfelder fuer angeforderte und wirksame Grenzen
- die indirekte Include-Transitivitaet verwendet eine deterministische BFS: Start-Translation-Units werden nach `(normalized_source_path, normalized_build_context_key)` sortiert, Include-Kanten pro Knoten nach `(normalized_including_path, normalized_included_path, include_origin, include_depth_kind, raw_spelling)`, und bereits besuchte Include-Knoten werden ueber den normalisierten Include-Knotenschluessel dedupliziert
- `include_node_budget` wird in dieser BFS-Entdeckungsreihenfolge verbraucht; sobald das Budget erreicht ist, werden keine weiteren neuen Include-Knoten mehr enqueued. Hotspots werden ausschliesslich aus den bis dahin deterministisch besuchten Knoten und Kanten gebildet, statt nachtraeglich anhand instabiler Container-Reihenfolge aus einer vollstaendigen Ergebnismenge entfernt zu werden.
- `--include-depth all` enthaelt `direct`, `indirect` und `mixed`; `--include-depth direct` zeigt nur rein direkte Hotspots, `--include-depth indirect` zeigt nur rein indirekte Hotspots
- `mixed` wird nur bei `--include-depth all` gezeigt; bei `direct` oder `indirect` wird `mixed` ausgeblendet und in der Uebersicht als ausgeblendete gemischte Hotspot-Anzahl ausgewiesen
- die wirksamen Include-Filter werden in allen Analyze-Reportformaten serialisiert, mindestens als `include_scope` und `include_depth`; bestehende Aufrufe ohne neue Flags serialisieren `include_scope=all` und `include_depth=all`
- bestehende Heuristik-Hinweise bleiben erhalten, weil der Source-Parsing-Adapter weiterhin keinen vollstaendigen Praeprozessor ersetzt
- Filter duerfen keine leeren Reports als Erfolg ohne Kontext ausgeben; die Uebersicht muss zeigen, welche Filter aktiv waren

Vorgesehene Artefakte:

- Erweiterung von `src/hexagon/model/include_classification.*`, `src/hexagon/model/include_filter_options.*` oder gleichwertigen Include-Modellen (urspruenglich `include_hotspot.h`, in AP M6-1.4 A.5 step 21pre aufgeteilt)
- Anpassung von `src/hexagon/ports/driven/include_resolver_port.h`
- Anpassung von `src/adapters/input/source_parsing_include_adapter.*`
- Anpassung von `src/hexagon/services/project_analyzer.*`
- CLI-Optionen in `src/adapters/cli/`
- Golden- und Adapter-Tests fuer Projekt-/Extern-/Unknown-Header sowie direkte/indirekte Sicht

**Ergebnis**: Include-Hotspots werden zielgerichteter nutzbar, ohne die verbleibende heuristische Natur der Include-Aufloesung zu verschleiern.

### 1.5 Analyseauswahl, Schwellenwerte und Impact-Budget konfigurierbar machen

M6 fuehrt eine explizite fachliche Konfiguration fuer Analyseumfang, Grenzwerte und Target-Graph-Traversierung ein. Diese Konfiguration bleibt command-lokal und wird ueber CLI-Optionen uebergeben.

Fuer `analyze` werden mindestens folgende Steuerungen benoetigt:

- `--analysis <list>` fuer Aktivierung bzw. Deaktivierung einzelner Analyseabschnitte; erlaubte Werte sind `all`, `tu-ranking`, `include-hotspots`, `target-graph` und `target-hubs`, mehrere Einzelwerte werden kommasepariert angegeben, der Default ist `all`
- `--tu-threshold <metric>=<n>` fuer TU-Ranking-Metrikschwellen; erlaubte Metriken sind `arg_count`, `include_path_count` und `define_count`, die Option darf mehrfach gesetzt werden, der Default ist keine zusaetzliche TU-Schwelle
- `--min-hotspot-tus <n>` als Mindestanzahl betroffener Translation Units fuer Include-Hotspots, Default `2`
- `--target-hub-in-threshold <n>` und `--target-hub-out-threshold <n>` fuer eingehende und ausgehende Target-Abhaengigkeiten bei Hub-Hervorhebung, Default jeweils `10`
- sichtbare Ausgabe der wirksamen Konfiguration in Reports
- klare CLI-Fehler fuer unbekannte Analysearten, negative Grenzwerte, widerspruechliche Optionen und aktiv angeforderte Datenquellen ohne definierten Fallback

Fuer `impact` wird mindestens folgende Steuerung benoetigt:

- `--impact-target-depth <n>` als angeforderte maximale reverse Target-Graph-Traversaltiefe mit Default `2`, erlaubtem Wertebereich `0` bis `32` und sichtbarer Report-Provenienz
- `--require-target-graph` als explizite Require-Option: fehlt ein nutzbarer Target-Graph, endet `impact` nonzero mit klarer Fehlermeldung und ohne Report; ohne diese Option degradiert `impact` kompatibel auf M5-Verhalten mit `impact_target_depth_effective=0`
- klare CLI-Fehler fuer negative Werte, Werte ueber `32`, nichtnumerische Werte und widerspruechliche Optionen
- Fehlermeldungen fuer `--impact-target-depth` muessen die Ursache konkret benennen: `negative value`, `not an integer` oder `value exceeds maximum 32`; alle drei Faelle enden als CLI-Verwendungsfehler ohne stdout-Report und ohne Zieldatei
- Diagnostics im Ergebnis, wenn die Traversierung wegen der wirksamen Tiefe, wegen Zyklen oder wegen eines internen Kantenbudgets begrenzt wurde

Wichtig:

- Konfiguration beeinflusst die Auswahl und Hervorhebung der Ergebnisse, nicht das Laden der Eingabedaten
- deaktivierte Analyseabschnitte duerfen keine versteckten Seiteneffekte auf andere aktive Analysen haben
- Der kompatible Target-Graph-Fallback hat Vorrang vor der allgemeinen Datenquellen-Fehlerregel: Fehlende oder nicht nutzbare Target-Graph-Daten sind fuer `analyze` und fuer `impact` ohne `--require-target-graph` kein harter CLI-Fehler, sondern fuehren zu Diagnostics, `not_loaded`-/Leerabschnitten und bei `impact` zu `impact_target_depth_effective=0`.
- `target-hubs` setzt `target-graph` voraus: Wird `--analysis target-hubs` ohne `target-graph` angegeben, ist das ein CLI-Verwendungsfehler mit der Ursache `target-hubs requires target-graph`; `--analysis all` enthaelt beide Abschnitte und ist gueltig
- Abhaengige Analyseabschnitte werden in M6 nicht automatisch ergaenzt: `--analysis` ist nach der Validierung eine geschlossene, kanonische Abschnittsauswahl, und Nutzer muessen Voraussetzungen wie `target-graph` explizit mit angeben.
- fehlende Target-Graph-Daten sind kein CLI-Verwendungsfehler fuer `--analysis all` oder `--analysis target-graph,target-hubs`; die betroffenen Abschnitte werden als `not_loaded` beziehungsweise leer mit Diagnostic ausgegeben. Der Fehler `target-hubs requires target-graph` betrifft ausschliesslich eine widerspruechliche Abschnittsauswahl, nicht die spaeter erkannte Datenlage.
- Defaults bleiben so gewaehlt, dass bestehende Nutzer ohne neue Optionen sinnvolle Ergebnisse erhalten
- `--analysis`-Listen werden durch Komma getrennt, Tokens werden um ASCII-Whitespace getrimmt, Werte sind lowercase und case-sensitiv, und die Reihenfolge nach der Validierung wird kanonisch nach `tu-ranking`, `include-hotspots`, `target-graph`, `target-hubs` normalisiert
- `--analysis all` darf nicht mit weiteren Analysewerten kombiniert werden; unbekannte Analysewerte, leere Listeneintraege, doppelte Analysewerte nach Trimming, mehrfaches Setzen von `--analysis`, unbekannte TU-Metriken, negative Schwellenwerte und nichtnumerische Schwellenwerte sind CLI-Verwendungsfehler ohne stdout-Report und ohne Zieldatei
- `--tu-threshold` darf mehrfach gesetzt werden, aber jede Metrik darf nur einmal vorkommen; doppelte Metriken nach Trimming sind CLI-Verwendungsfehler ohne stdout-Report und ohne Zieldatei, statt per "last wins" oder Parser-Reihenfolge aufgeloest zu werden
- angeforderte und wirksame Impact-Traversaltiefe sind Teil des fachlichen Ergebnisvertrags und duerfen nicht nur in der CLI leben
- `--quiet` und `--verbose` behalten ihre M5-Stream-Vertraege
- maschinenlesbare Formate muessen die wirksame Konfiguration strukturiert wiedergeben, damit CI-Auswertungen reproduzierbar sind

Vorgesehene Artefakte:

- neues oder erweitertes Request-Modell fuer `AnalyzeProjectRequest`
- erweitertes Request-Modell fuer `AnalyzeImpactRequest`
- Anpassung von `src/adapters/cli/cli_adapter.*`
- Anpassung von `src/hexagon/services/project_analyzer.*`
- Anpassung von `src/hexagon/services/impact_analyzer.*`
- Anpassung aller Reportadapter fuer wirksame Konfiguration
- CLI- und Service-Tests fuer Defaults, gueltige Konfigurationen, kompatiblen Target-Graph-Fallback, `--require-target-graph` und Fehlerfaelle

**Ergebnis**: Nutzer koennen Analyseberichte gezielter auf konkrete Fragestellungen zuschneiden, ohne neue Spezialkommandos oder Konfigurationsdateien zu benoetigen.

### 1.6 Vergleich zweier Analysezeitpunkte einfuehren

`F-11` wird in M6 als Vergleich zweier versionierter JSON-Analyseberichte umgesetzt. Dadurch muss `cmake-xray` nicht zwei Projekte gleichzeitig laden und analysieren, sondern nutzt die in M5 stabilisierte maschinenlesbare Reportbasis.

Vorgesehener CLI-Einstieg:

```text
cmake-xray compare --baseline <analysis.json> --current <analysis.json> --format console
cmake-xray compare --baseline <analysis.json> --current <analysis.json> --format markdown --output compare.md
cmake-xray compare --baseline <analysis.json> --current <analysis.json> --format json
cmake-xray compare --baseline <analysis.json> --current <analysis.json> --format console --allow-project-identity-drift
```

Der Vergleich soll mindestens ausgeben:

- identifizierte Eingabeberichte inklusive `format`, `format_version` und Report-Typ
- veraenderte Summary-Kennzahlen
- neu hinzugekommene, entfernte und veraenderte Translation-Unit-Ranking-Eintraege
- Veraenderungen an Include-Hotspots, inklusive Zaehlerdifferenzen
- Veraenderungen am Target-Graphen, sofern beide Berichte Target-Graph-Daten enthalten
- Veraenderungen an Target-Hubs, sofern beide Berichte Hub-Daten enthalten
- Diagnostics vom Typ `data_availability_drift`, wenn genau einer der beiden Berichte Target-Graph- oder Target-Hub-Daten enthaelt; in diesem Fall wird die betroffene Diff-Gruppe nicht berechnet, aber der Datenabfall bzw. Datenzuwachs muss in Console, Markdown und JSON sichtbar sein
- Diagnostics vom Typ `project_identity_drift`, wenn ein Compile-DB-only-Vergleich durch `--allow-project-identity-drift` trotz abweichender fallback-basierter Projektidentitaet fortgesetzt wird
- Diagnostics fuer inkompatible Schema-Versionen, fehlende Felder oder nicht vergleichbare Eingaben
- Diagnostics fuer unterschiedliche wirksame Analysekonfigurationen, damit Konfigurationsdrift von fachlichen Aenderungen unterscheidbar bleibt
- deterministisch sortierte Diff-Gruppen in der Reihenfolge `added`, `removed`, `changed`, jeweils nach stabilem Identitaetsschluessel und danach nach Anzeige-Name als Tie-Breaker
- einen eigenen JSON-Output-Vertrag fuer Compare mit `format: cmake-xray.compare`, eigener `format_version`, `report_type: compare`, `inputs`, `summary`, `diffs` und `diagnostics`

Wichtig:

- `compare` akzeptiert nur `analyze`-JSON-Berichte, keine `impact`-Berichte
- Impact-spezifische M6-Felder wie priorisierte Impact-Targets, Evidenzachsen, `impact_target_depth_requested` und `impact_target_depth_effective` gehoeren in M6 zum Impact-Reportvertrag, aber nicht zum Compare-Diff-Vertrag
- ein spaeterer Vergleich von `impact`-Berichten braucht einen eigenen Plan mit Schluessel-, Versions- und Diff-Regeln fuer Impact-spezifische Felder
- ein Vergleich zwischen JSON-Formatversionen ist nur zulaessig, wenn `spec/report-json.md` eine explizite Compare-Kompatibilitaetsmatrix fuer `(baseline_format_version, current_format_version)` enthaelt und die betroffene Kombination in Schema-/Golden-Tests abgedeckt ist
- die Kompatibilitaetsmatrix dokumentiert pro erlaubter Versionskombination mindestens Pflichtfelder, optionale Felder, Default-/Null-Mapping, umbenannte Felder, entfernte Felder und nicht vergleichbare Felder; nicht gelistete Versionskombinationen werden abgelehnt
- Die Compare-Kompatibilitaetsmatrix ist ein Implementierungs-Vorlaeufer fuer AP 1.6: Parser- und Difflogik duerfen erst umgesetzt oder erweitert werden, wenn die Matrix fuer die betroffene `format_version`-Kombination in `spec/report-json.md` dokumentiert und in Schema-/Golden-Tests angelegt ist.
- Fehlende Matrixeintraege sind in M6 immer Fail-fast-Ablehnungen; es gibt keinen impliziten Best-Effort-Vergleich und keinen nicht dokumentierten Migrationspfad.
- die Vergleichslogik arbeitet auf stabilen Identitaeten und Vergleichsschluesseln, nicht auf rein menschenlesbaren Anzeigezeilen
- Vergleichsidentitaeten werden aus fachlichen Schluesseln gebildet: Translation Units aus normalisiertem `source_path` plus normalisiertem Build-Kontextschluessel, Include-Hotspots aus normalisiertem Header-Anzeigepfad plus Herkunfts- und Tiefenklasse, Target-Knoten aus stabilem Target-Key und Target-Kanten aus stabilem `from`-/`to`-Target-Key plus Kantenart
- Pfadbasierte Vergleichsschluessel werden vor dem Vergleich relativ zur im jeweiligen JSON dokumentierten Projekt- bzw. Source-Root normalisiert; absolute Workspace-, Build- und Temp-Praefixe duerfen nicht allein zu fachlichen Diffs fuehren
- Pfadnormalisierung fuer Compare ist rein lexikalisch: Backslashes werden zu `/`, redundante `.`-Segmente werden entfernt, `..`-Segmente werden nur innerhalb der bekannten Projekt- bzw. Source-Root-Basis aufgeloest, ein trailing slash wird entfernt, Windows-Drive-Letters werden fuer Vergleichsschluessel lowercase normalisiert, und es erfolgt keine Symlink- oder Realpath-Aufloesung
- Pfadvergleich bleibt ansonsten case-sensitiv; eine spaetere plattformspezifische Case-Folding-Option waere ein eigener Vertragsumfang, weil sie fachliche Namenskollisionen verdecken kann
- beide Eingabeberichte transportieren ihre wirksame Analysekonfiguration strukturiert; unterschiedliche Werte fuer `analysis`, `tu_thresholds`, `include_scope`, `include_depth`, `min_hotspot_tus`, `target_hub_in_threshold` oder `target_hub_out_threshold` sind in M6 kein Abbruchgrund, muessen aber als `configuration_drift`-Diagnostics in Console, Markdown und JSON erscheinen
- `configuration_drift` muss maschinenlesbar mindestens `field`, `baseline_value`, `current_value`, `severity` und `ci_policy_hint` enthalten. M6 setzt `severity=warning`; `ci_policy_hint` markiert die Vergleichsbasis als `review_required`, damit CI oder nachgelagerte Qualitaetsregeln solche Vergleiche bewusst ablehnen koennen, ohne dass `compare` selbst nonzero endet.
- unterschiedliche Datenverfuegbarkeit fuer optionale Abschnitte ist in M6 kein Abbruchgrund, muss aber getrennt von `configuration_drift` als `data_availability_drift` erscheinen; mindestens `section`, `baseline_state` und `current_state` muessen maschinenlesbar enthalten sein
- beide Eingabeberichte muessen denselben normalisierten `project_identity`-Wert tragen; unterscheidet er sich, wird der Vergleich abgelehnt, ausser `--allow-project-identity-drift` erlaubt den unten beschriebenen Compile-DB-only-Sonderfall
- beide Eingabeberichte muessen ausserdem dieselbe `project_identity_source` verwenden; Cross-Mode-Vergleiche zwischen File-API-abgeleiteter Projektidentitaet und `fallback_compile_database_fingerprint` sind in M6 bewusst nicht zulaessig, auch wenn die normalisierten Analyseinhalte aehnlich aussehen
- Diese Cross-Mode-Guardrail bleibt in M6 ohne Escape-Hatch bestehen, weil File-API- und Compile-Database-only-Laeufe unterschiedliche Projektkontext- und Provenienzgarantien haben; `--allow-project-identity-drift` gilt nur fuer den Same-Mode-Fallback-Fall mit zwei Compile-DB-only-Berichten.
- `project_identity` wird im Analyze-JSON aus einer stabilen Projektkontextquelle gebildet, bevorzugt aus der normalisierten File-API-Source-Root-Provenienz und einem optionalen dokumentierten Projektkennwert
- fuer Compile-Database-only-Reports ohne File-API-Source-Root wird `project_identity` als `compile-db:<hash>` gebildet; der Hash basiert ausschliesslich auf den normalisierten relativen `source_path`-Werten, nicht auf normalisierten Build-Kontextschluesseln, Toolchain-Details, absoluten Workspace- oder Build-Praefixen
- Der Compile-DB-Fallback-Hash ist die lowercase Hex-Darstellung von SHA-256 ueber die UTF-8-Bytes einer kanonischen JSON-Payload ohne unbedeutende Whitespace-Zeichen: `{"kind":"cmake-xray.compile-db-project-identity","version":1,"source_paths":[...]}`. `source_paths` enthaelt die deduplizierten normalisierten relativen `source_path`-Werte in lexikografisch aufsteigender Byte-Reihenfolge nach UTF-8-Encoding; Objektschluessel stehen exakt in der gezeigten Reihenfolge, Arrays behalten die sortierte Reihenfolge, und String-Escaping folgt RFC 8259.
- Der Compile-DB-Fallback nimmt bewusst in Kauf, dass unterschiedliche Build-Kontexte mit identischem normalisiertem `source_path`-Satz dieselbe Projektidentitaet erhalten; fachliche Unterschiede aus Build-Kontexten erscheinen erst ueber Translation-Unit-Vergleichsschluessel und Drift-Diffs. Diese Einschraenkung ist ein akzeptierter M6-Trade-off und muss in Abnahmetests sichtbar sein.
- Eine Projektidentitaet gilt nur als belastbar, wenn `project_identity` nicht leer ist, `project_identity_source` einem dokumentierten M6-Quellwert entspricht und die Paarung aus Quelle und Wert konsistent ist. Fuer `fallback_compile_database_fingerprint` muss `project_identity` exakt `compile-db:` plus 64 lowercase Hex-Zeichen enthalten und aus den im Bericht enthaltenen normalisierten Translation-Unit-`source_path`-Werten reproduzierbar sein; leere Source-Path-Mengen, nicht normalisierbare Pfade oder ein nicht reproduzierbarer Hash machen den Bericht fuer Compare nicht vergleichbar.
- `--allow-project-identity-drift` wirkt ausschliesslich, wenn beide Eingabeberichte `project_identity_source=fallback_compile_database_fingerprint` verwenden, aber unterschiedliche `project_identity`-Werte tragen; der Vergleich laeuft dann weiter und muss `project_identity_drift` mit `baseline_project_identity`, `current_project_identity`, `baseline_source_path_count`, `current_source_path_count` und `shared_source_path_count` ausgeben
- `--allow-project-identity-drift` erlaubt keine Cross-Mode-Vergleiche, keine fehlenden oder unbrauchbaren Projektidentitaeten und keine abweichenden File-API-basierten Projektidentitaeten; diese Faelle bleiben nicht vergleichbare Eingaben
- Build-Kontextschluessel bleiben Teil der Translation-Unit-Vergleichsidentitaet und koennen fachliche Diffs erzeugen, duerfen aber nicht die fallback-basierte Projektidentitaet blockieren
- dieser fallback-basierte Projektvergleich ist zulaessig, muss aber in `inputs.project_identity_source=fallback_compile_database_fingerprint` und in den Compare-Diagnostics als eingeschraenkte Projektidentitaet sichtbar werden
- falls weder File-API-Source-Root noch Compile-Database-Fingerprint belastbar gebildet werden kann, wird Compare fuer diesen Bericht abgelehnt statt nur ueber rohe relative Pfade zu matchen
- Felder wie rohe Eingabepfade, Report-Erzeugungspfade, `cmake_file_api_resolved_path` oder andere hostbezogene Provenienzfelder sind Compare-Metadaten, aber keine Diff-Identitaet
- Ausgabeformate fuer `compare` sind in M6 ausschliesslich Console, Markdown und JSON; HTML und DOT sind in M6 nicht vorgesehen und duerfen auch nicht hinter einem Feature-Flag aktiviert werden
- DOT bleibt fuer M6-Compare ausgeschlossen, weil Differenzgraphen einen eigenen spaeteren Darstellungsvertrag brauchen

Fehlerregeln fuer `compare`:

- fehlende `--baseline`- oder `--current`-Argumente sind CLI-Verwendungsfehler
- nicht lesbare, syntaktisch ungueltige oder nicht schema-valide JSON-Dateien enden mit klarer Fehlermeldung, ohne Teilreport zu schreiben
- JSON-Berichte mit `report_type != analyze`, insbesondere `report_type=impact`, oder unbekanntem `format` werden abgelehnt
- fehlender, leerer, formal ungueltiger oder semantisch nicht reproduzierbarer `project_identity` wird als nicht vergleichbare Eingabe abgelehnt; nur ein gueltiger abweichender Compile-DB-Fallback-Wert darf mit `--allow-project-identity-drift` fortgesetzt werden
- fehlende, leere, unbekannte oder abweichende `project_identity_source` wird ebenfalls als nicht vergleichbare Eingabe abgelehnt; die Fehlermeldung muss `project identity source mismatch` oder `invalid project identity source` enthalten und den Nutzer auffordern, beide Analyze-Reports mit derselben belastbaren Eingabequelle neu zu erzeugen
- inkompatible `format_version`-Kombinationen werden abgelehnt, solange keine explizite Migrations- oder Vergleichsregel dokumentiert ist
- `--format dot`, `--format html` und `--output` mit `--format console` werden fuer M6 als unzulaessige Optionskombinationen behandelt
- bei Dateiausgabe gilt derselbe atomare Schreibvertrag wie fuer M5-Reportartefakte

Vorgesehene Artefakte:

- neues Compare-Ergebnismodell unter `src/hexagon/model/`
- neuer Driving Port oder Service fuer Analysevergleich
- JSON-Report-Leser in der Adapter- oder CLI-Schicht mit klarer Trennung zum Hexagon-Vertrag
- CLI-Unterkommando `compare`
- Console-, Markdown- und JSON-Ausgabe fuer Compare-Ergebnisse
- Dokumentation des Compare-JSON-Output-Vertrags in `spec/report-json.md` und `spec/report-json.schema.json`, inklusive Pflichtfeldern, optionalen Feldern, Nullability, Array-Regeln, Diff-Gruppen und eigener `format_version`
- Compare-Kompatibilitaetsmatrix in `spec/report-json.md`
- Schema-/Golden-Tests fuer jede erlaubte und mindestens eine inkompatible Vergleichsfaelle-Kombination aus der Matrix

**Ergebnis**: Nutzer koennen zwei Analysezeitpunkte reproduzierbar vergleichen und Aenderungen an Build-Komplexitaet, Include-Hotspots und Target-Struktur in CI oder Reviews sichtbar machen.

### 1.7 Referenzdaten, Dokumentation und Abnahme aktualisieren

M6 ist nur abnahmefaehig, wenn die neuen fachlichen Sichten in Referenzdaten, Goldens und Dokumentation nachvollziehbar sind.

M6 aktualisiert mindestens:

- `README.md` mit Target-Graph-, Include-Filter-, Schwellenwert- und Compare-Beispielen
- `docs/user/guide.md` mit praktischen Aufrufen fuer M6-Analyse- und Compare-Sichten
- `spec/report-json.md` und `spec/report-json.schema.json` mit M6-Feldern oder neuer JSON-Formatversion fuer Analyze, Impact und Compare; Compare erhaelt einen eigenen JSON-Output-Vertrag, und Impact-Felder werden dokumentiert, aber nicht in M6-Compare diffbar gemacht
- `spec/report-dot.md` mit Target-zu-Target-Kanten und M6-Graphregeln
- `spec/report-html.md` mit M6-Abschnitten
- `docs/examples/` mit repraesentativen M6-Beispielen fuer Analyze, Impact und Compare
- `docs/user/quality.md` mit M6-Testumfang
- `docs/user/performance.md` mit mindestens einer Bewertung fuer Target-Graph- und Compare-Pfade
- `CHANGELOG.md` fuer den M6-Stand

Tests und Abnahme muessen mindestens abdecken:

- File-API-Adapter-Tests fuer Target-Graph-Extraktion
- Service-Tests fuer Hub-Erkennung, Target-Graph-Impact-Priorisierung und Default-Fallback ohne Target-Graph
- Service- und Adapter-Tests fuer Include-Herkunft und Include-Tiefe
- CLI-Tests fuer Analyseauswahl, Schwellenwerte inklusive doppelter `--tu-threshold`-Metriken, Include-Filter, deterministische Include-Budget-Kappung, Impact-Traversaltiefe, `--require-target-graph`, optionale Abschnittszustaende und Compare-Fehlerregeln inklusive stabiler Pflichtphrasen
- Matrix-Tests fuer optionale Analyseabschnitte: absichtlich deaktiviert, angefordert aber `not_loaded`, angefordert und leer, widerspruechliche Auswahl wie `target-hubs` ohne `target-graph`, sowie `--analysis all` mit partiell fehlenden Optionaldaten
- Golden-Tests fuer Console, Markdown, HTML, JSON und DOT mit Target-Graph-Daten, zentralen Sortier-Tie-Breakern sowie Impact-Fallback ohne Target-Graph
- JSON-Schema-Tests fuer neue oder versionierte M6-Felder, den eigenen Compare-JSON-Output-Vertrag, `configuration_drift`-Policy-Felder und die Compare-Kompatibilitaetsmatrix
- DOT-Syntax-Tests mit Target-zu-Target-Kanten und Zyklen
- Compare-Tests fuer hinzugefuegte, entfernte und veraenderte Analyze-Eintraege, asymmetrische Target-Graph-/Hub-Datenverfuegbarkeit, `configuration_drift` mit CI-Policy-Hinweis, `--allow-project-identity-drift` bei Compile-DB-only-Reports, fehlende Kompatibilitaetsmatrix-Eintraege als Fail-fast, unbrauchbare Projektidentitaeten sowie Negativtests fuer `impact`-JSON-Berichte
- Golden-Tests fuer den Compile-DB-Fallback-Hash mit Unix- und Windows-artigen Pfadformen, unterschiedlicher Eingabereihenfolge, deduplizierten Source-Paths und reproduzierbarer kanonischer JSON-Payload
- Abnahmetests fuer den akzeptierten Compile-DB-Trade-off: gleicher `source_path`-Satz mit unterschiedlichen Build-Kontextschluesseln behalt dieselbe fallback-basierte Projektidentitaet, waehrend Build-Kontextunterschiede als TU-Diffs sichtbar bleiben
- Regressionstests, dass M5-Berichte ohne M6-Daten nicht unnoetig driften
- Dokumentationsbeispiele, die gegen aktuelle Generatorausgaben oder Goldens validiert werden

**Ergebnis**: M6 ist nicht nur implementiert, sondern ueber stabile Beispiele, Schemata und Tests nachvollziehbar abnehmbar.

### 1.8 Praeprozessorgenaue Include-Aufloesung und Konfigurations-Uebersteuerung

AP 1.8 nimmt zwei bislang offene Punkte als geplante, getrennte Folgearbeitsposition auf:

- Praeprozessorgenaue Include-Aufloesung als eigener Adapter-Vertrag (`PreprocessorIncludeAdapter`) mit eigenem Diagnostik-Vertrag.
- Konfigurationsdatei-basierter Override der Include-Origin-Klassifikation (`IncludeOrigin`) fuer heuristisch nicht eindeutig klassifizierbare Pfade; die Steuerung erfolgt ueber Konfigurationsdatei statt CLI.
- M6 bleibt CLI-zentriert, diese Punkte werden in AP 1.8 als eigener Vertragshorizont ohne Rückwirkung auf M6-Funktionsumfang abgeschlossen.

**Ergebnis**: AP 1.8 schafft die technische Grundlage fuer präzisere Include-Aufloesung und Datei-basierte Herkunftssteuerung in einem definierten Folgepaket.

## 2. Reihenfolge

| Schritt | Arbeitspaket | Abhaengig von |
|---|---|---|
| 1 | 1.1 Target-Graph-Vertraege im Kern schaerfen | M5-Abschluss |
| 2 | 1.2 Target-Graph-Ausgaben und Hub-Hervorhebung umsetzen | 1.1 |
| 3 | 1.3 Impact-Priorisierung ueber den Target-Graphen einfuehren | 1.1 |
| 4 | 1.4 Include-Sicht nach Herkunft und Tiefe verfeinern | M5-Abschluss |
| 5 | 1.5 Analyseauswahl, Schwellenwerte und Impact-Budget konfigurierbar machen | 1.2, 1.3, 1.4 |
| 6 | 1.6 Vergleich zweier Analysezeitpunkte einfuehren | 1.2, 1.5 |
| 7 | 1.7 Referenzdaten, Dokumentation und Abnahme aktualisieren | 1.2 bis 1.6 |
| 8 | 1.8 Praeprozessorgenaue Include-Aufloesung und Konfigurations-Uebersteuerung | 1.7 |

Schritte 2, 3 und 4 koennen teilweise parallel bearbeitet werden, sobald die Kernmodelle aus 1.1 stabil sind. AP 1.5 darf erst starten, wenn die Target-Graph-Impact-Vertraege aus 1.3 fuer `AnalyzeImpactRequest`, Fallback und `--require-target-graph` stabil sind. `compare` sollte erst nach der JSON-Formatentscheidung aus 1.2 beginnen.

## 3. Pruefung

M6 ist abgeschlossen, wenn mindestens folgende Pruefwege erfolgreich durchlaufen:

```text
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build && ctest --output-on-failure
```

Zusaetzlich muessen die bestehenden Projektpruefungen aus M5 weiter gruen bleiben:

```text
./scripts/validate.sh
./scripts/preflight.sh
```

Falls M6 neue JSON-Formatversionen, DOT-Regeln oder Doc-Example-Generatoren einfuehrt, muessen die zugehoerigen Schema-, Syntax- und Drift-Gates Teil von `ctest`, `validate.sh` oder einem dokumentierten CI-Pfad sein.

## 4. Rueckverfolgbarkeit

| Kennungen | M6-Arbeitspaket | Hinweis |
|---|---|---|
| `F-18` | 1.1, 1.2 | direkte Target-Abhaengigkeiten und textuelle Darstellung |
| `F-20` | 1.1, 1.2, 1.5 | Target-Hub-Hervorhebung und Schwellenwerte |
| `F-25` | 1.3 | priorisierte Target-Betroffenheit ueber den Target-Graphen |
| `F-16` | 1.4 | Projekt-Header vs. externe Header |
| `F-17` | 1.4 | direkte vs. indirekte Include-Sicht |
| `F-10` | 1.5 | konfigurierbare Schwellenwerte fuer Bewertungen |
| `F-37` | 1.5 | Aktivierung und Deaktivierung einzelner Analysen |
| `F-38` | 1.5 | Grenzwerte und Auswertungsschwellen |
| `F-11` | 1.6 | Vergleich zweier Analysezeitpunkte |

## 5. Offene Folgearbeiten

Vor dem Start der M6-Implementierung sind keine offenen CLI-Namensfragen mehr vorhanden.

Bereits festgelegte M6-CLI-Vertraege:

- `--analysis`, `--tu-threshold`, `--min-hotspot-tus`, `--target-hub-in-threshold`, `--target-hub-out-threshold`, `--include-scope`, `--include-depth`, `--impact-target-depth`, `--require-target-graph` und `compare --allow-project-identity-drift` sind M6-Vertragsbestandteil; ihre Defaults, Wertebereiche und Fehlerverhalten sind in AP 1.4, AP 1.5 und AP 1.6 verbindlich beschrieben
- Aenderungen an diesen Optionen sind vor Umsetzung des jeweils betroffenen Arbeitspakets nur noch per Plan-Review zulaessig

Nach M6 zu bewerten:

- `compare --format html` und `compare --format dot` als moegliche Folgearbeit getrennt bewerten
- Impact-Report-Vergleiche als moeglichen Folgeumfang mit eigenem Diff-Vertrag bewerten
- Cross-Mode-Compare zwischen File-API-Projektidentitaet und `fallback_compile_database_fingerprint` als moeglichen Folgeumfang mit expliziter Override-Option und Risiko-Diagnostics bewerten
