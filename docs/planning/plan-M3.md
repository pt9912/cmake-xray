# Plan M3 - Markdown-Reports, Referenzdaten und MVP-Abnahme (`v1.0.0`)

## 0. Dokumenteninformationen

| Feld | Wert |
|---|---|
| Dokument | Plan M3 `cmake-xray` |
| Version | `0.7` |
| Stand | `2026-04-22` |
| Status | Abgeschlossen |
| Referenzen | [Lastenheft](../../spec/lastenheft.md), [Design](../../spec/design.md), [Architektur](../../spec/architecture.md), [Phasenplan](./roadmap.md), [Plan M2](./plan-M2.md), [Qualitaet](../user/quality.md), [Releasing](../user/releasing.md) |

### 0.1 Zweck
Dieser Plan beschreibt die konkreten Schritte fuer Milestone M3 (`v1.0.0`). Ziel ist der lieferfaehige MVP-Stand: Die in M2 eingefuehrten fachlichen Analysen sollen als stabile Markdown-Berichte exportierbar sein, ueber versionierte Referenzdaten und erwartete Ausgaben abgesichert werden und in einer dokumentierten Referenzumgebung pruefbar bleiben.

### 0.2 Abschlusskriterium
M3 gilt als erreicht, wenn:

- `cmake-xray analyze` und `cmake-xray impact` neben der Konsolenausgabe auch einen deterministischen Markdown-Report erzeugen koennen
- das Ausgabeformat und ein optionaler Ausgabepfad per CLI steuerbar sind, ohne die bestehende Exit-Code-Semantik fuer Eingabefehler zu verwischen
- Markdown-Berichte die M2-Ergebnisse vollstaendig und reproduzierbar abbilden: Analysekontext, TU-Ranking, Include-Hotspots, Impact-Ergebnis sowie Diagnostics und Heuristik-Hinweise
- versionierte Referenzdaten und erwartete Konsolen- bzw. Markdown-Ausgaben fuer die zentralen Analysepfade sowie fuer regressionskritische M2-Randfaelle zu Pfadsemantik, TU-Identitaet und deterministischer Sortierung vorliegen und automatisiert geprueft werden
- README, Beispielausgaben, CHANGELOG und Release-Dokumentation den M3-/MVP-Stand widerspiegeln
- eine dokumentierte Referenzumgebung sowie Baseline-Messwerte fuer `250`, `500` und `1.000` Translation Units fuer `analyze` in den MVP-relevanten Ausgabeformaten `console` und `markdown` vorliegen, zusaetzlich dokumentierte Stichproben fuer `impact --format markdown` vorhanden sind und die Soll-Ziele aus `NF-04` und `NF-05` explizit bewertet werden
- die Docker-Pruefpfade fuer Test, Coverage-Gate, statische Analyse-/Metrik-Reports und Quality-Gate mit dem M3-Stand erfolgreich durchlaufen

Relevante Kennungen: `F-26`, `F-27`, `F-35`, `F-36`, `F-42`, `NF-04`, `NF-05`, `NF-06`, `NF-10`, `NF-18`, `NF-19`, `AK-03`, `AK-04`, `AK-05`, `AK-06`, `AK-08`, `AK-09`

### 0.3 Abgrenzung
Bestandteil von M3 sind:

- Markdown-Report-Erzeugung fuer Projektanalyse und Impact-Analyse
- CLI-Steuerung fuer Ausgabeformat und optionalen Ausgabepfad
- stabile, diffbare Report-Struktur fuer Menschen und CI-Artefakte
- versionierte Referenzdaten und Golden-Outputs fuer Kernfaelle
- dokumentierte Referenzumgebung und reproduzierbare Performance-Messung
- Beispielausgaben, Nutzungsdokumentation und Release-Vorbereitung fuer `v1.0.0`

Nicht Bestandteil von M3 sind:

- neue fachliche Analysearten jenseits von M2
- targetbezogene Zusatzdaten und targetbezogene Impact-Erweiterungen (`F-18` bis `F-20`, `F-24`, `F-25`)
- HTML-, JSON- oder DOT-Ausgaben (`F-28` bis `F-30`)
- `--quiet` sowie ein umfassender `--verbose`-Ausbau (`F-39`, `F-40`), sofern dies nicht zur M3-Dokumentation oder Fehlersichtbarkeit zwingend noetig ist
- Schema- oder Formatversionierung fuer maschinenlesbare Ausgabeformate (`NF-20`)

M3 baut auf den Ergebnissen aus M2 auf. Der Milestone dient der Stabilisierung, Exportierbarkeit und Lieferfaehigkeit des MVP und nicht der Einfuehrung eines zweiten Analysekerns.

## 1. Arbeitspakete

### 1.1 Report-Vertraege, Formatwahl und Verantwortungsgrenzen schaerfen

Die fuer M2 ausreichende Report-Strecke ist fuer M3 zu schmal. Neben Konsolenausgabe muessen jetzt auch persistierbare Markdown-Berichte erzeugt werden, ohne dass CLI, Dateisystem und Kern erneut eng gekoppelt werden.

Fuer M3 benoetigt der Report-Pfad mindestens:

- eine explizite Formatwahl in CLI und Composition Root fuer `console` und `markdown`; die Entscheidung ueber den aktiven Renderer erfolgt ausserhalb des Hexagons
- getrennte, fachlich klare Eintrittspunkte fuer Projektanalyse und Impact-Analyse; bestehende M2-Entscheidungen dazu bleiben erhalten
- eine stabile Verantwortungsgrenze: Der Report-Adapter erzeugt Text, die Entscheidung ueber `stdout` oder Dateipfad bleibt ausserhalb des Hexagons
- dieselben Kernmodelle und Diagnostics fuer Konsole und Markdown; Reporter duerfen keine eigene Fachlogik oder abweichende Ergebnisaggregation einfuehren

Wichtig:

- das Hexagon bleibt frei von Dateisystem- und CLI-Typen
- die Formatauswahl zur Laufzeit wird fuer M3 ausschliesslich in CLI und Composition Root festgelegt; ein neues Hexagon-Modell wie `ReportFormat` oder eine zusaetzliche Huelse wie `RenderedReport` ist dafuer nicht erforderlich
- die konkreten Format-Adapter bleiben reine Text-Renderer; sie kennen weder CLI-Optionen noch Dateisystem-Sinks und enthalten keine Dispatch-Logik
- die bestehenden textbasierten Report-Ports koennen in M3 erhalten bleiben; Port-Churn ist nur dann gerechtfertigt, wenn er fuer gemeinsam genutzte Hilfslogik oder klarere Fehlerpfade tatsaechlich noetig ist
- Ausgabe- oder Schreibfehler duerfen nicht als Eingabefehler maskiert werden; der bisher reservierte Exit-Code `1` wird in M3 fuer unerwartete Laufzeit- bzw. Report-Schreibfehler aktiviert
- M3 schaerft die Diagnostics-Vertraege: `AnalysisResult::diagnostics` und `ImpactResult::diagnostics` tragen nur reportweite Hinweise; elementbezogene Diagnostics verbleiben an Ranking-Eintraegen bzw. Include-Hotspots. Falls M2 noch Doppelerfassungen zwischen Elementen und Report-Ebene enthaelt, werden diese vor dem Rendern im Kern bereinigt. Die in M2 im `ConsoleReportAdapter` enthaltene Deduplikationslogik (`collect_displayed_diagnostics` / `collect_remaining_diagnostics`) wird dadurch obsolet und muss entsprechend angepasst oder entfernt werden, sobald der Kern die Bereinigung uebernimmt
- die Reihenfolge von Abschnitten, Listen und Diagnostics muss explizit dokumentiert und nicht vom Zufall der Eingabereihenfolge abhaengig sein; wenn Reporter selbst nicht sortieren, muss der Kern fuer reportweite Diagnostics eine stabile Reihenfolge liefern. Fuer M3 gilt als verbindliche Sortierregel: reportweite Diagnostics werden im Kern lexikographisch nach `(severity, message)` sortiert, bevor sie an das Ergebnis angehaengt werden; `warning` sortiert vor `note`
- die Ableitung der `Impact classification` fuer den Markdown-Report erfolgt deterministisch durch den Reporter aus den bestehenden Modellfeldern, ohne ein neues Hexagon-Modell: `direct` gilt, wenn `ImpactResult::heuristic == false` und mindestens ein Treffer vorliegt; `heuristic` gilt, wenn `ImpactResult::heuristic == true` und mindestens ein Treffer vorliegt; `uncertain` gilt, wenn keine Treffer vorliegen und `ImpactResult::heuristic == true`. Falls keine Treffer vorliegen und `ImpactResult::heuristic == false`, gilt `direct`, weil das Ergebnis vollstaendig ohne Heuristik ermittelt wurde. Da diese Ableitung rein auf bestehenden Modellwerten operiert und keine eigene Fachlogik einfuehrt, ist sie fuer Reporter zulaessig

Vorgesehene Artefakte:

- punktuelle Anpassung von `src/hexagon/ports/driving/generate_report_port.h`, falls fuer gemeinsame Hilfslogik oder klarere Fehlerpfade erforderlich
- punktuelle Anpassung von `src/hexagon/ports/driven/report_writer_port.h`, falls fuer gemeinsame Hilfslogik oder klarere Fehlerpfade erforderlich
- punktuelle Anpassung von `src/hexagon/services/report_generator.*`, falls fuer gemeinsame Hilfslogik oder klarere Fehlerpfade erforderlich
- Anpassung der CLI-/Composition-Root-Verdrahtung fuer die Formatwahl
- Anpassung von `src/main.cpp`

**Ergebnis**: Der Kern besitzt einen belastbaren Report-Vertrag, ohne fuer M3 neue Hexagon-Modelle fuer die Formatwahl, einen zusaetzlichen Dateisystem-Port oder einen Adapter-zu-Adapter-Dispatcher einzufuehren.

### 1.2 `MarkdownReportAdapter` fuer Projekt- und Impact-Berichte umsetzen

M3 fuehrt den im Architekturzielbild vorgesehenen `MarkdownReportAdapter` ein. Der Adapter muss dieselben fachlichen Ergebnisse wie der Konsolenreport darstellen, aber in einer diffbaren, gut verlinkbaren und fuer CI-Artefakte geeigneten Struktur.

Ein Markdown-Bericht fuer `analyze` soll genau folgende Hauptteile in genau dieser Reihenfolge enthalten:

- Titelzeile exakt `# Project Analysis Report`
- genau einen Leerblock
- eine flache Uebersichts-Liste mit exakt diesen Labels in dieser Reihenfolge:
  - `Report type: analyze`
  - `Compile database: <display path>`
  - `Translation units: <count>`
  - `Translation unit ranking entries: <shown> of <total>`
  - `Include hotspot entries: <shown> of <total>`
  - `Top limit: <n>`
  - `Include analysis heuristic: yes|no`
- Abschnitt exakt `## Translation Unit Ranking`
- Abschnitt exakt `## Include Hotspots`
- Abschnitt exakt `## Diagnostics`

Ein Markdown-Bericht fuer `impact` soll genau folgende Hauptteile in genau dieser Reihenfolge enthalten:

- Titelzeile exakt `# Impact Analysis Report`
- genau einen Leerblock
- eine flache Uebersichts-Liste mit exakt diesen Labels in dieser Reihenfolge:
  - `Report type: impact`
  - `Compile database: <display path>`
  - `Changed file: <display path>`
  - `Affected translation units: <count>`
  - `Impact classification: direct|heuristic|uncertain`
- fuer M3 ist die Semantik von `Impact classification` fest definiert:
  - die Klassifikation macht die im Design unterschiedenen Ergebnisklassen fuer Goldens und Beispiele explizit sichtbar, statt sie in einem Bool-Wert zusammenzufalten
  - `direct` ist nur zulaessig, wenn mindestens eine betroffene Translation Unit direkt ueber einen bekannten Quelldateipfad gefunden wurde und fuer das Ergebnis keine include-basierte Heuristik benoetigt wurde
  - `heuristic` gilt, sobald mindestens eine betroffene Translation Unit ueber den Include-Graphen abgeleitet wurde; direkte und heuristische Treffer duerfen gemeinsam vorkommen, die Ergebnis-Klassifikation bleibt dann `heuristic`
  - `uncertain` gilt, wenn keine betroffene Translation Unit gefunden wurde und der Null-Treffer deshalb nur unter heuristischer bzw. unvollstaendiger Datengrundlage bewertbar ist; die Datenluecke wird zusaetzlich ueber `## Diagnostics` erklaert
- bei `0` betroffenen Translation Units direkt nach der Uebersicht die exakte Zeile `No affected translation units found.`
- Abschnitt exakt `## Directly Affected Translation Units`
- Abschnitt exakt `## Heuristically Affected Translation Units`
- Abschnitt exakt `## Diagnostics`

Formatregeln fuer M3:

- Ausgabe als einfaches, breit kompatibles Markdown ohne HTML-Einbettung
- in M3 werden bewusst nur ATX-Ueberschriften und Listen verwendet; Markdown-Tabellen gehoeren nicht zum Report-Vertrag
- die Uebersichts-Liste verwendet immer eine ungeordnete Liste; `## Diagnostics` sowie die Impact-Abschnitte verwenden bei vorhandenen Eintraegen ungeordnete Listen; jede Eintragszeile beginnt exakt mit `- `
- keine ANSI-Farben, keine Zeitstempel, keine Hostnamen und keine anderen volatilen Metadaten
- `Tool version` wird bewusst nicht in den Report-Body aufgenommen; Release-Provenienz bleibt in `--help`, README, CHANGELOG und `application_info`, und reine Versionshebungen duerfen keine Report-Golden-Files invalidieren
- zwischen Titel, Uebersichts-Liste und jedem `##`-Abschnitt steht jeweils genau ein Leerblock; innerhalb eines Abschnitts gibt es keine zusaetzlichen Leerzeilen zwischen Geschwistern derselben Liste
- deterministische Abschnitts- und Listensortierung
- alle Listen werden exakt in der Reihenfolge der bereits im Kern vorliegenden Ergebnisvektoren ausgegeben; Reporter fuehren in M3 keine zusaetzliche Sortierschicht fuer `translation_units`, `include_hotspots`, deren `affected_translation_units` oder Diagnostics ein
- fuer `impact` gilt diese Regel ausdruecklich auch fuer reportweite Diagnostics: `ImpactResult::diagnostics` muss bereits im Kern in stabiler, von der Eingabereihenfolge unabhaengiger Reihenfolge vorliegen, damit Markdown- und Konsolen-Goldens bei permutierten `compile_commands.json` bytegleich bleiben koennen
- `Top limit: <n>` gibt immer den konfigurierten numerischen CLI-Wert wieder; ob Ranking- oder Hotspot-Listen vollstaendig sind, wird ausschliesslich ueber `Translation unit ranking entries: <shown> of <total>` und `Include hotspot entries: <shown> of <total>` ausgewiesen, nicht ueber einen synthetischen Wert wie `all`
- der Abschnitt `Translation Unit Ranking` wird immer als geordnete Liste ausgegeben; jeder Eintrag besteht exakt aus einer ersten Zeile `<rank>. <source_path> [directory: <directory>]`, einer unmittelbar folgenden Zeile `Metrics: arg_count=<n>, include_path_count=<n>, define_count=<n>` und optional einem unmittelbar folgenden Block `Diagnostics:`; alle Fortsetzungszeilen werden exakt so weit eingerueckt, dass sie syntaktisch zur geordneten Liste gehoeren und unter dem ersten Zeichen nach `<rank>. ` beginnen. Das bedeutet fuer `1.` bis `9.` vier Leerzeichen, fuer `10.` bis `99.` fuenf und ab `100.` sechs. Diese Regel dient primaer der bytegenauen Golden-Absicherung; gaengige Markdown-Renderer tolerieren auch leicht abweichende Einrueckung
- der Abschnitt `Include Hotspots` wird immer als geordnete Liste ausgegeben; jeder Eintrag besteht exakt aus einer ersten Zeile `<index>. Header: <path>`, danach aus den Fortsetzungszeilen `Affected translation units: <n>` und `Translation units:`; jede TU-Zeile lautet `- <source_path> [directory: <directory>]`, ein optionaler `Diagnostics:`-Block enthaelt Zeilen `- <severity>: <message>`. Alle Fortsetzungszeilen werden exakt so weit eingerueckt, dass sie syntaktisch zur geordneten Liste gehoeren und unter dem ersten Zeichen nach `<index>. ` beginnen; fuer einstellige, zweistellige und dreistellige Indizes gilt dieselbe Einrueckungsregel wie im Ranking
- die Impact-Abschnitte `Directly Affected Translation Units` und `Heuristically Affected Translation Units` werden bei vorhandenen Eintraegen als ungeordnete Listen ausgegeben; jeder Eintrag besteht exakt aus `- <source_path> [directory: <directory>]`, die Impact-Art wird nicht nochmals inline wiederholt, weil sie bereits ueber den Abschnittstitel kodiert ist
- M3 trennt explizit zwischen elementbezogenen und reportweiten Diagnostics: Inline `Diagnostics:`-Bloecke sind nur fuer elementbezogene Diagnostics zulaessig, also bei einzelnen Ranking-Eintraegen oder Include-Hotspots; der Abschnitt `## Diagnostics` enthaelt ausschliesslich echte reportweite Hinweise aus `AnalysisResult::diagnostics` bzw. `ImpactResult::diagnostics`
- der Abschnitt `## Diagnostics` wird bei vorhandenen Eintraegen als ungeordnete Liste ausgegeben und enthaelt ausschliesslich reportweite Diagnostics aus `AnalysisResult::diagnostics` bzw. `ImpactResult::diagnostics` in Modellreihenfolge; jeder Eintrag besteht exakt aus `- <severity>: <message>`
- inline `Diagnostics:`-Bloecke werden in M3 nicht zusaetzlich im globalen Abschnitt `## Diagnostics` wiederholt
- fuer `impact` gibt es in M3 keine elementbezogenen Diagnostics pro betroffener Translation Unit; Heuristik und Datenluecken dieses Analysepfads muessen deshalb mindestens ueber `Impact classification:` in der Uebersicht sichtbar sein und koennen im Abschnitt `## Diagnostics` detailliert ausgefuehrt werden
- leere Abschnitte sind die einzige Ausnahme von den Listenregeln und verwenden exakt diese Saetze: `No include hotspots found.`, `No directly affected translation units.`, `No heuristically affected translation units.`, `No diagnostics.`
- pfadtragende Felder verwenden Anzeige-Pfade, nicht Vergleichsschluessel und nicht rohe CLI-Eingaben. Fuer M3 gilt die M2-Trennung unveraendert:
  - `Compile database:` zeigt den lexikalisch normalisierten Aufrufpfad der uebergebenen `compile_commands.json`
  - `Changed file:` zeigt denselben normalisierten Anzeige-Pfad wie die Konsolenausgabe; relative `--changed-file`-Eingaben werden fuer Vergleiche relativ zum Verzeichnis der `compile_commands.json` aufgeloest und erst danach als Anzeige-Pfad gerendert
  - TU- und Header-Pfade werden mit derselben Anzeige-Helferlogik wie in der Konsole ausgegeben
- dynamische Pfad- und Meldungstexte werden fuer Markdown maskiert; mindestens Backslash, Backtick, Stern, Unterstrich, eckige Klammern, fuehrende `#` sowie fuehrende Listenmarker werden escaped
- Pfadangaben werden lexikalisch normalisiert und fuer Golden-Tests in einer kanonischen Aufrufform erzeugt: Die bytegenauen Referenzdateien entstehen aus einem festen Arbeitsverzeichnis mit fixture-relativen Eingabepfaden; Docker-Pruefungen verifizieren denselben Inhaltspfad funktional, aber nicht ueber identische Pfadstrings
- genau ein abschliessender Zeilenumbruch
- Heuristik-Hinweise und Datenluecken folgen derselben Platzierungsregel wie andere Diagnostics: elementbezogen inline, reportweit im Abschnitt `## Diagnostics`; komprimierte Statusfelder in der Uebersicht wie `Include analysis heuristic:` oder `Impact classification:` sind zulaessige Ergebniszusammenfassungen und keine zusaetzliche Diagnostik-Spiegelung

Ein moeglicher M3-Zielaufruf ist:

```text
cmake-xray analyze --compile-commands <path> --format markdown
cmake-xray analyze --compile-commands <path> --format markdown --output report.md
cmake-xray impact --compile-commands <path> --changed-file <path> --format markdown --output impact.md
```

Vorgesehene Artefakte:

- neue Dateien `src/adapters/output/markdown_report_adapter.h` und `src/adapters/output/markdown_report_adapter.cpp`
- Anpassung der CLI-/Composition-Root-Verdrahtung fuer die Wahl zwischen formatgebundenen Report-Strecken
- Anpassung des bestehenden `ConsoleReportAdapter`, damit Konsolen-Goldens stabil sind: Entfernung der reporterseitigen Diagnostics-Deduplikation zugunsten der Kern-Bereinigung, gegebenenfalls Angleichung der Vollstaendigkeitsangaben (`top X of Y`) an das gemeinsame Informationsmodell beider Formate, sowie Extraktion gemeinsamer Hilfsfunktionen fuer Layout, Diagnostics oder Pfaddarstellung
- Anpassung von `src/adapters/CMakeLists.txt`

**Ergebnis**: Projekt- und Impact-Ergebnisse koennen als stabiler Markdown-Text erzeugt werden, ohne fachliche Informationen gegenueber der Konsole zu verlieren.

### 1.3 CLI fuer `--format` und `--output` erweitern

Die Berichtserzeugung muss fuer Nutzer uebersichtlich bleiben. M3 erweitert deshalb die bestehenden Unterkommandos `analyze` und `impact`, statt einen separaten Parallelpfad einzufuehren.

Entscheidung fuer M3:

- Standard bleibt `--format console`
- unterstuetzte Werte fuer `--format` sind `console` und `markdown`
- `--output <path>` ist nur in Verbindung mit `--format markdown` zulaessig
- wenn `--format markdown` ohne `--output` verwendet wird, wird der Markdown-Text auf `stdout` geschrieben
- wenn `--output <path>` verwendet wird, schreibt die CLI den Report ausschliesslich dorthin; bei Erfolg bleibt `stdout` leer
- fuer `--output <path>` gilt in M3 ein stabiler Dateivertrag: geschrieben wird ueber eine temporaere Datei im Zielverzeichnis und der Zielpfad wird erst nach vollstaendig erfolgreichem Schreiben ersetzt; bei Fehler bleibt der Zielpfad unveraendert oder fehlt weiterhin, unter dem Zielnamen darf kein teilgeschriebener oder truncierter Report sichtbar werden. Die temporaere Datei verwendet ein dokumentiertes Namensmuster mit dem Praefix `.cmake-xray-` im Zielverzeichnis, damit bei einem Abbruch zurueckbleibende Dateien erkennbar und zuordenbar sind
- ein ungueltiges Format oder eine unzulaessige Optionskombination bleibt ein CLI-Verwendungsfehler mit Exit-Code `2`
- ein nicht beschreibbarer Report-Pfad oder ein Schreibfehler liefert Exit-Code `1` mit einer spezifischen Fehlermeldung der Form `error: cannot write report: <path>: <reason>` und einem handlungsorientierten Hinweis, zum Beispiel `hint: check the output path and directory permissions`
- bestehende Eingabefehler fuer `compile_commands.json` behalten ihre M1-/M2-Codes `3` und `4`

Anforderungen an die Hilfe und Fehlermeldungen:

- `analyze --help` und `impact --help` muessen die neue Format- und Output-Semantik sichtbar erklaeren
- Fehlermeldungen bei ungueltigen Kombinationen sollen den naechsten Schritt nennen, zum Beispiel `hint: use --format markdown when writing a report file`
- `--top` muss fuer Markdown dieselbe fachliche Semantik haben wie fuer die Konsole

Nicht Ziel von M3:

- mehrere gleichzeitige Report-Sinks in einem Aufruf
- ein eigenstaendiges `report`-Unterkommando neben `analyze` und `impact`
- implizite Dateinamensfindung oder automatische Report-Ablage ohne explizite Nutzerentscheidung

Vorgesehene Artefakte:

- Anpassung von `src/adapters/cli/cli_adapter.*`
- Anpassung von `src/adapters/cli/exit_codes.h`
- Anpassung von `src/main.cpp`
- gegebenenfalls kleine Hilfstypen fuer CLI-Konfiguration und Report-Ausgabe

**Ergebnis**: Markdown-Berichte sind ueber dieselben Kommandowege wie die Konsolenausgabe nutzbar, mit klarer Hilfe und stabiler Exit-Code-Semantik.

### 1.4 Referenzdaten, Golden Files und Reporter-Tests fuer M3 ausbauen

Mit M3 steigt die Regressionsempfindlichkeit: Schon kleine Aenderungen an Sortierung, Abschnittstiteln oder Diagnostics koennen Markdown-Berichte und dokumentierte Beispielausgaben ungewollt aendern. Deshalb braucht M3 nicht nur mehr Tests, sondern auch explizite Golden-Outputs.

Mindestens benoetigt:

- Adapter-Tests fuer `MarkdownReportAdapter`, insbesondere fuer feste Abschnittsreihenfolge, literal festgelegte Ueberschriften und Leersaetze, exakt spezifizierte mehrzeilige Listenform einschliesslich markerabhaengiger Einrueckung bei mehrstelligen Ordnungszahlen, Vollstaendigkeitsangaben zu `--top`, Markdown-Escaping, Impact-Klassifikation und Diagnostics
- CLI-Tests fuer `--format markdown`, `--output`, ungueltige Kombinationen, Report-Schreibfehler und den Dateivertrag ohne teilgeschriebene Zieldatei
- End-to-End-Tests, die zentrale Konsolen- und Markdown-Ausgaben gegen erwartete Dateien vergleichen
- Golden-Outputs fuer zentrale Erfolgsfaelle von `analyze` und `impact`, jeweils fuer Konsole und Markdown
- Tests dafuer, dass dieselben fachlichen Daten in Konsole und Markdown konsistent wiedergegeben werden, pfadtragende Felder dieselbe Anzeigesemantik verwenden (gleicher fachlicher Pfadinhalt, nicht notwendig byte-identische Strings, da Markdown-Escaping formatbedingte Unterschiede einfuehrt), `Impact classification` die Ergebnisklasse korrekt abbildet und reportweite `impact`-Diagnostics auch bei permutierten Eingaben stabil bleiben

Fuer M3 gilt eine klare Referenzquellen-Regel:

- `tests/reference/` ist die verbindliche Rohdatenbasis fuer versionierte Referenzprojekte, Generatoren und Performance-Szenarien
- fuer bytegenaue Goldens und `docs/examples/` gibt es pro Testfall genau eine kanonische Fixture-Quelle
- neue M3-spezifische Golden-Faelle liegen unter `tests/e2e/testdata/m3/`
- wenn ein regressionskritischer M2-Randfall fuer M3 weiterverwendet wird, wird entweder die bestehende M2-Fixture direkt als kanonische Quelle referenziert oder einmalig in `tests/e2e/testdata/m3/` gespiegelt; nach dieser Entscheidung duerfen Goldens, Beispiele und Regenerationspfade nur noch die gewaehlte Quelle verwenden

Sinnvolle Referenzfaelle unter `tests/e2e/testdata/m3/` sind:

- `report_project/` fuer einen typischen `analyze`-Durchlauf
- `report_impact_header/` fuer heuristische Header-Betroffenheit
- `report_impact_source/` fuer direkte TU-Betroffenheit
- `report_empty_impact/` fuer einen gueltigen Fall ohne Treffer
- `report_diagnostics/` fuer sichtbare Datenluecken und Heuristik-Hinweise
- `report_top_limit/` fuer `--top` in Konsole und Markdown

Ergaenzend zu neuen M3-Fixtures werden die fuer M3 regressionskritischen M2-Referenzfaelle weiterverwendet oder in `tests/e2e/testdata/m3/` gespiegelt. Dazu gehoeren mindestens Faelle fuer mehrfach vorkommende TU-Pfade (`duplicate_tu_entries`), Pfadsemantik und lexikalische Normalisierung (`path_semantics`) sowie permutierte Eingabereihenfolgen (`permuted_compile_commands`), damit Markdown-Renderer und Golden-Tests dieselbe Stabilitaet wie die M2-Konsolenausgabe absichern.

Golden-Output-Regeln fuer M3:

- erwartete Konsolen- und Markdown-Dateien liegen versioniert im Repository
- erwartete Ausgaben enthalten keine volatilen Werte wie Zeitstempel oder temporaere Verzeichnisse
- reine Versionshebungen ohne Format- oder Inhaltsaenderung duerfen keine Regeneration von Report-Golden-Files oder `docs/examples/` erzwingen
- die literal festgelegten Abschnittstitel, Leersaetze und Listenformen sind Teil des Golden-Vertrags und werden bytegenau abgesichert
- mehrzeilige Listeneintraege, markerabhaengige Einrueckung geordneter Listen, Diagnostikformate, `Impact classification` und die Vollstaendigkeitsangaben fuer `--top` gehoeren explizit zum Golden-Vertrag
- fuer `impact` gehoert auch die stabile Reihenfolge reportweiter Diagnostics zum Golden-Vertrag; dieselbe `compile_commands.json` in anderer Eingabereihenfolge darf keinen anderen Markdown- oder Konsolenreport erzeugen
- pfadtragende Golden-Files und `docs/examples/` werden aus derselben kanonischen Aufrufform erzeugt; Docker-Smoke-Tests sind kein Ersatz fuer diese bytegenaue Referenzquelle
- Vergleiche sollen byte-stabil sein oder hoechstens zeilenendungsbezogen auf LF normalisieren; fachliche Vergleiche "ungefaehr gleich" reichen fuer M3 nicht aus
- Konsolen-Golden-Files duerfen gegenueber Markdown eigene Layout- und Wortlaut-Erwartungen haben; reine Inhaltsparitaet ersetzt keinen formatspezifischen Regressionstest

Fuer die Nutzerdokumentation gilt:

- die kuratierten Beispielausgaben in `docs/examples/` werden aus denselben kanonischen Testfaellen und derselben kanonischen Aufrufform erzeugt wie die pfadtragenden Golden-Files; sie sind keine separate Wahrheitsquelle

**Ergebnis**: Markdown- und Konsolenberichte sind gegen unbeabsichtigte Layout- und Inhaltsregressionen abgesichert.

### 1.5 Versioniertes Referenzprojekt und Performance-Baseline aufbauen

Phase 3 enthaelt nicht nur Report-Export, sondern auch Qualitaets- und Stabilisierungsaussagen fuer reale Nutzung. M3 fuehrt deshalb eine dokumentierte Referenzumgebung und ein versioniertes Referenzprojekt fuer Performance- und Testaussagen ein.

Das Referenzprojekt soll:

- im Repository versioniert sein; externe Downloads oder lokale Ad-hoc-Projekte reichen nicht aus
- mehrere Groessenstufen abdecken, mindestens `250`, `500` und `1.000` Translation Units
- kontrollierte Include-Hotspots und reproduzierbare Compile-Daten enthalten
- fuer Performance-Messung und als Rohdatenbasis fuer daraus abgeleitete E2E-Fixtures wiederverwendbar sein

Die Messmethodik fuer M3 soll mindestens dokumentieren:

- Betriebssystem, Compiler, CMake-Version und relevante Hardware-/Containerdaten
- exakten Build- und Analyseaufruf
- ob warm oder kalt gemessen wird
- wie viele Messlaeufe in die Baseline eingehen
- wie Laufzeit und Speicher erhoben werden
- ob und wie `--top` die gemessene Ausgabe begrenzt
- welche Messungen als `NF-04`-/`NF-05`-Baselines fuer `analyze --format console` und `analyze --format markdown` gelten und welche nur als dokumentierte Stichproben fuer `impact --format markdown` zaehlen
- welche festen Eingaben und Artefaktpfade fuer die Baselines von `analyze --format console` und `analyze --format markdown` sowie fuer die dokumentierten Stichproben von `impact --format markdown` verwendet werden

Pragmatische Entscheidung fuer M3:

- die Performance-Baseline ist reproduzierbar zu messen, aber nicht Teil jedes Standard-`ctest`-Laufs
- sie wird als eigener dokumentierter Pruefpfad behandelt, lokal oder in einem separaten CI-Workflow
- die Bewertung gegen `NF-04` und `NF-05` erfolgt fuer `analyze` sowohl fuer `--format console` als auch fuer `--format markdown`
- die `NF-04`-/`NF-05`-Baseline bezieht sich in M3 explizit auf den dokumentierten Standardaufruf des ausgelieferten MVP-Analysepfads; dafuer wird die Ausgabe bewusst mit dem festen, dokumentierten Limit `--top 10` gemessen, damit der Messumfang fuer Konsole und Markdown reproduzierbar bleibt
- fuer diese Baselines werden dieselben versionierten Referenzprojekte `tests/reference/scale_250/`, `tests/reference/scale_500/` und `tests/reference/scale_1000/` jeweils in beiden Ausgabeformaten gemessen
- fuer dokumentierte Stichproben des separaten Impact-Pfads werden feste M3-Fixtures verwendet: `tests/e2e/testdata/m3/report_impact_header/compile_commands.json` zusammen mit `--changed-file include/common/config.h`; die Artefakte liegen nachvollziehbar unter `build/reports/performance/`
- Nichterreichen darf nicht stillschweigend uebergangen werden, sondern muss in der Dokumentation sichtbar sein
- Referenzumgebung, Messmethodik, Messartefakte und Soll-Ist-Bewertung werden verbindlich in `docs/user/performance.md` festgehalten

Anforderungen an den Generator:

- der Generator ist ein Shell- oder Python-Skript unter `tests/reference/`, das die Verzeichnisse `scale_250/`, `scale_500/` und `scale_1000/` mit reproduzierbaren Artefakten erzeugt
- jede Groessenstufe enthaelt eine `compile_commands.json` sowie die darin referenzierten Quelldateien und Header, weil die heuristische Include-Aufloesung (`SourceParsingIncludeAdapter`) echte Dateien auf der Festplatte benoetigt
- die generierten `compile_commands.json` und Quelldateien werden zusammen mit dem Generator versioniert, damit Performance-Messungen ohne vorherigen Generatorlauf reproduzierbar sind

Vorgesehene Artefakte:

- versionierter Generator und generierte Referenzprojekte unter `tests/reference/`
- neue Dokumentation `docs/user/performance.md`
- gegebenenfalls Hilfsskripte fuer reproduzierbare Messlaeufe unter `tests/reference/` oder `scripts/`

**Ergebnis**: `cmake-xray` besitzt eine nachvollziehbare Referenzbasis fuer Performance- und Stabilitaetsaussagen des MVP.

### 1.6 README, Beispielausgaben, Release-Dokumentation und Versionshebung abschliessen

M3 ist der MVP-Abschluss. Dokumentation und Release-Artefakte muessen deshalb nicht mehr nur den Entwicklungsstand beschreiben, sondern den lieferbaren Produktstand.

Die README soll fuer M3 mindestens enthalten:

- aktualisierten Projektstatus mit Verweis auf M2 und M3
- mindestens ein Installationsbeispiel fuer die Nutzung auf Linux, etwa als lokaler Quellbuild; optional ergaenzt um das Runtime-Image als Bezugsweg
- Beispiele fuer `analyze` und `impact` in der Konsole
- Beispiele fuer Markdown-Ausgabe via `--format markdown`
- Beispiel fuer `--output <path>`
- klaren Hinweis auf heuristische Include-/Impact-Aussagen
- Verweis auf Referenz- und Beispielausgaben
- Verweis auf `docs/user/performance.md`

Zusaetzlich sind bei Abschluss von M3 zu aktualisieren:

- `CHANGELOG.md` fuer `v1.0.0`; bis zum eigentlichen Release duerfen die Inhalte unter `## [Unreleased]` bleiben, die datierte Versionsueberschrift fuer `1.0.0` entsteht erst im Release-Commit
- `src/hexagon/model/application_info.h`
- Root-`CMakeLists.txt`
- `docs/user/releasing.md`, sodass Release-Ablauf, pruefpflichtige Artefakte und der MVP-Stand `v1.0.0` konsistent dokumentiert sind; die Releasing-Checkliste wird um Markdown-Report-Pruefung, Performance-Baseline-Dokumentation und Golden-Output-Regeneration erweitert
- gegebenenfalls `docs/planning/roadmap.md`, falls Status oder Formulierungen vom Entwurf in den erreichten MVP-Stand uebergehen

Sinnvolle Beispielartefakte unter `docs/examples/`:

- `analyze-console.txt`
- `analyze-report.md`
- `impact-console.txt`
- `impact-report.md`

**Ergebnis**: Nutzerdokumentation, Beispielausgaben und Release-Artefakte entsprechen dem auslieferbaren MVP-Stand `v1.0.0`.

## 2. Zielartefakte

Folgende Dateien oder Dateigruppen sollen nach M3 voraussichtlich neu entstehen oder substanziell ueberarbeitet sein:

| Bereich | Zielartefakte |
|---|---|
| Driving Port | gegebenenfalls punktuelle Anpassung von `src/hexagon/ports/driving/generate_report_port.h` |
| Driven Port | gegebenenfalls punktuelle Anpassung von `src/hexagon/ports/driven/report_writer_port.h` |
| Service | gegebenenfalls punktuelle Anpassung von `src/hexagon/services/report_generator.*` |
| Output-Adapter | `src/adapters/output/console_report_adapter.*`, neue Dateien `src/adapters/output/markdown_report_adapter.*`; die Laufzeit-Formatauswahl bleibt in der CLI-/Composition-Root-Verdrahtung |
| CLI | `src/adapters/cli/cli_adapter.*`, `src/adapters/cli/exit_codes.h` |
| Composition Root | `src/main.cpp` |
| Build | `src/adapters/CMakeLists.txt`, `tests/CMakeLists.txt` |
| Adapter-Tests | neue oder erweiterte Tests unter `tests/adapters/` fuer Markdown-Reporting und Output-Fehlerfaelle |
| E2E-Tests | neue oder erweiterte Tests unter `tests/e2e/` fuer Markdown und Golden-Outputs |
| Referenzdaten | neue, aus `tests/reference/` oder bestehenden Kleinfixturen abgeleitete Fixtures unter `tests/e2e/testdata/m3/`, weiterverwendete oder gespiegelt uebernommene regressionskritische M2-Referenzfaelle sowie versionierte Referenzprojekte oder Generatoren unter `tests/reference/` |
| Beispielausgaben | neue Dateien unter `docs/examples/` |
| Performance-Dokumentation | neue Datei `docs/user/performance.md` |
| Produktdokumentation | `README.md`, `CHANGELOG.md`, `docs/user/releasing.md`, gegebenenfalls `docs/planning/roadmap.md` |

Hinweis: Die konkreten Dateinamen duerfen von dieser Zielstruktur abweichen, solange die Trennung zwischen Kern, Report-Adaptern, CLI und Referenzdaten erhalten bleibt.

## 3. Reihenfolge

| Schritt | Arbeitspaket | Abhaengig von |
|---|---|---|
| 1a | 1.1 Report-Vertraege und Formatwahl schaerfen | M2 |
| 1b | 1.5 Referenzprojekt und Messmethodik festlegen | M2 |
| 2a | 1.2 `MarkdownReportAdapter` umsetzen | 1a |
| 2b | 1.3 CLI fuer `--format` und `--output` erweitern | 1a |
| 3 | 1.4 Golden-Outputs und Reporter-Tests ausbauen | 2a, 2b |
| 4 | 1.5 Performance-Baseline messen und dokumentieren | 1b, 3 |
| 5 | 1.6 README, Beispielausgaben und Release-Artefakte abschliessen | 2b, 3, 4 |

Schritte 1a und 1b koennen parallel bearbeitet werden. Das Referenzprojekt fuer Performance und groessere Regressionstests sollte frueh feststehen. Bytegenaue Reporter-Tests und Beispielausgaben bauen auf den kuratierten M3-Fixtures auf; diese muessen ihre Herkunft aus `tests/reference/` oder aus weiterverwendeten M2-Randfaellen explizit bewahren, damit keine zweite unabhaengige Wahrheitsquelle entsteht. Die eigentlichen Messungen aus 1.5 sollten jedoch erst gegen einen funktional stabilen M3-Stand erfolgen, damit nicht auf vorlaeufigen Report- oder Ausgabewegen gemessen wird.

## 4. Pruefung

M3 ist abgeschlossen, wenn folgende Pruefwege erfolgreich durchlaufen:

**Lokal**:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
mkdir -p build/reports
./build/cmake-xray --help
./build/cmake-xray analyze --help
./build/cmake-xray impact --help
./build/cmake-xray analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json
./build/cmake-xray analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json --format markdown > build/reports/analyze.stdout.md
./build/cmake-xray analyze --compile-commands tests/e2e/testdata/m3/report_project/compile_commands.json --format markdown --output build/reports/analyze.md
./build/cmake-xray impact --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json --changed-file include/common/config.h --format markdown --output build/reports/impact.md
ctest --test-dir build --output-on-failure
```

**Docker / Reports und Gates**:

```bash
docker build --target test -t cmake-xray:test .
docker build --target coverage-check -t cmake-xray:coverage-check .
docker build --target coverage-check --build-arg XRAY_COVERAGE_THRESHOLD=100 -t cmake-xray:coverage-check .
docker build --target quality -t cmake-xray:quality .
docker run --rm cmake-xray:quality
docker build --target quality-check -t cmake-xray:quality-check .
docker build --target runtime -t cmake-xray .
docker run --rm cmake-xray --help
docker run --rm \
  -v "$PWD/tests/e2e/testdata/m3:/data:ro" \
  cmake-xray analyze --compile-commands /data/report_project/compile_commands.json --format markdown \
  > build/reports/docker-analyze.stdout.md
docker run --rm \
  -v "$PWD/tests/e2e/testdata/m3:/data:ro" \
  cmake-xray impact --compile-commands /data/report_impact_header/compile_commands.json --changed-file include/common/config.h --format markdown \
  > build/reports/docker-impact.stdout.md
```

Die Docker-Smoke-Tests pruefen fuer M3 bewusst den Markdown-`stdout`-Pfad. Der Dateivertrag fuer `--output` wird lokal und in den automatisierten CLI-/E2E-Tests verifiziert; ein unvorbereiteter Bind-Mount nach `/out` ist wegen des nichtprivilegierten Runtime-Users kein stabiler, distributionsunabhaengiger Referenzpfad.

**Performance-Baseline**:

```bash
mkdir -p build/reports/performance
/usr/bin/time -v ./build/cmake-xray analyze --compile-commands tests/reference/scale_250/compile_commands.json --top 10 > build/reports/performance/xray-250-console.stdout.txt 2> build/reports/performance/xray-250-console.time.txt
/usr/bin/time -v ./build/cmake-xray analyze --compile-commands tests/reference/scale_500/compile_commands.json --top 10 > build/reports/performance/xray-500-console.stdout.txt 2> build/reports/performance/xray-500-console.time.txt
/usr/bin/time -v ./build/cmake-xray analyze --compile-commands tests/reference/scale_1000/compile_commands.json --top 10 > build/reports/performance/xray-1000-console.stdout.txt 2> build/reports/performance/xray-1000-console.time.txt
/usr/bin/time -v ./build/cmake-xray analyze --compile-commands tests/reference/scale_250/compile_commands.json --top 10 --format markdown > build/reports/performance/xray-250-markdown.stdout.md 2> build/reports/performance/xray-250-markdown.time.txt
/usr/bin/time -v ./build/cmake-xray analyze --compile-commands tests/reference/scale_500/compile_commands.json --top 10 --format markdown > build/reports/performance/xray-500-markdown.stdout.md 2> build/reports/performance/xray-500-markdown.time.txt
/usr/bin/time -v ./build/cmake-xray analyze --compile-commands tests/reference/scale_1000/compile_commands.json --top 10 --format markdown > build/reports/performance/xray-1000-markdown.stdout.md 2> build/reports/performance/xray-1000-markdown.time.txt
/usr/bin/time -v ./build/cmake-xray impact --compile-commands tests/e2e/testdata/m3/report_impact_header/compile_commands.json --changed-file include/common/config.h --format markdown > build/reports/performance/xray-impact-markdown-sample.stdout.md 2> build/reports/performance/xray-impact-markdown-sample.time.txt
```

Die Pruefung soll insbesondere bestaetigen:

- Konsole und Markdown verwenden dieselbe fachliche Datengrundlage
- `--format markdown` funktioniert fuer `analyze` und `impact`
- `--output` schreibt Reports reproduzierbar und mit passender Fehlerbehandlung; bei Erfolg bleibt `stdout` leer
- Schreibfehler im `--output`-Pfad hinterlassen unter dem Zielnamen keine teilgeschriebene oder truncierte Datei
- ungueltige Format-/Output-Kombinationen liefern Exit-Code `2`
- Schreibfehler fuer Report-Dateien liefern Exit-Code `1`
- Eingabefehler behalten ihre M1-/M2-Codes `3` und `4`
- die literal festgelegten Markdown-Ueberschriften, Leersaetze und Listenformen werden eingehalten
- pfadtragende Felder folgen der dokumentierten M2-/M3-Anzeigesemantik und geben keine rohen Vergleichsschluessel oder unnormalisierten CLI-Eingaben aus
- Golden-Outputs fuer Konsole und Markdown bleiben stabil, einschliesslich regressionskritischer Faelle fuer TU-Identitaet, Pfadsemantik und permutierte Eingabereihenfolge
- reportweite `impact`-Diagnostics bleiben auch bei permutierten Eingaben stabil
- statische Analyse- und Metrik-Reports gemaess `docs/user/quality.md` bleiben erzeugbar, einschliesslich `summary.txt`, `clang-tidy.txt`, `lizard.txt` und `lizard-warnings.txt`
- Coverage-Gate und Quality-Gate bleiben trotz Report-Ausbau gruen
- Referenzumgebung, Messwerte und Bewertung gegen `NF-04` und `NF-05` sind in `docs/user/performance.md` dokumentiert; `stdout`- und `time`-Artefakte der Baselines fuer `analyze --format console` und `analyze --format markdown` sowie dokumentierte Stichproben fuer `impact --format markdown` liegen nachvollziehbar vor

## 5. Rueckverfolgbarkeit

| Planbereich | Lastenheft-Kennungen |
|---|---|
| Markdown-Report und Report-Vertraege | `F-27`, `AK-06`, `NF-13` |
| Konsolen-/Markdown-Paritaet und Ergebnisbegrenzung | `F-26`, `F-42`, `AK-03`, `AK-04`, `AK-05` |
| Reproduzierbarkeit und Determinismus | `NF-15` |
| CLI fuer Format und Ausgabepfad | `F-31`, `F-32`, `F-33`, `F-34`, `F-35`, `F-36`, `AK-09`, `NF-02`, `NF-14` |
| Referenzdaten und Golden-Outputs | `NF-10`, `NF-18`, `NF-19` |
| Referenzumgebung und Performance-Baseline | `NF-04`, `NF-05`, `NF-06` |
| Release- und Produktdokumentation | `NF-16`, `NF-17`, `NF-18`, `AK-08` |

## 6. Entschiedene und offene Punkte

### 6.1 Entschieden

| Frage | Entscheidung | Verweis |
|---|---|---|
| Formatwahl in M3 | `analyze` und `impact` bleiben die einzigen Nutzerpfade; Report-Ausgabe wird ueber `--format console|markdown` erweitert | AP 1.3 |
| Format-Dispatch in M3 | Die CLI wertet `--format` aus; die Formatwahl erfolgt in der obersten Verdrahtung ueber formatgebundene Report-Strecken fuer `ConsoleReportAdapter` und `MarkdownReportAdapter`, ohne ein zusaetzliches Hexagon-Formatmodell oder einen Adapter-zu-Adapter-Dispatcher im Output-Layer | AP 1.1, 1.3 |
| Default-Ausgabe | Standard bleibt die Konsolenausgabe | AP 1.3 |
| Dateiausgabe | `--output <path>` ist nur fuer Markdown zulaessig; ohne `--output` geht Markdown auf `stdout`, mit `--output` bleibt `stdout` bei Erfolg leer; der Zielpfad wird nur nach vollstaendig erfolgreichem Schreiben ersetzt | AP 1.3 |
| Schreibfehler | nicht beschreibbare Report-Pfade nutzen Exit-Code `1`; Eingabefehler behalten `3` und `4` | AP 1.1, 1.3 |
| Verantwortung der Report-Strecke | Report-Adapter erzeugen Text; `stdout`/Dateisystem-Entscheidung bleibt ausserhalb des Hexagons | AP 1.1 |
| Markdown-Vertrag | M3 fixiert literal die Ueberschriften, Abschnittsreihenfolge, Listenformen einschliesslich markerabhaengiger Einrueckung und Eintragsformat, die expliziten Leer-Saetze als Ausnahme fuer leere Abschnitte, `--top`-Vollstaendigkeitsangaben, `Impact classification` und Escaping-Regeln; Reporter geben Ergebnisvektoren in Modellreihenfolge aus, und der Kern liefert dafuer insbesondere reportweite `impact`-Diagnostics bereits in stabiler Reihenfolge | AP 1.2, 1.4 |
| Impact-Klassifikation | Der Markdown-Report unterscheidet die Ergebnisklassen `direct`, `heuristic` und `uncertain`; damit bleiben direkte, heuristisch abgeleitete und unklare Null-Treffer sichtbar getrennt | AP 1.2, 1.4 |
| Platzierung von Diagnostics | elementbezogene Diagnostics bleiben inline; `## Diagnostics` enthaelt nur echte reportweite Hinweise und wird nicht mit Element-Diagnostics dupliziert. Fuer `impact` bleibt Unsicherheit zusaetzlich ueber `Impact classification` in der Uebersicht sichtbar | AP 1.1, 1.2, 1.4 |
| Versionsdarstellung im Report | Die Tool-Version ist kein Bestandteil des Report-Bodys; reine Versionshebungen duerfen Report-Goldens nicht invalidieren | AP 1.2, 1.4 |
| Pfaddarstellung im Report | Markdown nutzt dieselben Anzeige-Pfadregeln wie M2; Vergleichsschluessel bleiben intern, angezeigt werden normalisierte Display-Pfade | AP 1.2, 1.4 |
| Markdown-Stil | einfaches, diffbares Markdown ohne HTML, Zeitstempel oder sonstige volatile Metadaten | AP 1.2 |
| Golden-Outputs | versioniert im Repository und gegen stabile, nichtvolatile Ausgaben geprueft | AP 1.4 |
| Referenzquellen fuer Goldens und Performance | `tests/reference/` ist Rohdatenbasis fuer Referenzprojekte und Performance; fuer bytegenaue Goldens und `docs/examples/` gibt es pro Fall genau eine kanonische Fixture-Quelle, entweder unter `tests/e2e/testdata/m3/` oder bewusst weiterverwendet am gewaehlten M2-Pfad | AP 1.4, 1.5 |
| Performance-Pfad | reproduzierbar dokumentiert, aber kein Pflichtbestandteil jedes `ctest`-Laufs; `NF-04`/`NF-05` werden ueber Baselines fuer den dokumentierten M3-Standardaufruf von `analyze` mit festem `--top 10` in `console` und `markdown` bewertet, der separate Impact-Reportpfad wird zusaetzlich ueber feste Stichproben dokumentiert | AP 1.5 |
| Nicht-MVP-Formate | HTML, JSON und DOT bleiben ausserhalb von M3 | AP 0.3 |
| Diagnostics-Sortierregel | Reportweite Diagnostics werden im Kern lexikographisch nach `(severity, message)` sortiert; `warning` sortiert vor `note`. Reporter uebernehmen die Modellreihenfolge ohne eigene Sortierschicht | AP 1.1, 1.2 |
| Impact-Klassifikations-Ableitung | Die Ableitung von `Impact classification` erfolgt deterministisch im Reporter aus `ImpactResult::heuristic` und `affected_translation_units.empty()`, ohne ein neues Hexagon-Modell einzufuehren | AP 1.1, 1.2 |
| Konsolen-Adapter-Anpassung | Die in M2 reporterseitige Diagnostics-Deduplikation wird zugunsten der Kern-Bereinigung entfernt; der `ConsoleReportAdapter` wird fuer Golden-Faehigkeit und Informationspariaet mit dem Markdown-Adapter angepasst | AP 1.1, 1.2 |
| Schreibfehler-Meldungen | Report-Schreibfehler (Exit-Code `1`) erhalten eine spezifische Fehlermeldung mit Pfad und Grund sowie einen handlungsorientierten Hinweis | AP 1.3 |
| Temporaere Dateien bei `--output` | Temporaere Dateien verwenden das Namensmuster `.cmake-xray-*` im Zielverzeichnis | AP 1.3 |
| Referenzprojekt-Generator | Shell- oder Python-Skript unter `tests/reference/`; generierte Quelldateien und `compile_commands.json` werden versioniert, weil die heuristische Include-Aufloesung echte Dateien benoetigt | AP 1.5 |

### 6.2 Offen

Keine offenen Punkte fuer M3.
