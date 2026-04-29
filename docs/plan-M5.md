# Plan M5 - Weitere Reportformate, Release-Artefakte und CLI-Ausgabemodi (`v1.2.0`)

## 0. Dokumenteninformationen

| Feld | Wert |
|---|---|
| Dokument | Plan M5 `cmake-xray` |
| Version | `0.27` |
| Stand | `2026-04-25` |
| Status | Entwurf |
| Referenzen | [Lastenheft](./lastenheft.md), [Design](./design.md), [Architektur](./architecture.md), [Phasenplan](./roadmap.md), [Plan M4](./plan-M4.md), [Qualitaet](./quality.md), [Releasing](./releasing.md) |

### 0.1 Zweck
Dieser Plan beschreibt die konkreten Schritte fuer Milestone M5 (`v1.2.0`). Ziel ist der Ausbau nach M4: `cmake-xray` soll die vorhandenen Analyseergebnisse in weiteren Ausgabeformaten bereitstellen, maschinenlesbare Formate versionieren, einen offiziellen Release-Pfad mit Linux-Artefakt und OCI-Image absichern, erste Portabilitaetsnachweise fuer macOS und Windows liefern und die CLI um detailreiche bzw. reduzierte Ausgabemodi erweitern.

M5 erweitert primaer die Nutzbarkeit und Auslieferbarkeit des bereits vorhandenen Analyseumfangs. Neue fachliche Target-Graph-Analysen bleiben bewusst ausserhalb dieses Meilensteins und werden in Phase 6 geplant.

### 0.2 Abschlusskriterium
M5 gilt als erreicht, wenn:

- `cmake-xray analyze` und `cmake-xray impact` neben `console` und `markdown` auch `html`, `json` und `dot` als Ausgabeformate akzeptieren
- `--output <path>` fuer `markdown`, `html`, `json` und `dot` bei `analyze` und `impact` freigeschaltet ist und Reportdateien atomar schreibt, sodass bei Fehlern keine halb geschriebenen Zielartefakte als Erfolg zurueckbleiben
- der HTML-Export eigenstaendige, deterministische Berichte fuer Projektanalyse und Impact-Analyse erzeugt, ohne externe Laufzeitressourcen zu benoetigen
- der JSON-Export fuer Projektanalyse und Impact-Analyse ein dokumentiertes Schema mit Formatversion, Pflichtfeldern, Typen, Enums, Nullability-Regeln und Array-Regeln ausgibt und fuer Automatisierung stabil nutzbar ist
- der DOT-Export fuer die in M5 vorhandene Datenlage sinnvolle Graphviz-Diagramme erzeugt, ohne noch nicht implementierte Target-Graph-Semantik vorwegzunehmen
- `--verbose` diagnoseorientierte Zusatzinformationen ausgibt; bei maschinenlesbaren Formaten duerfen diese Zusatzinformationen ausschliesslich ueber `stderr` erscheinen und weder stdout-Reports noch Artefaktinhalte veraendern
- `--quiet` fuer erfolgreiche Console-Emissionen reduzierte Ausgabe liefert, ohne stdout-Reports fuer artefaktorientierte Formate zu kuerzen oder Erfolgsmeldungen bei Dateiausgabe einzufuehren, und Fehler weiterhin verstaendlich auf `stderr` meldet
- ein tag-basierter Release-Workflow ein versioniertes Linux-CLI-Artefakt mit Pruefsumme erzeugt
- ein OCI-kompatibles Container-Image fuer lokale Nutzung und CI erzeugt und dokumentiert wird
- README, Release-Dokumentation und Beispiele die Nutzung ohne interne Build-Pfade wie `./build/cmake-xray` beschreiben
- macOS- und Windows-Builds mindestens in CI oder dokumentierten Smoke-Checks bewertet sind; bekannte Einschraenkungen werden explizit dokumentiert
- automatisierte Adapter-, CLI-, Golden-Output- und Release-Smoke-Tests die neuen Formate, Modi und Bereitstellungspfade absichern

Relevante Kennungen: `F-28`, `F-29`, `F-30`, `F-35`, `F-36`, `F-39`, `F-40`, `F-42`, `F-43`, `F-44`, `NF-08`, `NF-09`, `NF-20`, `NF-21`, `S-06`, `S-07`, `S-08`, `S-12`, `S-13`

### 0.3 Abgrenzung
Bestandteil von M5 sind:

- `HtmlReportAdapter` fuer Projektanalyse und Impact-Analyse
- `JsonReportAdapter` fuer Projektanalyse und Impact-Analyse mit dokumentierter Formatversion
- `DotReportAdapter` fuer graphische Sicht auf die vorhandenen Analyseergebnisse
- CLI-Formatwahl fuer `html`, `json` und `dot`
- CLI-Dateiausgabe ueber `--output` fuer alle artefaktorientierten Formate `markdown`, `html`, `json` und `dot`
- CLI-Modi `--verbose` und `--quiet`
- tag-basierter Release-Workflow fuer Linux-CLI-Artefakt und OCI-Image
- Installations- und Nutzungsdokumentation fuer Release-Artefakte und Container
- Portabilitaetspruefung fuer macOS und Windows auf Build- und Smoke-Test-Niveau
- Referenzdaten, Golden-Outputs und Beispiele fuer die neuen Ausgabeformate

Nicht Bestandteil von M5 sind:

- direkte Target-Graph-Analysen und textuelle Darstellung direkter Target-Abhaengigkeiten (`F-18`)
- Hervorhebung von Targets mit vielen ein- oder ausgehenden Abhaengigkeiten (`F-20`)
- Priorisierung betroffener Targets ueber den Target-Graphen hinweg (`F-25`)
- verfeinerte Include-Sicht jenseits des MVP (`F-16`, `F-17`)
- erweiterte Analysekonfiguration, Schwellenwerte und Vergleichsansichten (`F-10`, `F-11`, `F-37`, `F-38`)
- automatische Erzeugung von CMake-File-API-Query-Dateien oder implizite CMake-Ausfuehrung
- vollstaendige funktionale Freigabe fuer alle macOS- und Windows-Varianten; M5 liefert zunaechst belastbare Build-/Smoke-Aussagen

M5 baut auf der M4-Target-Sicht auf. Wenn File-API-Daten vorhanden sind, duerfen HTML, JSON und DOT dieselben Target-Zuordnungen darstellen wie Konsole und Markdown. Ohne File-API-Daten bleiben Target-Abschnitte leer oder entfallen nach klar dokumentierter Formatregel.

## 1. Arbeitspakete

### 1.1 Report-Vertraege fuer neue Formate und Formatversionierung schaerfen

Die M3-/M4-Reportstrecke ist textorientiert. Fuer M5 muessen weitere Formate angebunden werden, ohne Fachlogik in einzelne Adapter zu verschieben oder bestehende Konsolen- und Markdown-Vertraege zu destabilisieren.

Fuer M5 benoetigt der Report-Pfad mindestens:

- eine erweiterte Formatwahl in CLI und Composition Root fuer `console`, `markdown`, `html`, `json` und `dot`
- eine erweiterte `--output`-Validierung fuer `markdown`, `html`, `json` und `dot` bei `analyze` und `impact`; `console` bleibt standardmaessig stdout-orientiert
- ein gemeinsamer Schreibpfad fuer Reportdateien, der Zielartefakte atomar ueber temporaere Datei und Rename ersetzt und Fehler sauber an die CLI meldet
- eine verpflichtende strukturierte Modellerweiterung fuer stabile Report-Eingabequellen: `AnalysisResult` und `ImpactResult` enthalten ein gemeinsames `ReportInputs`-Value-Object, damit Adapter weiterhin nur Ergebnisobjekte rendern und keine CLI-/Composition-Root-Details nachladen
- klare Adaptergrenzen: Jeder Report-Adapter rendert ausschliesslich vorhandene `AnalysisResult`- bzw. `ImpactResult`-Modelle
- gemeinsame Hilfsfunktionen fuer stabile Sortierung, Text-Escaping, Pfadanzeige und Diagnostics, soweit dadurch Dopplung zwischen Adaptern reduziert wird
- eine dokumentierte Formatversion fuer maschinenlesbare JSON-Ausgaben
- eine explizite Regel, welche Ergebnisfelder fuer DOT graphisch modelliert werden und welche nur als Label oder Kommentar erscheinen

Wichtig:

- Rueckwaertskompatibilitaet wird fuer M5 differenziert behandelt: Bestehende M3-/M4-Goldens fuer `console` und `markdown` bleiben im Normalmodus ohne neue Optionen byte-stabil, einschliesslich File-API- und Mixed-Input-Laeufen; `ReportInputs` wird in AP 1.1 nicht neu in diesen bestehenden Formaten sichtbar. Neue Provenienzsichtbarkeit in Console/Markdown braucht ein eigenes spaeteres Arbeitspaket oder explizite Quiet-/Verbose-Goldens.
- Formatadapter duerfen keine neuen Impact- oder Ranking-Entscheidungen treffen
- JSON ist der vollstaendige maschinenlesbare Vertragsausdruck in M5; DOT bleibt visualisierungsorientiert, erhaelt aber einen begrenzt stabilen Metadatenvertrag fuer die explizit benannten Graph-/Node-Attribute
- stdout bleibt der Standard, wenn kein `--output` angegeben ist; mit `--output` gilt weiterhin der M3-Vertrag: Der Reportinhalt wird ausschliesslich in die Datei geschrieben, erfolgreicher stdout bleibt leer, und Warnungen oder Fehler gehen nach `stderr`. Erfolgsmeldungen auf stdout sind fuer M5 mit `--output` nicht zulaessig, solange kein expliziter kuenftiger Status-Flag eingefuehrt wird.
- `--output` ersetzt vorhandene Zielartefakte bei erfolgreichem Schreiben; bei Render-, Schreib- oder Replace-Fehlern bleibt eine bereits vorhandene Zieldatei unveraendert erhalten
- die atomare Dateiausgabe verwendet eine kollisionssicher und exklusiv angelegte temporaere Datei im Zielverzeichnis und einen eigenen Replace-Wrapper statt nacktem `std::filesystem::rename(temp, target)`: POSIX nutzt `rename`/`renameat` fuer atomaren Replace, Windows nutzt `ReplaceFileW` oder `MoveFileExW` mit `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`; ein vorheriges Loeschen des Zielpfads ist nicht zulaessig, weil bei Fehlern die alte Datei unveraendert bleiben muss
- fuer `analyze` folgen alle Reportformate derselben `--top`-Begrenzung fuer Ranking-, Hotspot- und vergleichbare Listenabschnitte; `impact` erhaelt in M5 keine `--top`-Option und keine implizite Begrenzung seiner strukturierten Ergebnislisten
- DOT ist die visualisierungsorientierte Ausnahme: Auch `impact --format dot` wird ueber Graph-Budgets begrenzt, ohne das zugrunde liegende `ImpactResult` oder JSON-/HTML-/Markdown-Impact-Listen zu kuerzen
- DOT bleibt fuer `analyze` und `impact` ein begrenztes Artefakt: Analyze-Hotspot-Kontext wird ueber ein separates, kleines `context_limit` begrenzt, beide DOT-Reporttypen werden ueber ein globales `node_limit`-/`edge_limit`-Budget begrenzt; Analyze-Hotspot-Knoten tragen bei vorhandenem Kontext die Attribute `context_total_count`, `context_returned_count`, `context_truncated`, und beide Reporttypen tragen immer die Graph-Attribute `graph_node_limit`, `graph_edge_limit`, `graph_truncated`
- alle neuen Formate muessen deterministisch sein, damit Golden-Outputs sinnvoll diffbar bleiben
- Datums-, Laufzeit- oder Hostinformationen duerfen nicht automatisch in Reports erscheinen

Vorgesehene Artefakte:

- Anpassung der CLI-Formatvalidierung in `src/adapters/cli/`
- Anpassung der CLI-Output-Validierung und der atomaren Dateiausgabe fuer Reportartefakte
- gemeinsamer Atomic-Write-/Replace-Wrapper in der Infrastruktur- oder CLI-Adapter-Schicht mit plattformspezifischer Implementierung fuer POSIX und Windows
- Anpassung der Composition-Root-Verdrahtung in `src/main.cpp`
- Erweiterung von `src/hexagon/model/analysis_result.*` und `src/hexagon/model/impact_result.*` um ein strukturiertes `ReportInputs`-Modell, mindestens mit `compile_database_path`, `cmake_file_api_path` und bei `impact` `changed_file`
- `ReportInputs` ist ab M5 die kanonische Quelle fuer neue M5-Artefaktadapter und JSON-`inputs`; bestehende skalare Pfadfelder wie `AnalysisResult::compile_database_path` und `ImpactResult::changed_file` duerfen in AP 1.1 als Legacy-Presentation-Felder fuer byte-stabile Console-/Markdown-Ausgaben erhalten bleiben, duerfen aber nicht fuer JSON-/HTML-/DOT-`inputs` serialisiert werden
- `ReportInputs`-Felder sind im JSON-Vertrag immer vorhanden; gesetzte Pfade sind Strings, nicht gesetzte Eingaben sind `null`, und leere Strings duerfen nicht als Ersatz fuer fehlende Eingaben verwendet werden
- `ReportInputs` serialisiert stabile Display-Pfade aus der fachlichen Eingabeaufloesung: CLI-relative Pfade werden relativ zum Arbeitsverzeichnis normalisiert, absolute Pfade bleiben absolute Anzeige-Strings, und normalisierte interne Schluessel werden nicht in `inputs` serialisiert. Golden-Tests mit absoluten Pfaden duerfen nur synthetische, fixture-stabile Pfade wie `/project/...` oder plattformspezifisch fest kontrollierte Testwurzeln verwenden; CLI-End-to-End-Goldens duerfen keine host-/workspace-spezifischen Absolutpfade enthalten.
- fuer CMake File API werden Original- und Aufloesungspfad getrennt modelliert: `cmake_file_api_path` enthaelt den originalen stabilen Display-Pfad der CLI-Eingabe, `cmake_file_api_resolved_path` enthaelt den tatsaechlich verwendeten Build- oder Reply-Directory-Pfad nach Adapter-Aufloesung, folgt aber derselben Display-Pfad-Regel wie andere `ReportInputs`: relative Eingaben bleiben normalisierte relative Anzeige-Pfade, absolute Eingaben bleiben absolute Anzeige-Strings, und es erfolgt keine zusaetzliche Host-kanonische Aufloesung ueber Symlinks oder lokale Temp-/Home-Pfade. Beide Felder sind immer vorhanden und bei fehlendem Wert `null`
- der `CmakeFileApiAdapter` gibt den aufgeloesten Build-/Reply-Directory-Pfad als rohen lexikalischen Pfad ueber Metadaten in `BuildModelResult` an Hexagon-Services zurueck; die Display-Konvertierung relativ zu `report_display_base` gehoert ausschliesslich in `ProjectAnalyzer`/`ImpactAnalyzer`, damit Adapter keine host-spezifische Reportnormalisierung oder CLI-Basis kennen muessen
- `changed_file` nutzt fuer Display- und Provenienzregeln dieselbe fachliche Basis wie die Impact-Analyse: Bei Mixed-Input-Laeufen mit `--compile-commands` und `--cmake-file-api` hat die `compile_commands.json`-Directory Prioritaet; bei File-API-only-Laeufen wird die CMake-File-API-Source-Root verwendet. Der Pfad wird nicht pauschal relativ zum Prozess-Arbeitsverzeichnis interpretiert.
- Driving-Requests fuer `ProjectAnalyzer` und `ImpactAnalyzer` tragen eine explizite Display-Basis wie `invocation_cwd` oder `report_display_base`, die von der CLI gesetzt und in Service-Tests injiziert wird; Services duerfen fuer Report-Pfade nicht implizit `std::filesystem::current_path()` lesen
- bestehende Pfadhelper in `src/hexagon/services/analysis_support.*` werden so angepasst, dass Fallback-Basen explizit aus dem Request-Kontext kommen und nicht aus dem Prozess-CWD
- der bisher positionale Impact-Port wird in M5 auf einen expliziten `AnalyzeImpactRequest` umgestellt; der positionale Altvertrag darf kein Produktionspfad bleiben, weil er `report_display_base` und die fuer `ReportInputs` noetige Relativitaets-/Display-Provenienz nicht transportieren kann. Der Request enthaelt mindestens `compile_commands_path`, `changed_file_path`, `cmake_file_api_path`, `report_display_base` und die fuer `ReportInputs` noetigen Source-/Display-Metadaten
- `ReportInputs` dokumentiert die Provenienz ueber strukturierte Zusatzfelder wie `compile_database_source`, `cmake_file_api_source` und `changed_file_source`; `compile_database_source` und `cmake_file_api_source` verwenden im Modell die Quelle `cli` oder einen fehlenden Wert, der in JSON v1 als `null` serialisiert wird, und `changed_file_source` verwendet `compile_database_directory`, `file_api_source_root`, `cli_absolute`, `unresolved_file_api_source_root`, damit File-API-only-, Mixed-Input-, absolute-`--changed-file`- und File-API-Ladefehler-Laeufe eindeutig auswertbar bleiben. `unresolved_file_api_source_root` ist eine Fehlerergebnis-Provenienz und darf bei erfolgreichen Impact-Ergebnissen nicht auftreten. `changed_file*`-Felder existieren nur im Impact-JSON-Vertrag, weil `analyze` keine geaenderte Datei verarbeitet; ein fehlendes `--changed-file` wird bei `impact` von der CLI abgelehnt und nicht als JSON-`null` serialisiert.
- Anpassung der Producer-Pfade in `ProjectAnalyzer`, `ImpactAnalyzer` und den zugehoerigen Driving-Request-/Port-Vertraegen, damit `ReportInputs` beim Erzeugen von `AnalysisResult` und `ImpactResult` vollstaendig gesetzt wird und nicht nachtraeglich in der CLI an Adapter uebergeben werden muss
- Erweiterung von `src/hexagon/model/build_model_result.*` um den rohen aufgeloesten CMake-File-API-Build-/Reply-Pfad; die bestehende Source-Root bleibt fachliche Basis, waehrend der originale CLI-Display-Pfad ausschliesslich aus dem Driving-Request kommt
- `ReportWriterPort`/`GenerateReportPort` bleiben aus Adaptersicht ergebnisobjektzentriert; Signaturaenderungen sind nur zulaessig, wenn sie `ReportInputs` weiterhin als Teil des fachlichen Ergebnisvertrags transportieren und nicht als separaten CLI-Kontext in Adapter leaken
- Verbosity bleibt eine CLI-Emission-Policy in `src/adapters/cli/` und `src/main.cpp`; `ReportWriterPort`/`GenerateReportPort` erhalten keinen `OutputVerbosity`-Parameter, und fachliche Reportadapter rendern keine Verbose-/Quiet-Sondervarianten
- Console-Verbosity ist ein CLI-owned Sonderfall: Fuer `--format console` darf die CLI nach dem Erzeugen von `AnalysisResult`/`ImpactResult` eine eigene kurze, normale oder verbose Console-Emission waehlen, ohne den artefaktorientierten Report-Pfad oder dessen Ports mit `OutputVerbosity` zu erweitern
- neue Adapter unter `src/adapters/output/`
- Erweiterung von `src/adapters/CMakeLists.txt`
- Dokumentation der JSON-Formatversion in `docs/`

**Ergebnis**: Der Report-Pfad ist fuer weitere Ausgabeformate offen, ohne die fachliche Verantwortung aus dem Hexagon in Adapter zu verlagern.

### 1.2 `JsonReportAdapter` fuer stabile maschinenlesbare Ausgaben umsetzen

Der JSON-Export ist fuer Automatisierung, CI-Auswertung und Folgewerkzeuge gedacht. Deshalb muss das Format stabiler und expliziter sein als die menschenlesbaren Textberichte.

Ein JSON-Bericht fuer `analyze` soll mindestens enthalten:

- `format`: fester Formatbezeichner `cmake-xray.analysis`
- `format_version`: Schema-/Formatversion, initial `1`
- `report_type`: `analyze`
- `inputs`: verwendete Eingabequellen aus dem strukturierten `ReportInputs`-Modell, mindestens `compile_database_path`, `compile_database_source`, `cmake_file_api_path`, `cmake_file_api_resolved_path` und `cmake_file_api_source`
- `summary`: Translation-Unit-Anzahl, Ranking-Anzahl, Hotspot-Anzahl, Top-Limit, Beobachtungsherkunft und Target-Metadatenstatus
- `translation_unit_ranking`: Objekt mit `limit`, `total_count`, `returned_count`, `truncated` und deterministisch sortierten `items` inklusive Metriken, Diagnostics und Target-Zuordnungen
- `include_hotspots`: Objekt mit `limit`, `total_count`, `returned_count`, `truncated` und deterministisch sortierten `items`
- `diagnostics`: reportweite Diagnostics

Ein JSON-Bericht fuer `impact` soll mindestens enthalten:

- `format`: fester Formatbezeichner `cmake-xray.impact`
- `format_version`: Schema-/Formatversion, initial `1`
- `report_type`: `impact`
- `inputs`: verwendete Eingabequellen aus dem strukturierten `ReportInputs`-Modell, mindestens `compile_database_path`, `compile_database_source`, `cmake_file_api_path`, `cmake_file_api_resolved_path`, `cmake_file_api_source`, `changed_file` und `changed_file_source`
- `summary`: Anzahl betroffener Translation Units, Klassifikation, Beobachtungsherkunft, Target-Metadatenstatus und Anzahl betroffener Targets
- `directly_affected_translation_units`
- `heuristically_affected_translation_units`
- `directly_affected_targets`
- `heuristically_affected_targets`
- `diagnostics`

Wichtig:

- `inputs` darf nur Felder enthalten, die stabil im fachlichen Ergebnis- oder Request-Modell verfuegbar sind; `cmake_file_api_path` ist als Feld fuer M5 verpflichtend, damit File-API- und Mixed-Input-Laeufe vollstaendig dokumentiert werden koennen
- JSON folgt bei `analyze` der CLI-`--top`-Begrenzung fuer Ranking- und Hotspot-Listen, muss aber ueber `limit`, `total_count`, `returned_count` und `truncated` eindeutig anzeigen, ob die Ausgabe gekuerzt wurde; eine unlimitierte JSON-Ausgabe waere nur nach expliziter Schema-Entscheidung zulaessig
- `impact`-JSON enthaelt in M5 keine `limit`-/`truncated`-Felder, solange der CLI- und Port-Vertrag keine Impact-Begrenzung kennt; alle betroffenen Translation Units und Targets aus dem `ImpactResult` werden ausgegeben
- Markdown, HTML und DOT folgen bei `analyze` ebenfalls der CLI-`--top`-Begrenzung; HTML und Markdown kennzeichnen begrenzte Abschnitte menschenlesbar
- DOT enthaelt bei `analyze --top N` die Top-N-Ranking- und Top-N-Hotspot-Knoten sowie begrenzte Kontextkanten; fuer einen Top-Hotspot werden hoechstens `context_limit` der in `IncludeHotspot` gespeicherten betroffenen Translation Units als Kontextknoten ausgegeben, auch wenn diese Translation Units nicht selbst im Top-N-Ranking liegen. `context_total_count` zaehlt pro Hotspot die eindeutigen betroffenen Translation Units vor `context_limit` und vor globalem Graph-Budget; `context_returned_count` zaehlt pro Hotspot die eindeutigen Kontext-Translation-Units, die nach `context_limit`, globalem `node_limit` und globalem `edge_limit` tatsaechlich als Knoten im Graph enthalten und durch eine Kontextkante mit diesem Hotspot verbunden sind. Wenn dieselbe Kontext-TU mehreren Hotspots zugeordnet ist, wird sie pro Hotspot in dessen `context_*`-Werten gezaehlt, aber als Graphknoten nur einmal gegen das globale `node_limit`. Nach Anwendung des `edge_limit` werden reine Kontext-TU-Knoten ohne verbleibende Kontextkante entfernt, sofern sie nicht zugleich primaere Ranking-Knoten oder Target-Kontextknoten sind; `graph_node_limit`, `graph_truncated` und `context_returned_count` beziehen sich auf diesen finalen Graphzustand.
- DOT fuer `impact` ist trotz unbegrenztem `ImpactResult` ebenfalls budgetiert; der Graph enthaelt betroffene Translation Units und Targets nur bis zum globalen `node_limit`-/`edge_limit`-Budget und kennzeichnet Kuerzungen ueber `graph_truncated`
- `context_limit` ist fuer M5 Teil des DOT-Formatvertrags, kleiner als oder gleich dem wirksamen `--top`-Limit bei `analyze` und nicht pro Hotspot zu einem unbeschraenkten Gesamtgraphen addierbar, weil zusaetzlich ein globales `node_limit`-/`edge_limit`-Budget gilt
- M5 legt die DOT-Grenzen deterministisch und als M5-Formatvertrag fest: Fuer `analyze` gilt `context_limit = min(top_limit, 5)`, `node_limit = max(25, 4 * top_limit + 10)`, `edge_limit = max(40, 6 * top_limit + 20)`. Ohne explizites `--top` wird der wirksame Standard-Top-Wert der CLI verwendet. Fuer `impact` gilt mangels Top-Limit fest `node_limit = 100` und `edge_limit = 200`. Eine spaetere Konfigurierbarkeit waere eine explizite Vertragsaenderung mit Dokumentation, Tests und gegebenenfalls DOT-Metadaten fuer die wirksamen Limits.
- Knotenkuerzung erfolgt deterministisch in stabiler Sortierreihenfolge: bei `analyze` zuerst primaere Top-Ranking-Knoten, dann Top-Hotspot-Knoten, dann Target-Knoten, dann Hotspot-Kontext-Translation-Units sortiert nach Anzeige-Pfad; bei `impact` zuerst geaenderte Datei, dann direkt betroffene Translation Units, heuristisch betroffene Translation Units und Targets, jeweils nach Anzeige-Pfad bzw. Target-Name sortiert
- Kanten werden nur ausgegeben, wenn beide Endknoten im Budget enthalten sind. Bei erreichtem `edge_limit` gilt eine globale Kantenprioritaet mit lexikografischem Tie-Breaker nach stabiler Quell-ID, Ziel-ID und Kantenart: fuer `analyze` zuerst Translation-Unit-zu-Include-Hotspot-Kanten, dann Translation-Unit-zu-Target-Kanten; fuer `impact` zuerst geaenderte-Datei-zu-direkt-betroffener-Translation-Unit-Kanten, dann geaenderte-Datei-zu-heuristisch-betroffener-Translation-Unit-Kanten, dann direkt-betroffene-Translation-Unit-zu-Target-Kanten, dann heuristisch-betroffene-Translation-Unit-zu-Target-Kanten
- DOT-Attributwerte verwenden fuer den M5-Metadatenvertrag eine exakte Lexik: Integer werden als unquoted ASCII-Dezimalzahlen ohne Vorzeichen geschrieben, Booleans als unquoted lowercase `true` oder `false`, Strings als quoted DOT-Strings mit deterministischem Escaping
- bei Analyze-Hotspot-Kontext enthaelt jeder betroffene Hotspot-Knoten verpflichtend die Node-Attribute `context_total_count` integer, `context_returned_count` integer und `context_truncated` boolean; bei ungekuerztem Kontext ist `context_truncated=false`, und diese `context_*`-Attribute gelten nicht fuer `impact`
- DOT enthaelt fuer `analyze` und `impact` immer die Graph-Attribute `graph_node_limit` integer, `graph_edge_limit` integer und `graph_truncated` boolean; bei ungekuerztem Graph ist `graph_truncated=false`, damit Consumer budgetierte M5-Ausgaben von aelteren oder unbudgetierten DOT-Ausgaben unterscheiden koennen. Kommentare duerfen nur zusaetzlich erscheinen und sind nicht Teil des Vertrags
- JSON-Ausgaben muessen gueltiges UTF-8 und syntaktisch gueltiges JSON sein
- Felder mit leerer Menge werden als leere Arrays ausgegeben, nicht weggelassen, sofern sie Teil des Formatvertrags sind
- optionale fachliche Informationen koennen `null` sein, wenn das Schema dies explizit dokumentiert
- numerische Metriken bleiben numerisch und werden nicht als lokalisierte Strings gerendert
- die Reihenfolge von Objektfeldern soll im Adapter stabil bleiben, auch wenn JSON semantisch keine Feldreihenfolge garantiert
- Pfade werden als Anzeige-Strings ausgegeben; kanonische Normalisierungsschluessel duerfen nur aufgenommen werden, wenn sie keinen instabilen Hostbezug in Goldens einfuehren
- `docs/report-json.md` ist der verbindliche lesbare M5-Vertrag und dokumentiert fuer alle Felder Typ, Pflichtstatus, erlaubte Enum-Werte, Nullability, leere-Array-Regeln und Aenderungsregeln fuer kuenftige `format_version`-Erhoehungen; zusaetzlich entsteht ein maschinenlesbares Schema-Artefakt `docs/report-json.schema.json`, gegen das Golden-Outputs validiert werden
- Tests pruefen nicht nur syntaktisch gueltiges JSON, sondern auch Pflichtfelder, Array-statt-Weglassen-Regeln, numerische Typen, bekannte Enum-Werte und `null` nur an dokumentierten Stellen

Vorgesehene Artefakte:

- `src/adapters/output/json_report_adapter.h`
- `src/adapters/output/json_report_adapter.cpp`
- `tests/adapters/test_json_report_adapter.cpp`
- JSON-Golden-Outputs unter `tests/e2e/testdata/m5/`
- Dokumentation `docs/report-json.md` als verbindlicher lesbarer Formatvertrag und `docs/report-json.schema.json` als maschinenlesbares Validierungsartefakt; `docs/guide.md` verweist darauf nur nutzungsorientiert

**Ergebnis**: `cmake-xray` besitzt einen versionierten, automatisierbaren Reportvertrag fuer Analyse- und Impact-Ergebnisse.

### 1.3 `DotReportAdapter` fuer Graphviz-Ausgaben umsetzen

Der DOT-Export soll vorhandene Analyseergebnisse visualisieren, ohne M6-Target-Graph-Funktionen vorwegzunehmen. M5 darf deshalb nur Graphen aus bereits vorhandenen Daten bilden.

Fuer `analyze` ist in M5 ein Graph sinnvoll, der mindestens folgende Knotenarten abbildet:

- Translation Units
- Include-Hotspots bzw. Include-Dateien, sofern sie im Ergebnis vorhanden sind
- Targets, sofern M4-Target-Zuordnungen geladen wurden

Kanten in `analyze`:

- Translation Unit zu Include-Hotspot, wenn der Hotspot fuer diese Sicht belastbar aus dem Ergebnis ableitbar ist
- Translation Unit zu Target, wenn eine Target-Zuordnung vorhanden ist

Fuer `impact` ist in M5 ein Graph sinnvoll, der mindestens folgende Knotenarten abbildet:

- geaenderte Datei
- direkt betroffene Translation Units
- heuristisch betroffene Translation Units
- betroffene Targets, sofern Target-Zuordnungen vorhanden sind

Kanten in `impact`:

- geaenderte Datei zu direkt betroffener Translation Unit
- geaenderte Datei zu heuristisch betroffener Translation Unit mit unterscheidbarem Style
- betroffene Translation Unit zu Target

Wichtig:

- es werden keine Target-zu-Target-Kanten erzeugt, solange `F-18` nicht umgesetzt ist
- die DOT-Budget-, Sortier-, Edge-Prioritaets-, Attributlexik- und Truncation-Regeln sind Bestandteil dieses Arbeitspakets: `analyze` verwendet `context_limit = min(top_limit, 5)`, `node_limit = max(25, 4 * top_limit + 10)` und `edge_limit = max(40, 6 * top_limit + 20)`; `impact` verwendet fest `node_limit = 100` und `edge_limit = 200`
- Analyze-Hotspot-Kontext wird pro Hotspot-Knoten ueber die Node-Attribute `context_total_count`, `context_returned_count` und `context_truncated` beschrieben; das globale Graph-Budget wird fuer `analyze` und `impact` immer ueber die Graph-Attribute `graph_node_limit`, `graph_edge_limit` und `graph_truncated` beschrieben
- DOT-IDs muessen deterministisch und korrekt escaped sein
- Labels duerfen fuer Lesbarkeit gekuerzt werden, wenn der vollstaendige Pfad als Attribut erhalten bleibt
- der Graph muss auch fuer leere Ergebnisse gueltiges DOT ausgeben
- die Ausgabe ist ein Graphviz-Quelltext, keine gerenderte SVG-/PNG-Datei
- Adapter- und Golden-Tests enthalten Sonderzeichenfaelle fuer IDs und Labels, zum Beispiel Leerzeichen, Anfuehrungszeichen, Backslashes und plattformtypische Pfadtrenner

Vorgesehene Artefakte:

- `src/adapters/output/dot_report_adapter.h`
- `src/adapters/output/dot_report_adapter.cpp`
- `tests/adapters/test_dot_report_adapter.cpp`
- DOT-Golden-Outputs unter `tests/e2e/testdata/m5/`
- Validierung der DOT-Goldens mit `dot -Tsvg` in CI, sofern Graphviz verfuegbar ist; andernfalls dokumentierter Parser-/Syntax-Smoke mit demselben Escaping-Fokus
- Nutzungsbeispiel fuer `dot -Tsvg` in der Dokumentation

**Ergebnis**: Nutzer koennen vorhandene Analyse- und Impact-Ergebnisse in Graphviz-Werkzeuge ueberfuehren, ohne dass M5 eine neue fachliche Graphanalyse einfuehrt.

### 1.4 `HtmlReportAdapter` fuer eigenstaendige HTML-Berichte umsetzen

Der HTML-Export ist der wichtigste menschenlesbare Ausbau in M5. Er soll die Markdown-Informationen komfortabler lesbar machen, aber keine dynamische Webanwendung einfuehren.

Fuer HTML gilt:

- jeder Bericht ist eine vollstaendige HTML-Datei
- CSS wird inline eingebettet, damit Artefakte in CI und Releases ohne Begleitdateien nutzbar sind
- es werden keine externen Fonts, Skripte oder CDN-Ressourcen geladen
- Tabellen, Zusammenfassungen und Diagnostics muessen ohne JavaScript nutzbar sein
- Target-Zuordnungen aus M4 werden sichtbar dargestellt, wenn vorhanden
- leere Ergebnisabschnitte erhalten klare Leersaetze analog zu Markdown

Wichtig:

- alle nutzergelieferten Inhalte und Pfade muessen HTML-escaped werden
- HTML-Ausgaben duerfen keine Hostnamen, Zeitstempel oder absoluten Build-Umgebungsdetails ergaenzen, die nicht bereits Teil der fachlichen Ergebnisdaten sind
- die visuelle Struktur soll stabil genug fuer Golden-Tests sein; kosmetische Zufallselemente sind nicht zulaessig
- Barrierearme Basisstruktur ist Pflicht: sinnvolle Ueberschriftenhierarchie, Tabellenueberschriften und ausreichender Kontrast

Vorgesehene Artefakte:

- `src/adapters/output/html_report_adapter.h`
- `src/adapters/output/html_report_adapter.cpp`
- `tests/adapters/test_html_report_adapter.cpp`
- HTML-Golden-Outputs unter `tests/e2e/testdata/m5/`
- Beispielberichte unter `docs/examples/`

**Ergebnis**: `cmake-xray` kann Analyse- und Impact-Ergebnisse als portables HTML-Artefakt fuer Reviews, CI und Dokumentation bereitstellen.

### 1.5 CLI-Modi `--verbose` und `--quiet` einfuehren

M5 erweitert die Bedienbarkeit um zwei Ausgabemodi. Beide Modi duerfen die bestehende Fehlersemantik nicht verwischen.

`--verbose` soll mindestens:

- reportweite Diagnostics vollstaendig ausgeben
- Eingabequellen, Beobachtungsherkunft und Target-Metadatenstatus sichtbar machen, sofern nicht ohnehin im Format enthalten
- bei CLI-Fehlern zusaetzliche Kontextinformationen liefern, ohne Stacktraces oder interne Implementierungsdetails auszugeben
- fuer `console` eine CLI-eigene detailreichere Emission erzeugen duerfen, ohne `ConsoleReportAdapter` oder Report-Ports als Verbose-Variante umzubauen
- fuer `json` keine unstrukturierte Zusatztextausgabe auf `stdout` und keine Verbose-only-Aenderung am JSON-Artefakt erzeugen; zusaetzliche Verbose-Diagnostik geht ausschliesslich nach `stderr`
- fuer `markdown`, `html` und `dot` keine zusaetzlichen Verbose-only-Inhalte in Reportartefakte schreiben; verbose beeinflusst dort nur Console-/`stderr`-Diagnostik und bereits im fachlichen Ergebnis vorhandene Diagnostics

`--quiet` soll mindestens:

- bei erfolgreichem `console`-Aufruf die Ausgabe auf eine knappe Zusammenfassung reduzieren
- bei Dateiausgabe keine Erfolgsmeldungen auf `stdout` ausgeben; Warnungen und Fehler bleiben auf `stderr`
- Warnungen und Fehler weiterhin auf `stderr` ausgeben
- fuer `json`, `html`, `markdown` und `dot` die Reportdatei bzw. den Reportinhalt nicht fachlich kuerzen, wenn das Format als Ergebnisartefakt angefordert wurde
- fuer `json`, `html`, `markdown` und `dot` ohne `--output` den Report auf `stdout` byte-identisch zum Normalmodus ausgeben; `--quiet` darf keine Erfolgsmeldungen oder Zusatztexte in diesen stdout-Report mischen

Entscheidungen fuer M5:

- `--verbose` und `--quiet` sind gegenseitig exklusiv
- beide Optionen gelten fuer `analyze` und `impact`
- die dokumentierte und getestete CLI-Position ist command-lokal nach dem Subcommand, also `cmake-xray analyze --quiet ...` bzw. `cmake-xray impact --verbose ...`; globale Schreibweisen vor dem Subcommand sind in M5 nicht Teil des Vertrags
- Verbosity ist kein fachlicher Report-Parameter, sondern steuert nur CLI-Emissionen wie Console-Zusammenfassung, Erfolgsmeldungen, `stderr`-Diagnostik und Fehlerkontext
- `--format console` ist ausdruecklich ein CLI-owned Emissionspfad: quiet/normal/verbose werden dort in der CLI-Schicht entschieden; `ConsoleReportAdapter` kann fuer den Normalmodus bestehen bleiben, wird aber nicht zum allgemeinen Verbosity-Port-Vertrag
- Console-Normalmodus bleibt fuer bestehende Compile-Database-only-Goldens byte-stabil; Console-Quiet gibt nur Ergebnisstatus, zentrale Zaehler und Warn-/Fehlerhinweise aus, waehrend Console-Verbose zusaetzlich Eingabeprovenienz, Beobachtungsherkunft, Target-Metadatenstatus und vollstaendige Diagnostics ausgeben darf. Die konkrete Abschnittsreihenfolge wird in Golden-Tests fixiert und muss dokumentiert werden.
- `ReportWriterPort`, `GenerateReportPort` und die Formatadapter bleiben frei von einem `OutputVerbosity`-Parameter; Artefaktinhalte werden ausschliesslich durch Ergebnisdaten, Format und `analyze`-Top-Limit bestimmt
- Standardverhalten ohne beide Optionen bleibt rueckwaertskompatibel
- quiet beeinflusst nicht Exit-Codes
- verbose darf nur Console-/`stderr`-Goldens fuer explizite Verbose-Testfaelle aendern, nicht Markdown-, HTML-, DOT- oder JSON-Artefakt-Goldens

Vorgesehene Artefakte:

- Anpassung von `src/adapters/cli/cli_adapter.*`
- Anpassung von `src/main.cpp`
- kleines CLI-Modell fuer `OutputVerbosity` in der CLI-Schicht, ohne Aenderung der fachlichen Report-Port-Signaturen
- Tests in `tests/e2e/test_cli.cpp`
- Golden-Outputs fuer quiet/verbose unter `tests/e2e/testdata/m5/`

**Ergebnis**: Die CLI kann sowohl diagnoseorientiert als auch automationsfreundlich knapp betrieben werden.

### 1.6 Release-Artefakte und OCI-Image absichern

M5 macht die Bereitstellung aus dem Lastenheft verbindlich. Ziel ist nicht nur ein lokaler Docker-Build, sondern ein reproduzierbarer, tag-basierter Release-Pfad.

Der Release-Pfad muss mindestens:

- bei Tag-Push mit erlaubtem Semver-Tag einen Release-Workflow starten: regulaere Releases verwenden `vMAJOR.MINOR.PATCH`, Vorabversionen verwenden `vMAJOR.MINOR.PATCH-PRERELEASE`
- Tags vor dem Build fail-fast validieren und unbekannte Muster wie `vfoo`, unvollstaendige Versionen oder Build-Metadaten ohne dokumentierte Freigabe abbrechen
- die Root-`CMakeLists.txt`-`project(... VERSION ...)` bleibt numerisch (`MAJOR.MINOR.PATCH`), weil CMake dort keine Prerelease-Suffixe akzeptiert; Release-Builds verwenden `XRAY_APP_VERSION` aus dem validierten Tag ohne fuehrendes `v` als kanonische veroeffentlichte App-/Package-Version, `XRAY_VERSION_SUFFIX` ist nur fuer lokale oder nicht veroeffentlichende Builds zulaessig, und gleichzeitiges Setzen beider Quellen bricht fail-fast ab
- `cmake-xray --version`, die von `ApplicationInfo` gelieferte kompilierte bzw. generierte App-Version, Paketmetadaten und Release-Tag muessen dieselbe veroeffentlichte Semver-Version ohne fuehrendes `v` melden; fuer `v1.2.0-rc.1` ist die erwartete App-Version also `1.2.0-rc.1`, waehrend die CMake-Projektversion `1.2.0` bleibt
- die CLI erhaelt einen top-level `cmake-xray --version`-Pfad, der ohne Subcommand funktioniert, nur die veroeffentlichte App-Version ohne fuehrendes `v` auf stdout schreibt und keine Analyse initialisiert; das historische fuehrende `v` in statischen Versionsquellen wird entweder entfernt oder vor der Ausgabe normalisiert, aber Release-Checks pruefen die ausgegebene App-Version ohne `v`
- Test-, Coverage-, Quality- und Runtime-Pfade vor dem Veroeffentlichen ausfuehren
- Publishing-Schritte in einen finalen Release-Job nach allen Gates verschieben; Verify-/Build-Jobs duerfen Container und Archive bauen und testen, aber keine Images oder Release-Artefakte in externe Registries bzw. Releases pushen
- ein Linux-CLI-Artefakt als `.tar.gz` erzeugen
- im Archiv mindestens die ausfuehrbare Datei, Lizenz-/Hinweisdateien und kurze Nutzungsdokumentation enthalten
- eine SHA-256-Pruefsumme erzeugen
- vorhandene macOS- und Windows-Artefakt-Jobs im Release-Workflow bewusst einordnen: Entweder werden sie fuer M5-Releases deaktiviert, oder sie werden nur als experimentelle Preview-Artefakte veroeffentlicht, klar benannt und in Release Notes sowie Dokumentation als nicht vollstaendig freigegeben markiert
- ein OCI-kompatibles Runtime-Image bauen
- das Image mit dem Versions-Tag veroeffentlichen
- fuer regulaere Releases ohne Prerelease-Suffix zusaetzlich `latest` pflegen und Vorabversionen davon ausnehmen
- GitHub-Release-Notes aus dem Changelog verwenden
- den GitHub Release zunaechst als Draft oder staging-internen Release vorbereiten und erst nach erfolgreichem OCI-Image-Publish oeffentlich veroeffentlichen; der oeffentliche GitHub Release ist der letzte Publish-Schritt
- einen dokumentierten Recovery-/Rollback-Pfad fuer den Fall bereitstellen, dass Draft-Erstellung, Image-Publish oder finale Release-Publikation fehlschlagen
- einen automatisierten Release-Dry-Run gegen Mock-Targets bereitstellen: GitHub-Release-Operationen laufen gegen einen Fake-Publisher, der Zustandsuebergaenge und Reihenfolge assertiert (`draft_release_created` vor OCI-Publish, OCI-Publish erfolgreich vor `release_published`, oeffentlicher Release als letzter Publish-Schritt); ein reiner Noop ohne aufgezeichnete Assertions reicht nicht. OCI-Pushes muessen gegen ein lokales Container-Registry-Testziel laufen und Tagging, `latest`-Regel, Push und Manifest-Pruefung real ausueben; optionale echte Staging-Repositories duerfen ergaenzen, sind aber nicht Voraussetzung fuer M5

Wichtig:

- Release-Artefakte duerfen nicht von lokalen Build-Pfaden abhaengen
- offiziell freigegebene M5-Artefakte sind Linux-CLI-Archiv und OCI-Image; macOS-/Windows-Archive zaehlen nur dann zum Releaseumfang, wenn der Plan vor Abschluss explizit auf vollstaendige Plattformfreigabe erweitert wird
- der Runtime-Container muss `cmake-xray --help` ohne weitere Toolchain ausfuehren koennen
- die Dokumentation muss lokale Nutzung und CI-Nutzung des Containers zeigen
- fehlgeschlagene Release-Schritte muessen klar sichtbar sein und keine Teilveroeffentlichung als Erfolg melden
- insbesondere darf kein GHCR-Image veroeffentlicht werden, bevor Build-Artefakte und Smoke-Tests erfolgreich abgeschlossen sind; danach wird das Image gepusht, und erst danach wird der GitHub Release aus dem Draft-Status oeffentlich gemacht
- wenn Draft-Erstellung, Image-Publish oder finale Release-Publikation fehlschlagen, muss der Workflow entweder automatisch aufraeumen oder eine dokumentierte manuelle Recovery mit Draft-Loeschung, GHCR-Tag-Loeschung, Nachpublish, Retagging und Release-Notes-Korrektur erzwingen
- die Release-Dokumentation muss die erlaubten Tag-Muster, Prerelease-Behandlung und `latest`-Regel explizit nennen

Vorgesehene Artefakte:

- Umstrukturierung von `.github/workflows/release.yml`: kein GHCR-Push im Verify-Job, bewusste macOS-/Windows-Artefaktbehandlung, Draft-Release vor Image-Publish und oeffentliche Release-Publikation als letzter Schritt
- `scripts/release-dry-run.sh` als lokaler und CI-faehiger Entry-Point fuer Fake-Publisher, lokale Registry, Tag-/`latest`-Regeln und Publish-Reihenfolge
- CLI-Adapter- und `ApplicationInfo`-Anpassung fuer `cmake-xray --version` ohne fuehrendes `v`
- Pruefung und ggf. Anpassung von `Dockerfile`
- Aktualisierung von `docs/releasing.md`
- Aktualisierung von `README.md`
- automatisierte Release-Smoke-Tests fuer entpacktes Linux-Archiv, Container-Runtime, Tag-Validierung und Versionskonsistenz; dokumentierte manuelle Schritte sind nur fuer echte externe Publish-/Recovery-Szenarien zulaessig

**Ergebnis**: Nutzer koennen `cmake-xray` als versioniertes Linux-Artefakt oder Container-Image nutzen, ohne ein lokales Build-Verzeichnis zu kennen.

### 1.7 Erweiterte Plattformunterstuetzung bewerten

M5 soll macOS und Windows nicht vollstaendig produktisieren, aber belastbar feststellen, welche Teile funktionieren und welche Einschraenkungen bestehen.

Fuer macOS:

- Build mit einem gaengigen Clang-/CMake-Setup
- Ausfuehrung der Unit-Tests, soweit ohne Linux-spezifische Annahmen moeglich
- CLI-Smoke-Test fuer `--help`, `analyze` und `impact`
- Bewertung von Pfadnormalisierung und Zeilenenden in Goldens

Fuer Windows:

- Build mit einem gaengigen MSVC- oder ClangCL-/CMake-Setup
- Ausfuehrung der Unit-Tests, soweit ohne POSIX-spezifische Annahmen moeglich
- CLI-Smoke-Test fuer `--help`, `analyze` und `impact`
- Bewertung von Laufwerksbuchstaben, Backslashes und CRLF in Report-Ausgaben

Fuer CMake-/Compiler-Kompatibilitaet:

- dokumentierte Mindestversionen bleiben explizit
- mindestens eine aktuelle CMake-Version wird in CI oder Smoke-Checks verwendet
- bekannte Einschraenkungen der CMake File API aus M4 bleiben dokumentiert

Vorgesehene Artefakte:

- verpflichtende CI-Matrix fuer die plattformsichere atomare Replace-Semantik auf Linux, macOS und Windows
- CI-Matrix oder dokumentierte Smoke-Check-Skripte fuer weitere macOS-/Windows-Build- und CLI-Smokes ausserhalb der atomaren Dateiausgabe
- Aktualisierung von `docs/quality.md`
- Aktualisierung von `docs/releasing.md`
- falls noetig kleine Portabilitaetsfixes an Pfad- und Testlogik

**Ergebnis**: Der Projektstand ist nicht mehr nur implizit Linux-zentriert; macOS- und Windows-Risiken sind sichtbar und priorisierbar.

### 1.8 Referenzdaten, Dokumentation und Abnahme aktualisieren

Die neuen Formate und Release-Pfade sind nur dann nutzbar, wenn Beispiele, Goldens und Dokumentation konsistent sind.

M5 aktualisiert mindestens:

- Versionsquellen fuer `v1.2.0`: Root-`CMakeLists.txt` bleibt numerische Projektversion; Release-Builds verwenden `XRAY_APP_VERSION` aus dem validierten Tag ohne fuehrendes `v` als kanonische App-/Package-Version fuer `ApplicationInfo`, Paketmetadaten und `--version`, waehrend `XRAY_VERSION_SUFFIX` nur fuer lokale oder nicht veroeffentlichende Builds zulaessig ist und gleichzeitiges Setzen beider Quellen fail-fast abbricht
- `docs/examples/` mit repraesentativen Markdown-, HTML-, JSON- und DOT-Ausgaben, die entweder aus validierten Goldens generiert oder selbst in die jeweiligen Validierungsmanifeste aufgenommen werden
- `README.md` mit Formatwahl, quiet/verbose und Release-/Container-Nutzung
- `docs/guide.md` mit praktischen Aufrufen fuer alle M5-Formate
- `docs/releasing.md` mit finalem Artefaktumfang und Release-Verifikation
- `docs/quality.md` mit M5-Testumfang und Plattform-Smokes
- `docs/performance.md` mit File-API-Performance-Baseline als erster Folge-Meilenstein nach M4
- `CHANGELOG.md` fuer `v1.2.0`

Tests und Abnahme muessen mindestens abdecken:

- Adapter-Unit-Tests fuer HTML, JSON und DOT
- HTML-Adapter-/Golden-Tests mit Sonderzeichen in Pfaden und Diagnostics, mindestens `<`, `>`, `&`, Anfuehrungszeichen und Backslashes, und Assertions auf korrekt escaped HTML ohne Rohinjektion
- CLI-Tests fuer Formatwahl, ungueltige Formatwerte und `--format markdown|html|json|dot --output <path>` fuer `analyze` und `impact`
- CLI-Negativtest, dass `--format console --output <path>` abgelehnt wird, weil `--output` auf artefaktorientierte Formate begrenzt bleibt
- CLI-Tests fuer atomare Dateiausgabe: erfolgreiche Writes erzeugen vollstaendige Reports, vorhandene Zieldateien werden bei Erfolg ersetzt, und Fehlerfaelle lassen vorhandene Zieldateien auf Linux, macOS und Windows unveraendert
- Unit-/Integrationstests fuer den Atomic-Replace-Wrapper, die existierende Ziele ohne vorheriges Loeschen ersetzen, simulierte Fehlerpfade mit intakter alter Datei pruefen und unter Windows die gewaehlte `ReplaceFileW`-/`MoveFileExW`-Semantik abdecken
- CLI-Tests, dass `--quiet` und `--verbose` command-lokal nach `analyze` bzw. `impact` akzeptiert werden und globale Positionen vor dem Subcommand weiterhin als Usage-Fehler abgelehnt werden: nonzero Exit, Text auf `stderr`, kein stdout-Report und keine Zieldatei
- CLI-Tests fuer `--verbose`, `--quiet` und gegenseitigen Ausschluss
- CLI-Golden-Tests, dass `--quiet --format json|dot|html|markdown` ohne `--output` denselben stdout-Report wie der Normalmodus ausgibt und keine Erfolgsmeldungen in stdout mischt; mit `--output` muss stdout fuer alle Reportformate im Normal-, Quiet- und Verbose-Modus leer bleiben, waehrend Verbose-Zusatzdiagnostik ausschliesslich auf `stderr` erscheinen darf
- Dokumentations- und Stream-Vertrag-Sync fuer `docs/report-json.md`, `docs/report-dot.md` und `docs/report-html.md`: erfolgreiche Normal- und Quiet-Artefaktausgaben bleiben ohne `--output` stdout-only und mit `--output` stdout/stderr-leer; erfolgreiche Verbose-Artefaktausgaben duerfen ausschliesslich Zusatzdiagnostik auf `stderr` ausgeben und Report-stdout beziehungsweise Reportdatei nicht veraendern
- Golden-Output-Tests fuer `analyze` und `impact` in allen neuen Formaten
- Regressionstests, dass bestehende Console-/Markdown-Goldens fuer Compile-Database-only-, File-API- und Mixed-Input-Laeufe im Normalmodus byte-stabil bleiben und `ReportInputs` in AP 1.1 nicht neu in diesen bestehenden Formaten sichtbar wird
- Golden- und CLI-Tests, dass `--top` bei `analyze` fuer Markdown, HTML, JSON und DOT konsistent wirkt und kein Artefaktformat implizit vollstaendige Listen ausgibt
- DOT-Golden-Tests, dass `analyze --top N` fuer ausgegebene Top-Hotspots hoechstens `context_limit` betroffene Translation Units als Kontextknoten enthaelt, das globale `node_limit`-/`edge_limit`-Budget einhaelt, gekuerzten Kontext mit den verpflichtenden `context_*`-Node-Attributen kennzeichnet und die verpflichtenden `graph_*`-Graph-Attribute immer ausgibt
- DOT-Golden-Tests fuer gleichzeitige `context_limit`-, `node_limit`- und `edge_limit`-Kuerzung, damit `context_total_count`, `context_returned_count`, mehrfach referenzierte Kontext-Translation-Units und das Entfernen unverbundener reiner Kontextknoten deterministisch gezaehlt werden
- DOT-Golden-Tests, dass `impact --format dot` das feste Impact-`node_limit`-/`edge_limit`-Budget einhaelt und die verpflichtenden Graph-Attribute `graph_node_limit`, `graph_edge_limit` und `graph_truncated` immer ausgibt, mit `graph_truncated=false` im ungekuerzten Fall
- DOT-Golden-Tests fuer erreichte `edge_limit`-Faelle, die die globale Kantenprioritaet, lexikografische Tie-Breaker und die exakte Attributlexik fuer Integer, Boolean und Strings pruefen
- CLI- und Port-Tests, dass `impact` in M5 keine `--top`-Option akzeptiert und ImpactResult-basierte JSON-, HTML- und Markdown-Reports keine implizite fachliche Ergebnisbegrenzung oder JSON-`limit`-/`truncated`-Felder einfuehren; `impact --format dot` bleibt die visualisierungsorientierte Ausnahme mit festem `node_limit`-/`edge_limit`-Budget
- JSON-Schema-/Golden-Tests fuer `inputs.cmake_file_api_path` und `inputs.cmake_file_api_resolved_path` bei File-API- und Mixed-Input-Laeufen sowie fuer `limit`, `total_count`, `returned_count` und `truncated` bei gekuerzten und ungekuerzten `analyze`-Listen
- JSON-Schema-/Golden-Tests fuer `ReportInputs`-Pfad-Provenienz: CLI-relative Pfade, absolute Pfade, Build-Dir-vs.-Reply-Dir bei `--cmake-file-api` und relative `--changed-file` muessen stabile Display-Pfade und passende `*_source`-Enums liefern
- JSON-Schema-/Golden-Tests fuer feste JSON-`format`-Werte `cmake-xray.analysis` und `cmake-xray.impact`
- JSON-Schema-/Golden-Tests fuer alle in JSON v1 erlaubten `*_source`-Werte und Nullability-Regeln: `compile_database_source=cli|null`, `cmake_file_api_source=cli|null` und `changed_file_source=compile_database_directory|file_api_source_root|cli_absolute`; absolute-Pfad-Goldens muessen synthetische, fixture-stabile Pfade verwenden. `changed_file_source=unresolved_file_api_source_root` bleibt ein Modellwert fuer File-API-Fehlerergebnisse und wird in JSON v1 per Schema-/Adapter-Negativtest abgelehnt, weil nicht wiederherstellbare File-API-Ladefehler als Textfehler ohne JSON-Report enden.
- JSON-Schema-/Golden-Tests fuer Mixed-Input-Impact-Laeufe, dass relative `changed_file`-Pfade auf Basis der `compile_commands.json`-Directory interpretiert werden und `inputs.changed_file_source=compile_database_directory` ausgeben; File-API-only-Laeufe verwenden `file_api_source_root`, absolute CLI-Pfade verwenden `cli_absolute`
- Adapter-/Service-Tests, dass der vom `CmakeFileApiAdapter` aufgeloeste Build-/Reply-Pfad als roher lexikalischer Pfad ueber `BuildModelResult` transportiert wird, `ProjectAnalyzer`/`ImpactAnalyzer` daraus relativ zur `report_display_base` den Display-Pfad fuer `ReportInputs.cmake_file_api_resolved_path` erzeugen und kein CLI-Zustand in Adapter nachgereicht wird
- Port-/Service-Tests fuer `AnalyzeProjectRequest`, damit `compile_commands_path`, `cmake_file_api_path` und `report_display_base` explizit transportiert und fuer Analyze-`ReportInputs` genutzt werden
- Port-/Service-Tests fuer `AnalyzeImpactRequest`, damit `compile_commands_path`, `changed_file_path`, `cmake_file_api_path` und `report_display_base` explizit transportiert und fuer `ReportInputs` genutzt werden
- JSON-Schema-/Golden-Tests, dass `inputs.compile_database_path` und `inputs.cmake_file_api_path` bei `analyze` immer vorhanden sind und fehlende Eingaben als `null`, nie als leerer String oder weggelassenes Feld, erscheinen
- JSON-Schema-/Golden-Tests, dass `inputs.compile_database_path`, `inputs.cmake_file_api_path` und `inputs.changed_file` bei `impact` immer vorhanden sind; nicht verwendete optionale Analysequellen erscheinen als `null`, waehrend `inputs.changed_file` wegen der CLI-Pflicht fuer `impact` immer ein String ist
- Service-Tests, dass `ProjectAnalyzer` und `ImpactAnalyzer` Report-Pfade relativ zur expliziten `invocation_cwd`-/`report_display_base`-Request-Basis serialisieren und nicht vom Prozess-CWD abhaengen
- Tests, dass `--verbose` JSON-, Markdown-, HTML- und DOT-Artefakte nicht gegenueber dem Normalmodus veraendert und Zusatzdiagnostik nur Console/`stderr` betrifft
- Console-Golden-Tests fuer Quiet-, Normal- und Verbose-Modus, die die groben Abschnittsvertraege und die Normalmodus-Rueckwaertskompatibilitaet absichern
- Smoke-Test fuer Docker-Runtime-Image
- Smoke-Test fuer Linux-Release-Artefakt nach Entpacken
- Pruefsummen-Test, dass die SHA-256-Datei existiert, den richtigen Linux-Archivnamen referenziert, gegen das erzeugte Archiv erfolgreich validiert und die enthaltene Binary-Checksumme beziehungsweise dokumentierte Binary-Pruefsumme mit dem entpackten Artefakt abgleicht
- Reproduzierbarkeits-Smoke fuer das Linux-Archiv: zwei Builds aus gleichem Commit und gleicher Version muessen Archiv-Checksumme, entpackte Dateiliste, Dateimodi und Binary-Checksumme stabil vergleichen
- Versionscheck, dass Root-`CMakeLists.txt` die numerische Basisversion meldet, Release-Builds `XRAY_APP_VERSION` aus dem validierten Tag ohne fuehrendes `v` verwenden, `XRAY_VERSION_SUFFIX` nur fuer lokale oder nicht veroeffentlichende Builds zulaessig ist, gleichzeitiges Setzen beider Quellen fail-fast abbricht und `cmake-xray --version`, die kompilierte bzw. generierte `ApplicationInfo`-Version, App-/Package-Version-Quelle und Release-Tag dieselbe veroeffentlichte Semver-Version ohne fuehrendes `v` melden, inklusive Prerelease-Suffix bei Tags wie `v1.2.0-rc.1`
- CLI-Test, dass `cmake-xray --version` ohne Subcommand funktioniert, ausschliesslich die App-Version ohne fuehrendes `v` nach stdout schreibt und keine Analyse-/Report-Pfade initialisiert
- automatisierter Release-Test oder Workflow-Schritt fuer erlaubte Semver-Tags, Prerelease-Tags und Ablehnung ungueltiger Tags
- automatisierter Release-Dry-Run fuer Draft-Release, OCI-Image-Publish und finale oeffentliche Release-Publikation als letzten Schritt: `scripts/release-dry-run.sh` und ein gleichnamiger bzw. eindeutig benannter Workflow-Job muessen GitHub-Schritte mit einem Fake-Publisher samt Assertions fuer Zustandsuebergaenge und Reihenfolge sowie OCI-Schritte mit lokaler Test-Registry fuer Tagging, `latest`-Regel, Push und Manifest-Pruefung ausfuehren, ohne externe Veroeffentlichung
- dokumentierter manueller Dry-Run nur fuer Recovery-Pfade, die echte externe Publish-Zustaende in GitHub Releases oder GHCR voraussetzen
- Performance-Messung fuer den CMake-File-API-Pfad auf reproduzierbaren Referenzgroessen und Dokumentation der M5-Baseline in `docs/performance.md`: AP 1.8 erzeugt oder versioniert die fuer `scale_*` noetigen CMake-File-API-Reply-Fixtures ueber `tests/reference/generate_reference_projects.py` oder ein explizites File-API-Fixture-Skript, dokumentiert Generator, CMake-Version, Fixture-Manifest, Reply-Verzeichnis und Erzeugungskommando in `tests/reference/file-api-performance-manifest.json`, und verhindert, dass compile-db-only Referenzdaten als File-API-Baseline missverstanden werden
- Plattform-Abnahme fuer Linux, macOS und Windows gemaess AP 1.7: jede Plattform ist in `docs/quality.md` und `docs/releasing.md` mit Status `supported`, `validated_smoke` oder `known_limited` dokumentiert; macOS und Windows erreichen `validated_smoke` nur mit verpflichtendem CI-Required-Check oder schema-validiertem Smoke-Report fuer Build, Atomic-Replace und normative CLI-Smokes, andernfalls werden konkrete Einschraenkungen als `known_limited` dokumentiert
- Doku-Gate fuer Nutzeraufrufe: `README.md`, `docs/guide.md`, `docs/releasing.md` und `docs/examples/` duerfen keine interne Build-Pfad-Nutzung wie `./build/cmake-xray` voraussetzen und beschreiben Release-Artefakte, Container oder installierte Binary-Pfade als regulaere Nutzerwege; `docs/performance.md` ist als Entwickler- und Messdokument ausgenommen, muss interne Build-Pfade aber als reproduzierbare Messkommandos kennzeichnen und darf sie nicht als normale Nutzeraufrufe darstellen
- Validierung, dass JSON syntaktisch gueltig ist, `format_version` enthaelt, den Pflichtfeld-, Typ-, Enum-, Nullability- und Array-Regeln aus `docs/report-json.md` entspricht und gegen `docs/report-json.schema.json` validiert
- Validierung, dass DOT syntaktisch durch Graphviz `dot -Tsvg` oder einen gleichwertigen DOT-Parser akzeptiert wird und Escaping-Goldens korrekt verarbeitet werden
- Validierung, dass `docs/examples/` keine driftenden Kopien enthaelt: Markdown-, HTML-, JSON- und DOT-Beispiele werden entweder aus den validierten Goldens generiert und in CI gegen den aktuellen Generatorausgang verglichen oder ueber ein CTest-Gate wie `tests/validate_doc_examples.py` und `docs/examples/manifest.txt` selbst in die jeweiligen JSON-Schema-, DOT-Syntax-, HTML-Struktur- und Golden-Manifeste aufgenommen

**Ergebnis**: M5 ist nicht nur implementiert, sondern ueber Beispiele, Referenzdaten und Release-Dokumentation nachvollziehbar abnehmbar.

## 2. Reihenfolge und Abhaengigkeiten

Empfohlene Reihenfolge:

1. Report-Vertraege und Formatversionierung festlegen
2. JSON-Adapter umsetzen, weil er den stabilsten Datenvertrag erzwingt
3. DOT-Adapter auf Basis der vorhandenen Ergebnisdaten umsetzen
4. HTML-Adapter umsetzen
5. CLI-Formatwahl und quiet/verbose final verdrahten
6. Golden-Outputs und Beispiele fuer neue Formate erzeugen
7. Release-Workflow und Container-Verifikation absichern
8. macOS-/Windows-Smokes einrichten oder dokumentiert ausfuehren
9. README, Guide, Releasing, Quality und Changelog finalisieren

Abhaengigkeiten:

- `JsonReportAdapter`, `DotReportAdapter` und `HtmlReportAdapter` haengen an stabilen `AnalysisResult`- und `ImpactResult`-Modellen aus M4
- `DotReportAdapter` darf nur Target-Zuordnungen verwenden, die bereits in M4-Ergebnissen vorhanden sind
- `--quiet` und `--verbose` muessen vor Golden-Finalisierung feststehen
- Release-Workflow und Dokumentation muessen nach finaler CLI-Syntax aktualisiert werden

## 3. Risiken und Gegenmassnahmen

| Risiko | Auswirkung | Gegenmassnahme |
|---|---|---|
| JSON wird ohne klaren Vertrag eingefuehrt | Folgewerkzeuge brechen bei jeder Report-Aenderung | `format_version` dokumentieren und Golden-Tests fuer zentrale Felder pflegen |
| JSON-`inputs` verlangt Daten, die nicht im Ergebnisobjekt stehen | Adapter muessten CLI-Zustand kennen oder der Vertrag bleibt unerfuellt | strukturiertes `ReportInputs`-Modell als Bestandteil von `AnalysisResult` und `ImpactResult` einfuehren |
| JSON-`inputs` nutzt uneinheitliche Repraesentation fuer fehlende Pfade | Automatisierung muss `null`, leere Strings und fehlende Felder unterschiedlich abfangen | Felder immer ausgeben, gesetzte Pfade als String und fehlende Eingaben ausschliesslich als `null` serialisieren |
| JSON-`inputs` nutzt instabile Pfad-Provenienz | Golden-Outputs und Folgewerkzeuge unterscheiden CLI-Strings, Reply-Dirs und Normalisierungsschluessel falsch | stabile Display-Pfade und `*_source`-Enums im JSON-Vertrag festlegen |
| Originaler und aufgeloester CMake-File-API-Pfad werden vermischt | Folgewerkzeuge koennen Build-Dir- und Reply-Dir-Eingaben nicht reproduzierbar unterscheiden | `cmake_file_api_path` fuer Original-Display-Pfad und `cmake_file_api_resolved_path` fuer Adapter-Aufloesung getrennt serialisieren |
| Aufgeloester File-API-Pfad bleibt im Adapter verborgen | `ReportInputs.cmake_file_api_resolved_path` kann nur durch CLI-/Adapter-Leakage oder falsch gesetzte Werte entstehen | `BuildModelResult` um den rohen aufgeloesten Build-/Reply-Pfad erweitern; der originale CLI-Display-Pfad kommt weiter aus dem Request |
| Alte skalare Eingabepfadfelder bleiben neben `ReportInputs` aktiv | File-API-only-Laeufe koennen falsche Pfade in `inputs.compile_database_path` ausgeben | Legacy-Felder nur fuer bestehende Console-/Markdown-Ausgaben byte-stabil weiterpflegen; neue M5-Artefaktadapter und JSON-`inputs` muessen `ReportInputs` verwenden |
| Producer setzen `ReportInputs` nicht vollstaendig | JSON/HTML/DOT zeigen bekannte File-API- oder Mixed-Input-Kontexte als `null` | `ProjectAnalyzer`, `ImpactAnalyzer` und Driving-Requests/-Ports als Producer explizit anpassen und testen |
| Analyze-Listen sind durch `--top` gekuerzt, ohne dies zu kennzeichnen | Automatisierung verwechselt kurze Ausgaben mit vollstaendigen Projektdaten | `limit`, `total_count`, `returned_count` und `truncated` fuer Analyze-Ranking- und Hotspot-Listen verpflichtend machen |
| `--top` wird versehentlich auch fuer `impact` eingefuehrt | CLI- und Port-Vertraege divergieren und Impact-Ergebnisse werden unklar begrenzt | M5 begrenzt `--top` auf `analyze`; Impact akzeptiert keine Top-Option und gibt vollstaendige `ImpactResult`-Listen aus |
| `--output` bleibt nur teilweise fuer neue Formate nutzbar | Nutzer koennen neue Artefaktformate nicht verlaesslich in CI speichern | `--output` fuer `markdown`, `html`, `json` und `dot` explizit freischalten und atomare Schreibtests aufnehmen |
| Atomarer Write ueberschreibt vorhandene Dateien nicht portabel | Fehler koennen Zielartefakte zerstoeren oder Windows-Releases brechen | eigenen Replace-Wrapper mit POSIX-`rename`/`renameat` und Windows-`ReplaceFileW` oder `MoveFileExW` testen; kein vorheriges Loeschen des Zielpfads |
| Analyze-Artefaktformate interpretieren `--top` unterschiedlich | Goldens, CLI-Ausgabe und Automatisierung liefern widerspruechliche Ergebnismengen | einheitliche Top-Limit-Regel fuer Markdown, HTML, JSON und DOT bei `analyze` festlegen und testen |
| DOT-Hotspot-Kontext umgeht faktisch das `--top`-Limit | ein einzelner verbreiteter Header oder mehrere Top-Hotspots erzeugen sehr grosse Graphen trotz kleinem Top-Limit | DOT-Kontext mit separatem `context_limit` und globalem `node_limit`-/`edge_limit`-Budget begrenzen und Kuerzung maschinenlesbar kennzeichnen |
| Impact-DOT ist trotz unbegrenztem `ImpactResult` zu gross | haeufig inkludierte Header erzeugen sehr grosse Graphviz-Reports | feste Impact-DOT-Budgets `node_limit = 100` und `edge_limit = 200` mit `graph_truncated` anwenden |
| DOT-Grenzen werden unterschiedlich implementiert | Golden-Outputs werden instabil oder Graphen wachsen unerwartet | feste M5-Formeln fuer `context_limit`, `node_limit`, `edge_limit`, stabile Knoten-/Kantenkuerzung und exakte Attributlexik dokumentieren |
| DOT-Kuerzungsmetadaten stehen nur in Kommentaren oder nur bei Kuerzung | Graphviz-Consumer koennen budgetierte M5-Ausgaben nicht verlaesslich erkennen | verpflichtende Graph-Attribute mit exakten Namen und Typen immer ausgeben; Kommentare nur zusaetzlich erlauben |
| macOS-/Windows-Replace-Semantik wird nur dokumentiert, aber nicht getestet | atomare Dateiausgabe kann auf anderen Plattformen anders fehlschlagen | atomare Replace-Tests als CI-Gate fuer Linux, macOS und Windows aufnehmen |
| `--verbose` veraendert HTML-/Markdown-/DOT-Artefakte | Golden-Outputs und CI-Artefakte werden durch Diagnosemodus instabil | Verbose-only-Inhalte fuer diese Artefakte ausschliessen und Zusatzdiagnostik auf Console/`stderr` beschraenken |
| DOT suggeriert Target-Graph-Semantik, die M5 noch nicht besitzt | Nutzer interpretieren Graphen falsch | keine Target-zu-Target-Kanten erzeugen und Legende/Labels klar formulieren |
| DOT-Escaping ist syntaktisch fehlerhaft | Graphviz-Berichte brechen erst bei Nutzern | Sonderzeichen-Goldens mit Graphviz oder Parser validieren |
| HTML-Goldens werden durch kosmetische Details instabil | Tests werden teuer und rauschanfaellig | keine Zeitstempel, keine Zufalls-IDs, deterministische Struktur |
| Position von `--quiet`/`--verbose` bleibt uneinheitlich | Help-Text, Tests und Nutzeraufrufe divergieren | command-lokale Position nach `analyze`/`impact` als M5-Vertrag festlegen und testen |
| `--verbose` schreibt Zusatztext in maschinenlesbare Ausgaben | JSON-/DOT-Nutzung in CI bricht | stdout und Reportartefakte byte-stabil halten; Zusatzdiagnostik ausschliesslich nach `stderr` ausgeben |
| Release-Trigger akzeptiert ungueltige Tags | fehlerhafte Releases oder falsche `latest`-Tags entstehen | Semver-/Prerelease-Tags fail-fast validieren und `latest` nur fuer regulaere Releases setzen |
| Prerelease-Versionen kollidieren mit CMake-`project(... VERSION ...)` | CMake akzeptiert keinen Suffix, waehrend Artefakte `1.2.0-rc.1` melden sollen | numerische CMake-Projektversion von veroeffentlichter App-/Package-Version trennen und `XRAY_APP_VERSION` oder `XRAY_VERSION_SUFFIX` pruefen |
| OCI-Dry-Run nutzt nur Noop und prueft keinen Push | Tagging, `latest`, Registry-Push und Manifest-Probleme fallen erst im echten Release auf | lokale Test-Registry fuer OCI-Dry-Run verpflichtend machen; Noop nur fuer GitHub-Release-API-Schritte erlauben |
| macOS-/Windows-Artefakte werden trotz begrenzter Freigabe veroeffentlicht | Nutzer interpretieren Preview-Builds als offiziell unterstuetzte Releases | Release-Workflow entweder auf Linux/OCI begrenzen oder Plattformarchive deutlich als experimentell dokumentieren |
| GHCR-Image wird vor Abschluss aller Release-Gates gepusht | Teilveroeffentlichung ohne passenden GitHub Release bleibt zurueck | GHCR-Push erst nach Build- und Smoke-Gates ausfuehren, aber vor finaler oeffentlicher Release-Publikation |
| Oeffentlicher GitHub Release entsteht vor erfolgreichem OCI-Publish | Release zeigt ein fehlendes Container-Artefakt oder falsche Installationshinweise | GitHub Release zuerst als Draft vorbereiten, OCI-Image veroeffentlichen und erst danach den Release oeffentlich machen |
| Release-Workflow funktioniert nur fuer lokale Pfade | Artefakte sind nicht real nutzbar | entpacktes Linux-Archiv und Container-Image separat smoke-testen |
| File-API-Pfad hat keine Performance-Baseline | der erste Folge-Meilenstein nach M4 erfuellt die Performance-Dokumentation nicht | M5 misst den CMake-File-API-Pfad auf versionierten oder reproduzierbar generierten File-API-Reply-Fixtures fuer die Referenzgroessen und dokumentiert Generator, CMake-Version, Reply-Verzeichnis, `tests/reference/file-api-performance-manifest.json` und Messkommando in `docs/performance.md` |
| Versionsquellen bleiben auf `1.1.0` | ein `v1.2.0`-Release meldet intern die alte Version | numerische CMake-Projektversion, kompilierte bzw. generierte `ApplicationInfo`-Version, App-/Package-Version-Quelle, `--version` und Release-Tag gemeinsam pruefen |
| Windows-Pfade brechen Golden-Tests | Portabilitaetsziel bleibt theoretisch | Pfadanzeige und Normalisierung explizit testen; Goldens plattformrobust gestalten |

## 4. Rueckverfolgbarkeit

| Kennung | Umsetzung in M5 |
|---|---|
| `F-28` | `HtmlReportAdapter` fuer `analyze` und `impact` |
| `F-29` | `JsonReportAdapter` fuer `analyze` und `impact` |
| `F-30` | `DotReportAdapter` fuer vorhandene Analyse- und Impact-Daten |
| `F-35` | Eingabe- und Ausgabepfade, inklusive `ReportInputs`-Provenienz und Dateiausgabe ueber `--output` fuer artefaktorientierte Formate |
| `F-36` | Formatwahl fuer `html`, `json` und `dot` |
| `F-39` | `--verbose` fuer diagnoseorientierte CLI-Ausgabe |
| `F-40` | `--quiet` fuer reduzierte CLI-Ausgabe |
| `F-42` | begrenzte Analyze-Ausgabe mit `limit`, `total_count`, `returned_count` und `truncated` |
| `F-43` | versioniertes Linux-CLI-Artefakt mit Pruefsumme |
| `F-44` | OCI-kompatibles Container-Image |
| `NF-08` | macOS-/Windows-Build- und Smoke-Bewertung |
| `NF-09` | dokumentierte Compiler-/CMake-Kompatibilitaet und Smoke-Checks |
| `NF-20` | dokumentierte JSON-Formatversion |
| `NF-21` | Release- und Container-Dokumentation ohne interne Build-Pfade |
| `S-06` | HTML-Datei als Ausgabeschnittstelle |
| `S-07` | JSON als versionierte Ausgabeschnittstelle |
| `S-08` | DOT/Graphviz als Ausgabeschnittstelle |
| `S-12` | Linux-Release-Archiv |
| `S-13` | Container-Image fuer lokale Nutzung und CI |
