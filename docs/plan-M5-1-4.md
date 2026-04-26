# Plan M5 - AP 1.4 HtmlReportAdapter fuer eigenstaendige HTML-Berichte umsetzen

## Ziel

AP 1.4 implementiert den HTML-Report als portables, eigenstaendiges M5-Artefakt fuer Reviews, CI-Artefakte und Dokumentation.

Der HTML-Export ist der wichtigste menschenlesbare Ausbau in M5. Er soll die Informationen aus Markdown komfortabler lesbar machen, ohne eine dynamische Webanwendung einzufuehren. HTML bleibt ein statischer Bericht: eine vollstaendige Datei, inline CSS, keine externen Laufzeitressourcen, kein JavaScript und keine Host-/Zeit-/Umgebungsdaten ausserhalb der fachlichen Ergebnisdaten.

Nach AP 1.4 kann `cmake-xray analyze --format html` und `cmake-xray impact --format html` produktiv genutzt werden. Die Ausgabe ist ein vollstaendiges HTML5-Dokument, das direkt aus einer Datei oder als CI-Artefakt geoeffnet werden kann.

## Scope

Umsetzen:

- `HtmlReportAdapter` fuer Analyze- und Impact-Ergebnisse.
- Freischaltung von `--format html` fuer `analyze` und `impact`.
- Freischaltung von `--output <path>` fuer `--format html` ueber den gemeinsamen M5-Atomic-Writer.
- Vollstaendige eigenstaendige HTML5-Dokumente mit inline CSS.
- Deterministische Dokumentstruktur, Abschnittsreihenfolge, Tabellenstruktur, CSS-Reihenfolge und Escaping-Regeln.
- Barrierearme Basisstruktur mit sinnvoller Ueberschriftenhierarchie, Tabellenueberschriften, `scope`-Attributen, ausreichend Kontrast und nicht rein farbbasierter Statusdarstellung.
- Sichtbare Target-Zuordnungen aus M4, sofern vorhanden.
- Klare Leersaetze fuer leere Ergebnisabschnitte analog zu Markdown.
- Adapter-, CLI-, Wiring-, Golden-, Escaping- und Binary-E2E-Tests.
- Nutzungsdokumentation und Beispielberichte unter `docs/examples/`.

Nicht veraendern:

- Bestehende Console-, Markdown-, JSON- und DOT-Ausgaben.
- Fachliche Analyseentscheidungen in `ProjectAnalyzer` oder `ImpactAnalyzer`.
- Report-Ports so, dass Adapter CLI-Kontext erhalten.
- DOT-Budgetierung oder JSON-Schema.
- Verbosity-Verhalten aus AP 1.5.

## Nicht umsetzen

Nicht Bestandteil von AP 1.4:

- JavaScript, interaktive Filter, clientseitige Suche oder einklappbare Bereiche.
- Externe Fonts, CDN-Ressourcen, Bilder, Stylesheets oder Skripte.
- Gerenderte Diagramme oder eingebettete DOT/SVG-Visualisierungen.
- Eine Webanwendung, Routing, Bundler-Setup oder Frontend-Framework.
- Konfigurierbare HTML-Themes oder eigene CSS-Dateien neben dem Report.
- Target-zu-Target-Graph-Analyse (`F-18`) oder neue Target-Priorisierung.
- HTML-Fehlerdokumente fuer Parser-, Eingabe-, Render- oder Schreibfehler.
- Migration von Console/Markdown auf `ReportInputs`.

Diese Punkte folgen in anderen M5-Arbeitspaketen oder bleiben bewusst ausserhalb von M5.

## Voraussetzungen aus AP 1.1 bis AP 1.3

AP 1.4 baut auf folgenden AP-1.1-Vertraegen auf:

- `AnalysisResult` und `ImpactResult` enthalten `ReportInputs`.
- `ReportInputs` ist die kanonische Eingabeprovenienz fuer neue M5-Artefaktadapter.
- `AnalyzeProjectRequest` und `AnalyzeImpactRequest` transportieren Eingabepfade und `report_display_base`.
- Der gemeinsame atomare Schreibpfad fuer Reportdateien existiert und ist testbar.
- `html` ist ein bekannter Formatwert und wird vor AP 1.4 als `recognized but not implemented` abgelehnt.
- Report-Ports bleiben ergebnisobjektzentriert.

AP 1.4 entfernt die AP-1.1-Sperre fuer `--format html`. `--format html` darf nach AP 1.4 den Fehler `recognized but not implemented` nicht mehr liefern.

AP 1.2 und AP 1.3 sind keine fachliche Voraussetzung fuer HTML, aber AP 1.4 muss die dort geschaerften Regeln respektieren:

- deterministische Sortierung und byte-stabile Goldens.
- `analyze` nutzt den wirksamen `top_limit`.
- `impact` erhaelt keine `--top`-Option.
- Erfolgreiche artefaktorientierte stdout-/stderr-Vertraege gelten auch fuer HTML.
- JSON- und DOT-Goldens, Schemata und Syntax-Gates duerfen durch AP 1.4 nicht veraendert werden.

Falls der M5-konforme Atomic-Writer aus AP 1.1 noch nicht vorhanden oder nicht testbar ist, ist AP 1.4 insgesamt nicht abnahmefaehig. Eine teilweise Abnahme nur fuer HTML-stdout ohne HTML-Dateiausgabe ist nicht zulaessig.

## Dateien

Voraussichtlich zu aendern:

- `src/adapters/cli/cli_adapter.{h,cpp}`
- `src/main.cpp`
- `src/adapters/CMakeLists.txt`
- `tests/CMakeLists.txt`
- `tests/adapters/test_port_wiring.cpp`
- `tests/e2e/test_cli.cpp`
- `tests/e2e/run_e2e.sh`
- `README.md`
- `docs/guide.md`
- `docs/quality.md`

Neue Dateien:

- `src/adapters/output/html_report_adapter.h`
- `src/adapters/output/html_report_adapter.cpp`
- `tests/adapters/test_html_report_adapter.cpp`
- `docs/report-html.md`
- HTML-Report-Goldens unter `tests/e2e/testdata/m5/html-reports/`
- Golden-Manifest `tests/e2e/testdata/m5/html-reports/manifest.txt` oder eine gleichwertige explizite Liste aller HTML-Report-Goldens
- Beispielberichte unter `docs/examples/`, mindestens ein Analyze- und ein Impact-Beispiel
- optional `tests/validate_html_reports.py`, falls ein repository-lokaler Struktur-Smoke ueber reine Golden-Diffs hinaus noetig wird

## HTML-Dokumentvertrag

### Gemeinsame Regeln

Der HTML-Vertrag wird in `docs/report-html.md` verbindlich dokumentiert. Dieses Dokument beschreibt:

- Reporttypen.
- Dokumentstruktur.
- Pflichtabschnitte.
- Abschnitts-, Tabellen- und Feldreihenfolge.
- Escaping-Regeln.
- CSS- und Barrierefreiheitsregeln.
- Regeln fuer leere Abschnitte.
- Aenderungsregeln fuer kuenftige HTML-Vertragsaenderungen.

Allgemeine HTML-Regeln:

- HTML-Ausgaben muessen gueltiges UTF-8 sein.
- Jeder Report ist ein vollstaendiges HTML5-Dokument mit `<!doctype html>`, `html`, `head` und `body`.
- Das Root-Element verwendet `lang="en"`, weil die bestehende Reportausgabe und Feldnamen englisch sind.
- Das Dokument enthaelt genau ein `main`-Element.
- Das Dokument enthaelt genau ein `h1`.
- CSS wird ausschliesslich in einem `style`-Element im `head` eingebettet.
- Es werden keine externen Fonts, Stylesheets, Skripte, Bilder, Favicons oder CDN-Ressourcen referenziert.
- AP 1.4 erzeugt kein JavaScript und keine `script`-Elemente.
- AP 1.4 erzeugt keine HTML-Kommentare.
- Datums-, Laufzeit-, Hostnamen- oder Build-Umgebungsinformationen duerfen nicht automatisch erscheinen.
- Alle fachlich relevanten Informationen stehen als sichtbarer Text in statischen HTML-Elementen.
- HTML erzeugt keine JSON- oder DOT-spezifischen Metadaten wie `graph_*`, `context_*` oder JSON-Schema-Felder.

### Dokumentgeruest

Das Dokumentgeruest ist fuer Analyze und Impact identisch:

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
- Die `head`-Reihenfolge ist hart festgelegt: `meta charset`, `meta viewport`, `meta xray-report-format-version`, `title`, `style`.
- Der Adapter erzeugt keine optionalen Meta-Tags fuer Generator, Zeit, Host, Commit oder Buildpfad.

### CSS-Vertrag

CSS-Regeln:

- CSS ist statisch und im Adapter als deterministischer String hinterlegt.
- CSS enthaelt keine zufaelligen Werte, keine generierten Klassen und keine Buildzeitdaten.
- CSS nutzt keine externen `@import`-Regeln und keine `url(...)`-Referenzen.
- CSS nutzt keine Animationen.
- CSS darf System-Schriften ueber eine statische Font-Stack-Liste verwenden, aber keine externen Fonts laden.
- CSS muss fuer Bildschirm und Druck lesbare Defaults liefern.
- Tabellen muessen horizontal scrollbar sein duerfen, ohne Seitenlayout zu sprengen.
- Farbkontrast fuer Text und Status-Badges muss mindestens WCAG-AA-nahe Basiswerte erreichen; AP 1.4 dokumentiert die konkreten Farbwerte in `docs/report-html.md`.
- Statusinformationen duerfen nicht ausschliesslich ueber Farbe transportiert werden; sichtbarer Text wie `warning`, `note`, `direct`, `heuristic`, `loaded`, `partial` oder `not_loaded` bleibt immer vorhanden.

Klassen:

- CSS-Klassen stammen ausschliesslich aus festen Adapter-Literalen.
- Nutzergelieferte Inhalte duerfen nie in Klassennamen, IDs oder Style-Attribute einfliessen.
- AP 1.4 erzeugt keine Inline-`style`-Attribute auf fachlichen Elementen.

### Escaping und Textausgabe

HTML-Escaping ist Pflicht fuer alle nutzer- oder eingabedatenbasierten Inhalte:

- Textknoten escapen mindestens `&`, `<` und `>`.
- Attributwerte escapen mindestens `&`, `<`, `>`, `"` und `'`.
- AP 1.4 erzeugt keine `pre`- oder `code`-Elemente fuer fachliche Werte.
- Whitespace in fachlichen Textwerten wird vor dem HTML-Escaping einheitlich normalisiert: `\r\n` und `\r` werden zu `\n`, jedes `\n` wird als sichtbarer Separator ` / ` ausgegeben, jedes `\t` wird zu einem einzelnen Leerzeichen, und sonstige ASCII-Control-Zeichen unter `0x20` werden zu einem einzelnen Leerzeichen.
- Pfade, Target-Namen, Directories, Diagnostics, Input-Werte und Summary-Werte nutzen denselben Whitespace-Vertrag.
- Rohes HTML aus Pfaden, Target-Namen, Diagnostics oder Compile-Argumenten wird nie interpretiert.
- URLs werden in AP 1.4 nicht aus fachlichen Daten erzeugt; Pfade werden als Text ausgegeben, nicht als Links.
- Leere Strings werden als leerer Textwert nur ausgegeben, wenn das Modell tatsaechlich einen leeren String enthaelt. Fehlende optionale Eingaben aus `ReportInputs` werden als sichtbarer Text `not provided` ausgegeben.

Sonderzeichen-Goldens muessen mindestens abdecken:

- `<`
- `>`
- `&`
- `"`
- `'`
- Backslash
- Pfadtrenner `/` und `\`
- Newline in Diagnostics
- Strings, die wie HTML-Tags aussehen, zum Beispiel `<script>alert(1)</script>`

### Deterministische Reihenfolge

Die HTML-Ausgabe ist byte-stabil.

Dokumentreihenfolge:

1. `<!doctype html>`.
2. `html`.
3. `head` mit `meta charset`, `meta viewport`, `meta xray-report-format-version`, `title`, `style`.
4. `body`.
5. `main`.
6. `header` mit `h1` und kurzer Summary-Liste.
7. `section` fuer Inputs.
8. reporttypspezifische fachliche Sections.
9. `section` fuer Diagnostics.

Attributreihenfolge:

- Attribute werden in der Reihenfolge geschrieben, in der sie in `docs/report-html.md` je Elementtyp dokumentiert sind.
- Fuer Standard-Strukturelemente gilt `class` vor `data-*`.
- Nutzergelieferte Daten erscheinen nie in `id`-Attributen.

Tabellenreihenfolge:

- `table` enthaelt optional `caption`, dann `thead`, dann `tbody`.
- Header-Zellen verwenden `th scope="col"`.
- Zeilenreihenfolge folgt den reporttypspezifischen Sortierregeln.
- Zellenreihenfolge ist in `docs/report-html.md` dokumentiert und golden-getestet.

### Barrierearme Basisstruktur

AP 1.4 ist keine vollstaendige Accessibility-Zertifizierung, muss aber eine solide statische Basis liefern:

- genau ein `h1`.
- `h2` fuer Hauptsections.
- `h3` nur fuer Untersections.
- Tabellen haben `caption` oder eine unmittelbar zugeordnete Section-Ueberschrift.
- Tabellenheader nutzen `scope="col"`.
- Status-Badges enthalten Text.
- Diagnostics enthalten Severity-Text.
- Farbkontrast wird ueber feste CSS-Farben dokumentiert.
- Kein Inhalt wird nur per Farbe oder CSS-Pseudo-Element vermittelt.
- Das Dokument bleibt ohne CSS in sinnvoller Lesereihenfolge.

## Analyze-HTML-Vertrag

### Pflichtsections

Ein Analyze-HTML-Report enthaelt in dieser Reihenfolge:

1. Header/Summary.
2. Inputs.
3. Translation Unit Ranking.
4. Include Hotspots.
5. Target Metadata.
6. Diagnostics.

### Header/Summary

Der Header enthaelt:

- `h1`: `cmake-xray analyze report`.
- Reporttyp `analyze`.
- `format_version` aus `xray::hexagon::model::kReportFormatVersion`.
- Anzahl Translation Units.
- Anzahl Ranking-Eintraege im Ergebnis.
- Anzahl Include-Hotspots im Ergebnis.
- wirksamer `top_limit`.
- Include-Analyse-Heuristik als `true` oder `false`.
- Observation Source.
- Target Metadata Status.

Regeln:

- Summary-Werte werden als sichtbare Key-Value-Liste ausgegeben.
- Numerische Werte bleiben nicht lokalisiert.
- Keine Zeit- oder Hostfelder.

### Inputs

Die Inputs-Section nutzt `AnalysisResult::inputs` als kanonische Quelle.

Pflichtzeilen:

- `compile_database_path`
- `compile_database_source`
- `cmake_file_api_path`
- `cmake_file_api_resolved_path`
- `cmake_file_api_source`

Regeln:

- Fehlende Werte werden als `not provided` gerendert.
- Leere Strings duerfen nicht als Ersatz fuer fehlende Eingaben verwendet werden.
- Legacy-Felder wie `AnalysisResult::compile_database_path` sind fuer HTML-Provenienz nicht die kanonische Quelle.

### Translation Unit Ranking

Die Ranking-Section enthaelt eine Tabelle.

Spalten:

1. `Rank`.
2. `Source path`.
3. `Directory`.
4. `Score`.
5. `Arguments`.
6. `Include paths`.
7. `Defines`.
8. `Targets`.
9. `Diagnostics`.

Regeln:

- Es werden hoechstens `top_limit` Ranking-Zeilen ausgegeben.
- Die Reihenfolge folgt der Reihenfolge in `AnalysisResult::translation_units` nach Anwendung des Top-Limits.
- Translation Units mit identischem `source_path` bleiben durch die `Directory`-Spalte unterscheidbar.
- Targets werden innerhalb einer Zelle deterministisch nach `display_name`, `type`, `unique_key` sortiert.
- Target-Anzeige nutzt `display_name`; `type` wird sichtbar daneben ausgegeben, damit gleichnamige Targets unterscheidbar bleiben.
- Diagnostics in der Zelle behalten die Modellreihenfolge.
- Wenn die Section leer ist, erscheint der Leersatz `No translation units to report.` statt einer leeren Tabelle.
- Wenn `AnalysisResult::translation_units.size() > top_limit`, wird ein sichtbarer Hinweis ausgegeben: `Showing N of M entries.`

### Include Hotspots

Die Include-Hotspot-Section enthaelt eine Tabelle.

Spalten:

1. `Header`.
2. `Affected translation units`.
3. `Translation unit context`.

Regeln:

- Es werden hoechstens `top_limit` Hotspot-Zeilen ausgegeben.
- Die Reihenfolge folgt der Reihenfolge in `AnalysisResult::include_hotspots` nach Anwendung des Top-Limits.
- `Header` stammt aus `IncludeHotspot::header_path`.
- `Affected translation units` gibt die Gesamtanzahl aus dem Hotspot-Modell aus.
- `Translation unit context` zeigt hoechstens `top_limit` betroffene Translation Units pro Hotspot.
- Kontext-Translation-Units zeigen `source_path` und `directory`.
- Wenn der Kontext pro Hotspot gekuerzt ist, wird sichtbar `Showing N of M translation units.` ausgegeben.
- Wenn die Section leer ist, erscheint der Leersatz `No include hotspots to report.` statt einer leeren Tabelle.

### Target Metadata

Die Target-Metadata-Section enthaelt:

- Observation Source.
- Target Metadata Status.
- Anzahl Translation Units mit Target-Mapping.
- Liste eindeutiger Targets, sofern Target-Metadaten vorhanden sind.

Regeln:

- Targets werden nach `display_name`, `type`, `unique_key` sortiert.
- Gleichnamige Targets mit unterschiedlichen Typen bleiben durch sichtbaren `type` unterscheidbar.
- Wenn keine Target-Metadaten geladen wurden, erscheint der Leersatz `No target metadata loaded.`
- Es werden keine Target-zu-Target-Beziehungen dargestellt.

### Diagnostics

Die Diagnostics-Section enthaelt reportweite Diagnostics aus `AnalysisResult::diagnostics`.

Regeln:

- Diagnostics werden als Liste oder Tabelle mit Severity und Message ausgegeben.
- Reihenfolge bleibt Modellreihenfolge.
- Severity wird als Text ausgegeben.
- Wenn keine Diagnostics vorhanden sind, erscheint `No diagnostics.`

## Impact-HTML-Vertrag

### Pflichtsections

Ein Impact-HTML-Report enthaelt in dieser Reihenfolge:

1. Header/Summary.
2. Inputs.
3. Directly Affected Translation Units.
4. Heuristically Affected Translation Units.
5. Directly Affected Targets.
6. Heuristically Affected Targets.
7. Diagnostics.

### Header/Summary

Der Header enthaelt:

- `h1`: `cmake-xray impact report`.
- Reporttyp `impact`.
- `format_version` aus `xray::hexagon::model::kReportFormatVersion`.
- geaenderte Datei.
- Anzahl betroffener Translation Units.
- Klassifikation.
- Observation Source.
- Target Metadata Status.
- Anzahl betroffener Targets.

Regeln:

- Summary-Werte werden als sichtbare Key-Value-Liste ausgegeben.
- `changed_file` stammt aus `ImpactResult::inputs.changed_file`.
- Wenn `ReportInputs.changed_file_source=unresolved_file_api_source_root` in einem Ergebnis auftaucht, ist das fuer HTML ein nicht renderbares Fehlerergebnis; die CLI meldet Text auf `stderr`, liefert nonzero Exit und erzeugt kein HTML-Fehlerdokument.

### Inputs

Die Inputs-Section nutzt `ImpactResult::inputs` als kanonische Quelle.

Pflichtzeilen:

- `compile_database_path`
- `compile_database_source`
- `cmake_file_api_path`
- `cmake_file_api_resolved_path`
- `cmake_file_api_source`
- `changed_file`
- `changed_file_source`

Regeln:

- Wegen der CLI-Pflicht ist `changed_file` in erfolgreichen Impact-HTML-Reports ein sichtbarer String.
- Fehlende optionale Eingaben werden als `not provided` gerendert.
- Legacy-Felder wie `ImpactResult::changed_file` sind fuer HTML-Provenienz nicht die kanonische Quelle.

### Affected Translation Units

Directly und heuristically affected Translation Units werden in getrennten Sections ausgegeben.

Tabellenspalten:

1. `Source path`.
2. `Directory`.
3. `Impact`.
4. `Targets`.

Regeln:

- `impact` ist `direct` oder `heuristic`.
- Impact-Listen werden in M5 nicht ueber `--top` begrenzt.
- Reihenfolge folgt `ImpactResult::affected_translation_units`, getrennt nach `ImpactKind`.
- Innerhalb gleicher Impact-Art wird nach `source_path`, `directory`, `unique_key` sortiert, sofern das Modell keine bereits stabile Reihenfolge garantiert; `docs/report-html.md` legt den finalen Pfad fest und Goldens pruefen ihn.
- Targets innerhalb einer Zelle werden nach `display_name`, `type`, `unique_key` sortiert.
- Leere Sections zeigen `No directly affected translation units.` beziehungsweise `No heuristically affected translation units.`

### Affected Targets

Directly und heuristically affected Targets werden in getrennten Sections ausgegeben.

Tabellenspalten:

1. `Name`.
2. `Type`.
3. `Impact`.

Regeln:

- `Name` stammt aus `TargetInfo::display_name`.
- `Type` stammt aus `TargetInfo::type`.
- Sortierung erfolgt nach `display_name`, `type`, `unique_key`.
- Gleichnamige Targets mit unterschiedlichen Typen bleiben sichtbar unterscheidbar.
- Leere Sections zeigen `No directly affected targets.` beziehungsweise `No heuristically affected targets.`

### Diagnostics

Die Diagnostics-Section enthaelt reportweite Diagnostics aus `ImpactResult::diagnostics`.

Regeln:

- Diagnostics werden als Liste oder Tabelle mit Severity und Message ausgegeben.
- Reihenfolge bleibt Modellreihenfolge.
- Severity wird als Text ausgegeben.
- Wenn keine Diagnostics vorhanden sind, erscheint `No diagnostics.`

## Adapter- und Port-Grenzen

`HtmlReportAdapter` rendert fachliche Inhalte ausschliesslich aus `AnalysisResult` und `ImpactResult`.

Regeln:

- `HtmlReportAdapter` implementiert `xray::hexagon::ports::driven::ReportWriterPort`.
- Im Produktivpfad wird `HtmlReportAdapter` ueber `ReportGenerator` als `xray::hexagon::ports::driving::GenerateReportPort` an die CLI verdrahtet.
- `ReportPorts` in `src/adapters/cli/cli_adapter.h` erhaelt ein explizites `html`-Feld oder eine gleichwertig typisierte Formatzuordnung, die auf den HTML-`ReportGenerator` zeigt.
- Der Adapter bekommt keinen CLI-Kontext.
- Der Adapter verwendet `ReportInputs` als Eingabeprovenienz.
- Der Adapter nutzt nicht die Legacy-Presentation-Felder fuer Console/Markdown als kanonische Quelle fuer HTML-Provenienz.
- Fuer Analyze darf der bestehende Renderparameter `top_limit` beziehungsweise `write_analysis_report(result, top_limit)` genutzt werden, solange er nur die Berichtssicht begrenzt. Gemeint ist der von der CLI bereits validierte und wirksame Top-Wert.
- `impact` bekommt keinen Top-Limit-Parameter.
- Der Adapter rendert immer zuerst vollstaendig in einen `std::string` oder gleichwertigen Speicherpuffer und gibt erst nach erfolgreichem Abschluss des Renderns an die CLI zurueck. Streaming in `std::ostream` waehrend des Renderns ist fuer AP 1.4 nicht zulaessig.
- Der Adapter fuehrt keine JSON- oder DOT-spezifischen Metadaten ein.
- Der Adapter trifft keine neuen Impact-, Ranking- oder Target-Priorisierungsentscheidungen.
- Report-Ports bleiben ergebnisobjektzentriert.

Hilfsfunktionen:

- HTML-Escaping-, CSS- und Render-Hilfen bleiben in AP 1.4 file-local im `HtmlReportAdapter`, zum Beispiel im anonymen Namespace der `.cpp`-Datei.
- Gemeinsame Sortier- oder Label-Hilfen duerfen nur extrahiert werden, wenn sie bereits von mehreren Adaptern genutzt werden oder echte Dopplung reduzieren.
- Neue Hilfsfunktionen duerfen keine Prozess-CWD-, Host- oder CLI-Zustaende lesen.

## CLI-/Output-Vertrag

CLI-Regeln:

- `--format html` ist fuer `analyze` und `impact` lauffaehig.
- `--format html --output <path>` schreibt in eine Datei und nutzt den gemeinsamen M5-konformen Atomic-Writer aus AP 1.1.
- Erfolgreiche `--format html`-Aufrufe ohne `--output` schreiben ausschliesslich HTML auf stdout.
- Erfolgreiche `--format html`-Aufrufe ohne `--output` lassen stderr leer.
- Erfolgreiche `--format html --output <path>`-Aufrufe schreiben ausschliesslich in die Zieldatei.
- Erfolgreiche `--format html --output <path>`-Aufrufe lassen stdout und stderr leer.
- Reportinhalt wird bei `--output` nicht zusaetzlich auf stdout dupliziert.
- `--format html` liefert nach AP 1.4 nicht mehr den AP-1.1-Fehler `recognized but not implemented`.
- Nach AP 1.4 sind die M5-Formatwerte `console`, `markdown`, `json`, `dot` und `html` implementiert. Formatverfuegbarkeits- und Usage-Hinweise duerfen keine veraltete `html is recognized but not implemented`-Meldung mehr enthalten.

`--top`-Regeln:

- Bei `analyze` beeinflusst `--top` die HTML-Listen `Translation Unit Ranking` und `Include Hotspots` sowie Hotspot-Kontextlisten.
- Bei `impact` fuehrt AP 1.4 keine neue `--top`-Semantik ein.
- Tests stellen sicher, dass `impact --top ...` weiterhin abgelehnt wird, sofern es vor AP 1.4 nicht Teil des Vertrags ist.
- `--top 0` bleibt durch den bestehenden positiven CLI-Validator abgelehnt und erzeugt kein HTML.

## Fehlervertrag

Parser- und Usage-Fehler:

- CLI-Parser-/Verwendungsfehler bleiben Textfehler auf `stderr`.
- Beispiele sind fehlende Pflichtoptionen, unbekannte Optionen und `impact --format html` ohne `--changed-file`.
- Diese Fehler erzeugen kein HTML-Fehlerdokument.
- Exit-Code ist ungleich `0`.

Nicht wiederherstellbare Eingabefehler:

- Fehler vor Reporterzeugung bleiben fuer `--format html` Textfehler auf `stderr`.
- Beispiele sind nicht vorhandene Eingabedateien, ungueltige `compile_commands.json`-Dateien und ungueltige CMake-File-API-Reply-Verzeichnisse.
- Ein `ImpactResult` mit `ReportInputs.changed_file_source=unresolved_file_api_source_root` gilt fuer HTML als nicht renderbares Fehlerergebnis, weil die geaenderte Datei keine belastbare fachliche Basis hat.
- Diese Fehler erzeugen kein HTML-Fehlerdokument.
- Exit-Code ist ungleich `0`.

Impact-Negativfall-Matrix:

| Fall | Zeitpunkt | Ergebnis |
|---|---|---|
| `impact --format html` ohne `--changed-file` | CLI-Usage-Validierung vor Analyse | Textfehler auf `stderr`, nonzero Exit, kein HTML |
| `impact --format html --top N` | CLI-Usage-Validierung vor Analyse | Textfehler auf `stderr`, nonzero Exit, kein HTML |
| `impact --format html --changed-file ""` | CLI-Usage- oder Pfadvalidierung vor Analyse, falls leerer Wert erzeugbar ist | Textfehler auf `stderr`, nonzero Exit, kein HTML |
| nicht vorhandene Compile-Database oder File-API-Eingabe | Eingabeladen vor Reporterzeugung | Textfehler auf `stderr`, nonzero Exit, kein HTML |
| ungueltige Compile-Database oder File-API-Reply-Daten | Eingabeladen vor Reporterzeugung | Textfehler auf `stderr`, nonzero Exit, kein HTML |
| `ReportInputs.changed_file == std::nullopt` in einem sonst erzeugten `ImpactResult` | HTML-Render-Precondition | Render-Fehler als Text auf `stderr`, nonzero Exit, kein HTML |
| `ReportInputs.changed_file_source == unresolved_file_api_source_root` | HTML-Render-Precondition | Textfehler auf `stderr`, nonzero Exit, kein HTML |
| erfolgreiches `ImpactResult` mit Diagnostics | normale Reporterzeugung | HTML-Report mit Diagnostics-Section, Exit `0` |

Praezedenz:

1. CLI-Usage-Fehler gewinnen vor Eingabeladen.
2. Eingabeladefehler gewinnen vor Reporterzeugung.
3. HTML-Render-Preconditions werden erst auf einem vorhandenen `ImpactResult` geprueft.
4. Schreib-/Output-Fehler werden erst nach erfolgreichem Vollrendern behandelt.

Service-Ergebnisse mit Diagnostics:

- Service-Ergebnisse mit Diagnostics oder Teildaten koennen als regulaere HTML-Reports ausgegeben werden.
- Diagnostics erscheinen in den dokumentierten Diagnostics-Sections.
- Diagnostics werden HTML-escaped.
- Es werden keine HTML-Kommentare fuer Diagnostics erzeugt.

Render-, Schreib- und Output-Fehler:

- Schreib-, Render- und Output-Fehler liefern Textfehler auf `stderr`.
- Exit-Code ist ungleich `0`.
- Render-Fehler vor Emission schreiben kein partielles HTML auf stdout, weil die CLI HTML erst nach erfolgreichem Vollrendern des kompletten Strings auf stdout oder in den Atomic-Writer uebergibt.
- Bei echten stdout-Transportfehlern koennen bereits geschriebene Bytes nicht zurueckgenommen werden; die CLI erkennt solche Fehler best-effort nach dem Schreiben und meldet sie als Textfehler auf `stderr`.
- Bei `--output` bleibt eine bestehende Zieldatei unveraendert.
- Render-Fehler umfassen HTML-Escaping-/Serialisierungsfehler und explizite Fehler aus der CLI-internen Render-Abstraktion.
- Tests nutzen einen werfenden oder fehlerliefernden Renderer-Stub, damit kein Adapter-spezifischer Sonderfall die Fehlergrenze umgeht.

## HTML-Validierung und Golden-Strategie

AP 1.4 nutzt keine externe HTML-Validator-Dependency.

Pflichtgates:

- Adaptertests pruefen Struktur, Escaping und Pflichtabschnitte.
- E2E-Tests vergleichen HTML-Ausgaben byte-stabil gegen Goldens.
- Ein repository-lokaler Struktur-Smoke darf zusaetzlich mit Python-Standardbibliothek pruefen, dass Goldens `<!doctype html>`, `html`, `head`, `body`, `main`, genau ein `h1`, keine `script`-Elemente und keine externen Ressourcen enthalten.
- Falls ein Struktur-Smoke entsteht, heisst das CTest-Target `html_structure_validation`.
- Der Struktur-Smoke nutzt ausschliesslich die Python-Standardbibliothek und fuehrt keine `pip install`-Schritte ein.
- CTest selbst installiert keine Systempakete und greift nicht auf das Netzwerk zu.

Goldens:

- HTML-Goldens liegen unter `tests/e2e/testdata/m5/html-reports/`.
- Das Manifest gleicht Verzeichnis und Eintraege beidseitig ab.
- Goldens duerfen keine Host-/Workspace-spezifischen Absolutpfade enthalten.
- Goldens mit absoluten Pfaden verwenden nur fixture-stabile synthetische Pfade wie `/project/...` oder gezielt kontrollierte Windows-Style-Strings.
- Goldens enthalten keine Zeitstempel, Hostnamen, zufaellige IDs oder Buildpfade.

## Implementierungsreihenfolge

Die Umsetzung erfolgt in drei verbindlichen Tranchen plus einer optionalen Haertungstranche. Jede Tranche endet mit einem vollstaendigen Lauf der Docker-Gates aus `README.md` ("Tests und Quality Gates") und `docs/quality.md`. Die globalen Abnahmekriterien dieses Plans gelten zusaetzlich am Ende von Tranche C.

### Tranche A - Vertrag, Escaping, CSS und Testskelett

Kein produktiver CLI-Adapter; Vertrag, Hilfsfunktionen und Testskelett.

1. HTML-Vertrag in `docs/report-html.md` festlegen, einschliesslich Dokumentgeruest, CSS, Escaping, Abschnittsreihenfolge, Tabellenstruktur, Leersaetzen, Accessibility-Basis und Golden-Regeln.
2. `tests/adapters/test_html_report_adapter.cpp` mit ersten Escape-, Struktur-, CSS- und Leersatztests anlegen.
3. HTML-Escaping-, Attribut-Escaping- und CSS-Hilfen file-local im Adapter implementieren; Extraktion in eine output-interne Utility ist in AP 1.4 nicht vorgesehen.
4. Statischen CSS-String festlegen und in Tests auf verbotene externe Ressourcen pruefen.
5. Optionalen Struktur-Smoke `tests/validate_html_reports.py` nur anlegen, wenn byte-stabile Goldens und Adaptertests nicht ausreichen; falls angelegt, nur mit Python-Standardbibliothek.
6. Manifest `tests/e2e/testdata/m5/html-reports/manifest.txt` anlegen, in Tranche A noch ohne vollstaendige CLI-Goldens.
7. `docs/quality.md` um den neuen HTML-Golden-/Struktur-Testumfang ergaenzen.

Abnahme Tranche A: alle Docker-Gates gruen; Escape-Tests pruefen `<`, `>`, `&`, Anfuehrungszeichen, Apostroph, Backslash, Newline und `<script>`-artige Eingaben; CSS-Tests pruefen keine externen Ressourcen und kein JavaScript.

Definition of Done Tranche A:

- [ ] `docs/report-html.md` dokumentiert den HTML-Vertrag.
- [ ] erste Adaptertests fuer Struktur, Escaping, CSS und Leersaetze existieren.
- [ ] CSS ist statisch, inline-faehig und frei von externen Ressourcen.
- [ ] Manifest fuer HTML-Goldens existiert.
- [ ] `docs/quality.md` beschreibt den HTML-Testumfang.
- [ ] Docker-Gates aus `README.md` und `docs/quality.md` sind gruen.

### Tranche B - Adapter, Wiring, CLI-Freischaltung

Der Adapter funktioniert; E2E-Goldens folgen erst in Tranche C.

1. `src/adapters/output/html_report_adapter.{h,cpp}` implementieren; Adapter rendert ausschliesslich aus `AnalysisResult`/`ImpactResult` und nutzt `top_limit` nur fuer Analyze.
2. Analyze-HTML mit Header, Inputs, Ranking, Hotspots, Target Metadata und Diagnostics implementieren.
3. Impact-HTML mit Header, Inputs, direkt/heuristisch betroffenen Translation Units, direkt/heuristisch betroffenen Targets und Diagnostics implementieren.
4. Leersaetze fuer alle leeren Sections implementieren.
5. Composition Root in `src/main.cpp` und Reportports um den HTML-Port erweitern; `ReportPorts` in `src/adapters/cli/cli_adapter.h` erhaelt ein explizites `html`-Feld oder eine gleichwertig typisierte Formatzuordnung.
6. CLI-Adapter `src/adapters/cli/cli_adapter.{h,cpp}` so anpassen, dass `--format html` als implementiert gilt, an den HTML-Port verdrahtet wird und mit `--output` ueber den AP-1.1-Atomic-Writer schreibt.
7. AP-1.1-Sperre fuer `--format html` entfernen.
8. Formatverfuegbarkeits- und Usage-Hinweise in `src/adapters/cli/cli_adapter.cpp` aktualisieren, sodass `html` bei verfuegbaren Formaten genannt wird und keine veraltete `html is recognized but not implemented`-Meldung uebrig bleibt.
9. `tests/adapters/test_port_wiring.cpp` so erweitern, dass `--format html` an den `HtmlReportAdapter` verdrahtet ist und nicht in den Console-Fallback faellt; bestehende `ReportPorts`-Testfixtures erhalten das neue `html`-Feld beziehungsweise die neue Formatzuordnung.
10. Regressionscheck, dass bestehende Console-/Markdown-/JSON-/DOT-Goldens unveraendert bleiben.

Abnahme Tranche B: alle Docker-Gates gruen; `--format html` produziert vollstaendiges HTML; `recognized but not implemented` nicht mehr fuer `html`; Console-, Markdown-, JSON- und DOT-Goldens byte-stabil.

Definition of Done Tranche B:

- [ ] `HtmlReportAdapter` rendert Analyze und Impact aus Ergebnisobjekten.
- [ ] `ReportPorts` und `src/main.cpp` verdrahten HTML ueber `ReportGenerator`.
- [ ] `--format html` ist fuer `analyze` und `impact` lauffaehig.
- [ ] `--format html --output <path>` nutzt den gemeinsamen Atomic-Writer-Pfad.
- [ ] stdout/stderr-Vertrag fuer erfolgreiche stdout- und Dateiausgabe ist getestet.
- [ ] Formatverfuegbarkeitsmeldungen enthalten keine HTML-Not-Implemented-Reste.
- [ ] bestehende Console-/Markdown-/JSON-/DOT-Goldens bleiben unveraendert.
- [ ] Docker-Gates aus `README.md` und `docs/quality.md` sind gruen.

### Tranche C - E2E-Goldens, CLI-/Stream-/Fehler-Tests, Beispiele und Nutzerdoku

Echte Binary-Verifikation und Vertragsfestschreibung der CLI-Ausgaben.

1. Analyze-Goldens unter `tests/e2e/testdata/m5/html-reports/` als statische Dateien anlegen. Abgedeckt sind mindestens:
   - Compile-Database-only.
   - File-API-only mit Target-Zuordnungen.
   - Mixed-Input.
   - Lauf ohne explizites `--top`, damit der CLI-Default-Top-Wert im HTML-Pfad abgedeckt ist.
   - `--top`-Fall ohne Kuerzung.
   - `--top`-Fall mit Ranking- und Hotspot-Kuerzung.
   - Hotspot-Kontext-Kuerzung pro Hotspot.
   - "leeres" Ergebnis mit dokumentierten Leersaetzen.
2. Impact-Goldens erzeugen. Abgedeckt sind mindestens:
   - Compile-Database-only mit relativem `--changed-file`.
   - File-API-only mit relativem `--changed-file`.
   - mindestens ein absolutes `--changed-file`, damit Quoting, Pfadnormalisierung und Labelbildung fuer absolute Pfade regressionsgeschuetzt sind. AP-1.2-/1.3-Lehre: das absolute Golden bekommt Per-Plattform-Varianten (zum Beispiel `impact_absolute.html` und `impact_absolute_windows.html`), weil POSIX `/project/...` als absolut akzeptiert, Windows aber `C:/project/...` verlangt. `tests/e2e/run_e2e.sh` waehlt das passende Golden ueber `case "$(uname -s)"`.
   - Mixed-Input.
   - direkte und heuristische Translation Units.
   - direkte und heuristische Targets.
   - keine betroffenen Targets.
   - keine Diagnostics.
3. Escaping-Goldens fuer Analyze oder Impact aufnehmen, mindestens mit `<`, `>`, `&`, Anfuehrungszeichen, Apostroph, Backslash, Newline und `<script>`-artigen Strings.
4. Manifest aktualisieren; der HTML-Golden- oder Struktur-Test gleicht Verzeichnis und Manifest beidseitig ab.
5. `tests/e2e/test_cli.cpp` erweitern um:
   - HTML-stdout-Vertrag, leeres stderr, vollstaendiges HTML.
   - `--format html --output <path>`: Datei geschrieben, stdout und stderr leer, atomar.
   - HTML-Render-Fehler ueber injizierten `CliReportRenderer`-Doppelgaenger; bestehende Zieldatei bleibt unveraendert, kein partielles HTML auf stdout.
   - Fehlerpfade: `impact --format html` ohne `--changed-file`, nicht vorhandene Eingaben, ungueltiges `compile_commands.json`, ungueltige File-API-Reply-Daten, Schreibfehler. Alle als Text auf stderr, nonzero Exit, kein HTML-Fehlerdokument.
   - Negativtest, dass `impact` keine `--top`-Option akzeptiert.
   - Negativtest, dass `--top 0 --format html` abgelehnt wird und kein HTML entsteht.
6. `tests/e2e/run_e2e.sh` und das CTest-Ziel `e2e_binary` um Binary-Smokes fuer `analyze --format html` und mindestens einen `impact --format html`-Fall ergaenzen, sodass die Verdrahtung inklusive `src/main.cpp` getestet ist.
7. Beispielberichte unter `docs/examples/` erzeugen oder als stabile Fixtures ablegen, mindestens `analyze.html` und `impact.html`.
8. `README.md` um HTML in Formatliste und Beispiele ergaenzen — als Pflichtbestandteil des Tranche-C-Commits, nicht als separater Folge-Commit (AP-1.2-/1.3-Lehre).
9. `docs/guide.md` um produktive Nutzung von `--format html` und `--format html --output` ergaenzen.
10. `docs/quality.md` um die in Tranche C neu hinzukommenden HTML-Golden-, Struktur- und E2E-Gates ergaenzen.
11. Coverage-, Lizard- und Clang-Tidy-Gates muessen nach AP 1.4 weiterhin gruen sein; neue Befunde aus dem HTML-Adapter werden in Tranche C behoben und nicht auf Tranche D verschoben.

Abnahme Tranche C: alle Docker-Gates aus `README.md` und `docs/quality.md` gruen; `e2e_binary` gruen; alle HTML-Goldens sind byte-stabil; Escaping-, Struktur-, Accessibility-Basis-, Top-Limit- und Leersatzvertraege sind durch Goldens und Adaptertests abgedeckt; globale Abnahmekriterien dieses Plans erfuellt.

Definition of Done Tranche C:

- [ ] Analyze- und Impact-HTML-Goldens decken die in Tranche C gelisteten Faelle ab.
- [ ] Escaping-Goldens decken HTML-Sonderzeichen und Whitespace-Normalisierung ab.
- [ ] leere Sections zeigen dokumentierte Leersaetze.
- [ ] `--output` schreibt nur in die Datei; stdout und stderr bleiben bei Erfolg leer.
- [ ] Fehlerpfade erzeugen Textfehler und kein HTML-Fehlerdokument.
- [ ] `e2e_binary` deckt `analyze --format html` und `impact --format html` ab.
- [ ] `docs/examples/` enthaelt stabile Analyze- und Impact-Beispielberichte.
- [ ] `README.md`, `docs/guide.md` und `docs/quality.md` sind aktualisiert.
- [ ] Coverage-, Lizard- und Clang-Tidy-Gates sind gruen.

### Tranche D - Optionale Haertung

Ohne diese Tranche gilt AP 1.4 als abgenommen, sobald Tranche C gruen ist.

- Zusaetzliche plattformspezifische Pfad-Edge-Cases, etwa Windows-Drives und UNC-aehnliche Strings.
- Weitere ASCII-Escaping-Edge-Cases.
- Druckdarstellung weiter verbessern, solange Goldens deterministisch bleiben.
- Optionaler HTML-Struktur-Smoke, falls er in Tranche C noch nicht noetig war.
- Zusaetzliche gezielte Regressionstests fuer Grenzfaelle, die ueber die verpflichtenden Coverage-, Lizard- und Clang-Tidy-Gates aus Tranche C hinausgehen.

Definition of Done Tranche D:

- [ ] Jede zusaetzliche Haertung ist durch einen fokussierten Test oder ein Golden abgesichert.
- [ ] Keine optionale Haertung veraendert den dokumentierten AP-1.4-Vertrag ohne Update von `docs/report-html.md`.
- [ ] Docker-Gates aus `README.md` und `docs/quality.md` bleiben gruen.

## Entscheidungen

Diese Entscheidungen sind vor Umsetzungsbeginn getroffen und in die Tranchen eingearbeitet:

- HTML ist ein statisches Ein-Datei-Artefakt. Begruendung: CI- und Release-Artefakte sollen ohne Begleitdateien nutzbar sein.
- Kein JavaScript. Begruendung: Der M5-Umfang ist Berichtserzeugung, keine Webanwendung; statische Tabellen reichen fuer den vorhandenen Analyseumfang.
- Keine externen Ressourcen. Begruendung: Berichte muessen offline und in gesperrten CI-Umgebungen funktionieren.
- `HtmlReportAdapter` bleibt ein `ReportWriterPort` und wird ueber `ReportGenerator` als `GenerateReportPort` verdrahtet. Begruendung: Das folgt dem bestehenden Console-/Markdown-/JSON-/DOT-Pfad und vermeidet einen Sonderweg in der CLI.
- Analyze nutzt `top_limit` als Renderparameter. Begruendung: Das entspricht dem etablierten `GenerateReportPort::generate_analysis_report(result, top_limit)`-Vertrag und vermeidet CLI-Kontext im Adapter.
- Impact erhaelt kein `--top`. Begruendung: M5 begrenzt nur Analyze-Reportlisten; fachliche Impact-Ergebnislisten bleiben vollstaendig.
- HTML nutzt `ReportInputs` fuer Provenienz. Begruendung: Neue M5-Artefaktadapter duerfen nicht auf Legacy-Presentation-Felder fuer Console/Markdown zurueckfallen.
- CSS bleibt statisch und file-local. Begruendung: AP 1.4 braucht kein Frontend-Asset-System und keine neue Build-Abhaengigkeit.
- Python-Struktur-Smoke bleibt standardbibliotheks-only, falls er eingefuehrt wird. Begruendung: HTML-Gates sollen keine neue Dependency-Kette neben dem JSON-Schema-Validator einfuehren.
- Keine neuen pip-Dependencies in AP 1.4. Begruendung: AP-1.2-Lehre: jeder neue Python-Validator zog Hash-Pinning und einen `requirements*.txt`-Eintrag im Workflow nach sich. AP 1.4 verzichtet darauf, indem der optionale Struktur-Smoke ausschliesslich `html.parser`, `re` und `pathlib` aus der Standardbibliothek nutzt; falls trotzdem eine externe Dependency entsteht, ist sie zwingend ueber `pip-compile --generate-hashes` zu pinnen, bevor sie in den CI-Workflows aktiviert wird.
- README-Pflicht in Tranche C: Header, Feature-Liste und "nicht Ziel"-Liste werden gleichzeitig mit der HTML-Freischaltung aktualisiert. Begruendung: AP-1.2-/1.3-Lehre, dass die README-Aktualisierung sonst als separater Folge-Commit nachgezogen werden muss; Tranche C bringt sie in den Hauptcommit.
- Per-Plattform-Goldens fuer absolute Pfadeingaben. Begruendung: AP-1.2-/1.3-Lehre nach Windows-MSYS-CI-Failure: `/project/...` ist auf POSIX absolut, auf Windows aber relativ; ein einziges Golden kann beide Plattformen nicht byte-stabil abdecken. AP 1.4 fuehrt `*_windows.html`-Varianten ein, sobald ein Goldenfall absolute `--changed-file`-Pfade verwendet, und `tests/e2e/run_e2e.sh` waehlt ueber `case "$(uname -s)"` das passende Golden.

## Tests

Adaptertests:

- `tests/adapters/test_html_report_adapter.cpp` prueft Analyze- und Impact-Serialisierung.
- Tests pruefen `<!doctype html>`, `html lang="en"`, `head`, `body`, `main`, genau ein `h1`.
- Tests pruefen `meta name="xray-report-format-version"` und `main[data-format-version]` gegen `xray::hexagon::model::kReportFormatVersion`.
- Tests pruefen keinen `script`-Tag, keine `@import`-Regeln und keine `url(...)`-Ressourcen.
- Tests pruefen Pflichtsections und Section-Reihenfolge.
- Tests pruefen Tabellenstruktur mit `thead`, `tbody`, `th scope="col"` und stabiler Spaltenreihenfolge.
- Tests pruefen HTML-Escaping fuer Textknoten und Attributwerte.
- Tests pruefen Whitespace-Normalisierung fuer Newline, CRLF, Carriage Return, Tab und ASCII-Control-Zeichen in Diagnostics, Pfaden und Target-Namen.
- Tests pruefen keine unescaped `<script>`-artigen Strings.
- Tests pruefen CSS-Klassen als feste Literale ohne nutzergelieferte Werte.
- Tests pruefen Leersaetze fuer jede leere Section.
- Tests pruefen Target-Darstellung mit `display_name`, `type` und stabiler Sortierung.
- Tests pruefen Translation Units mit gleichem `source_path` und unterschiedlichem `directory`.
- Tests pruefen gleichnamige Targets mit unterschiedlichem `type`.
- Tests pruefen reportweite Diagnostics und Item-Diagnostics mit Severity-Text.

Top-Limit- und Listen-Tests:

- Analyze-Tests pruefen, dass Ranking und Hotspots `top_limit` beachten.
- Analyze-Tests pruefen Default-`top_limit` ueber einen CLI- oder E2E-Fall ohne explizites `--top`.
- Analyze-Tests pruefen Hotspot-Kontext-Kuerzung pro Hotspot.
- CLI-Tests pruefen, dass `--top 0` auch fuer `--format html` abgelehnt wird und kein HTML entsteht.
- Impact-Tests pruefen, dass Impact-Listen nicht ueber `top_limit` begrenzt werden.

Port- und Wiring-Tests:

- `tests/adapters/test_port_wiring.cpp` prueft, dass `html` an den `HtmlReportAdapter` verdrahtet ist.
- Port-/Wiring-Tests pruefen ausdruecklich, dass `--format html` nicht in den bestehenden Console-Fallback faellt und keinen Console-Adapter nutzt.
- Tests pruefen, dass `--format html` nach AP 1.4 nicht mehr den AP-1.1-Fehler `recognized but not implemented` liefert.
- Tests pruefen, dass Usage-/Formatverfuegbarkeitsmeldungen `html` nicht mehr als nicht implementiert ausweisen.
- Binary-Smoke-Tests ueber `tests/e2e/run_e2e.sh` und das CTest-Ziel `e2e_binary` pruefen, dass auch die echte Binary inklusive `src/main.cpp` den HTML-Adapter verdrahtet.

E2E-Golden-Tests:

- E2E-Tests erzeugen HTML-Ausgaben mit der echten CLI.
- Die erzeugten HTML-Reports werden byte-stabil gegen Golden-Dateien verglichen.
- Reine Adaptertests reichen nicht aus.
- Mindestens ein Analyze-Golden laeuft ohne explizites `--top` und deckt den CLI-Default-Top-Wert ab.
- HTML-Goldens decken Compile-Database-only-, File-API-only- und Mixed-Input-Laeufe bei `analyze` ab.
- HTML-Goldens decken Compile-Database-only-, File-API-only- und Mixed-Input-Laeufe bei `impact` ab.
- Analyze-Goldens pruefen Ranking-, Hotspot-, Target-Metadata- und Diagnostics-Sections.
- Impact-Goldens pruefen direkte und heuristische Translation Units sowie direkte und heuristische Targets.
- Escaping-Goldens pruefen Sonderzeichen in Pfaden, Target-Namen und Diagnostics.
- Goldens pruefen Leersaetze.
- Goldens pruefen keine externen Ressourcen und kein JavaScript.

CLI- und Stream-Tests:

- `analyze --format html` liefert vollstaendiges HTML auf stdout und leeres stderr.
- `impact --format html` liefert vollstaendiges HTML auf stdout und leeres stderr.
- Binary-E2E-Smokes fuehren `analyze --format html` und mindestens einen `impact --format html`-Fall ueber die gebaute `cmake-xray`-Binary aus, nicht nur ueber direkt instanziierte CLI-Adaptertests.
- `--format html --output <path>` schreibt vollstaendiges HTML in die Datei.
- Erfolgreiche `--format html --output <path>`-Aufrufe lassen stdout und stderr leer.
- Tests pruefen, dass `--format html` tatsaechlich HTML rendert und nicht den Console-Reportpfad trifft.
- Tests pruefen, dass `impact --format html --top ...` abgelehnt wird, solange `impact --top` nicht Teil des M5-Vertrags ist.

Fehlerpfad-Tests:

- `impact --format html` ohne `--changed-file` liefert Text auf stderr, nonzero Exit und kein HTML-Fehlerdokument.
- Nicht vorhandene Eingabepfade liefern Text auf stderr, nonzero Exit und kein HTML-Fehlerdokument.
- Ungueltiges Compile-Database-JSON liefert Text auf stderr, nonzero Exit und kein HTML-Fehlerdokument.
- Ungueltige CMake-File-API-Reply-Daten liefern Text auf stderr, nonzero Exit und kein HTML-Fehlerdokument.
- `changed_file_source=unresolved_file_api_source_root` liefert Text auf stderr, nonzero Exit und kein HTML-Fehlerdokument.
- Schreibfehler liefern Text auf stderr, nonzero Exit und kein HTML-Fehlerdokument.
- CLI-/Schreibpfad-Tests decken einen simulierten HTML-Render-Fehler vor dem Atomic-Writer ab.
- Der simulierte Render-Fehler prueft nonzero Exit, Text auf stderr, leeres stdout und unveraenderte bestehende Zieldatei bei `--output`.
- Tests pruefen, dass der HTML-Adapter vor jeder Emission vollstaendig in einen String rendert und kein partieller stdout-Report entstehen kann, wenn der Renderer vor Rueckgabe fehlschlaegt.

Atomic-Writer-Tests:

- HTML-`--output` nutzt den gemeinsamen M5-konformen Atomic-Writer aus AP 1.1.
- AP 1.4 dupliziert keine reinen Atomic-Replace-Unit-Tests aus AP 1.1.
- HTML-spezifisch erforderlich ist ein CLI-/Wiring-Smoke, der nachweist, dass `--format html --output <path>` den gemeinsamen Atomic-Writer-Pfad nutzt, stdout und stderr leer laesst und den Report nicht zusaetzlich auf stdout dupliziert.

Rueckwaertskompatibilitaets-Tests:

- Bestehende Console-/Markdown-/JSON-/DOT-Goldens bleiben unveraendert.
- HTML-Freischaltung fuehrt keine Verbosity-Parameter in Reportports oder Artefaktadapter ein.
- HTML-Freischaltung fuehrt keine Target-Graph-Semantik ein.

## Abnahmekriterien

AP 1.4 ist abnahmefaehig, wenn:

- `cmake-xray analyze --format html` und `cmake-xray impact --format html` lauffaehig sind.
- `--format html --output <path>` ueber den gemeinsamen Atomic-Writer schreibt.
- Erfolgreiche HTML-Stdout-Ausgaben keine unstrukturierten Zusatztexte enthalten.
- Erfolgreiche HTML-Dateiausgaben stdout und stderr leer lassen.
- `docs/report-html.md` vorhanden ist und den HTML-Vertrag dokumentiert.
- HTML-Reports vollstaendige HTML5-Dokumente mit inline CSS sind.
- HTML-Reports keine externen Ressourcen, kein JavaScript und keine HTML-Kommentare enthalten.
- Alle nutzergelieferten Inhalte und Pfade korrekt HTML-escaped werden.
- Whitespace in fachlichen Textwerten nach dem dokumentierten einheitlichen Vertrag normalisiert wird.
- Dokument-, Abschnitts-, Tabellen- und Attributreihenfolge byte-stabil und dokumentiert sind.
- `format_version` in HTML-Metadaten den Wert von `xray::hexagon::model::kReportFormatVersion` verwendet.
- Analyze-HTML die Sections Header, Inputs, Ranking, Hotspots, Target Metadata und Diagnostics enthaelt.
- Impact-HTML die Sections Header, Inputs, direkt/heuristisch betroffene Translation Units, direkt/heuristisch betroffene Targets und Diagnostics enthaelt.
- Analyze-HTML den wirksamen `top_limit` fuer Ranking, Hotspots und Hotspot-Kontext beachtet.
- Impact-HTML keine implizite `top_limit`-Begrenzung einfuehrt.
- Leere Sections dokumentierte Leersaetze statt leerer Tabellen ausgeben.
- Target-Zuordnungen sichtbar dargestellt werden, sofern vorhanden.
- Gleichnamige Targets mit unterschiedlichem `type` unterscheidbar bleiben.
- Parser-, Eingabe-, Render- und Schreibfehler als Textfehler ohne HTML-Fehlerdokument getestet sind.
- Echte CLI-Ausgaben gegen Goldens verglichen werden.
- Binary-E2E-Smokes die HTML-Verdrahtung der echten Binary inklusive `src/main.cpp` abdecken.
- Beispielberichte unter `docs/examples/` vorhanden sind.
- `README.md`, `docs/guide.md` und `docs/quality.md` die produktive HTML-Nutzung und die neuen Gates auffuehren.
- Coverage-, Lizard- und Clang-Tidy-Gates nach AP 1.4 gruen bleiben.
- Console-, Markdown-, JSON- und DOT-Verhalten unveraendert bleibt.

## Offene Folgearbeiten

Folgearbeiten ausserhalb von AP 1.4:

- AP 1.5 definiert Quiet-/Verbose-Verhalten.
- M6 kann interaktive oder graphische HTML-Erweiterungen nur einfuehren, wenn dafuer ein eigener Vertrag, Tests und Sicherheitsregeln entstehen.
- Eine spaetere HTML-Vertragsversion kann optional clientseitige Features, weitere Accessibility-Checks oder Diagnostic-Visualisierungen einfuehren, muss dann aber `docs/report-html.md`, Goldens und Tests entsprechend migrieren.

**Ergebnis**: `cmake-xray` kann Analyse- und Impact-Ergebnisse als portables HTML-Artefakt fuer Reviews, CI und Dokumentation bereitstellen.
