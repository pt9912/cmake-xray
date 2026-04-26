# Plan M5 - AP 1.3 DotReportAdapter fuer Graphviz-Ausgaben umsetzen

## Ziel

AP 1.3 implementiert den DOT-Report als visualisierungsorientierten M5-Reportadapter fuer Graphviz-Werkzeuge.

Der DOT-Export soll vorhandene Analyse- und Impact-Ergebnisse als gerichteten Graphen darstellen, ohne M6-Target-Graph-Funktionen vorwegzunehmen. AP 1.3 modelliert deshalb ausschliesslich Daten, die bereits in `AnalysisResult` und `ImpactResult` vorhanden sind: Translation Units, Include-Hotspots beziehungsweise Include-Dateien, geaenderte Dateien, betroffene Translation Units und vorhandene M4-Target-Zuordnungen.

Nach AP 1.3 kann `cmake-xray analyze --format dot` und `cmake-xray impact --format dot` produktiv genutzt werden. Die Ausgabe ist Graphviz-DOT-Quelltext, nicht gerendertes SVG oder PNG.

## Scope

Umsetzen:

- `DotReportAdapter` fuer Analyze- und Impact-Ergebnisse.
- Freischaltung von `--format dot` fuer `analyze` und `impact`.
- Freischaltung von `--output <path>` fuer `--format dot` ueber den gemeinsamen M5-Atomic-Writer.
- Deterministische DOT-IDs, Labels, Attribute, Sortierung und Kuerzung.
- Feste M5-Graph-Budgets fuer Analyze und Impact.
- Graph-Attribute `graph_node_limit`, `graph_edge_limit` und `graph_truncated` fuer alle DOT-Reports.
- Analyze-Hotspot-Kontextattribute `context_total_count`, `context_returned_count` und `context_truncated`.
- Adapter-, CLI-, Wiring-, Golden- und Syntax-Smoke-Tests.
- Nutzungsdokumentation mit mindestens einem Beispiel fuer `dot -Tsvg`.

Nicht veraendern:

- Bestehende Console-, Markdown- und JSON-Ausgaben.
- Fachliche Analyseentscheidungen in `ProjectAnalyzer` oder `ImpactAnalyzer`.
- Report-Ports so, dass Adapter CLI-Kontext erhalten.
- Impact-Ergebnislisten fuer JSON, HTML oder Markdown.
- Verbosity-Verhalten aus AP 1.5.

## Nicht umsetzen

Nicht Bestandteil von AP 1.3:

- Target-zu-Target-Kanten oder direkte Target-Graph-Analyse (`F-18`).
- Priorisierung betroffener Targets ueber den Target-Graphen hinweg (`F-25`).
- Gerenderte SVG-/PNG-Ausgaben.
- Interaktive Graphansichten.
- Konfigurierbare DOT-Budgets oder neue CLI-Optionen fuer Graphlimits.
- HTML-Renderer und HTML-spezifische Formatdetails.
- Migration von Console/Markdown auf `ReportInputs`.

Diese Punkte folgen in anderen M5-Arbeitspaketen oder bleiben bewusst ausserhalb von M5.

## Voraussetzungen aus AP 1.1 und AP 1.2

AP 1.3 baut auf folgenden AP-1.1-Vertraegen auf:

- `AnalysisResult` und `ImpactResult` enthalten `ReportInputs`.
- `ReportInputs` ist die kanonische Eingabeprovenienz fuer neue M5-Artefaktadapter.
- `AnalyzeProjectRequest` und `AnalyzeImpactRequest` transportieren Eingabepfade und `report_display_base`.
- Der gemeinsame atomare Schreibpfad fuer Reportdateien existiert und ist testbar.
- `dot` ist ein bekannter Formatwert und wird vor AP 1.3 als `recognized but not implemented` abgelehnt.
- Report-Ports bleiben ergebnisobjektzentriert.

AP 1.3 entfernt die AP-1.1-Sperre fuer `--format dot` oder begrenzt sie auf andere noch nicht implementierte Formate. `--format dot` darf nach AP 1.3 den Fehler `recognized but not implemented` nicht mehr liefern.

AP 1.2 ist keine fachliche Voraussetzung fuer DOT, aber AP 1.3 muss die in AP 1.2 geschaerften Regeln fuer deterministische Sortierung, `--top`-Begrenzung bei `analyze`, `impact` ohne `--top` und artefaktorientierte stdout-/stderr-Vertraege respektieren. Wenn AP 1.2 bereits umgesetzt ist, duerfen keine JSON-Goldens oder JSON-Schema-Gates durch AP 1.3 veraendert werden.

Falls der M5-konforme Atomic-Writer aus AP 1.1 noch nicht vorhanden oder nicht testbar ist, ist AP 1.3 insgesamt nicht abnahmefaehig. Eine teilweise Abnahme nur fuer DOT-stdout ohne DOT-Dateiausgabe ist nicht zulaessig.

## Dateien

Voraussichtlich zu aendern:

- `src/adapters/cli/cli_adapter.{h,cpp}`
- `src/main.cpp`
- `src/adapters/CMakeLists.txt`
- `tests/CMakeLists.txt`
- `tests/adapters/test_port_wiring.cpp`
- `tests/e2e/test_cli.cpp`
- `tests/e2e/run_e2e.sh`
- `docs/guide.md`
- `docs/quality.md`

Neue Dateien:

- `src/adapters/output/dot_report_adapter.h`
- `src/adapters/output/dot_report_adapter.cpp`
- `tests/adapters/test_dot_report_adapter.cpp`
- `docs/report-dot.md`
- DOT-Report-Goldens unter `tests/e2e/testdata/m5/dot-reports/`
- Golden-Manifest `tests/e2e/testdata/m5/dot-reports/manifest.txt` oder eine gleichwertige explizite Liste aller DOT-Report-Goldens
- optional `tests/validate_dot_reports.py` oder ein gleichwertiges repository-lokales DOT-Syntax-Smoke-Skript, falls Graphviz nicht in allen Testpfaden verfuegbar ist

## DOT-Dokumentvertrag

### Gemeinsame Regeln

Der DOT-Vertrag wird in `docs/report-dot.md` verbindlich dokumentiert. Dieses Dokument beschreibt:

- Reporttypen.
- Graph-, Node- und Edge-Attribute.
- Pflichtattribute.
- optionale Attribute und Weglassregeln.
- Attributtypen und exakte Lexik.
- Budget-, Sortier- und Kuerzungsregeln.
- Escaping-Regeln.
- Aenderungsregeln fuer kuenftige DOT-Vertragsaenderungen.

Allgemeine DOT-Regeln:

- DOT-Ausgaben muessen gueltiges UTF-8 und syntaktisch gueltiger Graphviz-DOT-Quelltext sein.
- Der Top-Level-Graph ist ein gerichteter Graph (`digraph`).
- Analyze verwendet den Graphnamen `cmake_xray_analysis`.
- Impact verwendet den Graphnamen `cmake_xray_impact`.
- Der Graph ist deterministisch: gleiche Eingabedaten und gleicher wirksamer `--top`-Wert erzeugen byte-stabile DOT-Ausgabe.
- Datums-, Laufzeit-, Hostnamen- oder Build-Umgebungsinformationen duerfen nicht automatisch erscheinen.
- Kommentare duerfen nur zusaetzlich erscheinen und sind nicht Teil des stabilen Vertrags. Golden-Tests duerfen nicht auf Kommentare angewiesen sein.
- Alle fachlich relevanten Informationen stehen in Graph-, Node- oder Edge-Attributen, nicht ausschliesslich in Kommentaren.
- DOT erzeugt keine JSON-, HTML- oder Markdown-spezifischen Metadaten.
- DOT darf keine Target-zu-Target-Kanten erzeugen, solange `F-18` nicht umgesetzt ist.

### Attributlexik und Escaping

M5 legt eine exakte Attributlexik fest:

- Integer werden als unquoted ASCII-Dezimalzahlen ohne Vorzeichen geschrieben.
- Booleans werden als unquoted lowercase `true` oder `false` geschrieben.
- Strings werden als quoted DOT-Strings geschrieben.
- Stringwerte escapen mindestens Backslash, Anfuehrungszeichen, Newline, Carriage Return und Tab deterministisch.
- Unbekannte Steuerzeichen werden deterministisch escaped oder durch eine dokumentierte Ersatzsequenz ausgegeben; rohe Steuerzeichen sind in Goldens nicht zulaessig.
- Labels werden deterministisch aus dem jeweiligen Anzeige-Wert gebildet und duerfen nie die einzige Quelle fuer fachliche Identitaet sein.
- Fuer Pfadlabels wird zuerst die letzte Pfadkomponente verwendet; Trenner sind `/` und `\`. Wenn keine letzte Komponente bestimmbar ist, wird der vollstaendige Anzeige-Pfad verwendet.
- Fuer Target-Labels wird `TargetInfo::display_name` verwendet.
- Labels mit hoechstens 48 Zeichen werden unveraendert ausgegeben.
- Labels mit mehr als 48 Zeichen werden per Middle-Truncation auf exakt 48 Zeichen gekuerzt: die ersten 22 Zeichen, dann `...`, dann die letzten 23 Zeichen.
- Gekuerzte Labels verwenden ausschliesslich ASCII, keine Unicode-Ellipse.
- Vollstaendige Pfade, Namen, Directories, Typen und Schluessel bleiben in den jeweiligen `path`-, `name`-, `directory`-, `type`- und `unique_key`-Attributen unverkuerzt erhalten.

DOT-IDs:

- Node-IDs sind stabile ASCII-IDs und keine rohen Pfade.
- Der Adapter bildet IDs aus Knotentyp und stabiler Position nach der dokumentierten Sortier- und Budgetlogik, zum Beispiel `tu_1`, `hotspot_1`, `target_1` und `changed_file`.
- Edge-Ausgaben referenzieren ausschliesslich diese stabilen IDs.
- Rohe nutzergelieferte Pfade, Target-Namen und Diagnostics duerfen nie unescaped als ID erscheinen.
- Innerhalb eines Knotentyps wird nach fachlicher Identitaet dedupliziert: Translation Units nach `TranslationUnitReference::unique_key`, Include-Hotspots nach Anzeige-Pfad, Targets nach `TargetInfo::unique_key`, geaenderte Datei als einzelner Impact-Knoten.

Statement-Reihenfolge:

- Die Ausgabe beginnt mit der `digraph`-Zeile.
- Danach folgen Graph-Attribute in exakt dieser Reihenfolge: `xray_report_type`, `format_version`, `graph_node_limit`, `graph_edge_limit`, `graph_truncated`.
- Danach folgen Node-Statements in der fuer Analyze beziehungsweise Impact dokumentierten Knotenprioritaet.
- Danach folgen Edge-Statements in der fuer Analyze beziehungsweise Impact dokumentierten Kantenprioritaet.
- Die Ausgabe endet mit der schliessenden Graph-Klammer.

Attribut-Reihenfolge innerhalb von Statements:

- Translation-Unit-Knoten: `kind`, optional `impact`, optional `rank`, optional `context_only`, `label`, `path`, `directory`, `unique_key`.
- Include-Hotspot-Knoten: `kind`, `label`, `path`, `context_total_count`, `context_returned_count`, `context_truncated`.
- Target-Knoten: `kind`, optional `impact`, `label`, `name`, `type`, `unique_key`.
- Changed-File-Knoten: `kind`, `label`, `path`.
- Kanten: `kind`, optional `style`.
- Optionale Attribute werden nur ausgegeben, wenn sie fuer den jeweiligen Knoten- oder Kantentyp definiert sind; wenn sie erscheinen, behalten sie die oben festgelegte Position.

### Gemeinsame Graph-Attribute

Jeder DOT-Report enthaelt mindestens:

- `xray_report_type`: quoted String, `analyze` oder `impact`.
- `format_version`: Integer, initial `1`.
- `graph_node_limit`: Integer.
- `graph_edge_limit`: Integer.
- `graph_truncated`: Boolean.

Regeln:

- `format_version` verwendet denselben initialen Wert wie `xray::hexagon::model::kReportFormatVersion`, damit DOT-Consumer M5-DOT von kuenftigen Vertragsversionen unterscheiden koennen.
- `graph_node_limit` und `graph_edge_limit` geben die fuer diesen Report wirksamen Budgets aus.
- `graph_truncated=false` wird auch bei ungekuerztem Graph verpflichtend ausgegeben.
- `graph_truncated=true`, wenn ein Kandidatenknoten oder eine Kandidatenkante wegen `node_limit`, `edge_limit` oder Analyze-`context_limit` nicht im finalen Graph enthalten ist.
- `graph_truncated` bezieht sich auf den finalen Graphzustand nach Entfernen reiner, unverbundener Kontextknoten.

## Analyze-DOT-Vertrag

### Knoten

Ein Analyze-DOT-Graph enthaelt nur Knoten aus vorhandenen `AnalysisResult`-Daten:

- Translation Units aus der Top-N-Ranking-Sicht.
- Include-Hotspots beziehungsweise Include-Dateien aus der Top-N-Hotspot-Sicht.
- Target-Knoten aus vorhandenen M4-Target-Zuordnungen.
- begrenzte Kontext-Translation-Units fuer ausgegebene Include-Hotspots.

Pflichtattribute fuer Translation-Unit-Knoten:

- `kind="translation_unit"`.
- `label`: gekuerztes oder ungekuerztes Leselabel.
- `path`: vollstaendiger Anzeige-Pfad aus `TranslationUnitReference::source_path`.
- `directory`: Anzeige-Directory aus `TranslationUnitReference::directory`.
- `unique_key`: fachlicher Translation-Unit-Schluessel aus `TranslationUnitReference::unique_key`.

Optionale Translation-Unit-Attribute:

- `rank`: Integer fuer primaere Ranking-Knoten.
- `context_only`: Boolean, wenn die Translation Unit nur als Hotspot-Kontext und nicht als primaerer Ranking-Knoten enthalten ist.

Pflichtattribute fuer Include-Hotspot-Knoten:

- `kind="include_hotspot"`.
- `label`: gekuerztes oder ungekuerztes Leselabel.
- `path`: vollstaendiger Anzeige-Pfad aus `IncludeHotspot::header_path`.
- `context_total_count`: Integer.
- `context_returned_count`: Integer.
- `context_truncated`: Boolean.

Pflichtattribute fuer Target-Knoten:

- `kind="target"`.
- `label`: Target-Anzeigename.
- `name`: Target-Anzeigename aus `TargetInfo::display_name`.
- `type`: Target-Typ aus `TargetInfo::type`.
- `unique_key`: fachlicher Target-Schluessel aus `TargetInfo::unique_key`.

### Kanten

Analyze-DOT erzeugt nur folgende Kantenarten:

- Translation Unit zu Include-Hotspot, wenn der Hotspot-Kontext aus `IncludeHotspot` ableitbar ist.
- Translation Unit zu Target, wenn eine Target-Zuordnung vorhanden ist.

Pflichtattribute fuer Kanten:

- `kind`: `tu_include_hotspot` oder `tu_target`.

Regeln:

- Analyze-Kanten tragen kein `style`-Attribut.
- Analyze-Kanten tragen kein `label`-Attribut.
- Kanten werden nur ausgegeben, wenn beide Endknoten im finalen Graph enthalten sind.
- Es werden keine Include-zu-Include-Kanten erzeugt.
- Es werden keine Target-zu-Target-Kanten erzeugt.
- Es werden keine Kanten aus Daten abgeleitet, die nicht im Ergebnisobjekt vorhanden sind.

### Analyze-Budgets

Analyze-DOT verwendet den wirksamen `--top`-Wert des Analyze-Reports:

```text
context_limit = min(top_limit, 5)
node_limit = max(25, 4 * top_limit + 10)
edge_limit = max(40, 6 * top_limit + 20)
```

Regeln:

- Ohne explizites `--top` wird der wirksame Standard-Top-Wert der CLI verwendet.
- `top_limit=0` ist ein gueltiger Edge-Case: `context_limit=0`, `node_limit=25` und `edge_limit=40`; es werden keine primaeren Ranking- oder Hotspot-Knoten aufgenommen. Das Auslassen fachlicher Daten durch `top_limit=0` ist keine DOT-Budget-Kuerzung und setzt fuer sich allein nicht `graph_truncated=true`.
- `context_limit` gilt pro ausgegebenem Hotspot, erzeugt aber keinen unbegrenzten Gesamtgraphen, weil zusaetzlich `node_limit` und `edge_limit` gelten.
- Top-Ranking- und Top-Hotspot-Listen folgen derselben `--top`-Begrenzung wie Markdown, HTML und JSON.
- Der DOT-Adapter kuerzt nur die Graphsicht, nicht das zugrunde liegende `AnalysisResult`.

### Analyze-Knotenprioritaet

Knotenkandidaten werden deterministisch in dieser Reihenfolge aufgenommen:

1. primaere Top-Ranking-Translation-Units.
2. Top-Hotspot-Knoten.
3. Target-Knoten fuer Translation Units aus der Kandidatenmenge.
4. Hotspot-Kontext-Translation-Units, sortiert nach `source_path`, `directory` und `unique_key`.

Tie-Breaker:

- Translation Units nach `source_path`, `directory` und `unique_key`, wenn die fachliche Ranking-Sortierung gleich ist.
- Include-Hotspots nach Anzeige-Pfad, wenn Hotspot-Metriken gleich sind.
- Targets nach `display_name`, `type` und `unique_key`.
- Kontext-Translation-Units nach `source_path`, `directory` und `unique_key`.

Wenn `node_limit` erreicht ist, werden spaetere Kandidaten nicht aufgenommen und `graph_truncated=true` gesetzt.

### Analyze-Hotspot-Kontext

Fuer einen Top-Hotspot werden hoechstens `context_limit` der in `IncludeHotspot` gespeicherten betroffenen Translation Units als Kontextknoten ausgegeben, auch wenn diese Translation Units nicht selbst im Top-N-Ranking liegen.

Zaehlregeln:

- `context_total_count` zaehlt pro Hotspot die eindeutigen betroffenen Translation Units vor `context_limit` und vor globalem Graph-Budget.
- `context_returned_count` zaehlt pro Hotspot die eindeutigen Kontext-Translation-Units, die nach `context_limit`, `node_limit` und `edge_limit` tatsaechlich als Knoten im finalen Graph enthalten und durch eine Kontextkante mit diesem Hotspot verbunden sind.
- Wenn dieselbe Kontext-Translation-Unit mehreren Hotspots zugeordnet ist, wird sie pro Hotspot in dessen `context_*`-Werten gezaehlt, aber als Graphknoten nur einmal gegen das globale `node_limit`.
- `context_truncated=true`, wenn `context_total_count > context_returned_count`.
- Nach Anwendung des `edge_limit` werden reine Kontext-Translation-Unit-Knoten ohne verbleibende Kontextkante entfernt, sofern sie nicht zugleich primaere Ranking-Knoten oder Target-Kontextknoten sind.
- `graph_node_limit`, `graph_truncated` und `context_returned_count` beziehen sich auf diesen finalen Graphzustand.

### Analyze-Kantenprioritaet

Kantenkandidaten werden nur fuer vorhandene Endknoten gebildet und deterministisch priorisiert:

1. Translation-Unit-zu-Include-Hotspot-Kanten.
2. Translation-Unit-zu-Target-Kanten.

Innerhalb einer Prioritaetsgruppe erfolgt der Tie-Breaker lexikografisch nach stabiler Quell-ID, Ziel-ID und Kantenart.

Wenn `edge_limit` erreicht ist, werden spaetere Kandidaten nicht aufgenommen und `graph_truncated=true` gesetzt.

## Impact-DOT-Vertrag

### Knoten

Ein Impact-DOT-Graph enthaelt nur Knoten aus vorhandenen `ImpactResult`-Daten:

- geaenderte Datei.
- direkt betroffene Translation Units.
- heuristisch betroffene Translation Units.
- betroffene Targets aus vorhandenen M4-Target-Zuordnungen.

Pflichtattribute fuer den Knoten der geaenderten Datei:

- `kind="changed_file"`.
- `label`: gekuerztes oder ungekuerztes Leselabel.
- `path`: vollstaendiger Anzeige-Pfad aus `ReportInputs.changed_file`.

Regeln:

- Erfolgreiche Impact-DOT-Reports setzen `ReportInputs.changed_file` voraus.
- `changed_file_source=unresolved_file_api_source_root` ist ein File-API-Fehlerpfad und wird in AP 1.3 nicht als DOT-Graph gerendert; die CLI meldet diesen Fall als Textfehler auf `stderr`, liefert nonzero Exit und erzeugt keinen DOT-Fehlergraphen.

Pflichtattribute fuer Translation-Unit-Knoten:

- `kind="translation_unit"`.
- `impact`: `direct` oder `heuristic`.
- `label`: gekuerztes oder ungekuerztes Leselabel.
- `path`: vollstaendiger Anzeige-Pfad aus `TranslationUnitReference::source_path`.
- `directory`: Anzeige-Directory aus `TranslationUnitReference::directory`.
- `unique_key`: fachlicher Translation-Unit-Schluessel aus `TranslationUnitReference::unique_key`.

Pflichtattribute fuer Target-Knoten:

- `kind="target"`.
- `impact`: `direct` oder `heuristic`, wenn der Target-Status eindeutig aus `ImpactResult` ableitbar ist.
- `label`: Target-Anzeigename.
- `name`: Target-Anzeigename aus `TargetInfo::display_name`.
- `type`: Target-Typ aus `TargetInfo::type`.
- `unique_key`: fachlicher Target-Schluessel aus `TargetInfo::unique_key`.

Wenn ein Target sowohl direkt als auch heuristisch betroffen ist, gewinnt `impact="direct"` fuer den Target-Knoten. Der Adapter darf daraus keine zusaetzliche fachliche Priorisierung ableiten; es ist nur eine Darstellungsregel.

### Kanten

Impact-DOT erzeugt nur folgende Kantenarten:

- geaenderte Datei zu direkt betroffener Translation Unit.
- geaenderte Datei zu heuristisch betroffener Translation Unit mit unterscheidbarem Style.
- betroffene Translation Unit zu Target.

Pflichtattribute fuer Kanten:

- `kind`: `changed_file_direct_tu`, `changed_file_heuristic_tu`, `direct_tu_target` oder `heuristic_tu_target`.

Style-Regeln:

- Direkte Impact-Kanten muessen sich von heuristischen Kanten unterscheiden.
- Die Unterscheidung erfolgt ueber das stabile DOT-Attribut `style`.
- Kanten mit `kind="changed_file_direct_tu"` und `kind="direct_tu_target"` tragen verpflichtend `style="solid"`.
- Kanten mit `kind="changed_file_heuristic_tu"` und `kind="heuristic_tu_target"` tragen verpflichtend `style="dashed"`.
- Style-Attribute sind Teil des Golden-Vertrags und duerfen nicht zufaellig oder Graphviz-versionabhaengig sein.

Regeln:

- Impact-Kanten tragen kein `label`-Attribut.
- Kanten werden nur ausgegeben, wenn beide Endknoten im finalen Graph enthalten sind.
- Es werden keine Target-zu-Target-Kanten erzeugt.
- Es werden keine Include-Kanten in Impact-DOT erzeugt, solange sie nicht explizit im `ImpactResult` vorhanden sind.

### Impact-Budgets

Impact-DOT verwendet feste M5-Budgets:

```text
node_limit = 100
edge_limit = 200
```

Regeln:

- `impact` erhaelt in AP 1.3 keine `--top`-Option.
- Das zugrunde liegende `ImpactResult` bleibt unbegrenzt.
- JSON, HTML und Markdown duerfen aus der DOT-Budgetierung keine Impact-Listenkuerzung uebernehmen.
- DOT kennzeichnet jede Kuerzung ueber `graph_truncated=true`.

### Impact-Knotenprioritaet

Knotenkandidaten werden deterministisch in dieser Reihenfolge aufgenommen:

1. geaenderte Datei.
2. direkt betroffene Translation Units, sortiert nach `source_path`, `directory` und `unique_key`.
3. heuristisch betroffene Translation Units, sortiert nach `source_path`, `directory` und `unique_key`.
4. Target-Knoten, sortiert nach `display_name`, `type` und `unique_key`.

Wenn `node_limit` erreicht ist, werden spaetere Kandidaten nicht aufgenommen und `graph_truncated=true` gesetzt.

### Impact-Kantenprioritaet

Kantenkandidaten werden nur fuer vorhandene Endknoten gebildet und deterministisch priorisiert:

1. geaenderte-Datei-zu-direkt-betroffener-Translation-Unit-Kanten.
2. geaenderte-Datei-zu-heuristisch-betroffener-Translation-Unit-Kanten.
3. direkt-betroffene-Translation-Unit-zu-Target-Kanten.
4. heuristisch-betroffene-Translation-Unit-zu-Target-Kanten.

Innerhalb einer Prioritaetsgruppe erfolgt der Tie-Breaker lexikografisch nach stabiler Quell-ID, Ziel-ID und Kantenart.

Wenn `edge_limit` erreicht ist, werden spaetere Kandidaten nicht aufgenommen und `graph_truncated=true` gesetzt.

## Adapter- und Port-Grenzen

`DotReportAdapter` rendert fachliche Inhalte ausschliesslich aus `AnalysisResult` und `ImpactResult`.

Regeln:

- Der Adapter bekommt keinen CLI-Kontext.
- Der Adapter verwendet `ReportInputs` als Eingabeprovenienz, wo Provenienz fuer DOT-Attribute benoetigt wird.
- Der Adapter nutzt nicht die Legacy-Presentation-Felder fuer Console/Markdown als kanonische Quelle fuer M5-Provenienz.
- Fuer Analyze darf der bestehende Renderparameter `top_limit` beziehungsweise `write_analysis_report(result, top_limit)` genutzt werden, solange er nur die Berichtssicht begrenzt. Gemeint ist der von der CLI bereits validierte und wirksame Top-Wert.
- `impact` bekommt keinen Top-Limit-Parameter.
- Der Adapter fuehrt keine JSON-, HTML- oder Markdown-spezifischen Metadaten ein.
- Der Adapter trifft keine neuen Impact-, Ranking- oder Target-Priorisierungsentscheidungen.
- Report-Ports bleiben ergebnisobjektzentriert.

Hilfsfunktionen:

- Gemeinsame DOT-Escaping-Hilfen bleiben lokal im DOT-Adapter oder in einer kleinen output-internen Utility, falls spaeter HTML/JSON nicht davon profitieren.
- Gemeinsame Sortier- oder Label-Hilfen duerfen nur extrahiert werden, wenn sie bereits von mehreren Adaptern genutzt werden oder echte Dopplung reduzieren.
- Neue Hilfsfunktionen duerfen keine Prozess-CWD-, Host- oder CLI-Zustaende lesen.

## CLI-/Output-Vertrag

CLI-Regeln:

- `--format dot` ist fuer `analyze` und `impact` lauffaehig.
- `--format dot --output <path>` schreibt in eine Datei und nutzt den gemeinsamen M5-konformen Atomic-Writer aus AP 1.1.
- Erfolgreiche `--format dot`-Aufrufe ohne `--output` schreiben ausschliesslich DOT-Quelltext auf stdout.
- Erfolgreiche `--format dot`-Aufrufe ohne `--output` lassen stderr leer.
- Erfolgreiche `--format dot --output <path>`-Aufrufe schreiben ausschliesslich in die Zieldatei.
- Erfolgreiche `--format dot --output <path>`-Aufrufe lassen stdout und stderr leer.
- Reportinhalt wird bei `--output` nicht zusaetzlich auf stdout dupliziert.
- `--format dot` liefert nach AP 1.3 nicht mehr den AP-1.1-Fehler `recognized but not implemented`.
- `--format html` bleibt bis AP 1.4 als bekannter, aber nicht implementierter Wert abgewiesen, sofern AP 1.4 noch nicht umgesetzt ist.

`--top`-Regeln:

- Bei `analyze` beeinflusst `--top` die primaeren Ranking- und Hotspot-Knoten sowie die DOT-Budgetformeln.
- Bei `impact` fuehrt AP 1.3 keine neue `--top`-Semantik ein.
- Tests stellen sicher, dass `impact --top ...` weiterhin abgelehnt wird, sofern es vor AP 1.3 nicht Teil des Vertrags ist.

## Fehlervertrag

Parser- und Usage-Fehler:

- CLI-Parser-/Verwendungsfehler bleiben Textfehler auf `stderr`.
- Beispiele sind fehlende Pflichtoptionen, unbekannte Optionen und `impact --format dot` ohne `--changed-file`.
- Diese Fehler erzeugen keinen DOT-Fehlergraphen.
- Exit-Code ist ungleich `0`.

Nicht wiederherstellbare Eingabefehler:

- Fehler vor Reporterzeugung bleiben fuer `--format dot` Textfehler auf `stderr`.
- Beispiele sind nicht vorhandene Eingabedateien, ungueltige `compile_commands.json`-Dateien und ungueltige CMake-File-API-Reply-Verzeichnisse.
- Ein `ImpactResult` mit `ReportInputs.changed_file_source=unresolved_file_api_source_root` gilt fuer DOT als nicht renderbares Fehlerergebnis, weil der Changed-File-Knoten keine belastbare fachliche Basis hat.
- Diese Fehler erzeugen keinen DOT-Fehlergraphen.
- Exit-Code ist ungleich `0`.

Service-Ergebnisse mit Diagnostics:

- Service-Ergebnisse mit Diagnostics oder Teildaten koennen als regulaere DOT-Reports ausgegeben werden.
- Diagnostics werden in AP 1.3 nicht als eigene Diagnostic-Knoten modelliert, solange kein stabiler Graphvertrag dafuer definiert ist.
- Wenn Diagnostics fuer Lesbarkeit aufgenommen werden, dann nur als deterministisch escaped Graph-Kommentar oder Graph-Attribut ausserhalb des Pflichtvertrags; Golden-Tests duerfen Pflichtverhalten nicht an optionale Kommentare koppeln.

Render-, Schreib- und Output-Fehler:

- Schreib-, Render- und Output-Fehler liefern Textfehler auf `stderr`.
- Exit-Code ist ungleich `0`.
- Render-Fehler vor Emission schreiben kein partielles DOT auf stdout.
- Bei echten stdout-Transportfehlern koennen bereits geschriebene Bytes nicht zurueckgenommen werden; die CLI erkennt solche Fehler best-effort nach dem Schreiben und meldet sie als Textfehler auf `stderr`.
- Bei `--output` bleibt eine bestehende Zieldatei unveraendert.
- Render-Fehler umfassen DOT-Escaping-/Serialisierungsfehler und explizite Fehler aus der CLI-internen Render-Abstraktion.
- Tests nutzen einen werfenden oder fehlerliefernden Renderer-Stub, damit kein Adapter-spezifischer Sonderfall die Fehlergrenze umgeht.

## Graphviz- und Syntax-Validierung

DOT-Goldens muessen syntaktisch validiert werden.

Bevorzugter Pfad:

- CI- und Docker-Testumgebung stellen Graphviz bereit.
- Ein CTest-Gate rendert alle DOT-Goldens mit `dot -Tsvg`.
- Das SVG-Ergebnis wird nicht als Golden versioniert; der Test prueft nur erfolgreiche DOT-Parsing-/Rendering-Faehigkeit.

Fallback-Pfad, falls Graphviz in einem Testpfad nicht verfuegbar ist:

- Ein repository-lokaler Parser-/Syntax-Smoke prueft mindestens balancierte Graphstruktur, quoted String-Escaping, Attributlisten und Edge-/Node-Statements.
- Der Fallback darf nicht still skippen.
- Der Fallback muss dieselben Escaping-Goldens verarbeiten wie der Graphviz-Pfad.
- `docs/quality.md` dokumentiert, welcher Pfad in welcher Umgebung aktiv ist.

CTest selbst installiert keine Systempakete und greift nicht auf das Netzwerk zu. Installation von Graphviz oder Parser-Abhaengigkeiten erfolgt ausschliesslich im Bootstrap-Schritt der jeweiligen Umgebung.

## Implementierungsreihenfolge

Die Umsetzung erfolgt in drei verbindlichen Tranchen plus einer optionalen Haertungstranche. Jede Tranche endet mit einem vollstaendigen Lauf der Docker-Gates aus `README.md` ("Tests und Quality Gates") und `docs/quality.md`. Die globalen Abnahmekriterien dieses Plans gelten zusaetzlich am Ende von Tranche C.

### Tranche A - Vertrag, Hilfslogik und Syntax-Gate

Kein produktiver CLI-Adapter; Vertrag, Hilfsfunktionen und Testskelett.

1. DOT-Vertrag in `docs/report-dot.md` festlegen, einschliesslich Graphnamen, `format_version`, Attributlexik, Label-Kuerzungsalgorithmus, Statement-/Attribut-Reihenfolge, Escaping, Node-/Edge-Kinds, Budgets, Sortier-Tie-Breakern und Weglassregeln.
2. `tests/adapters/test_dot_report_adapter.cpp` mit ersten Escape-, ID- und Attributlexik-Tests anlegen.
3. DOT-Escaping- und Label-Hilfen im Adapter oder einer output-internen Utility implementieren.
4. Budgetberechnung fuer Analyze und Impact als klein testbare Funktionen implementieren.
5. CTest-Gate fuer DOT-Goldens vorbereiten: Graphviz `dot -Tsvg` oder dokumentierter Fallback-Syntax-Smoke.
6. Manifest `tests/e2e/testdata/m5/dot-reports/manifest.txt` anlegen, in Tranche A noch ohne vollstaendige CLI-Goldens.
7. `docs/quality.md` um das neue DOT-Syntax-Gate ergaenzen.

Abnahme Tranche A: alle Docker-Gates gruen; Escape-/Attributlexik-Tests pruefen Leerzeichen, Anfuehrungszeichen, Backslashes und plattformtypische Pfadtrenner.

### Tranche B - Adapter, Wiring, CLI-Freischaltung

Der Adapter funktioniert; E2E-Goldens folgen erst in Tranche C.

1. `src/adapters/output/dot_report_adapter.{h,cpp}` implementieren; Adapter rendert ausschliesslich aus `AnalysisResult`/`ImpactResult` und nutzt `top_limit` nur fuer Analyze.
2. Analyze-DOT mit Top-Ranking-, Hotspot-, Target- und Kontextknoten implementieren.
3. Impact-DOT mit geaenderter Datei, direkt/heuristisch betroffenen Translation Units und Target-Knoten implementieren.
4. Node- und Edge-Budgetierung inklusive finalem Entfernen reiner unverbundener Kontextknoten implementieren.
5. Composition Root in `src/main.cpp` und Reportports um den DOT-Port erweitern; `ReportPorts` in `src/adapters/cli/cli_adapter.h` erhaelt ein explizites `dot`-Feld oder eine gleichwertig typisierte Formatzuordnung.
6. CLI-Adapter `src/adapters/cli/cli_adapter.{h,cpp}` so anpassen, dass `--format dot` als implementiert gilt, an den DOT-Port verdrahtet wird und mit `--output` ueber den AP-1.1-Atomic-Writer schreibt.
7. AP-1.1-Sperre fuer `--format dot` entfernen; noch nicht umgesetzte Formate bleiben weiter abgewiesen.
8. `tests/adapters/test_port_wiring.cpp` so erweitern, dass `--format dot` an den `DotReportAdapter` verdrahtet ist und nicht in den Console-Fallback faellt.
9. Regressionscheck, dass bestehende Console-/Markdown-/JSON-Goldens unveraendert bleiben.

Abnahme Tranche B: alle Docker-Gates gruen; `--format dot` produziert syntaktisch gueltigen DOT-Quelltext; `recognized but not implemented` nicht mehr fuer `dot`; Console-, Markdown- und JSON-Goldens byte-stabil.

### Tranche C - E2E-Goldens, CLI-/Stream-/Fehler-Tests, Nutzerdoku

Echte Binary-Verifikation und Vertragsfestschreibung der CLI-Ausgaben.

1. Analyze-Goldens unter `tests/e2e/testdata/m5/dot-reports/` als statische Dateien anlegen. Abgedeckt sind mindestens:
   - Compile-Database-only.
   - File-API-only mit Target-Zuordnungen.
   - Mixed-Input.
   - `--top`-Fall ohne Kuerzung (`graph_truncated=false`).
   - `--top`-Fall mit `context_limit`-Kuerzung.
   - Fall mit gleichzeitig wirksamem `context_limit`, `node_limit` und `edge_limit`.
   - leeres Ergebnis mit gueltigem DOT und `graph_truncated=false`.
2. Impact-Goldens erzeugen. Abgedeckt sind mindestens:
   - Compile-Database-only mit relativem `--changed-file`.
   - File-API-only mit relativem `--changed-file`.
   - Mixed-Input.
   - direkte und heuristische Translation Units mit unterscheidbarem Style.
   - Target-Zuordnungen fuer direkte und heuristische Betroffenheit.
   - Budget-Kuerzung mit `graph_truncated=true`.
   - ungekuerzter Impact-Graph mit `graph_truncated=false`.
3. Escaping-Goldens fuer Analyze oder Impact aufnehmen, mindestens mit Leerzeichen, Anfuehrungszeichen, Backslashes und plattformtypischen Pfadtrennern.
4. Manifest aktualisieren; der DOT-Syntax-Test gleicht Verzeichnis und Manifest beidseitig ab.
5. `tests/e2e/test_cli.cpp` erweitern um:
   - DOT-stdout-Vertrag, leeres stderr, syntaktisch gueltiger DOT.
   - `--format dot --output <path>`: Datei geschrieben, stdout und stderr leer, atomar.
   - DOT-Render-Fehler ueber injizierten `CliReportRenderer`-Doppelgaenger; bestehende Zieldatei bleibt unveraendert, kein partieller DOT auf stdout.
   - Fehlerpfade: `impact --format dot` ohne `--changed-file`, nicht vorhandene Eingaben, ungueltiges `compile_commands.json`, ungueltige File-API-Reply-Daten, Schreibfehler. Alle als Text auf stderr, nonzero Exit, kein DOT-Fehlergraph.
   - Negativtest, dass `impact` keine `--top`-Option akzeptiert.
6. `tests/e2e/run_e2e.sh` und das CTest-Ziel `e2e_binary` um Binary-Smokes fuer `analyze --format dot` und mindestens einen `impact --format dot`-Fall ergaenzen, sodass die Verdrahtung inklusive `src/main.cpp` getestet ist.
7. `docs/guide.md` um produktive Nutzung von `--format dot`, `--format dot --output` und `dot -Tsvg` ergaenzen.
8. `docs/quality.md` um die in Tranche C neu hinzukommenden DOT-Golden-, Syntax- und E2E-Gates ergaenzen.
9. Coverage-, Lizard- und Clang-Tidy-Gates muessen nach AP 1.3 weiterhin gruen sein; neue Befunde aus dem DOT-Adapter werden in Tranche C behoben und nicht auf Tranche D verschoben.

Abnahme Tranche C: alle Docker-Gates gruen; `e2e_binary` gruen; alle DOT-Goldens sind syntaktisch gueltig; Budget-, Truncation-, Kontext- und Escaping-Vertraege sind durch Goldens und Adaptertests abgedeckt; globale Abnahmekriterien dieses Plans erfuellt.

### Tranche D - Optionale Haertung

Ohne diese Tranche gilt AP 1.3 als abgenommen, sobald Tranche C gruen ist.

- Zusaetzliche plattformspezifische Pfad-Edge-Cases, etwa Windows-Drives und UNC-aehnliche Strings.
- UTF-8-/Escape-Edge-Cases, etwa Unicode-Pfade und Steuerzeichen in Diagnostics, sofern das Projekt Unicode-Goldens in M5 bewusst freigibt.
- Graphviz-Smokes fuer mehrere Ausgabeformate, zum Beispiel `dot -Tsvg` und `dot -Tplain`.
- Zusaetzliche gezielte Regressionstests fuer Grenzfaelle, die ueber die verpflichtenden Coverage-, Lizard- und Clang-Tidy-Gates aus Tranche C hinausgehen.

## Entscheidungen

Diese Entscheidungen sind vor Umsetzungsbeginn getroffen und in die Tranchen eingearbeitet:

- DOT ist Graphviz-Quelltext, kein gerendertes Bild. Begruendung: Graphviz-Ausgaben sollen in CI und Dokumentation flexibel weiterverarbeitet werden; Rendering ist Tooling des Nutzers.
- Node-IDs sind adaptergenerierte ASCII-IDs statt roher Pfade. Begruendung: Pfade enthalten Sonderzeichen und Plattformtrenner; Attribute transportieren die fachliche Identitaet stabiler und testbarer.
- Budgets sind fuer M5 fest und nicht konfigurierbar. Begruendung: DOT soll grobe Visualisierung liefern, ohne neue CLI-Komplexitaet oder unbegrenzte Graphen einzufuehren.
- Analyze nutzt `top_limit` als Renderparameter. Begruendung: Das entspricht dem etablierten `GenerateReportPort::generate_analysis_report(result, top_limit)`-Vertrag und vermeidet CLI-Kontext im Adapter.
- Impact erhaelt kein `--top`. Begruendung: M5 begrenzt nur die DOT-Graphsicht; fachliche Impact-Ergebnislisten bleiben vollstaendig.
- Target-zu-Target-Kanten bleiben verboten. Begruendung: Diese Kanten waeren eine neue Target-Graph-Analyse und gehoeren zu M6 beziehungsweise `F-18`.
- Diagnostics werden nicht als Pflichtknoten modelliert. Begruendung: AP 1.3 braucht einen stabilen Graphvertrag fuer fachliche Ergebnisbeziehungen; Diagnostic-Visualisierung waere ein eigener Vertrag.

## Tests

Adaptertests:

- `tests/adapters/test_dot_report_adapter.cpp` prueft Analyze- und Impact-Serialisierung.
- Tests pruefen `digraph cmake_xray_analysis` und `digraph cmake_xray_impact`.
- Tests pruefen Pflichtattribute `xray_report_type`, `format_version`, `graph_node_limit`, `graph_edge_limit` und `graph_truncated`.
- Tests pruefen Statement-Reihenfolge: Graph-Attribute, dann Node-Statements, dann Edge-Statements.
- Tests pruefen die dokumentierte Attribut-Reihenfolge innerhalb von Graph-, Node- und Edge-Statements.
- Tests pruefen den Label-Kuerzungsalgorithmus inklusive 48-Zeichen-Grenze und Middle-Truncation.
- Tests pruefen Analyze-Hotspot-Attribute `context_total_count`, `context_returned_count` und `context_truncated`.
- Tests pruefen, dass Include-Hotspot-`path` aus `IncludeHotspot::header_path` stammt.
- Tests pruefen Integer-, Boolean- und String-Lexik exakt.
- Tests pruefen Escaping fuer Leerzeichen, Anfuehrungszeichen, Backslashes, Newline, Tab und plattformtypische Pfadtrenner.
- Tests pruefen stabile Node-ID-Erzeugung ohne rohe Pfade als IDs.
- Tests pruefen mindestens einen Tie-Breaker pro sortierter Node- und Edge-Gruppe.
- Tests pruefen, dass Translation Units mit gleichem `source_path` und unterschiedlichem `directory` beziehungsweise `unique_key` als getrennte Knoten erhalten bleiben.
- Tests pruefen, dass Targets mit gleichem `display_name` und unterschiedlichem `type` beziehungsweise `unique_key` als getrennte Knoten erhalten bleiben.
- Tests pruefen die Pflichtattribute `directory` und `unique_key` fuer Translation-Unit-Knoten.
- Tests pruefen die Pflichtattribute `type` und `unique_key` fuer Target-Knoten.
- Tests pruefen die exakten Impact-Style-Werte `style="solid"` fuer direkte Kanten und `style="dashed"` fuer heuristische Kanten.
- Tests pruefen, dass Analyze-Kanten kein `style`-Attribut tragen und keine Kanten ein `label`-Attribut traegt.
- Tests pruefen, dass `changed_file_source=unresolved_file_api_source_root` fuer DOT als Textfehler ohne DOT-Graph behandelt wird.
- Tests pruefen leere Analyze- und Impact-Ergebnisse als gueltigen DOT-Graph.

Budget- und Truncation-Tests:

- Analyze-Tests pruefen `context_limit = min(top_limit, 5)`.
- Analyze-Tests pruefen `node_limit = max(25, 4 * top_limit + 10)`.
- Analyze-Tests pruefen `edge_limit = max(40, 6 * top_limit + 20)`.
- Analyze-Tests pruefen den Edge-Case `top_limit=0` mit `context_limit=0`, `node_limit=25`, `edge_limit=40` und ohne primaere Ranking-/Hotspot-Knoten.
- Impact-Tests pruefen `node_limit = 100` und `edge_limit = 200`.
- Tests pruefen `graph_truncated=false` im ungekuerzten Fall.
- Tests pruefen `graph_truncated=true`, wenn Knoten oder Kanten wegen Budget fehlen.
- Tests pruefen, dass reine Kontext-Translation-Unit-Knoten ohne verbleibende Kontextkante final entfernt werden.
- Tests pruefen mehrfach referenzierte Kontext-Translation-Units und korrekte `context_returned_count`-Zaehlung pro Hotspot.

Port- und Wiring-Tests:

- `tests/adapters/test_port_wiring.cpp` prueft, dass `dot` an den `DotReportAdapter` verdrahtet ist.
- Port-/Wiring-Tests pruefen ausdruecklich, dass `--format dot` nicht in den bestehenden Console-Fallback faellt und keinen Console-Adapter nutzt.
- Tests pruefen, dass `--format dot` nach AP 1.3 nicht mehr den AP-1.1-Fehler `recognized but not implemented` liefert.
- Binary-Smoke-Tests ueber `tests/e2e/run_e2e.sh` und das CTest-Ziel `e2e_binary` pruefen, dass auch die echte Binary inklusive `src/main.cpp` den DOT-Adapter verdrahtet.

E2E-Golden-Tests:

- E2E-Tests erzeugen DOT-Ausgaben mit der echten CLI.
- Die erzeugten DOT-Reports werden byte-stabil gegen Golden-Dateien verglichen.
- Dieselben erzeugten Reports werden durch Graphviz `dot -Tsvg` oder den dokumentierten Syntax-Smoke validiert.
- Reine Adaptertests reichen nicht aus.
- DOT-Goldens decken Compile-Database-only-, File-API-only- und Mixed-Input-Laeufe bei `analyze` ab.
- DOT-Goldens decken Compile-Database-only-, File-API-only- und Mixed-Input-Laeufe bei `impact` ab.
- Analyze-Goldens pruefen Top-Ranking-, Hotspot-, Target- und Kontextknoten.
- Analyze-Goldens pruefen `context_limit`-, `node_limit`- und `edge_limit`-Kuerzung.
- Impact-Goldens pruefen direkte und heuristische Kantenstile.
- Impact-Goldens pruefen feste Impact-Budgets und `graph_truncated`.
- Escaping-Goldens pruefen Sonderzeichen in IDs, Labels und Attributwerten.

CLI- und Stream-Tests:

- `analyze --format dot` liefert gueltigen DOT-Quelltext auf stdout und leeres stderr.
- `impact --format dot` liefert gueltigen DOT-Quelltext auf stdout und leeres stderr.
- Binary-E2E-Smokes fuehren `analyze --format dot` und mindestens einen `impact --format dot`-Fall ueber die gebaute `cmake-xray`-Binary aus, nicht nur ueber direkt instanziierte CLI-Adaptertests.
- `--format dot --output <path>` schreibt gueltigen DOT-Quelltext in die Datei.
- Erfolgreiche `--format dot --output <path>`-Aufrufe lassen stdout und stderr leer.
- Tests pruefen, dass `--format dot` tatsaechlich DOT rendert und nicht den Console-Reportpfad trifft.
- Tests pruefen, dass `impact --format dot --top ...` abgelehnt wird, solange `impact --top` nicht Teil des M5-Vertrags ist.

Fehlerpfad-Tests:

- `impact --format dot` ohne `--changed-file` liefert Text auf stderr, nonzero Exit und keinen DOT-Fehlergraphen.
- Nicht vorhandene Eingabepfade liefern Text auf stderr, nonzero Exit und keinen DOT-Fehlergraphen.
- Ungueltiges Compile-Database-JSON liefert Text auf stderr, nonzero Exit und keinen DOT-Fehlergraphen.
- Ungueltige CMake-File-API-Reply-Daten liefern Text auf stderr, nonzero Exit und keinen DOT-Fehlergraphen.
- `changed_file_source=unresolved_file_api_source_root` liefert Text auf stderr, nonzero Exit und keinen DOT-Fehlergraphen.
- Schreibfehler liefern Text auf stderr, nonzero Exit und keinen DOT-Fehlergraphen.
- CLI-/Schreibpfad-Tests decken einen simulierten DOT-Render-Fehler vor dem Atomic-Writer ab.
- Der simulierte Render-Fehler prueft nonzero Exit, Text auf stderr, leeres stdout und unveraenderte bestehende Zieldatei bei `--output`.

Atomic-Writer-Tests:

- DOT-`--output` nutzt den gemeinsamen M5-konformen Atomic-Writer aus AP 1.1.
- Tests pruefen mindestens, dass ein simulierter Schreib-/Replace-Fehler eine bestehende DOT-Zieldatei unveraendert laesst.

Rueckwaertskompatibilitaets-Tests:

- Bestehende Console-/Markdown-/JSON-Goldens bleiben unveraendert.
- DOT-Freischaltung fuehrt keine Verbosity-Parameter in Reportports oder Artefaktadapter ein.
- DOT-Freischaltung fuehrt keine Target-Graph-Semantik ein.

## Abnahmekriterien

AP 1.3 ist abnahmefaehig, wenn:

- `cmake-xray analyze --format dot` und `cmake-xray impact --format dot` lauffaehig sind.
- `--format dot --output <path>` ueber den gemeinsamen Atomic-Writer schreibt.
- Erfolgreiche DOT-Stdout-Ausgaben keine unstrukturierten Zusatztexte enthalten.
- Erfolgreiche DOT-Dateiausgaben stdout und stderr leer lassen.
- `docs/report-dot.md` vorhanden ist und den DOT-Vertrag dokumentiert.
- Analyze-DOT die Knotenarten Translation Unit, Include-Hotspot und Target abbildet, sofern diese Daten vorhanden sind.
- Impact-DOT die Knotenarten geaenderte Datei, direkt betroffene Translation Unit, heuristisch betroffene Translation Unit und Target abbildet, sofern diese Daten vorhanden sind.
- Keine Target-zu-Target-Kanten erzeugt werden.
- Alle DOT-Reports die Graph-Attribute `graph_node_limit`, `graph_edge_limit` und `graph_truncated` enthalten.
- Alle DOT-Reports das Graph-Attribut `format_version=1` enthalten.
- Statement- und Attribut-Reihenfolge byte-stabil und dokumentiert sind.
- Label-Kuerzung byte-stabil nach dem dokumentierten 48-Zeichen-Middle-Truncation-Algorithmus erfolgt.
- Analyze-Hotspot-Knoten die Attribute `context_total_count`, `context_returned_count` und `context_truncated` enthalten.
- Analyze-Hotspot-Knoten `path` aus `IncludeHotspot::header_path` ausgeben.
- Analyze-DOT die festen M5-Formeln fuer `context_limit`, `node_limit` und `edge_limit` verwendet.
- Impact-DOT die festen M5-Budgets `node_limit = 100` und `edge_limit = 200` verwendet.
- Knoten- und Kantenkuerzung deterministisch ist und `graph_truncated` korrekt setzt.
- DOT-IDs, Labels und Attributwerte deterministisch und korrekt escaped sind.
- Translation Units nach `TranslationUnitReference::unique_key` und Targets nach `TargetInfo::unique_key` dedupliziert werden, ohne gleichnamige Modellobjekte zusammenzuklappen.
- Translation-Unit-Knoten `directory` und `unique_key` ausgeben.
- Target-Knoten `type` und `unique_key` ausgeben.
- Direkte und heuristische Impact-Kanten die exakt dokumentierten `style`-Attribute tragen.
- Leere Ergebnisse gueltigen DOT-Quelltext erzeugen.
- Echte CLI-Ausgaben gegen Goldens verglichen und anschliessend syntaktisch validiert werden.
- Binary-E2E-Smokes die DOT-Verdrahtung der echten Binary inklusive `src/main.cpp` abdecken.
- Parser-, Eingabe-, Render- und Schreibfehler als Textfehler ohne DOT-Fehlergraph getestet sind.
- `docs/guide.md` die produktive DOT-Nutzung und ein `dot -Tsvg`-Beispiel beschreibt.
- `docs/quality.md` die neuen DOT-Syntax-, Golden- und E2E-Gates auffuehrt.
- Coverage-, Lizard- und Clang-Tidy-Gates nach AP 1.3 gruen bleiben.
- Console-, Markdown- und JSON-Verhalten unveraendert bleibt.

## Offene Folgearbeiten

Folgearbeiten ausserhalb von AP 1.3:

- AP 1.4 implementiert HTML-Rendering und HTML-spezifische Ausgabe-/Escaping-Regeln.
- AP 1.5 definiert Quiet-/Verbose-Verhalten.
- M6 kann Target-zu-Target-Kanten einfuehren, wenn `F-18` umgesetzt und fachlich dokumentiert ist.
- Eine spaetere DOT-Vertragsversion kann konfigurierbare Budgets, Diagnostic-Knoten oder weitere Graphattribute einfuehren, muss dann aber `docs/report-dot.md`, Goldens und Tests entsprechend migrieren.

**Ergebnis**: Nutzer koennen vorhandene Analyse- und Impact-Ergebnisse in Graphviz-Werkzeuge ueberfuehren, ohne dass M5 eine neue fachliche Graphanalyse einfuehrt.
