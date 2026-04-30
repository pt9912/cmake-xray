# Plan M6 - AP 1.5 Analyseauswahl, Schwellenwerte und Impact-Budget konfigurierbar machen

## Ziel

AP 1.1 bis AP 1.4 haben fachliche Sichten (Target-Graph, Target-Hubs,
Impact-Priorisierung, Include-Klassifikation) eingefuehrt, jeweils mit
fest in den Code eingebauten Defaults. AP 1.5 macht **Analyseumfang**
und **Schwellenwerte** ueber CLI-Optionen explizit steuerbar, ohne
neue Spezialkommandos oder Konfigurationsdateien zu benoetigen. Damit
erfuellt M6 die Lastenheft-Kennungen `F-10` (konfigurierbare
Schwellenwerte), `F-37` (Aktivierung/Deaktivierung einzelner Analysen)
und `F-38` (Grenzwerte und Auswertungsschwellen).

Konkret:

- `--analysis <list>` aktiviert oder deaktiviert einzelne
  Analyseabschnitte fuer `analyze`.
- `--tu-threshold <metric>=<n>` setzt Mindest-Schwellenwerte fuer das
  TU-Ranking pro Metrik.
- `--min-hotspot-tus <n>` setzt die Mindestanzahl betroffener
  Translation Units fuer Include-Hotspots.
- `--target-hub-in-threshold <n>` und `--target-hub-out-threshold <n>`
  ueberschreiben die in AP 1.1 hardcoded Defaults `(10, 10)`.
- `--impact-target-depth <n>` und `--require-target-graph` aus AP 1.3
  bleiben unveraendert; AP 1.5 dokumentiert sie als Bestandteil des
  M6-CLI-Vertrags und ergaenzt nur die Reportausgabe der wirksamen
  Werte, falls AP 1.3 das nicht vollstaendig erfasst hatte.

Die wirksame Konfiguration wird in allen fuenf Reportformaten
serialisiert, damit CI-Werkzeuge reproduzierbar auswerten koennen,
welche Schwellen einen Reportstand erzeugt haben.

AP 1.5 hebt `kReportFormatVersion` von `4` (eingefuehrt in AP 1.4) auf
`5`, weil neue Pflichtfelder im Analyze-JSON zugefuegt werden und
bestehende Felder (`target_hubs.thresholds`, `min_hotspot_tus`) nun
CLI-getrieben statt hardcoded sind.

## Scope

Umsetzen:

- CLI-Option `--analysis <list>` mit Werten `all`, `tu-ranking`,
  `include-hotspots`, `target-graph`, `target-hubs`. Default `all`.
- CLI-Option `--tu-threshold <metric>=<n>` mit Metriken `arg_count`,
  `include_path_count`, `define_count`. Mehrfach-Setzen erlaubt, aber
  jede Metrik nur einmal.
- CLI-Option `--min-hotspot-tus <n>` mit Default `2`.
- CLI-Optionen `--target-hub-in-threshold <n>` und
  `--target-hub-out-threshold <n>` mit Default jeweils `10`.
- Validierungslogik:
  - `--analysis all` darf nicht mit weiteren Analysewerten kombiniert
    werden.
  - `--analysis target-hubs` ohne `target-graph` ist
    CLI-Verwendungsfehler `"--analysis: target-hubs requires target-graph"`.
  - Doppelte `--tu-threshold`-Metriken sind CLI-Verwendungsfehler.
  - Negative oder nicht-numerische Schwellenwerte sind CLI-Fehler.
- Erweiterung von `AnalyzeProjectRequest` um die neuen
  Konfigurationsfelder.
- Erweiterung von `AnalysisResult` um die wirksamen Konfigurationswerte.
- Erweiterung der bestehenden TU-Ranking-, Include-Hotspot- und
  Hub-Klassifikations-Helper, sodass sie die konfigurierten Werte
  statt der hardcoded Defaults verwenden.
- Section-Aktivierungs-Status pro Analyseabschnitt
  (`active`/`disabled`/`not_loaded`) im `AnalysisResult`.
- Reportausgabe der wirksamen Konfiguration in Console, Markdown,
  HTML, JSON und DOT.
- `kReportFormatVersion` von `4` auf `5` heben; Schema-Update.
- Goldens fuer alle fuenf Formate fuer typische Konfigurationen.

Nicht umsetzen:

- Konfigurationsdatei (z. B. `cmake-xray.toml`). M6 verwendet
  ausschliesslich CLI-Optionen.
- CLI-Steuerung von `include_depth_limit` und `include_node_budget`
  aus AP 1.4. Beide bleiben in M6 hardcoded auf `32`/`10000`.
- Aenderung am Verhalten von `--top` aus M5. AP 1.5 ergaenzt
  Schwellenwerte vor dem `--top`-Schritt, nicht parallel dazu.
- Neue CLI-Aliase oder Kurzformen fuer die neuen Optionen.
- Auto-Vervollstaendigung von `--analysis target-hubs` zu
  `--analysis target-graph,target-hubs`. AP 1.5 verlangt explizite
  Angabe; abhaengige Sektionen werden nicht automatisch ergaenzt.

Diese Punkte bleiben bewusst ausserhalb von M6.

## Voraussetzungen aus AP 1.1-1.4 und M5

AP 1.5 baut auf folgenden Vertraegen auf:

- `target_hubs_in`/`target_hubs_out`-Berechnung und
  `kDefaultTargetHubInThreshold=10`/`kDefaultTargetHubOutThreshold=10`
  aus AP 1.1.
- `target_graph`/`target_graph_status` und Hub-Sicht in
  Reports aus AP 1.2.
- `--impact-target-depth` und `--require-target-graph` aus AP 1.3.
- `IncludeOrigin`/`IncludeDepthKind`-Klassifikation,
  `--include-scope`/`--include-depth` und Filter-Statistik aus
  AP 1.4.
- `kReportFormatVersion=4` aus AP 1.4 als Ausgangsbasis fuer den
  Sprung auf `5`.
- M3 Include-Hotspot-Mindestanzahl betroffener TUs (aktuell hardcoded
  `2` in `analysis_support.cpp`).
- M3 TU-Ranking ohne Schwellenwertfilter.

AP 1.5 hebt `kReportFormatVersion` von `4` auf `5`. Damit besteht eine
**harte Tranchen-Reihenfolge**: AP-1.5-A.3 (Schema v5) darf erst nach
AP-1.4-A.3 (Schema v4) landen. Modell-, CLI-Parser- und Service-Logik
(AP-1.5-A.1/A.2) koennen parallel zu AP 1.4 entwickelt werden, weil
sie schemafrei sind.

## Dateien

Voraussichtlich zu aendern:

- `src/hexagon/model/analysis_result.h`
- `src/hexagon/model/report_format_version.h`
- `src/hexagon/ports/driving/analyze_project_port.h`
- `src/hexagon/services/project_analyzer.{h,cpp}`
- `src/hexagon/services/analysis_support.{h,cpp}`
- `src/hexagon/services/target_graph_support.{h,cpp}`
- `src/adapters/cli/cli_adapter.{h,cpp}`
- `src/adapters/output/console_report_adapter.cpp`
- `src/adapters/output/markdown_report_adapter.cpp`
- `src/adapters/output/html_report_adapter.cpp`
- `src/adapters/output/json_report_adapter.cpp`
- `src/adapters/output/dot_report_adapter.cpp`
- `src/main.cpp`
- `docs/report-json.md`
- `docs/report-json.schema.json`
- `docs/report-dot.md`
- `docs/report-html.md`
- `tests/hexagon/test_project_analyzer.cpp`
- `tests/adapters/test_console_report_adapter.cpp`
- `tests/adapters/test_markdown_report_adapter.cpp`
- `tests/adapters/test_html_report_adapter.cpp`
- `tests/adapters/test_json_report_adapter.cpp`
- `tests/adapters/test_dot_report_adapter.cpp`
- `tests/e2e/test_cli.cpp`
- `tests/e2e/testdata/m5/<format>-reports/manifest.txt`
- `tests/e2e/testdata/m6/<format>-reports/manifest.txt`
- `docs/examples/manifest.txt`
- `docs/examples/generation-spec.json`

Neue Dateien (optional):

- `src/hexagon/services/analysis_configuration.{h,cpp}` falls
  Konfigurationsmodell und Validierungs-Helper als eigener Helper
  sauber sind.
- `tests/hexagon/test_analysis_configuration.cpp` analog.
- `tests/e2e/testdata/m6/<format>-reports/analyze-config-*.<ext>` als
  Goldens-Familie.

## Format-Versionierung

`xray::hexagon::model::kReportFormatVersion` steigt von `4` auf `5`.
Begruendung:

- Analyze-JSON erhaelt einen neuen `analysis_configuration`-Block mit
  `analysis_sections`, `tu_thresholds`, `min_hotspot_tus`,
  `target_hub_in_threshold` und `target_hub_out_threshold`.
- Analyze-JSON erhaelt einen neuen `analysis_section_states`-Block mit
  pro-Section `active`/`disabled`/`not_loaded`.
- `target_hubs.thresholds` aus AP 1.2 wird semantisch von "hardcoded
  Defaults" zu "wirksame CLI-Werte". Schema-Werte aendern sich nicht;
  die Bedeutung wird strikter.
- Bestehende implizite Defaults (TU-Ranking ohne Schwellen,
  `min_hotspot_tus=2` hardcoded) werden serialisiert; ohne neue
  Pflichtfelder waere die Reproduzierbarkeit unvollstaendig.

Wie in AP 1.2-1.4 gilt:

- Kein Dual-Output, kein CLI-Schalter zurueck zu v4.
- Schema `report-json.schema.json` setzt `FormatVersion.const` auf
  `5`.
- Goldens unter `m5/` und `m6/` werden in AP 1.5 inhaltlich auf v5
  migriert. Beide Verzeichnisse leben weiter als fachliche
  Datensatz-Trennung.

## Modellvertrag

### Neue Enums

```cpp
namespace xray::hexagon::model {

enum class AnalysisSection {
    tu_ranking,
    include_hotspots,
    target_graph,
    target_hubs,
};

enum class AnalysisSectionState {
    active,
    disabled,
    not_loaded,
};

enum class TuRankingMetric {
    arg_count,
    include_path_count,
    define_count,
};

}  // namespace xray::hexagon::model
```

Regeln fuer `AnalysisSection`:

- `tu_ranking`, `include_hotspots`, `target_graph`, `target_hubs`
  decken die vier konfigurierbaren Analyseabschnitte ab.
- Sortierstaerke ueber explizite Rangtabelle:
  `tu_ranking=0`, `include_hotspots=1`, `target_graph=2`,
  `target_hubs=3`. Diese Reihenfolge ist auch die kanonische
  Section-Reihenfolge in Reportausgaben.
- AP 1.5 fuegt keine weiteren Sections hinzu; `--analysis all` hat
  einen festen, abgeschlossenen Wert-Bereich.

Regeln fuer `AnalysisSectionState`:

- `active`: Section wurde aktiviert UND Daten sind verfuegbar.
- `disabled`: Section wurde nicht angefordert, also steht sie nicht in
  `analysis_configuration.effective_sections`. Der frueher denkbare
  Fall `target-hubs` ohne `target-graph` ist kein `disabled`-Zustand,
  sondern ein CLI-Verwendungsfehler und erreicht das Modell nicht.
- `not_loaded`: Section wurde aktiviert, aber Daten sind nicht
  verfuegbar (z. B. `target-graph` aktiviert, aber Compile-DB-only-
  Pfad ohne File-API).

Regeln fuer `TuRankingMetric`:

- Drei Metriken aus dem M5-`RankedTranslationUnit`-Modell
  (`arg_count`, `include_path_count`, `define_count`).
- AP 1.5 fuegt keine `score`-Schwelle hinzu, weil das M5-Score-Feld
  ein abgeleiteter Wert ist; Filter sollen auf den Quell-Metriken
  agieren.

### Erweiterung von `AnalysisResult`

```cpp
struct AnalysisConfiguration {
    std::vector<AnalysisSection> requested_sections;
    std::vector<AnalysisSection> effective_sections;
    std::map<TuRankingMetric, std::size_t> tu_thresholds;
    std::size_t min_hotspot_tus{2};
    std::size_t target_hub_in_threshold{10};
    std::size_t target_hub_out_threshold{10};
};

struct AnalysisResult {
    // ... bestehende Felder ...
    AnalysisConfiguration analysis_configuration;
    std::map<AnalysisSection, AnalysisSectionState> analysis_section_states;
};
```

Regeln:

- `requested_sections` ist die durch den Nutzer (oder Default `all`)
  angeforderte Section-Liste, kanonisch sortiert nach Rangtabelle.
- `effective_sections` ist die validierte, kanonische Section-Auswahl
  nach Aufloesung von `all`; bei `--analysis all` oder ohne
  `--analysis` enthaelt sie alle vier Sektionen. Bei
  `--analysis tu-ranking,target-graph,target-hubs` enthaelt sie diese
  drei. Datenverfuegbarkeit wird danach ueber
  `analysis_section_states` ausgedrueckt, nicht durch Entfernen aus
  `effective_sections`.
- `tu_thresholds` ist ein Map von `TuRankingMetric` auf Mindest-Wert.
  Leere Map (keine `--tu-threshold`-Option) bedeutet keine
  Schwellenfilterung. Wenn eine Metrik gesetzt ist, gibt es fuer diese
  Metrik genau einen Eintrag (CLI verhindert Duplikate).
  Im Request- und Servicemodell darf die Map leer oder partiell sein.
  Reportadapter normalisieren sie vor der Ausgabe auf alle drei
  Metriken; fehlende Metriken werden als `0` serialisiert.
- `min_hotspot_tus`, `target_hub_in_threshold`,
  `target_hub_out_threshold` sind die wirksamen Schwellen
  (CLI-Default oder CLI-gesetzt).
- `analysis_section_states` enthaelt fuer jede der vier Sections
  genau einen Eintrag; das Feld ist Pflicht und immer voll besetzt.
  Invariante: `state=disabled` genau dann, wenn die Section nicht in
  `effective_sections` steht. `state=active|not_loaded` ist nur fuer
  Sections in `effective_sections` erlaubt.

### Erweiterung von `AnalyzeProjectRequest`

```cpp
struct AnalyzeProjectRequest {
    // ... bestehende Felder ...
    std::vector<AnalysisSection> analysis_sections;
    std::map<TuRankingMetric, std::size_t> tu_thresholds;
    std::size_t min_hotspot_tus{2};
    std::size_t target_hub_in_threshold{10};
    std::size_t target_hub_out_threshold{10};
};
```

Regeln:

- `analysis_sections` ist die schon im CLI-Adapter normalisierte
  Section-Auswahl (kanonisch sortiert, deduplifziert,
  `--analysis all` durch alle vier Sections ersetzt).
- Alle anderen Felder sind die schon im CLI-Adapter validierten
  Werte. Service-Tests duerfen sie direkt setzen, ohne den
  CLI-Pfad nachzubauen.
- `tu_thresholds` darf hier leer oder partiell sein. Die Semantik ist:
  fehlende Metrik = Schwelle `0` = kein Filter fuer diese Metrik.
  JSON, HTML, Markdown, Console und DOT serialisieren daraus immer alle
  drei Metriken in kanonischer Reihenfolge.

## CLI-Vertrag

### `--analysis <list>`

- **Wert**: kommagetrennte Liste, Tokens nach ASCII-Whitespace-Trim.
- **Erlaubte Werte**: `all`, `tu-ranking`, `include-hotspots`,
  `target-graph`, `target-hubs`.
- **Default**: `all`.
- **Normalisierung**: nach der Validierung wird die Liste kanonisch
  sortiert nach Rangtabelle; Duplikate sind verboten.
- **Fehlerphrasen** (CLI-Verwendungsfehler, Exit-Code `2`, Text auf
  `stderr`, kein stdout-Report, keine Zieldatei):
  - `"--analysis: unknown analysis '<value>'; allowed: all, tu-ranking, include-hotspots, target-graph, target-hubs"`
    bei unbekanntem Token.
  - `"--analysis: 'all' must not be combined with other analysis values"`
    bei `--analysis all,tu-ranking` oder aehnlich.
  - `"--analysis: duplicate analysis value '<value>'"` bei doppelten
    Tokens nach Trimming.
  - `"--analysis: target-hubs requires target-graph"` bei
    `--analysis target-hubs` oder `--analysis target-hubs,tu-ranking`
    ohne `target-graph` in derselben Liste.
  - `"--analysis: empty analysis value in list"` bei leerem Token
    (z. B. `--analysis tu-ranking,,target-graph`).
  - `"--analysis: option specified more than once"` bei doppeltem
    Setzen der Option selbst.

Wichtig:

- Die Validierung erfolgt vor dem Lesen der Eingabedaten. Fehlende
  Target-Graph-Daten sind kein CLI-Fehler bei `--analysis all`
  oder `--analysis target-graph,target-hubs`; die betroffenen
  Sektionen werden als `not_loaded` markiert.
- Der Fehler `target-hubs requires target-graph` betrifft
  ausschliesslich die widerspruechliche Abschnittsauswahl, nicht
  die spaeter erkannte Datenlage.
- `--analysis` ist nach der Validierung eine geschlossene, kanonische
  Abschnittsauswahl. Nutzer muessen Voraussetzungen explizit
  mitangeben.

### `--tu-threshold <metric>=<n>`

- **Wert**: `<metric>=<n>` mit Metriken `arg_count`,
  `include_path_count`, `define_count`.
- **`<n>`**: ganze Zahl `>= 0`. Eine Schwelle von `0` ist
  semantisch "keine Filterung" und wird trotzdem akzeptiert; das
  vereinfacht Skripte.
- **Mehrfach-Setzen**: erlaubt, aber jede Metrik nur einmal.
- **Default**: keine zusaetzliche TU-Schwelle (leere Map).
- **Fehlerphrasen**:
  - `"--tu-threshold: invalid syntax '<value>'; expected <metric>=<n>"`
    bei fehlendem `=`.
  - `"--tu-threshold: unknown metric '<metric>'; allowed: arg_count, include_path_count, define_count"`
    bei unbekannter Metrik.
  - `"--tu-threshold: not an integer"` bei nicht-numerischem `<n>`.
  - `"--tu-threshold: negative value"` bei `<n> < 0`.
  - `"--tu-threshold: duplicate metric '<metric>'"` bei zweiter
    Angabe derselben Metrik.

### `--min-hotspot-tus <n>`

- **Wert**: ganze Zahl `>= 0`.
- **Default**: `2` (M3-Konstante aus `analysis_support.cpp`).
- **Fehlerphrasen**:
  - `"--min-hotspot-tus: not an integer"`.
  - `"--min-hotspot-tus: negative value"`.
  - `"--min-hotspot-tus: option specified more than once"`.

### `--target-hub-in-threshold <n>` und `--target-hub-out-threshold <n>`

- **Wert**: ganze Zahl `>= 0`.
- **Default**: `10` (AP-1.1-Konstanten).
- **Fehlerphrasen** analog `--min-hotspot-tus`, mit jeweiligem
  Optionsnamen.

### `--impact-target-depth <n>` und `--require-target-graph`

Beide bleiben aus AP 1.3 unveraendert. AP 1.5 fuehrt keine zusaetzliche
Validierung ein. Die Reportausgabe der wirksamen Werte
(`impact_target_depth_requested`/`effective`) bleibt aus AP 1.3
bestehen; AP 1.5 ergaenzt sie nur, falls das Format dort nicht
explizit war.

### Zusammenspiel

- Alle neuen `analyze`-Optionen sind kombinierbar; reichenfolgenfrei.
- `--analysis all --target-hub-in-threshold 5` ist gueltig; die
  Schwelle wirkt nur, wenn Target-Graph-Daten verfuegbar sind.
- `--analysis tu-ranking --target-hub-in-threshold 5` ist gueltig;
  die Hub-Schwelle wird im Modell gesetzt, aber `target-hubs`-
  Section ist `disabled` und wird in Reports nicht ausgegeben.
  Der Hub-Threshold-Wert erscheint trotzdem in der serialisierten
  `analysis_configuration`, damit reproduzierbar bleibt, mit
  welchen Werten der Lauf konfiguriert war.
- `--quiet`/`--verbose` aus AP M5-1.5 bleiben unveraendert.

## Section-Aktivierungs-Vertrag

Pro Section wird im `ProjectAnalyzer` der Status berechnet:

| Bedingung | Section-State |
|---|---|
| Section ist in `analysis_sections` UND Daten sind verfuegbar | `active` |
| Section ist in `analysis_sections` ABER Daten sind nicht verfuegbar | `not_loaded` |
| Section ist NICHT in `analysis_sections` | `disabled` |

Datenverfuegbarkeit:

- `tu_ranking`: immer verfuegbar, sobald eine Eingabe (Compile-DB
  oder File-API) erfolgreich geladen wurde.
- `include_hotspots`: immer verfuegbar.
- `target_graph`: verfuegbar, wenn die bestehende Target-Graph-
  Ermittlung `loaded` oder `partial` liefert; sonst `not_loaded`.
- `target_hubs`: verfuegbar genau dann, wenn `target_graph` verfuegbar
  ist. Eine angeforderte Hub-Section kann deshalb nur `active` oder
  `not_loaded` sein; eine nicht angeforderte Hub-Section ist
  `disabled`.

Invarianten:

- `analysis_section_states` ist die primaere Section-State-Quelle fuer
  alle Adapter.
- `target_graph_status` beschreibt zusaetzlich den Datenzustand des
  Target-Graph-Modells und muss mit
  `analysis_section_states["target-graph"]` konsistent sein:
  - `disabled` genau dann, wenn `target-graph` nicht in
    `effective_sections` steht.
  - `not_loaded` genau dann, wenn `target-graph` angefordert wurde, aber
    keine Target-Graph-Daten verfuegbar sind.
  - `loaded`/`partial` nur, wenn `target-graph` angefordert wurde und
    Daten verfuegbar sind.
- `analysis_section_states["target-hubs"]` darf nur `active` sein, wenn
  `target_graph_status` `loaded` oder `partial` ist.

Effekt auf Modellfelder:

- `disabled`/`not_loaded`-Section laesst die zugehoerigen Modellfelder
  in `AnalysisResult` UNVERAENDERT (also weiterhin gesetzt vom
  bestehenden Service-Code). Adapter sehen den Section-State und
  blenden den Section-Output aus.
- Damit bleibt das Modell selbst orthogonal zur Section-Konfiguration;
  Reproduzierbarkeit der Daten und CI-Auswertung ueber alternative
  Adapter (z. B. JSON fuer alle Sections, HTML nur fuer aktive)
  bleibt moeglich.

## Schwellenwert-Anwendung

Schwellenwerte greifen vor dem `--top`-Schritt:

- **TU-Schwellen** (`tu_thresholds`): Eine TU wird nur dann ins
  `RankedTranslationUnit`-Set aufgenommen, wenn alle gesetzten
  Schwellen erfuellt sind. Beispiel: `--tu-threshold include_path_count=5`
  schliesst alle TUs mit `include_path_count < 5` aus.
- **`min_hotspot_tus`**: Ein Hotspot wird nur ins
  `IncludeHotspot`-Set aufgenommen, wenn `affected_translation_units.size() >= min_hotspot_tus`.
  Default `2` reproduziert das M3-Verhalten.
- **`target_hub_in_threshold`/`target_hub_out_threshold`**: Statt
  hardcoded `10` aus AP 1.1 verwenden `target_graph_support::compute_target_hubs`-
  Aufrufe die CLI-Werte. Die `kDefaultTargetHub*Threshold`-Konstanten
  bleiben als Fallback fuer Tests.

Filter wirken VOR `--top`:

- TU-Ranking: zuerst Schwellenfilter, dann Sortierung, dann `--top`.
- Hotspots: zuerst `min_hotspot_tus`-Filter, dann
  `--include-scope`/`--include-depth`-Filter aus AP 1.4, dann
  Sortierung, dann `--top`.

Begruendung der Reihenfolge: Schwellenwerte sind fachliche
Mindestgrenzen, `--top` ist eine Anzeige-Begrenzung. Wenn eine TU
unter der Schwelle liegt, soll sie auch nicht zur `total_count`
beitragen, sondern als ausgefilterter Eintrag zaehlen.

Statistik-Felder im `AnalysisResult`:

- `tu_ranking_total_count_after_thresholds`: Anzahl TUs nach
  TU-Schwellenfilter.
- `tu_ranking_excluded_by_thresholds_count`: Anzahl TUs, die durch
  TU-Schwellen ausgeschlossen wurden.
- `include_hotspot_excluded_by_min_tus_count`: Anzahl Hotspots, die
  durch `min_hotspot_tus`-Filter ausgeschlossen wurden.

Diese drei Felder ergaenzen die bestehenden Statistik-Felder aus
AP 1.4 und werden in JSON, HTML, Markdown und Console serialisiert.

## Reportausgabe

### JSON

Neuer `analysis_configuration`-Block (`format_version=5`):

```json
{
  "analysis_configuration": {
    "analysis_sections": ["tu-ranking", "include-hotspots", "target-graph", "target-hubs"],
    "tu_thresholds": {
      "arg_count": 0,
      "include_path_count": 5,
      "define_count": 0
    },
    "min_hotspot_tus": 2,
    "target_hub_in_threshold": 10,
    "target_hub_out_threshold": 10
  },
  "analysis_section_states": {
    "tu-ranking": "active",
    "include-hotspots": "active",
    "target-graph": "not_loaded",
    "target-hubs": "not_loaded"
  }
}
```

Regeln:

- `analysis_configuration.analysis_sections` ist immer vorhanden und
  kanonisch sortiert. Bei `--analysis all` enthaelt es alle vier
  Sections; bei `--analysis tu-ranking` enthaelt das Feld
  `["tu-ranking"]`.
- `analysis_configuration.tu_thresholds` ist ein Objekt mit allen drei
  Metriken als Pflichtschluessel. Nicht gesetzte Metriken haben Wert
  `0` (semantisch "keine Filterung"). Damit bleibt das Schema
  geschlossen mit `additionalProperties: false`. Diese Vollbesetzung
  ist eine JSON-/Report-Normalisierung; sie aendert nicht den
  Servicemodell-Vertrag, in dem eine leere oder partielle
  `tu_thresholds`-Map erlaubt ist.
- `analysis_section_states` ist ein Objekt mit allen vier Sections
  als Pflichtschluessel.
- `target_hubs.thresholds` aus AP 1.2 wird auf die wirksamen Werte
  aus `analysis_configuration.target_hub_*_threshold` gesetzt;
  semantisch identisch, datentechnisch synchron.

Feldreihenfolge im Analyze-JSON (`format_version=5`):

1. `format`
2. `format_version`
3. `report_type`
4. `inputs`
5. `summary`
6. `analysis_configuration` *(neu in v5)*
7. `analysis_section_states` *(neu in v5)*
8. `include_filter`
9. `translation_unit_ranking`
10. `include_hotspots`
11. `target_graph_status`
12. `target_graph`
13. `target_hubs`
14. `diagnostics`

`summary` erhaelt zusaetzliche Felder:

| Feld | Typ | Pflicht | Werte |
| --- | --- | --- | --- |
| `tu_ranking_total_count_after_thresholds` | integer | ja | TUs nach Schwellenfilter. |
| `tu_ranking_excluded_by_thresholds_count` | integer | ja | TUs, durch Schwellen ausgeschlossen. |
| `include_hotspot_excluded_by_min_tus_count` | integer | ja | Hotspots, durch `min_hotspot_tus` ausgeschlossen. |

Sections, deren `analysis_section_states`-Eintrag `disabled` oder
`not_loaded` ist, werden trotzdem im JSON ausgegeben, aber mit
deterministischen Empty-Section-Strukturen:

- `disabled`-`tu-ranking`: `translation_unit_ranking.items=[]`,
  Zaehler `0`, neuer Status `analysis_section_states.tu-ranking="disabled"`.
- `disabled`-`include-hotspots`: `include_hotspots.items=[]`, Zaehler
  `0`.
- `disabled`-`target-graph`: `target_graph.nodes=[]`,
  `target_graph.edges=[]`, `target_graph_status="disabled"` (neuer
  Wert in v5, siehe unten).
- `disabled`-`target-hubs`: `target_hubs.inbound=[]`,
  `target_hubs.outbound=[]`.
- `not_loaded`-`target-graph`: `target_graph.nodes=[]`,
  `target_graph.edges=[]`, `target_graph_status="not_loaded"`.
- `not_loaded`-`target-hubs`: `target_hubs.inbound=[]`,
  `target_hubs.outbound=[]`, waehrend `target_graph_status` den
  Zustand des Target-Graph-Modells (`not_loaded`) traegt.
- `not_loaded` ist fuer `tu-ranking` und `include-hotspots` im
  normalen Analyze-Pfad nicht erwartet; falls es durch einen
  Eingangsdatenfehler entsteht, werden dieselben Empty-Strukturen wie
  bei `disabled` serialisiert und der Section-State bleibt
  `not_loaded`.

Erweiterung von `TargetGraphStatus`-Enum (im Modell und im Schema):

```cpp
enum class TargetGraphStatus {
    not_loaded,
    loaded,
    partial,
    disabled,  // neu in AP 1.5
};
```

Regeln:

- `disabled` bedeutet "Target-Graph-Section nicht angefordert";
  semantisch unterschiedlich von `not_loaded` ("angefordert, aber Daten
  nicht verfuegbar").
- Das Schema erweitert die `TargetGraphStatus`-Enum um `disabled`.

### HTML

Neue Pflicht-Section vor `Translation Unit Ranking`:

```
1. Header / Summary
2. Inputs
3. Analysis Configuration  (neu)
4. Translation Unit Ranking
...
```

Section `Analysis Configuration` Inhalt:

- `h2`: `Analysis Configuration`.
- Sichtbarer Block mit:
  - `Sections: tu-ranking, include-hotspots, target-graph, target-hubs`
    (kommagetrennte Liste der `effective_sections`).
  - `TU thresholds: arg_count=0, include_path_count=5, define_count=0`
    (immer alle drei Metriken).
  - `Min hotspot TUs: 2`.
  - `Target hub thresholds: in=10, out=10`.
- Fuer jede Section ein Section-State-Badge im
  Section-Header der jeweiligen Section (z. B. neben dem
  `Target Graph`-`h2` ein Badge `Status: not_loaded` oder
  `Status: disabled`).
- Wenn Section `disabled`: Section bleibt mit `h2` und Absatz
  `Section disabled.`. Diese Variante ergaenzt den bestehenden
  Empty-Section-Vertrag aus AP 1.2 fuer den `disabled`-Fall.
- Wenn Section `not_loaded`: Section bleibt mit `h2` und Absatz
  `Section not loaded.`. Fuer `target-graph` und `target-hubs` ersetzt
  dieser Absatz die Tabellen/Graph-Details; fuer `tu-ranking` und
  `include-hotspots` gilt dieselbe Empty-Section-Darstellung, falls der
  Zustand wider Erwarten auftritt.

### DOT

Neue Graph-Attribute fuer Analyze-DOT:

- `graph_analysis_sections` (string-quoted): kommagetrennte Liste
  der `effective_sections`, z. B. `"tu-ranking,target-graph"`.
- `graph_min_hotspot_tus` (integer).
- `graph_target_hub_in_threshold` (integer).
- `graph_target_hub_out_threshold` (integer).
- `graph_tu_threshold_arg_count` (integer).
- `graph_tu_threshold_include_path_count` (integer).
- `graph_tu_threshold_define_count` (integer).

Diese sieben sind Pflicht ab AP 1.5 fuer Analyze-DOT.

Wenn eine Section `disabled` oder `not_loaded` ist, werden ihre
Knoten und Kanten nicht ausgegeben. Die `graph_*`-Konfigurations-
Attribute bleiben trotzdem vorhanden; der Section-State selbst wird in
DOT nicht als eigener Knoten serialisiert.

### Console

Neuer Abschnitt **Analysis Configuration** als erste Sektion nach
Summary:

```
Analysis Configuration:
  Sections: tu-ranking, include-hotspots, target-graph, target-hubs
  TU thresholds: arg_count=0, include_path_count=5, define_count=0
  Min hotspot TUs: 2
  Target hub thresholds: in=10, out=10

Section states:
  tu-ranking: active
  include-hotspots: active
  target-graph: not_loaded
  target-hubs: not_loaded
```

Wenn eine Section `disabled` oder `not_loaded` ist, wird der
zugehoerige fachliche Reportabschnitt komplett weggelassen. Der
`Section states`-Block im `Analysis Configuration`-Abschnitt bleibt die
einzige Sichtbarkeit dieser Section in Console.

### Markdown

Neuer Abschnitt **Analysis Configuration** mit Liste und Section-States:

```markdown
## Analysis Configuration

- Sections: `tu-ranking`, `include-hotspots`, `target-graph`, `target-hubs`
- TU thresholds: `arg_count=0`, `include_path_count=5`, `define_count=0`
- Min hotspot TUs: `2`
- Target hub thresholds: in=`10`, out=`10`

### Section States

| Section | State |
|---|---|
| tu-ranking | active |
| include-hotspots | active |
| target-graph | not_loaded |
| target-hubs | not_loaded |
```

Markdown-Tabellen-Escaping aus AP 1.2 gilt fuer alle Zellen.

Wenn eine Section `disabled` oder `not_loaded` ist, wird der
zugehoerige fachliche Abschnitt im Markdown komplett weggelassen. Die
Section bleibt ausschliesslich in der `Section States`-Tabelle sichtbar.

## Tests

Service-Tests `tests/hexagon/test_project_analyzer.cpp`:

- Default-Aufruf (keine neuen Optionen): alle vier Sections aktiv,
  Defaults gesetzt, Goldens v5-konform.
- `--analysis tu-ranking`: nur `tu-ranking` aktiv, Rest `disabled`.
  `target_graph_status="disabled"`.
- `--analysis target-graph` (ohne `target-hubs`): `target-graph`
  aktiv, `target-hubs` `disabled`. CLI laesst diese Kombination zu.
- `--analysis target-graph,target-hubs` mit File-API-Daten:
  beide Sections aktiv.
- `--analysis target-graph,target-hubs` mit Compile-DB-only:
  beide `not_loaded`. Diagnostic vorhanden.
- `--tu-threshold include_path_count=5`: TUs mit
  `include_path_count < 5` werden nicht ins Ranking aufgenommen;
  `tu_ranking_excluded_by_thresholds_count > 0`.
- Mehrere `--tu-threshold` pro unterschiedlicher Metrik: Filter
  als Konjunktion (alle Schwellen muessen erfuellt sein).
- `--min-hotspot-tus 5`: Hotspots mit `< 5` betroffenen TUs werden
  ausgefiltert; `include_hotspot_excluded_by_min_tus_count > 0`.
- `--target-hub-in-threshold 3`: Targets mit `in_count >= 3`
  werden zu Inbound-Hubs.
- Schwellen wirken vor `--top`: `tu_ranking_total_count_after_thresholds`
  spiegelt das Filterergebnis vor `--top`.

CLI-Tests `tests/e2e/test_cli.cpp`:

- `--analysis all` wird akzeptiert.
- `--analysis tu-ranking,target-graph` wird akzeptiert, Reihenfolge
  in `analysis_configuration` ist kanonisch.
- `--analysis all,tu-ranking` ergibt Exit-Code `2`,
  `"--analysis: 'all' must not be combined with other analysis values"`.
- `--analysis target-hubs` ergibt Exit-Code `2`,
  `"--analysis: target-hubs requires target-graph"`.
- `--analysis target-hubs,tu-ranking` ohne `target-graph` ergibt
  denselben Fehler.
- `--analysis foo` ergibt Exit-Code `2`, mit Hinweisliste der
  erlaubten Werte.
- `--analysis tu-ranking,tu-ranking` ergibt Exit-Code `2`,
  `"--analysis: duplicate analysis value 'tu-ranking'"`.
- `--analysis tu-ranking,,target-graph` ergibt Exit-Code `2`,
  `"--analysis: empty analysis value in list"`.
- Doppeltes `--analysis tu-ranking --analysis target-graph` ergibt
  Exit-Code `2`,
  `"--analysis: option specified more than once"`.
- `--tu-threshold arg_count=5` wird akzeptiert.
- `--tu-threshold arg_count=5 --tu-threshold arg_count=10` ergibt
  Exit-Code `2`,
  `"--tu-threshold: duplicate metric 'arg_count'"`.
- `--tu-threshold abc=5` ergibt Exit-Code `2`,
  `"--tu-threshold: unknown metric 'abc'; ..."`.
- `--tu-threshold arg_count=foo` ergibt Exit-Code `2`,
  `"--tu-threshold: not an integer"`.
- `--tu-threshold arg_count=-1` ergibt Exit-Code `2`,
  `"--tu-threshold: negative value"`.
- `--tu-threshold arg_count` (ohne `=`) ergibt Exit-Code `2`,
  `"--tu-threshold: invalid syntax 'arg_count'; expected <metric>=<n>"`.
- `--min-hotspot-tus 0`, `--min-hotspot-tus 100`: akzeptiert.
- `--min-hotspot-tus -1`: Exit-Code `2`, negative.
- `--min-hotspot-tus abc`: Exit-Code `2`, not an integer.
- Doppeltes `--min-hotspot-tus 2 --min-hotspot-tus 3`: Exit-Code `2`.
- `--target-hub-in-threshold` und `--target-hub-out-threshold` analog.
- Default-Test: alle vier Defaults
  (`min_hotspot_tus=2`, `target_hub_in_threshold=10`,
  `target_hub_out_threshold=10`, alle drei `tu_thresholds` mit `0`)
  erscheinen in der serialisierten Konfiguration.

Adapter-Tests fuer alle Reportformate:

- JSON: `analysis_configuration` und `analysis_section_states`-Bloecke
  immer vorhanden mit Pflichtschluesseln.
- DOT: alle sieben neuen `graph_*`-Attribute immer gesetzt.
- HTML: `Analysis Configuration`-Section mit korrekten Werten;
  Section-State-Badges in den jeweiligen Sections sichtbar.
- Console: `Analysis Configuration`-Block byteweise gepinnt;
  deaktivierte Sections werden weggelassen.
- Markdown: `## Analysis Configuration`-Section mit Listen-/
  Tabellenformat byteweise gepinnt.

E2E-/Golden-Tests:

- `analyze-config-default.<ext>`: Default-Aufruf, alle Sections
  aktiv.
- `analyze-config-tu-ranking-only.<ext>`: `--analysis tu-ranking`,
  nur `tu-ranking` aktiv.
- `analyze-config-thresholds.<ext>`: gemischte Schwellen
  (`--tu-threshold include_path_count=5 --min-hotspot-tus 5
  --target-hub-in-threshold 3 --target-hub-out-threshold 3`).
- `analyze-config-target-graph-only.<ext>`: `--analysis target-graph`
  ohne `target-hubs`.
- `analyze-config-not-loaded.<ext>`: `--analysis target-graph,target-hubs`
  mit Compile-DB-only.

Schema-Tests:

- `kReportFormatVersion == 5` mit
  `report-json.schema.json::FormatVersion::const`.
- Negativtest: ein `format_version=4`-JSON validiert NICHT gegen das
  v5-Schema.
- Negativtest: ein v5-JSON ohne `analysis_configuration`-Block
  validiert NICHT.
- Negativtest: ein v5-JSON mit `tu_thresholds`-Objekt, dem eine der
  drei Metriken fehlt, validiert NICHT.
- Negativtest: `target_graph_status` mit unbekanntem Wert
  (z. B. `frozen`) validiert NICHT.

Regressionstests:

- M5-Compile-DB-only-Goldens: nach AP-1.5-Migration enthalten sie
  alle Default-Konfigurationswerte; sonst byte-stabil.
- AP-1.4-Filter-Statistiken bleiben in v5 erhalten und werden parallel
  zu den neuen Schwellen-Statistiken serialisiert.

## Rueckwaertskompatibilitaet

Pflicht:

- Bestehende M5- und AP-1.1-bis-1.4-Felder bleiben erhalten.
- Default-Aufrufe ohne neue CLI-Optionen liefern dieselben fachlichen
  Daten wie AP 1.4 (TU-Ranking ohne Schwellen, `min_hotspot_tus=2`,
  Hub-Thresholds `10`/`10`).
- `target_hubs.thresholds`-Werte aus AP 1.2 bleiben in JSON identisch
  serialisiert; nur die Quelle (CLI statt Hardcoded) aendert sich.

Verboten:

- Stille Verhaltensaenderung an TU-Ranking-Sortierung oder Hotspot-
  Sortierung.
- Migrations-Banner. Versionssprung steht ausschliesslich in
  `format_version=5` und Schema.

## Implementierungsreihenfolge

Innerhalb von **A.1 (Modelle und CLI-Parser)**:

1. Neue Enums `AnalysisSection`, `AnalysisSectionState`,
   `TuRankingMetric`.
2. `AnalysisConfiguration` und Erweiterung von `AnalysisResult`.
3. `AnalyzeProjectRequest`-Erweiterung.
4. CLI-Parser fuer alle fuenf neuen Optionen mit allen Fehlerphrasen.
5. CLI-Tests fuer alle Fehlerphrasen.

Innerhalb von **A.2 (Service-Logik und Section-States)**:

6. Section-State-Berechnung im `ProjectAnalyzer`.
7. Schwellenwert-Anwendung in `analysis_support` und
   `target_graph_support`.
8. Statistik-Felder
   (`tu_ranking_excluded_by_thresholds_count`,
   `include_hotspot_excluded_by_min_tus_count`).
9. Service-Tests fuer alle Schwellen- und Section-Faelle.

Innerhalb von **A.3 (Schema und Format-Vertrag)**:

10. `kReportFormatVersion` auf `5` heben.
11. `TargetGraphStatus` um `disabled` erweitern.
12. `report-json.schema.json` auf v5.
13. `report-json.md` auf v5.

Innerhalb von **A.4 (JSON- und DOT-Adapter)**:

14. JSON-Adapter implementiert v5-Output.
15. DOT-Adapter implementiert v5-Output.
16. `report-dot.md` auf v5.
17. Goldens migriert.

Innerhalb von **A.5 (HTML-, Markdown- und Console-Adapter)**:

18. HTML-, Markdown- und Console-Adapter implementieren v5-Output.
19. `report-html.md` auf v5.
20. Goldens fuer alle drei Formate.

Innerhalb von **A.6 (Audit-Pass)**:

21. CLI-E2E-Tests durchlaufen.
22. Docker-Gates ausfuehren.
23. Liefer-Stand-Block aktualisieren.

## Tranchen-Schnitt

AP 1.5 wird in sechs Sub-Tranchen geliefert:

- **A.1 Modelle und CLI-Parser**: Modellerweiterung,
  CLI-Optionen-Parsing mit allen Fehlerphrasen.
- **A.2 Service-Logik und Section-States (atomar)**: Section-State-
  Berechnung, Schwellenwert-Anwendung, Statistik-Felder.
- **A.3 Schema und Format-Vertrag (atomar)**: Schema- und
  Format-Vertrags-Doku-Update auf v5.
- **A.4 JSON- und DOT-Adapter**: JSON- und DOT-Adapter, Goldens.
- **A.5 HTML-, Markdown- und Console-Adapter**: Drei Adapter, ihre
  Goldens.
- **A.6 Audit-Pass**.

**Harte Tranchen-Abhaengigkeit zu AP 1.4**: A.3 (Schema v5) darf erst
nach AP-1.4-A.3 (Schema v4) landen. A.1 und A.2 sind schemafrei und
parallel zu AP 1.4 entwickelbar.

## Liefer-Stand

Wird nach dem Schnitt der A-Tranchen mit Commit-Hashes befuellt. Bis
dahin ist AP 1.5 nicht abnahmefaehig.

- A.1 (Modelle und CLI-Parser): noch nicht ausgeliefert.
- A.2 (Service-Logik und Section-States): noch nicht ausgeliefert.
- A.3 (Schema und Format-Vertrag): noch nicht ausgeliefert.
- A.4 (JSON- und DOT-Adapter): noch nicht ausgeliefert.
- A.5 (HTML-, Markdown- und Console-Adapter): noch nicht ausgeliefert.
- A.6 (Audit-Pass): noch nicht ausgeliefert.

## Abnahmekriterien

AP 1.5 ist abgeschlossen, wenn:

- `kReportFormatVersion == 5` in C++ und Schema;
- `--analysis <list>` mit allen oben genannten Fehlerphrasen
  validiert; `target-hubs requires target-graph` als
  Konfigurationsfehler vor Eingabevalidierung erkannt wird;
- `--tu-threshold <metric>=<n>`, `--min-hotspot-tus`,
  `--target-hub-in-threshold`, `--target-hub-out-threshold` mit
  ihren jeweiligen Fehlerphrasen validieren;
- Schwellenwerte vor `--top` wirken; Statistik-Felder
  `tu_ranking_excluded_by_thresholds_count` und
  `include_hotspot_excluded_by_min_tus_count` korrekt gezaehlt
  werden;
- `analysis_section_states` fuer alle vier Sections immer gesetzt
  ist; `disabled`/`not_loaded`/`active`-Klassifikation korrekt;
- deaktivierte Sections in Reportoutput unterdrueckt werden;
- die wirksame Konfiguration (`analysis_configuration`) in allen
  fuenf Reportformaten serialisiert wird;
- `target_hubs.thresholds`-Werte aus den CLI-Optionen statt
  Hardcoded gespeist werden;
- Default-Aufrufe ohne neue Flags M5/AP-1.4-kompatibles Verhalten
  liefern und Defaults `min_hotspot_tus=2`, `target_hub_*=10`
  serialisieren;
- alle Goldens unter `tests/e2e/testdata/m5/` und `m6/` v5-konform
  sind;
- der Docker-Gate-Lauf gemaess `docs/quality.md` (Tranchen-Gate fuer
  M6 AP 1.5) gruen ist;
- `git diff --check` sauber ist.

## Offene Folgearbeiten

Diese Punkte werden bewusst in spaeteren Arbeitspaketen umgesetzt:

- Konfigurationsdatei (`cmake-xray.toml` o. ae.) fuer wiederkehrende
  Analyseprofile. M6 verwendet ausschliesslich CLI-Optionen.
- CLI-Steuerung von `include_depth_limit` und
  `include_node_budget` aus AP 1.4. Beide bleiben in M6 hardcoded.
- Compare-Vertrag fuer `format_version=5`-Reports mit
  Compare-Kompatibilitaetsmatrix (AP 1.6). AP 1.6 muss
  `analysis_configuration`-Drift als `configuration_drift`-
  Diagnostic ausgeben (siehe AP-1.6-Plan).
- DOT-Hub-Hervorhebung ueber Knoten-Attribute (`is_inbound_hub`,
  `is_outbound_hub`). AP 1.5 ergaenzt nur die Schwellen-Konfiguration;
  visuelle Hub-Hervorhebung im DOT-Graph waere ein eigener
  Adapter-Vertrag mit Versionssprung.
