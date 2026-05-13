# Plan M6 - AP 1.8 Praeprozessorgenaue Include-Aufloesung und Konfigurations-Uebersteuerung

## Ziel

AP 1.4 fuehrt in M6 eine belastbarere Include-Sicht ein:
`IncludeOrigin` klassifiziert Hotspots als `project`, `external` oder
`unknown`, und `IncludeDepthKind` unterscheidet direkte, indirekte und
gemischte Sicht. Diese Klassifikation bleibt fuer M6 bewusst
heuristisch, weil der Source-Parsing-Adapter keinen vollstaendigen
C++-Praeprozessor ersetzt und keine projektlokale Konfigurationsdatei
auswertet.

AP 1.8 nimmt zwei Folgepunkte aus
`docs/planning/in-progress/plan-M6.md` als eigenen Vertragshorizont auf:

- Praeprozessorgenaue Include-Aufloesung ueber einen separaten
  `PreprocessorIncludeAdapter`.
- Konfigurationsdatei-basierte Uebersteuerung der
  `IncludeOrigin`-Klassifikation fuer Pfade, die heuristisch nicht
  eindeutig oder projektspezifisch anders zu bewerten sind.

AP 1.8 ist kein versteckter Umbau der bestehenden M6-CLI-Optionen.
Die bereits festgelegten Optionen `--include-scope` und
`--include-depth` bleiben gueltig. AP 1.8 verbessert die Datenquelle
und die Herkunftssteuerung hinter diesen Optionen, ohne bestehende
M6-Aufrufe inkompatibel zu machen.

## Scope

Umsetzen:

- Neuer Driven Adapter `PreprocessorIncludeAdapter` fuer
  praeprozessorgenaue Include-Kanten.
- Neuer Port- oder Adapter-Vertrag, der direkte Include-Kanten,
  transitive Include-Kanten, System-/Project-Provenienz und
  Diagnostik vom konkreten Praeprozessor entkoppelt.
- Expliziter Auswahlpfad zwischen bestehender heuristischer
  Source-Parsing-Aufloesung und praeprozessorgenaurem Adapter.
- Konfigurationsdatei fuer Include-Origin-Overrides.
- Konfigurationsparser mit validiertem Schema und klaren
  Fehlerregeln.
- Override-Regeln fuer `IncludeOrigin`, die nach normalisierten
  Pfadmustern und dokumentierter Prioritaet angewendet werden.
- Diagnostics, wenn Override-Regeln nicht matchen, mehrdeutig sind
  oder von praeprozessorgenauer Provenienz abweichen.
- Tests fuer Praeprozessor-Adapter, Override-Regeln, Diagnosefaelle
  und deterministische Ergebnisreihenfolge.

Nicht umsetzen:

- Neue CLI-Optionen als Ersatz fuer `--include-scope` oder
  `--include-depth`.
- Automatische Erzeugung von CMake-File-API-Query-Dateien.
- Automatische CMake-Ausfuehrung, Compiler-Probing oder
  Build-System-Mutation.
- Vollstaendige semantische C++-Analyse jenseits der fuer Include-
  Kanten noetigen Praeprozessorinformationen.
- Stille Aenderung des Standardverhaltens bestehender Analyze-Aufrufe:
  Ohne CLI-Option und ohne Konfigurationsdatei bleibt der heuristische
  Source-Parsing-Resolver aktiv.
- Projektweite Policy-Engine fuer alle Analyseoptionen. Die
  Konfigurationsdatei steuert in AP 1.8 ausschliesslich
  Include-Origin-Overrides und die Praeprozessor-Include-Aufloesung.
- Aenderung der Compare-Vertraege aus AP 1.6.
- HTML- oder DOT-Compare-Ausgaben.

Diese Punkte bleiben Folgeumfang oder gehoeren in eigene Plaene.

## Voraussetzungen aus AP 1.4 und AP 1.7

AP 1.8 baut auf folgenden M6-Vertraegen auf:

- `IncludeOrigin` mit den Werten `project`, `external`, `unknown`.
- `IncludeDepthKind` mit den Werten `direct`, `indirect`, `mixed`.
- `--include-scope <all|project|external|unknown>`.
- `--include-depth <all|direct|indirect>`.
- Bestehender M6-Default: Ohne explizite Resolver-Auswahl nutzt
  `analyze` den heuristischen Source-Parsing-Resolver.
- Strukturierte Include-Filter- und Include-Hotspot-Felder in den
  Analyze-Reportformaten.
- Konsolidierte M6-Dokumentation und Beispiele aus AP 1.7.

AP 1.8 darf erst starten, wenn AP 1.4 in Code, Schema und Goldens
geliefert ist. AP 1.7 ist fuer die Doku-/Beispielbasis relevant:
Wenn AP 1.8 nach dem M6-Release umgesetzt wird, muss es die dann
gueltigen `README.md`, `docs/user/guide.md`, `docs/user/quality.md`
und Formatvertraege aktualisieren, statt die AP-1.7-Abnahme
nachtraeglich umzudeuten.

## Dateien

Voraussichtlich zu aendern:

- `src/hexagon/ports/driven/include_resolver_port.h`
- `src/adapters/input/source_parsing_include_adapter.*`
- `src/hexagon/model/include_classification.*` und
  `src/hexagon/model/include_filter_options.*` oder gleichwertige
  Include-Modelle (urspruenglich `include_hotspot.h`, in M6 AP 1.4
  A.5 step 21pre aufgeteilt)
- `src/hexagon/model/analysis_request.*` oder gleichwertige
  Analyze-Request-Modelle
- `src/hexagon/services/project_analyzer.*`
- `src/adapters/cli/cli_adapter.*`
- `src/adapters/output/json_report_adapter.*`
- `src/adapters/output/console_report_adapter.*`
- `src/adapters/output/markdown_report_adapter.*`
- `src/adapters/output/html_report_adapter.*`
- `src/adapters/output/dot_report_adapter.*`
- `spec/report-json.md`
- `spec/report-json.schema.json`
- `spec/report-dot.md`
- `spec/report-html.md`
- `README.md`
- `docs/user/guide.md`
- `docs/user/quality.md`
- `docs/examples/manifest.txt`
- `docs/examples/generation-spec.json`

Neue Dateien:

- `src/adapters/input/preprocessor_include_adapter.{h,cpp}`
- `src/hexagon/model/include_origin_override.h`
- `src/hexagon/services/include_origin_override.{h,cpp}`
- `src/adapters/input/include_config_reader.{h,cpp}`
- `spec/include-config.md`
- `spec/include-config.schema.json`
- `tests/adapters/test_preprocessor_include_adapter.cpp`
- `tests/adapters/test_include_config_reader.cpp`
- `tests/hexagon/test_include_origin_override.cpp`
- `tests/e2e/testdata/m6/include-config/`

Die konkreten Dateinamen koennen beim Umsetzungsschnitt an die dann
existierende Modulstruktur angepasst werden. Der fachliche Vertrag aus
diesem Plan bleibt dabei massgeblich.

## Adapter-Vertrag

Der `PreprocessorIncludeAdapter` liefert Include-Kanten, keine fertigen
Hotspots. Die Hotspot-Aggregation bleibt im Hexagon.

Die Resolver-Auswahl ist Teil des Analyze-Requests. AP 1.8 fuehrt
dafuer ein Feld `include_resolution_mode` mit genau diesen Werten ein:

- `heuristic`: Default. Erzwingt den bestehenden
  Source-Parsing-Resolver und ignoriert den Praeprozessor-Adapter.
- `auto`: Nutzt den praeprozessorgenauen Adapter, wenn alle
  Voraussetzungen erfuellt sind; sonst Fallback auf
  `heuristic` mit Diagnostic. `auto` ist nur aktiv, wenn
  CLI oder Konfigurationsdatei es explizit setzen.
- `preprocessor`: erzwingt den praeprozessorgenauen Adapter. Wenn die
  Voraussetzungen nicht erfuellt sind, ist das ein CLI-/Eingabefehler
  ohne stdout-Report und ohne Zieldatei.

CLI und Konfigurationsdatei duerfen dieses Feld setzen. Wenn beide es
setzen und die Werte abweichen, ist das ein CLI-/Konfigurationsfehler.
Wenn nur die Konfigurationsdatei es setzt, gilt dieser Wert. Wenn nur
die CLI es setzt, gilt die CLI. Ohne explizite Setzung gilt
`heuristic`.
Der konkrete CLI-Name wird in A.4 festgelegt; bis dahin ist
`--include-resolver <auto|heuristic|preprocessor>` der Arbeitsname und
in Tests sowie Doku konsistent zu verwenden.

Der Adapter liefert pro Kante mindestens:

- normalisierten Pfad der einbindenden Datei
- normalisierten Pfad der eingebundenen Datei, falls aufloesbar
- rohe Include-Schreibweise aus dem Quelltext
- Include-Art (`quote`, `angle` oder `unknown`)
- Herkunftsprovenienz aus dem Praeprozessor (`project`, `system`,
  `external`, `unknown` oder adapter-spezifisch auf den M6-Vertrag
  abgebildet)
- direkte Tiefe fuer unmittelbare Includes
- transitive Tiefe oder Parent-Kante fuer indirekte Includes
- Diagnostics fuer nicht aufloesbare Includes, Makro-Includes,
  konditionale Pfade, Budgetgrenzen und Adapterfehler

Wichtig:

- Der Adapter darf keine fachlichen Hotspot-Schwellen anwenden.
- Der Adapter darf keine `IncludeOrigin`-Overrides anwenden; Overrides
  sind ein separater Hexagon-Service nach der Adapter-Provenienz.
- Die Reihenfolge der gelieferten Kanten ist nicht vertragsrelevant;
  das Hexagon sortiert deterministisch nach dem M6-Sortiervertrag.
- Adapterfehler muessen als Diagnostics modellierbar sein. Nur
  grundlegende Eingabefehler oder explizite Require-Modi duerfen den
  CLI-Lauf hart abbrechen.
- Wenn der Praeprozessor im Modus `auto` nicht verfuegbar ist, muss der
  bestehende heuristische Include-Resolver weiterhin nutzbar bleiben.
  Nur `include_resolution_mode=preprocessor` ist der harte
  Praeprozessor-Pfad und darf fehlende Praeprozessorvoraussetzungen zum
  Abbruch machen.

Die Praeprozessor-Provenienz wird vor Overrides deterministisch auf
`IncludeOrigin` abgebildet:

| Praeprozessor-Provenienz | Vorlaeufige `IncludeOrigin` |
|---|---|
| `project` | `project` |
| `system` | `external` |
| `external` | `external` |
| `unknown` | `unknown` |
| adapter-spezifisch, nicht dokumentiert | `unknown` plus Diagnostic |

Konfigurations-Overrides werden danach angewendet und koennen diesen
vorlaeufigen Wert ersetzen. Die urspruengliche Adapter-Provenienz
bleibt als Provenienzfeld oder Diagnostic erhalten.

Overrides werden immer nach der Resolver-Auswahl und nach der
Hotspot-Aggregation angewendet, unabhaengig davon, ob der wirksame
Resolver `heuristic` oder `preprocessor` ist. Dadurch korrigiert
dieselbe Konfigurationsdatei sowohl heuristisch unklare Pfade als auch
praeprozessorgenaue Provenienz, ohne dass Nutzer den Resolver-Modus
wechseln muessen.

Budgetgrenzen sind in AP 1.8 exakt:

- `preprocessor_include_depth_limit`: maximale transitive Include-Tiefe,
  Default `32`.
- `preprocessor_include_node_budget`: maximale Anzahl normalisierter
  Include-Knoten pro Analyze-Lauf, Default `10000`.
- `preprocessor_include_edge_budget`: maximale Anzahl Include-Kanten
  pro Analyze-Lauf, Default `50000`.

Diese Budgets begrenzen keine Report-Top-Listen, sondern die
Praeprozessor-Include-Aufloesung. Wird ein Budget erreicht, wird die
Aufloesung deterministisch in BFS-Entdeckungsreihenfolge gekappt und
ein Diagnostic mit Budgetname, Limit und erreichter Zaehlerzahl
ausgegeben. Laufzeitlimits und Dateigroessenlimits sind nicht Teil des
AP-1.8-Budgetvertrags.

Die BFS-Reihenfolge ist Teil des Vertrags:

- Startdateien sortieren nach `(normalized_source_path,
  normalized_build_context_key)`.
- Include-Kanten pro Parent sortieren nach
  `(normalized_including_path, normalized_included_path,
  include_kind, raw_spelling, preliminary_include_origin)`.
- Neue Include-Knoten werden ueber den normalisierten Include-Pfad
  dedupliziert; der erste Besuch in dieser sortierten BFS-Reihenfolge
  gewinnt.
- `preprocessor_include_node_budget` und
  `preprocessor_include_edge_budget` werden in genau dieser
  Entdeckungsreihenfolge verbraucht.

Adapter-Lieferreihenfolge, Dateisystem-Reihenfolge,
Hash-Container-Reihenfolge oder Praeprozessor-Callback-Reihenfolge
duerfen das gekappte Ergebnis nicht beeinflussen.

## Konfigurationsdatei

AP 1.8 fuehrt eine projektlokale Konfigurationsdatei fuer
Include-Origin-Overrides ein. Vorgeschlagener Dateiname:
`cmake-xray.toml`.

Discovery-Vertrag:

- AP 1.8 verwendet eine einzige `include_config_root`.
  Praezedenz: explizite Source-Root aus CMake-File-API, sonst
  normalisierte Projektwurzel, sonst CLI-CWD nur fuer
  Compile-Database-only-Laeufe ohne belastbare Projektwurzel.
- Default-Suche erfolgt ausschliesslich in dieser
  `include_config_root`, die auch fuer Report-Pfade und Pattern-
  Matching verwendet wird.
- Die aktuelle Working Directory ist kein Suchanker, ausser sie ist
  identisch mit dieser Root.
- Es wird keine rekursive Suche in Eltern- oder Unterverzeichnissen
  durchgefuehrt.
- Mehrere `cmake-xray.toml`-Dateien in Unterverzeichnissen werden
  ignoriert und erzeugen keine implizite Uebersteuerung.
- Wenn AP 1.8 eine explizite CLI-Option fuer den Config-Pfad einfuehrt
  (Arbeitsname `--config <path>`), muss der Pfad relativ zur aktuellen
  Working Directory oder absolut aufgeloest, danach normalisiert und im
  Report als explizite Quelle markiert werden.
- Ein explizit gesetzter, nicht lesbarer oder ungueltiger Config-Pfad
  ist ein CLI-/Konfigurationsfehler. Eine nicht vorhandene Default-
  Datei ist kein Fehler.
- Symlinks werden fuer Discovery nicht realpath-kanonisiert. Der
  Default-Pfad entsteht aus der lexikalisch normalisierten
  `include_config_root` plus `cmake-xray.toml`; wenn dieser Pfad durch
  einen Symlink erreichbar ist und die Datei lesbar ist, gilt sie als
  vorhanden. Der Report gibt den lexikalisch normalisierten
  Config-Pfad aus, nicht zwingend den physischen Realpath.
- Pattern-Matching verwendet die gleichen lexikalisch normalisierten
  `include_config_root`-relativen Pfade wie die Reports. Es erfolgt
  keine Symlink-Aufloesung und kein Plattform-Case-Folding; Windows-
  Laufwerksbuchstaben werden nur fuer die Root-Normalisierung
  lowercase behandelt, nicht fuer relative Match-Schluessel.

Minimaler Vertragsumfang:

```toml
include_resolution_mode = "heuristic"

[include_origin]
project = ["src/**", "include/**"]
external = ["third_party/**", "vendor/**"]
unknown = []
```

Regeln:

- `[include_origin]` darf fehlen oder leer sein. Dann werden keine
  Origin-Overrides angewendet.
- Die Schluessel `project`, `external` und `unknown` sind optional;
  fehlende Schluessel entsprechen einer leeren Liste.
- Jeder vorhandene Schluessel muss eine Liste von Strings enthalten.
  `include_origin = {}` ist gleichwertig zu einem leeren
  `[include_origin]`-Block.
- Inhaltlich ungueltige Konfigurationsdateien sind hart
  fehlgeschlagene CLI-/Konfigurationsfehler: unbekannte Top-Level-
  Schluessel, unbekannte `[include_origin]`-Schluessel, falsche Typen,
  ungueltige Muster, mehrdeutige gleich spezifische Ziel-Origins,
  Absolutpfade oder nicht normalisierbare Pfade enden nonzero, ohne
  stdout-Report und ohne Zieldatei. Es gibt keinen partiellen Lauf mit
  Default- oder Teilkonfiguration.
- Pfadmuster werden relativ zur normalisierten `include_config_root`
  ausgewertet.
- Backslashes werden vor dem Matching zu `/` normalisiert.
- Matching ist case-sensitiv; plattformspezifisches Case-Folding ist
  Folgeumfang.
- Muster duerfen nur dokumentierte Glob-Konstrukte verwenden:
  `*`, `**` und `?`.
- Absolutpfade in Overrides sind unzulaessig.
- `..`-Segmente nach Normalisierung sind unzulaessig.
- Ein Pfad darf nach Prioritaetsaufloesung genau eine finale
  `IncludeOrigin` erhalten.
- Spezifischere Muster gewinnen vor allgemeineren Mustern. Die
  Spezifitaet eines Musters ist das Tupel
  `(literal_path_segment_count, literal_character_count,
  -wildcard_token_count, -double_star_count)`, absteigend sortiert.
  Ein Segment ist literal, wenn es keine Glob-Zeichen enthaelt. Bei
  gleicher Spezifitaet und unterschiedlicher Ziel-Origin ist das ein
  Konfigurationsfehler.
- Bei gleicher Spezifitaet und gleicher Ziel-Origin werden doppelte
  Treffer deterministisch dedupliziert. Die Reihenfolge in der Datei
  hat keine fachliche Bedeutung; Diagnostics duerfen die Duplikate als
  Hinweis melden, aber sie duerfen das Ergebnis nicht veraendern.
- Overrides ersetzen nur die Herkunftsklassifikation, nicht die
  Include-Tiefe und nicht die Praeprozessor-Kanten.

Die Konfigurationsdatei ist optional. Ohne Datei bleibt das Verhalten
identisch zum M6-Default: `include_resolution_mode=heuristic`, keine
Origin-Overrides, keine Config-Diagnostics ausser der reportbaren
Information `config_loaded=false`.

## Report- und Formatvertrag

AP 1.8 muss in Analyze-Reports sichtbar machen:

- welche Include-Aufloesungsquelle aktiv war:
  `heuristic` oder `preprocessor`;
- welcher `include_resolution_mode` angefordert wurde und welcher
  Resolver effektiv verwendet wurde;
- ob eine Konfigurationsdatei geladen wurde;
- welche Override-Regelanzahl geladen und wirksam wurde;
- wie viele Include-Hotspots nach Adapter-Provenienz und nach finaler
  Override-Origin klassifiziert wurden;
- Diagnostics fuer nicht angewendete, mehrdeutige oder ungueltige
  Override-Regeln;
- Diagnostics fuer Praeprozessor-Fallbacks und Budgetgrenzen.

JSON muss die neuen Felder strukturiert abbilden. Falls die Felder
Pflichtfelder in bestehenden Objekten werden, ist
`kReportFormatVersion` zu erhoehen. Falls sie als optionale,
abwaertskompatible Felder eingefuehrt werden, muss
`spec/report-json.md` die Kompatibilitaetsregel explizit festhalten.

Console, Markdown, HTML und DOT beziehen sich hier ausschliesslich auf
Analyze- und Impact-Reports. Compare-Ausgaben bleiben von AP 1.8
unberuehrt. Analyze-/Impact-Reports duerfen keine widerspruechlichen
Origin-Werte ausgeben: Wenn ein Override greift, muss der finale Wert
sichtbar sein; die urspruengliche Adapter-Provenienz darf als
Provenienz oder Diagnostic erscheinen.

## Implementierungsreihenfolge

Innerhalb von **A.1 (Konfigurationsvertrag und Parser)**:

1. `spec/include-config.md` und optionales Schema anlegen.
2. Konfigurationsparser implementieren.
3. Validierungsfehler und Diagnostics festlegen.
4. Tests fuer gueltige, leere, ungueltige und mehrdeutige Dateien.

Innerhalb von **A.2 (Include-Origin-Override-Service)**:

5. Override-Modell und Matching-Regeln implementieren.
6. Prioritaets- und Konfliktregeln testen.
7. Integration in die Include-Hotspot-Aggregation.

Innerhalb von **A.3 (PreprocessorIncludeAdapter)**:

8. Adapter-Port und Adapter-Implementierung einfuehren.
9. Fallback zum heuristischen Resolver erhalten.
10. Diagnostics fuer nicht verfuegbare Praeprozessorinformationen.
11. Adaptertests mit direkten, indirekten, System- und Makro-Includes.

Innerhalb von **A.4 (Report- und CLI-Integration)**:

12. Analyze-Request und CLI-Wiring fuer Konfigurationsdatei,
    `include_resolution_mode` und Include-Resolver-Auswahl.
13. Reportadapter um aktive Datenquelle, Override-Provenienz und
    Diagnostics erweitern.
14. Formatvertraege und Schema aktualisieren.
15. Goldens und `docs/examples/` aktualisieren.

Innerhalb von **A.5 (Audit-Pass)**:

16. `make docs-check`.
17. Schema-, Golden- und Adaptertests.
18. Docker-Gates gemaess `docs/user/quality.md`.
19. Liefer-Stand-Block aktualisieren.

## Tranchen-Schnitt

AP 1.8 wird in fuenf Sub-Tranchen geliefert:

- **A.1 Konfigurationsvertrag und Parser**: Datei- und
  Validierungsvertrag ohne fachliche Anwendung.
- **A.2 Include-Origin-Override-Service**: deterministische
  Pattern-Anwendung auf bestehende Include-Hotspots.
- **A.3 PreprocessorIncludeAdapter**: neue Datenquelle mit Fallback
  und Diagnostics.
- **A.4 Report- und CLI-Integration**: Nutzerpfad, Reportfelder,
  Formatdokumente, Goldens.
- **A.5 Audit-Pass**: Doku, Beispiele, Gates, Liefer-Stand.

Begruendung: Die Konfigurationsdatei muss zuerst vertraglich stabil
sein, bevor ihre Semantik in den Hexagon-Service wandert. Der
Praeprozessor-Adapter bleibt getrennt, damit Override-Regeln auch mit
dem bestehenden heuristischen Resolver testbar sind. Report- und
CLI-Wiring kommt erst nach stabilen Datenmodellen.

## Liefer-Stand

Wird nach dem Schnitt der A-Tranchen mit Commit-Hashes befuellt.
Bis dahin ist AP 1.8 nicht abnahmefaehig.

- A.1 (Konfigurationsvertrag und Parser): noch nicht ausgeliefert.
- A.2 (Include-Origin-Override-Service): noch nicht ausgeliefert.
- A.3 (PreprocessorIncludeAdapter): noch nicht ausgeliefert.
- A.4 (Report- und CLI-Integration): noch nicht ausgeliefert.
- A.5 (Audit-Pass): noch nicht ausgeliefert.

## Abnahmekriterien

AP 1.8 ist abgeschlossen, wenn:

- eine optionale Konfigurationsdatei Include-Origin-Overrides
  deterministisch anwenden kann;
- dieselben Overrides in den wirksamen Modi `heuristic`, `auto` und
  `preprocessor` nach der Hotspot-Aggregation angewendet werden;
- fehlende Default-Konfiguration ohne Diagnostic-Flaeche fuer Nutzer
  beim M6-kompatiblen Default bleibt und nur `config_loaded=false`
  reportet;
- ungueltige, mehrdeutige oder nicht normalisierbare
  Konfigurationsdateien nonzero enden und keinen Report schreiben;
- der bestehende heuristische Include-Resolver ohne
  Konfigurationsdatei unveraendert nutzbar bleibt;
- `include_resolution_mode=heuristic` der Default ist und bestehende
  Aufrufe ohne Config/CLI-Auswahl keine praeprozessorgenauen Ergebnisse
  aktivieren;
- `include_resolution_mode=auto` bei fehlendem Praeprozessor
  deterministisch auf den heuristischen Resolver zurueckfaellt und ein
  Diagnostic erzeugt;
- `include_resolution_mode=preprocessor` bei fehlenden
  Praeprozessorvoraussetzungen nonzero endet und keinen Report
  schreibt;
- der `PreprocessorIncludeAdapter` direkte und indirekte Include-
  Kanten inklusive Provenienz und Diagnostics liefert;
- Praeprozessor-Fallbacks und erreichte
  `preprocessor_include_depth_limit`,
  `preprocessor_include_node_budget` oder
  `preprocessor_include_edge_budget` sichtbar sind und keine stillen
  Datenqualitaetsverluste verursachen;
- Budget-Kappung bei gleicher Eingabe unabhaengig von Adapter-,
  Dateisystem- und Container-Reihenfolge byte-stabil bleibt;
- `--include-scope` und `--include-depth` auf den finalen
  klassifizierten Hotspot-Daten arbeiten;
- JSON, Console, Markdown, HTML und DOT die aktive Include-Datenquelle
  und Override-Provenienz konsistent darstellen;
- `spec/report-json.md`, `spec/report-json.schema.json`,
  `spec/report-dot.md`, `spec/report-html.md` und
  `spec/include-config.md` den Ist-Stand dokumentieren;
- `docs/user/guide.md`, `docs/user/quality.md` und `README.md` die
  neue Konfigurationsdatei und den Praeprozessor-Pfad beschreiben;
- `docs/examples/` repraesentative Beispiele fuer Override und
  Praeprozessor-Provenienz enthaelt;
- `make docs-check` gruen ist;
- alle betroffenen CTest-, Schema-, Golden- und Docker-Gates gruen
  sind;
- `git diff --check` sauber ist.

## Offene Folgearbeiten

Diese Punkte werden bewusst nicht in AP 1.8 umgesetzt:

- globale Analyseprofile fuer alle CLI-Optionen;
- plattformspezifisches Case-Folding fuer Override-Matching;
- Auto-Discovery und Installation fehlender Praeprozessor-Tools;
- vollstaendige C++-Semantikanalyse;
- Impact-Compare;
- HTML- und DOT-Compare-Output;
- Cross-Mode-Compare mit expliziter Override-Option.
