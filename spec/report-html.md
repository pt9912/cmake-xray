# HTML Report Contract

> **Implementierungsstand:** AP M5-1.4 Tranche A liefert den HTML-Vertrag, das statische CSS, file-locale Escaping- und Whitespace-Hilfen und das Adapter-Testskelett. Tranche B implementiert den produktiven `HtmlReportAdapter` und schaltet `--format html` an der CLI frei. Bis dahin lehnt die CLI `--format html` weiterhin als `recognized but not implemented in this build` ab. Die in diesem Dokument festgehaltenen Regeln sind ab Tranche A bindend; folgende Tranchen erweitern den Vertrag nicht ohne Update dieses Dokuments.
>
> **M6 AP 1.2 Tranche A.3:** `format_version` steigt auf `2`. Analyze-HTML traegt zwei zusaetzliche Pflichtsections `Target Graph` und `Target Hubs` zwischen `Target Metadata` und `Diagnostics`. Impact-HTML traegt eine zusaetzliche Pflichtsection `Target Graph Reference` zwischen `Heuristically Affected Targets` und `Diagnostics`. Beide Reporttypen behalten die neuen Sections auch bei `target_graph_status="not_loaded"` mit `h2` und sichtbarem Leersatz, analog zu `Target Metadata` aus M5.
>
> **M6 AP 1.3 Tranche A.4:** `format_version` steigt auf `3`. Impact-HTML traegt eine zusaetzliche Pflichtsection `Prioritised Affected Targets` zwischen `Target Graph Reference` und `Diagnostics`; sie zeigt die Reverse-BFS-priorisierte Sicht ueber den Target-Graphen mit Tiefen-Metadaten. Analyze-HTML aendert sich nicht (Reverse-BFS gilt nur fuer Impact). Die Section bleibt bei `target_graph_status="not_loaded"` mit `h2` plus Tiefen-Header und Fallback-Absatz erhalten.
>
> **M6 AP 1.4 Tranche A.5:** `format_version` steigt auf `4`. Analyze-HTML erweitert die `Include Hotspots`-Section um eine Pflicht-Filter-Zeile (`<p class="include-filter">`) zwischen `<h2>` und Tabelle/Empty-Marker, eine optionale Budget-Note (`<p class="include-budget-note">`) bei `include_node_budget_reached=true`, und zwei neue Pflichtspalten `Origin` und `Depth` in der Hotspot-Tabelle. Sechs neue CSS-Badge-Klassen (`badge--project`, `badge--external`, `badge--unknown`, `badge--direct`, `badge--indirect`, `badge--mixed`) landen im inline-CSS-Block. Die Filter-Zeile spiegelt Analyse-Konfiguration, nicht Ergebnis, und erscheint auch im Empty-Hotspot-Fall. Impact-HTML aendert sich strukturell nicht; die `format_version`-Anhebung dokumentiert nur die Analyze-Erweiterung. Konsole- und Markdown-Pendants leben in `docs/planning/in-progress/plan-M6-1-4.md` (Steps 22/23) und tragen denselben Filter-/Origin-/Depth-Datenstrom auf format-spezifischer Praesentation.
>
> **M6 AP 1.5 Tranche A.5:** `format_version` steigt auf `5`. Analyze-HTML traegt eine neue Pflichtsection `<section class="analysis-configuration">` zwischen `Inputs` und `Translation Unit Ranking` mit einem `<dl>` fuer die vier Konfigurationswerte (`Sections`, `TU thresholds`, `Min hotspot TUs`, `Target hub thresholds`) und einer Untersection `<h3>Section States</h3>` mit einem zweiten `<dl class="section-states">` fuer die vier Sections und ihren `AnalysisSectionState`. Jede der vier fachlichen Sections (`Translation Unit Ranking`, `Include Hotspots`, `Target Graph`, `Target Hubs`) traegt ab v5 inline im `<h2>` ein Section-State-Badge `<span class="badge badge--state-<state>">Status: <state></span>` mit `<state>` einer von `active`, `disabled`, `not_loaded`. Wenn `analysis_section_states[<section>]` `disabled` oder `not_loaded` ist, ersetzt die Section ihren Body durch genau einen Absatz `<p class="empty">Section disabled.</p>` bzw. `<p class="empty">Section not loaded.</p>`; die Section bleibt mit `<h2>` und Status-Badge erhalten. Die bis AP 1.2 in `Target Graph` und `Target Hubs` getragenen Section-spezifischen Leersaetze `Target graph not loaded.` und `Target hubs not available.` entfallen im Analyze-HTML zugunsten dieses unifizierten Markers. Drei neue CSS-Badge-Modifier-Klassen `badge--state-active`, `badge--state-disabled`, `badge--state-not-loaded` ergaenzen den CSS-Vertrag; sie sind reine Modifier-Markierungen und tragen keine erzwungenen Farben. `Target Hubs` liest seine `Incoming threshold` / `Outgoing threshold`-Werte ab v5 aus `analysis_configuration.target_hub_in_threshold` bzw. `target_hub_out_threshold` (CLI-konfigurierbar), nicht mehr aus den Default-Konstanten. Impact-HTML aendert sich strukturell nicht; insbesondere bleibt die `Target Graph Reference`-Section bei `target_graph_status="not_loaded"` mit dem Legacy-Leersatz `Target graph not loaded.`, weil Impact keine `analysis_section_states`-Surface fuehrt. Konsole- und Markdown-Pendants leben in `docs/planning/in-progress/plan-M6-1-5.md` (Steps 18a/18b) und tragen denselben Konfigurations-/Section-State-Datenstrom auf format-spezifischer Praesentation.

## Zweck

Dieses Dokument ist der verbindliche Vertrag fuer den HTML-Report von `cmake-xray`. Er wird durch Adapter-Unit-Tests (`tests/adapters/test_html_report_adapter.cpp`) und ab Tranche C durch Binary-E2E-Goldens unter `tests/e2e/testdata/m5/html-reports/` byte-stabil festgehalten.

HTML-Reports sind statische Ein-Datei-Artefakte, gedacht fuer Reviews, CI-Artefakte und Offline-Dokumentation. Sie enthalten kein JavaScript, keine externen Ressourcen und keine Buildzeit-/Hostdaten ausserhalb der fachlichen Ergebnisdaten.

## Geltungsbereich

- `cmake-xray analyze --format html`
- `cmake-xray impact --format html`
- `cmake-xray analyze|impact --format html --output <path>` ueber den AP-1.1-Atomic-Writer.

## Allgemeine Regeln

- Ausgaben sind gueltiges UTF-8 und ein vollstaendiges HTML5-Dokument mit `<!doctype html>`, `html`, `head` und `body`.
- `html` traegt `lang="en"`, weil Feldnamen, Section-Titel und Reportsprache englisch sind.
- Genau ein `main`-Element, genau ein `h1`.
- CSS ausschliesslich inline ueber genau ein `style`-Element im `head`.
- Keine `script`-Elemente, keine `iframe`, keine `link rel="stylesheet"`, keine `@import`-Regeln, keine `url(...)`-Ressourcen, keine externen Fonts oder Bilder.
- Keine HTML-Kommentare.
- Keine Datums-, Hostnamen-, Buildpfad- oder Zeitfelder.
- Alle fachlichen Werte werden HTML-escaped (siehe unten); rohe HTML-Tags aus Pfaden, Diagnostics oder Target-Namen werden nie interpretiert.

## Dokumentgeruest

Das Geruest ist fuer Analyze und Impact identisch:

```html
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="xray-report-format-version" content="5">
  <title>cmake-xray analyze report</title>
  <style>...</style>
</head>
<body>
  <main class="report" data-report-type="analyze" data-format-version="5">
    ...
  </main>
</body>
</html>
```

Regeln:

- `meta[name="xray-report-format-version"]` und `main[data-format-version]` verwenden den Wert von `xray::hexagon::model::kReportFormatVersion`.
- `title` ist `cmake-xray analyze report` oder `cmake-xray impact report`.
- `main[data-report-type]` ist `analyze` oder `impact`.
- Das `style`-Element ist byte-stabil und fuer Analyze und Impact identisch.
- `head`-Reihenfolge: `meta charset`, `meta viewport`, `meta xray-report-format-version`, `title`, `style`.
- Keine optionalen Meta-Tags fuer Generator, Zeit, Host, Commit oder Buildpfad.

## CSS-Vertrag

- CSS ist statisch und im Adapter als deterministischer String hinterlegt; identisch fuer Analyze und Impact.
- Keine zufaelligen Werte, generierten Klassen oder Buildzeitdaten.
- Keine `@import`, kein `url(...)`, keine Animationen, keine externen Fonts.
- System-Fontstack erlaubt: `system-ui, -apple-system, "Segoe UI", Roboto, Helvetica, Arial, sans-serif` fuer Body, `ui-monospace, "SFMono-Regular", "Menlo", "Consolas", monospace` fuer Codeartiges.
- Hintergrundfarbe Body `#ffffff`, Vordergrundfarbe `#1a1a1a`. Kontrastverhaeltnis &ge; 4.5:1 (WCAG AA fuer Fliesstext).
- Status-Badges nutzen Hintergrund + Text in Kombination, nie nur Farbe; sichtbarer Text wie `direct`, `heuristic`, `loaded`, `partial`, `not_loaded`, `warning`, `note` bleibt erhalten.
- M6 AP 1.4 ergaenzt sechs CSS-Klassen `badge--project`, `badge--external`, `badge--unknown`, `badge--direct`, `badge--indirect`, `badge--mixed` als Vertragsbestandteil. Sie tragen die Origin- und Depth-Werte der `Include Hotspots`-Tabelle sowie die Filter-Zeilen-Badges (`scope=<span class="badge badge--<scope>">…`, `depth=<span class="badge badge--<depth>">…`) ein. M6 AP 1.4 A.5 Step 25c hat zusaetzlich die beiden M5-Impact-Evidenz-Klassen auf die gleiche BEM-Doppeldash-Notation gehoben: `badge-direct` → `badge--evidence-direct`, `badge-heuristic` → `badge--evidence-heuristic`. Damit folgen alle Badge-Klassen einheitlich der `badge--<X>`-Form; die bis dahin bestehende Single-/Doppel-Dash-Naming-Asymmetrie ist beseitigt.
- M6 AP 1.5 ergaenzt drei CSS-Modifier-Klassen `badge--state-active`, `badge--state-disabled`, `badge--state-not-loaded` als Vertragsbestandteil. Sie kennzeichnen das Section-State-Badge `<span class="badge badge--state-<state>">Status: <state></span>` im `<h2>` jeder der vier fachlichen Analyze-Sections (`Translation Unit Ranking`, `Include Hotspots`, `Target Graph`, `Target Hubs`). Die Klassen sind reine Modifier-Markierungen; das Base-`.badge`-Styling liefert die Affordanz, und das textbasierte Section-State-Label (`active` / `disabled` / `not_loaded`) traegt die fachliche Information unabhaengig von der CSS-Farbe.
- Tabellen sind auf dem Bildschirm horizontal scrollbar (`overflow-x: auto`), ohne dass das Seitenlayout bricht. Im Drucklayout setzt der Adapter `.table-wrap { overflow-x: visible }`, damit gedruckte Tabellen nicht abgeschnitten werden.
- Drucklayout (`@media print`) entfernt Hintergrundfarben und reduziert Schriftfarbkontrast nicht unter Lesbarkeit.
- Eine `@page { margin: 1.5cm }`-Regel setzt deterministische Druckraender; weitere Druckregeln sind ausschliesslich im Adapter-CSS hinterlegt und enthalten keine zufaelligen oder dynamischen Werte.
- Im Drucklayout gilt `thead { display: table-header-group }` (Tabellenkopf wiederholt sich auf jeder gedruckten Seite) sowie `tr { break-inside: avoid; page-break-inside: avoid }` (Tabellenzeilen werden nicht ueber Seitenwechsel zerschnitten). Beide Regeln sind reine Render-Hints; Tabellenstruktur, Spalten- und Zeilenreihenfolge bleiben unveraendert und byte-stabil.

CSS-Klassen stammen ausschliesslich aus festen Adapter-Literalen. Nutzergelieferte Inhalte erscheinen nie in `id`-Attributen, Klassennamen oder Inline-`style`-Attributen.

## Escaping

Textknoten werden mindestens fuer `&`, `<`, `>` HTML-escaped:

| Eingabe | Ausgabe |
|---|---|
| `&` | `&amp;` |
| `<` | `&lt;` |
| `>` | `&gt;` |

Attributwerte werden zusaetzlich fuer `"` und `'` escaped:

| Eingabe | Ausgabe |
|---|---|
| `"` | `&quot;` |
| `'` | `&#39;` |

Whitespace in fachlichen Textwerten wird vor dem HTML-Escaping einheitlich normalisiert:

- `\r\n` und `\r` werden zu `\n`.
- Jedes `\n` wird als sichtbarer Separator ` / ` ausgegeben.
- Jedes `\t` wird zu einem Leerzeichen.
- Sonstige ASCII-Control-Bytes unter `0x20` werden zu einem Leerzeichen.

Pfade, Target-Namen, Directories, Diagnostics-, Input- und Summary-Werte verwenden denselben Whitespace-Vertrag. AP 1.4 erzeugt keine `pre`/`code`-Elemente fuer fachliche Werte und keine Links aus fachlichen Pfaden. Leere Strings werden als leerer Textwert nur ausgegeben, wenn das Modell tatsaechlich einen leeren String enthaelt; fehlende optionale Eingaben aus `ReportInputs` werden als sichtbarer Text `not provided` ausgegeben.

### Disambiguierung kollidierender Target-Display-Namen (M6 AP 1.2)

- Die `Target Graph`-, `Target Hubs`- und `Target Graph Reference`-Sections nutzen denselben gemeinsamen Helper `disambiguate_target_display_names()` wie Console und Markdown. Eindeutigkeitspruefung erfolgt pro Liste (Direct Target Dependencies, Inbound-Hubs, Outbound-Hubs, Target Graph Reference).
- Wenn ein `display_text` innerhalb der jeweiligen Liste nicht eindeutig ist, ergaenzt der Helper das Suffix ` [key: <unique_key>]`. Eindeutige `display_text`-Werte bleiben unveraendert.
- `<external>::*`-Schluessel im Suffix enthalten spitze Klammern; das Standard-HTML-Escaping macht daraus `&lt;external&gt;::<raw_id>`. Adapter-Tests pinnen das Verhalten als Regression gegen versehentliche Roh-HTML-Ausgabe.
- Externe Edge-Ziele werden in der `Target Graph`-Tabelle nicht zusaetzlich mit einem `[external]`-Wort markiert; die `Resolution`-Spalte traegt bereits den Wert `external`, und der `External target`-Zelleninhalt enthaelt die HTML-escapte `raw_id`. Die Console- und Markdown-Suffix-Reihenfolge `[external] [key: ...]` gilt nicht fuer HTML, weil die Zellenstruktur bereits dieselbe Information traegt.

## Deterministische Reihenfolge

Dokumentreihenfolge:

1. `<!doctype html>`.
2. `html`.
3. `head` mit `meta charset`, `meta viewport`, `meta xray-report-format-version`, `title`, `style`.
4. `body`.
5. `main`.
6. `header` mit `h1` und kurzer Summary-Liste.
7. `section` fuer Inputs.
8. reporttypspezifische fachliche Sections (siehe Analyze-/Impact-Vertrag unten).
9. `section` fuer Diagnostics.

Attributreihenfolge:

- Standard-Strukturelemente: `class` vor `data-*`.
- Tabellenheader nutzen `th scope="col"`.
- Nutzergelieferte Daten erscheinen nie in `id`-Attributen.

Tabellenreihenfolge:

- `table` enthaelt optional `caption`, dann `thead`, dann `tbody`.
- Zeilenreihenfolge folgt den reporttypspezifischen Sortierregeln.
- Zellenreihenfolge ist je Tabelle weiter unten dokumentiert und in Goldens festgeschrieben.

## Barrierearme Basisstruktur

- Genau ein `h1`.
- `h2` fuer Hauptsections, `h3` nur fuer Untersections.
- Tabellen haben `caption` oder eine unmittelbar zugeordnete Section-Ueberschrift.
- Tabellenheader nutzen `scope="col"`.
- Status-Badges enthalten Text.
- Diagnostics enthalten Severity-Text.
- Farbkontrast wird ueber feste CSS-Farben dokumentiert.
- Kein Inhalt wird nur per Farbe oder CSS-Pseudo-Element vermittelt.
- Das Dokument bleibt ohne CSS in sinnvoller Lesereihenfolge.

## Analyze-HTML-Vertrag

Pflichtsections (in Reihenfolge):

1. Header / Summary
2. Inputs
3. Analysis Configuration (M6 AP 1.5)
4. Translation Unit Ranking
5. Include Hotspots
6. Target Metadata
7. Target Graph (M6 AP 1.2)
8. Target Hubs (M6 AP 1.2)
9. Diagnostics

Ab v5 traegt jede der vier fachlichen Sections `Translation Unit Ranking`, `Include Hotspots`, `Target Graph` und `Target Hubs` inline im `<h2>` ein Section-State-Badge `<span class="badge badge--state-<state>">Status: <state></span>`, wobei `<state>` aus `result.analysis_section_states[<section>]` stammt (Mapping: `AnalysisSection::tu_ranking` → `tu-ranking`, `include_hotspots` → `include-hotspots`, `target_graph` → `target-graph`, `target_hubs` → `target-hubs`). Wenn der Section-State `disabled` oder `not_loaded` ist, ersetzt die Section ihren gesamten Body durch genau einen Absatz `<p class="empty">Section disabled.</p>` bzw. `<p class="empty">Section not loaded.</p>`; die Section bleibt mit `<h2>` und Status-Badge im Output. Tabellen, Filter-Zeilen, Statusbadges innerhalb der Section, Empty-Marker der Section selbst und alle weiteren AP-1.2-/AP-1.4-Bausteine entfallen in dieser Variante.

### Header / Summary (analyze)

- `h1`: `cmake-xray analyze report`.
- Sichtbar: Reporttyp `analyze`, `format_version`, Anzahl Translation Units, Anzahl Ranking-Eintraege, wirksames `top_limit`.
- Keine Zeit-/Hostfelder.

### Inputs

Quelle: `AnalysisResult::inputs`. Pflichtzeilen `compile_database_path`, `compile_database_source`, `cmake_file_api_path`, `cmake_file_api_resolved_path`, `cmake_file_api_source`. Fehlende Werte als `not provided`.

### Analysis Configuration (M6 AP 1.5)

- `<section class="analysis-configuration">` mit `<h2>Analysis Configuration</h2>` (kein Status-Badge, da diese Section keinen Eintrag in `analysis_section_states` traegt).
- Erstes `<dl>` mit vier Pflicht-Key/Value-Paaren in dieser Reihenfolge:
  1. `<dt>Sections</dt><dd><effective>...</dd>` — kommagetrennte Liste der `analysis_configuration.effective_sections` in Eingabereihenfolge, HTML-escapt. Leere Liste bleibt zulaessig (`<dd></dd>`); semantisch entspricht das `--analysis ` ohne Werte, was die CLI bereits ablehnt, aber bare-Fixture-Adapter-Tests treffen den Pfad.
  2. `<dt>TU thresholds</dt><dd>arg_count=<n>, include_path_count=<n>, define_count=<n></dd>` — die drei Werte aus `analysis_configuration.tu_thresholds`. Fehlt eine der drei Metriken im Map, gilt `0`.
  3. `<dt>Min hotspot TUs</dt><dd><n></dd>` — `analysis_configuration.min_hotspot_tus`.
  4. `<dt>Target hub thresholds</dt><dd>in=<n>, out=<n></dd>` — `analysis_configuration.target_hub_in_threshold` und `target_hub_out_threshold`. Defaults `10`/`10`.
- Anschliessend `<h3>Section States</h3>` und ein zweites `<dl class="section-states">` mit genau vier `<dt>/<dd>`-Paaren in fester Reihenfolge: `tu-ranking`, `include-hotspots`, `target-graph`, `target-hubs`. Jeder `<dd>` traegt den zugehoerigen `AnalysisSectionState`-Text (`active`, `disabled`, `not_loaded`).
- Die Section ist immer vorhanden, unabhaengig von der CLI-Konfiguration; ihre Werte spiegeln Konfiguration, nicht Ergebnis.
- Konsole und Markdown bringen denselben Datenstrom in formatspezifischer Praesentation (siehe `spec/report-json.md` und Plan §678-698 / §702-727).

### Translation Unit Ranking

Spalten: `Rank`, `Source path`, `Directory`, `Score`, `Arguments`, `Include paths`, `Defines`, `Targets`, `Diagnostics`.
Hoechstens `top_limit` Zeilen. Bei `translation_units.size() > top_limit` zusaetzlich der Hinweis `Showing N of M entries.`. Leersatz `No translation units to report.` wenn die Liste leer ist.

Ab v5: Wenn `analysis_section_states["tu-ranking"]` `disabled` oder `not_loaded` ist, ersetzt der unifizierte `<p class="empty">Section disabled.</p>` / `Section not loaded.`-Absatz die Tabelle und alle Empty-/Showing-Marker; das `<h2>Translation Unit Ranking <span class="badge badge--state-<state>">Status: <state></span></h2>` bleibt erhalten.

### Include Hotspots

Ab v5: Das `<h2>` traegt inline ein Section-State-Badge `<span class="badge badge--state-<state>">Status: <state></span>`. Wenn `analysis_section_states["include-hotspots"]` `disabled` oder `not_loaded` ist, ersetzt der unifizierte `<p class="empty">Section disabled.</p>` / `Section not loaded.`-Absatz Filter-Zeile, Budget-Note, Showing-Hinweis, Empty-Marker und Tabelle; `<h2>` bleibt erhalten.

Section-Struktur in fester Reihenfolge (wenn Section `active`):

1. `<h2>Include Hotspots <span class="badge badge--state-active">Status: active</span></h2>`.
2. **Pflicht-Filter-Zeile** als `<p class="include-filter">`: `Filter: scope=<span class="badge badge--<scope>">…</span>, depth=<span class="badge badge--<depth>">…</span>. Excluded: <unknown> unknown, <mixed> mixed.`. `<scope>` ist einer von `all`, `project`, `external`, `unknown`; `<depth>` ist einer von `all`, `direct`, `indirect`. `<unknown>` und `<mixed>` sind die Zaehler `include_hotspot_excluded_unknown_count` bzw. `include_hotspot_excluded_mixed_count` aus dem `AnalysisResult`. Die Zeile spiegelt Analyse-Konfiguration, nicht Ergebnis, und erscheint deshalb auch im Empty-Hotspot-Fall direkt unter dem `<h2>`.
3. **Optionale Budget-Note** als `<p class="include-budget-note">Note: include analysis stopped at <effective> nodes (budget reached).</p>`. Erscheint genau dann, wenn `include_node_budget_reached=true`. `<effective>` ist der Wert von `include_node_budget_effective`. Wording ist Vertragsbestandteil.
4. **Optionale Top-Limit-Hinweiszeile** `<p>Showing N of M entries.</p>` bei `include_hotspots.size() > top_limit`. Wording ist identisch zu Translation Unit Ranking.
5. Empty-Marker `<p class="empty">No include hotspots to report.</p>` ODER Hotspot-Tabelle.

Hotspot-Tabelle: Spalten `Header`, `Origin`, `Depth`, `Affected translation units`, `Translation unit context` in genau dieser Reihenfolge. Hoechstens `top_limit` Zeilen.

- `Origin`-Zelle: `<span class="badge badge--<origin>">…</span>` mit `<origin>` einer von `project`, `external`, `unknown`.
- `Depth`-Zelle: `<span class="badge badge--<depth_kind>">…</span>` mit `<depth_kind>` einer von `direct`, `indirect`, `mixed`.
- `Affected translation units`-Zelle: Anzahl der zugeordneten TUs als Klartextzahl.
- `Translation unit context`-Zelle: `<ul>` der ersten `top_limit` TUs; bei Kuerzung sichtbarer Hinweis `<p>Showing N of M translation units.</p>` ueber der Liste; bei leerem Kontext `<span class="empty">no translation units</span>`.

### Target Metadata

Observation Source, Target Metadata Status, Anzahl TU mit Target-Mapping, Liste eindeutiger Targets sortiert nach `display_name`, `type`, `unique_key`. Leersatz `No target metadata loaded.`.

### Target Graph (analyze, M6 AP 1.2)

- `h2`: `Target Graph`. Ab v5 inline mit Section-State-Badge `<span class="badge badge--state-<state>">Status: <state></span>`, wobei `<state>` aus `analysis_section_states["target-graph"]` stammt.
- Wenn `analysis_section_states["target-graph"]` `disabled` oder `not_loaded` ist (v5): Section bleibt mit `<h2>` und ersetzt den Body durch genau einen Absatz `<p class="empty">Section disabled.</p>` bzw. `<p class="empty">Section not loaded.</p>`. Der Legacy-Leersatz `Target graph not loaded.` aus AP 1.2 wird im Analyze-HTML durch diesen unifizierten Marker abgeloest; die Impact-`Target Graph Reference`-Section traegt ihn weiterhin (siehe Impact-Vertrag).
- Wenn `analysis_section_states["target-graph"]` `active` ist: Sichtbarer Statuswert `Status: <target_graph_status>` in einem `<p>`-Element direkt unter dem `<h2>`. Die Status-Badge-Klassen `badge--loaded`, `badge--partial`, `badge--disabled` und `badge--not-loaded` werden hier weiterhin wiederverwendet; sie fuehren den Modellstatus, waehrend das `<h2>`-Badge den Section-State traegt.
- Wenn Section `active` und `target_graph.edges` leer: Tabelle entfaellt, Leersatz `No direct target dependencies.`.
- Wenn Section `active` und `target_graph.edges` nicht leer: Tabelle mit Spalten `From`, `To`, `Resolution`, `External target`. `External target`-Zelle bleibt leer fuer `resolution="resolved"` und enthaelt den HTML-escapten `raw_id` fuer `resolution="external"`.
- Zeilenreihenfolge nach `(from_unique_key, to_unique_key, kind, from_display_name, to_display_name)`, identisch zu JSON `target_graph.edges` und Console/Markdown. HTML-Adapter rufen `target_graph_support::sort_target_graph` NICHT erneut auf; sie verlassen sich auf die bereits sortierten Modelldaten aus `AnalysisResult`.

### Target Hubs (analyze, M6 AP 1.2)

- `h2`: `Target Hubs`. Ab v5 inline mit Section-State-Badge `<span class="badge badge--state-<state>">Status: <state></span>`, wobei `<state>` aus `analysis_section_states["target-hubs"]` stammt.
- Wenn `analysis_section_states["target-hubs"]` `disabled` oder `not_loaded` ist (v5): Section bleibt mit `<h2>` und ersetzt den Body durch den unifizierten `<p class="empty">Section disabled.</p>` bzw. `<p class="empty">Section not loaded.</p>`-Absatz. Der Legacy-Leersatz `Target hubs not available.` entfaellt.
- Wenn Section `active`: Sichtbare Schwellenwerte `Incoming threshold: <in_threshold>. Outgoing threshold: <out_threshold>.` Ab v5 stammen beide Werte aus `analysis_configuration.target_hub_in_threshold` bzw. `target_hub_out_threshold` (CLI-konfigurierbar; Defaults `10`/`10`); bis v4 waren sie fest auf `10` gesetzt.
- Tabelle mit Spalten `Direction`, `Threshold`, `Targets`. Eine Zeile fuer `Inbound`, eine fuer `Outbound` (feste Reihenfolge). Die `Threshold`-Spalte traegt denselben CLI-konfigurierbaren Wert wie der einleitende Absatz.
- `Targets`-Zelle: kommagetrennte Display-Namen, jeweils HTML-escapt. Bei Namens-Kollision innerhalb der Liste mit ` [key: <unique_key>]`-Suffix; spitze Klammern aus `<external>::*`-Schluesseln werden HTML-escaped. Innerhalb jeder Zelle ist die kommaweise Reihenfolge nach `(unique_key, display_name, type)` sortiert, identisch zu JSON `target_hubs.inbound`/`target_hubs.outbound`.
- Leerer `inbound`- bzw. `outbound`-Vector: `Targets`-Zelle traegt den Leersatz `No incoming hubs.` bzw. `No outgoing hubs.` als Textinhalt.

### Diagnostics (analyze)

Reportweite Diagnostics aus `AnalysisResult::diagnostics`, Severity als Text, Reihenfolge bleibt Modellreihenfolge. Leersatz `No diagnostics.`.

## Impact-HTML-Vertrag

Pflichtsections (in Reihenfolge):

1. Header / Summary
2. Inputs
3. Directly Affected Translation Units
4. Heuristically Affected Translation Units
5. Directly Affected Targets
6. Heuristically Affected Targets
7. Target Graph Reference (M6 AP 1.2)
8. Prioritised Affected Targets (M6 AP 1.3)
9. Diagnostics

### Header / Summary (impact)

- `h1`: `cmake-xray impact report`.
- Sichtbar: Reporttyp `impact`, `format_version`, geaenderte Datei, Anzahl betroffener TU, Klassifikation, Observation Source, Target Metadata Status.

### Directly / Heuristically Affected Translation Units

Spalten: `Source path`, `Directory`, `Targets`. Reihenfolge folgt der Reihenfolge in `ImpactResult::affected_translation_units` nach Filter auf die jeweilige Klassifikation. Leersatz `No directly affected translation units.` bzw. `No heuristically affected translation units.`.

### Directly / Heuristically Affected Targets

Spalten: `Display name`, `Type`. Reihenfolge: `display_name`, `type`, `unique_key`. Gleichnamige Targets mit unterschiedlichem `type` bleiben ueber die `Type`-Spalte unterscheidbar. Leersatz `No directly affected targets.` bzw. `No heuristically affected targets.`.

### Target Graph Reference (impact, M6 AP 1.2)

- Identischer Tabellen-Vertrag wie `Target Graph` im Analyze-HTML (Spalten `From`, `To`, `Resolution`, `External target`; Sortierung nach `(from_unique_key, to_unique_key, kind, from_display_name, to_display_name)`).
- `h2`: `Target Graph Reference`.
- Sichtbarer Statuswert: `Status: <target_graph_status>` mit denselben Status-Badge-Klassen.
- Bei `target_graph_status="not_loaded"`: `h2` plus Absatz `Target graph not loaded.` (gleicher Leersatz wie Analyze; Section bleibt im Output).
- Bei `target_graph_status="loaded"` oder `"partial"` mit leerer `target_graph.edges`-Liste: Leersatz `No direct target dependencies.` ohne Tabelle.
- Es gibt KEINE Hub-Section im Impact-HTML; diese Asymmetrie spiegelt den JSON-Vertrag (`target_hubs` existiert nur im Analyze-Output).
- AP 1.3 ergaenzt eine separate Section `Prioritised Affected Targets` (siehe naechster Abschnitt); die Reverse-BFS-priorisierte Sicht lebt nicht in dieser Lesedaten-Section.

### Prioritised Affected Targets (impact, M6 AP 1.3)

- `h2`: `Prioritised Affected Targets`.
- Sichtbare Tiefenangabe in einem `p`-Element direkt nach `h2`:
  - `target_graph_status="loaded"` oder `"partial"`: `Requested depth: <n>. Effective depth: <m>.`
  - `target_graph_status="not_loaded"`: `Requested depth: <n>. Effective depth: 0 (no graph).`
- Wenn `target_graph_status="not_loaded"`: zusaetzlicher Absatz `Target graph not loaded; prioritisation skipped.`. Keine Tabelle.
- Wenn `target_graph_status="loaded"` oder `"partial"` und `prioritized_affected_targets` leer: Absatz `No prioritised targets.`. Keine Tabelle.
- Wenn `prioritized_affected_targets` nicht leer: Tabelle mit Spalten `Display name`, `Type`, `Priority class`, `Graph distance`, `Evidence strength`, `Unique key`. Zeilenreihenfolge nach dem 4-Tupel `(priority_class, graph_distance, evidence_strength, unique_key)`, identisch zur Service-Sortierung.
- Die `Unique key`-Spalte uebernimmt die Disambiguierung kollidierender Display-Namen direkt aus dem Modellfeld; ein zusaetzlicher `[key: ...]`-Suffix wie in AP 1.2 ist nicht noetig, weil die Spalte den Schluessel sowieso fuehrt.
- Section bleibt in jeder Statusvariante im Output (nicht weggelassen), damit Konsumenten die Tiefen-Metadaten zuverlaessig finden.

### Diagnostics (impact)

Reportweite Diagnostics aus `ImpactResult::diagnostics`. Leersatz `No diagnostics.`.

## Goldens

- Goldens liegen unter `tests/e2e/testdata/m5/html-reports/` und `tests/e2e/testdata/m6/html-reports/`. Beide Verzeichnisse enthalten ab AP M6-1.5 A.5 Step 20 ausschliesslich `format_version=5`-Goldens (Step 20 migriert die `format_version=4`-Goldens aus AP M6-1.4 A.3 inhaltlich); die `m5/`-/`m6/`-Trennung ist fachlich (Datensatz-Szenarien), nicht versionell, analog zur JSON- und DOT-Goldens-Aufteilung.
- Manifeste `tests/e2e/testdata/m5/html-reports/manifest.txt` und `tests/e2e/testdata/m6/html-reports/manifest.txt` listen jeden Goldenpfad einmal; Verzeichnis und Manifest werden beidseitig abgeglichen.
- Goldens enthalten keine Host-/Workspace-spezifischen Absolutpfade. Synthetische Pfade wie `/project/...` oder `C:/project/...` bleiben erlaubt.
- Goldens enthalten keine Zeitstempel, Hostnamen, zufaellige IDs oder Buildpfade.
- Goldens fuer absolute `--changed-file`-Eingaben werden per Plattform gesplittet (`*_windows.html` neben dem POSIX-Golden), weil die Pfadsemantik auf Windows abweicht (siehe `docs/planning/plan-M5-1-4.md`, Tranche C).
- M6-Pflichtszenarien sind `analyze-compile-db-only`, `analyze-file-api-loaded`, `analyze-file-api-partial`, `impact-compile-db-only`, `impact-file-api-loaded` und `impact-file-api-partial` jeweils als `.html` unter `tests/e2e/testdata/m6/html-reports/`. M5-File-API- und Mixed-Input-Goldens unter `tests/e2e/testdata/m5/html-reports/` werden in AP 1.2 A.3 inhaltlich auf v2 migriert.

## Aenderungsregeln

- Vertragsaenderungen werden in `spec/report-html.md` vorgenommen, bevor der Adapter sie umsetzt.
- Aenderungen an Pflichtsections, Spaltenreihenfolge, CSS-Klassen oder Escaping-Regeln erhoehen `xray::hexagon::model::kReportFormatVersion`.
- Wenn AP 1.4 oder eine spaetere Vertragsversion neue Felder einfuehrt, werden Goldens und Tests im selben Schritt migriert.
- HTML-Erweiterungen, die JavaScript, externe Ressourcen oder dynamische Inhalte einfuehren wuerden, sind nicht Teil von M5 und brauchen einen eigenen Vertrag.
