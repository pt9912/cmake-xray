# HTML Report Contract

> **Implementierungsstand:** AP M5-1.4 Tranche A liefert den HTML-Vertrag, das statische CSS, file-locale Escaping- und Whitespace-Hilfen und das Adapter-Testskelett. Tranche B implementiert den produktiven `HtmlReportAdapter` und schaltet `--format html` an der CLI frei. Bis dahin lehnt die CLI `--format html` weiterhin als `recognized but not implemented in this build` ab. Die in diesem Dokument festgehaltenen Regeln sind ab Tranche A bindend; folgende Tranchen erweitern den Vertrag nicht ohne Update dieses Dokuments.

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
  <meta name="xray-report-format-version" content="1">
  <title>cmake-xray analyze report</title>
  <style>...</style>
</head>
<body>
  <main class="report" data-report-type="analyze" data-format-version="1">
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
3. Translation Unit Ranking
4. Include Hotspots
5. Target Metadata
6. Diagnostics

### Header / Summary (analyze)

- `h1`: `cmake-xray analyze report`.
- Sichtbar: Reporttyp `analyze`, `format_version`, Anzahl Translation Units, Anzahl Ranking-Eintraege, wirksames `top_limit`.
- Keine Zeit-/Hostfelder.

### Inputs

Quelle: `AnalysisResult::inputs`. Pflichtzeilen `compile_database_path`, `compile_database_source`, `cmake_file_api_path`, `cmake_file_api_resolved_path`, `cmake_file_api_source`. Fehlende Werte als `not provided`.

### Translation Unit Ranking

Spalten: `Rank`, `Source path`, `Directory`, `Score`, `Arguments`, `Include paths`, `Defines`, `Targets`, `Diagnostics`.
Hoechstens `top_limit` Zeilen. Bei `translation_units.size() > top_limit` zusaetzlich der Hinweis `Showing N of M entries.`. Leersatz `No translation units to report.` wenn die Liste leer ist.

### Include Hotspots

Spalten: `Header`, `Affected translation units`, `Translation unit context`. Hoechstens `top_limit` Zeilen. Pro Hotspot hoechstens `top_limit` Kontext-TUs; bei Kuerzung sichtbarer Hinweis `Showing N of M translation units.`. Leersatz `No include hotspots to report.`.

### Target Metadata

Observation Source, Target Metadata Status, Anzahl TU mit Target-Mapping, Liste eindeutiger Targets sortiert nach `display_name`, `type`, `unique_key`. Leersatz `No target metadata loaded.`.

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
7. Diagnostics

### Header / Summary (impact)

- `h1`: `cmake-xray impact report`.
- Sichtbar: Reporttyp `impact`, `format_version`, geaenderte Datei, Anzahl betroffener TU, Klassifikation, Observation Source, Target Metadata Status.

### Directly / Heuristically Affected Translation Units

Spalten: `Source path`, `Directory`, `Targets`. Reihenfolge folgt der Reihenfolge in `ImpactResult::affected_translation_units` nach Filter auf die jeweilige Klassifikation. Leersatz `No directly affected translation units.` bzw. `No heuristically affected translation units.`.

### Directly / Heuristically Affected Targets

Spalten: `Display name`, `Type`. Reihenfolge: `display_name`, `type`, `unique_key`. Gleichnamige Targets mit unterschiedlichem `type` bleiben ueber die `Type`-Spalte unterscheidbar. Leersatz `No directly affected targets.` bzw. `No heuristically affected targets.`.

### Diagnostics (impact)

Reportweite Diagnostics aus `ImpactResult::diagnostics`. Leersatz `No diagnostics.`.

## Goldens

- Goldens liegen unter `tests/e2e/testdata/m5/html-reports/`.
- Manifest `tests/e2e/testdata/m5/html-reports/manifest.txt` listet jeden Goldenpfad einmal; Verzeichnis und Manifest werden beidseitig abgeglichen.
- Goldens enthalten keine Host-/Workspace-spezifischen Absolutpfade. Synthetische Pfade wie `/project/...` oder `C:/project/...` bleiben erlaubt.
- Goldens enthalten keine Zeitstempel, Hostnamen, zufaellige IDs oder Buildpfade.
- Goldens fuer absolute `--changed-file`-Eingaben werden per Plattform gesplittet (`*_windows.html` neben dem POSIX-Golden), weil die Pfadsemantik auf Windows abweicht (siehe `docs/plan-M5-1-4.md`, Tranche C).

## Aenderungsregeln

- Vertragsaenderungen werden in `docs/report-html.md` vorgenommen, bevor der Adapter sie umsetzt.
- Aenderungen an Pflichtsections, Spaltenreihenfolge, CSS-Klassen oder Escaping-Regeln erhoehen `xray::hexagon::model::kReportFormatVersion`.
- Wenn AP 1.4 oder eine spaetere Vertragsversion neue Felder einfuehrt, werden Goldens und Tests im selben Schritt migriert.
- HTML-Erweiterungen, die JavaScript, externe Ressourcen oder dynamische Inhalte einfuehren wuerden, sind nicht Teil von M5 und brauchen einen eigenen Vertrag.
