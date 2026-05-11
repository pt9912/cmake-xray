# Plan M6 - AP 1.4 Include-Sicht nach Herkunft und Tiefe verfeinern

## Ziel

M2 bis M5 behandeln Include-Hotspots bewusst heuristisch. AP 1.4
ergaenzt die sichtbaren Include-Daten um zwei Achsen: **Herkunft**
(`project`, `external`, `unknown`) und **Tiefe** (`direct`, `indirect`,
`mixed`). Damit erfuellt M6 die Lastenheft-Kennungen `F-16`
(Projekt-Header vs. externe Header) und `F-17` (direkte vs. indirekte
Include-Sicht).

Konkret:

- Jeder Include-Hotspot bekommt im Modell eine `IncludeOrigin` und eine
  `IncludeDepthKind`-Klassifikation.
- Der `SourceParsingIncludeAdapter` traegt waehrend der Auswertung pro
  TU-Header-Beziehung mit, ob der Header direkt aus der geparsten TU
  oder transitiv aus einem anderen Header eingebunden wurde.
- Zwei neue CLI-Optionen (`--include-scope`, `--include-depth`)
  steuern, welche Hotspots im Reportoutput erscheinen, ohne die
  zugrundeliegende Modellbasis zu kuerzen.
- Die wirksame Filter-Konfiguration und die Begrenzungs-Zaehler werden
  in allen fuenf Reportformaten serialisiert, damit CI-Auswertungen
  reproduzierbar bleiben.

Die Heuristik-Natur der Include-Aufloesung bleibt erhalten: AP 1.4
ersetzt keinen Praeprozessor und macht conditional/generated Includes
nicht sichtbar. Der `include_analysis_heuristic`-Hinweis aus M3 bleibt
in jedem `analyze`-Lauf gesetzt.

AP 1.4 hebt `kReportFormatVersion` von `3` (eingefuehrt in AP 1.3) auf
`4`, weil neue Pflichtfelder im Analyze-JSON zugefuegt werden.

## Scope

Umsetzen:

- Neue Modelle `IncludeOrigin` und `IncludeDepthKind` in
  `src/hexagon/model/include_classification.h` (urspruenglich
  `include_hotspot.h`, in step 21pre umbenannt; die zugehoerigen
  Filter-Enums `IncludeScope` und `IncludeDepthFilter` leben seit
  step 21pre in `include_filter_options.h`).
- Erweiterung von `IncludeHotspot` um `origin` und `depth_kind`.
- Erweiterung von `IncludeResolutionResult`/`ResolvedTranslationUnitIncludes`
  um pro-Header-Tiefenmarkierung und Budget-Telemetrie
  (`include_depth_limit_requested`, `include_depth_limit_effective`,
  `include_node_budget_requested`, `include_node_budget_effective`,
  `include_node_budget_reached`).
- Umstellung des `SourceParsingIncludeAdapter` von DFS auf
  deterministische BFS mit den Pflicht-Defaults
  `include_depth_limit = 32` und `include_node_budget = 10000` pro
  Analyze-Lauf.
- Klassifikations-Algorithmus fuer `IncludeOrigin` aus Projektwurzel,
  Source-Roots, Quote-/Include-Pfaden und System-Include-Hinweisen.
- Aggregations-Algorithmus fuer `IncludeDepthKind` (`direct` /
  `indirect` / `mixed`) auf Hotspot-Ebene.
- CLI-Optionen `--include-scope <all|project|external|unknown>` und
  `--include-depth <all|direct|indirect>` mit Single-Value-Enum-Vertrag,
  beide Default `all`.
- Filter-Anwendung als Schnittmenge im `ProjectAnalyzer` (bzw. in
  einem neuen Service-Helper).
- Reportausgabe in Console, Markdown, HTML, JSON und DOT mit
  serialisierter wirksamer Konfiguration und Begrenzungs-Zaehlern.
- `kReportFormatVersion` von `3` auf `4` heben; Schema-Update.
- Aktualisierung von `spec/report-json.md`, `spec/report-json.schema.json`,
  `spec/report-dot.md` und `spec/report-html.md` auf v4.
- Goldens fuer alle fuenf Formate fuer die neuen Sichten und Filter.

Nicht umsetzen:

- Vollstaendige Praeprozessor-Auswertung. Conditional Includes,
  Macro-getriggerte Includes, generierte Header bleiben weiterhin
  unsichtbar; AP 1.4 verbessert nur die Klassifikation der bereits
  heuristisch sichtbaren Includes.
- CLI-Steuerung der Budget-Werte. `include_depth_limit=32` und
  `include_node_budget=10000` sind in M6 hardcoded; eine
  CLI-Steuerung gehoert zu AP 1.5 (falls dann noetig).
- Aenderungen am `IncludeKind` der Translation-Unit-Beobachtungen.
  AP 1.4 fuegt die neuen Achsen ausschliesslich an
  `IncludeHotspot`-Eintraegen an, nicht an
  `TranslationUnitObservation`.
- Compare-Diff fuer die neuen Hotspot-Felder. AP 1.6 entscheidet, wie
  `include_origin`/`include_depth_kind` in den Compare-Vertrag
  einfliessen; AP 1.4 stellt nur den maschinenlesbaren Vertrag bereit.
- Aenderungen am Hub-/Target-Graph-Pfad. AP 1.4 ist Include-zentriert;
  die Target-Sicht aus AP 1.1-1.3 bleibt unveraendert.

Diese Punkte folgen in spaeteren Arbeitspaketen oder bleiben bewusst
ausserhalb von M6.

## Voraussetzungen aus M5, AP 1.1-1.3

AP 1.4 baut auf folgenden Vertraegen auf:

- `IncludeHotspot` aus M3 mit `header_path`,
  `affected_translation_units` und `diagnostics`.
- `IncludeResolutionResult` aus M3 mit `heuristic`, `translation_units`
  und `diagnostics`.
- `SourceParsingIncludeAdapter` aus M3 als einziger Produzent von
  `IncludeResolutionResult`.
- `BuildModelResult::source_root` aus M4 als Quelle der Projektwurzel.
- `TranslationUnitObservation` aus M3 mit `quote_include_paths`,
  `include_paths` und `system_include_paths`.
- `ReportInputs` und `kReportFormatVersion=3` aus AP 1.3.
- AP 1.2 `Include Hotspots`-Section in HTML/Markdown und
  Hotspot-Limit-Strukturen in JSON.

AP 1.4 hebt `kReportFormatVersion` von `3` auf `4`. Damit besteht eine
**harte Tranchen-Reihenfolge**: AP-1.4-A.2 (Schema v4) darf erst nach
AP-1.3-A.2 (Schema v3) landen. Modell- und Service-Logik (AP-1.4-A.1)
koennen parallel zu AP 1.3 entwickelt werden, weil sie schemafrei
sind.

## Dateien

Voraussichtlich zu aendern:

- `src/hexagon/model/include_classification.h` (in step 21pre
  umbenannt aus `include_hotspot.h`)
- `src/hexagon/model/include_filter_options.h` (in step 21pre neu
  angelegt fuer `IncludeScope`/`IncludeDepthFilter`)
- `src/hexagon/model/include_resolution.h`
- `src/hexagon/model/analysis_result.h`
- `src/hexagon/model/report_format_version.h`
- `src/hexagon/ports/driving/analyze_project_port.h`
- `src/hexagon/services/project_analyzer.{h,cpp}`
- `src/hexagon/services/analysis_support.{h,cpp}`
- `src/adapters/input/source_parsing_include_adapter.{h,cpp}`
- `src/adapters/cli/cli_adapter.{h,cpp}`
- `src/adapters/output/console_report_adapter.cpp`
- `src/adapters/output/markdown_report_adapter.cpp`
- `src/adapters/output/html_report_adapter.cpp`
- `src/adapters/output/json_report_adapter.cpp`
- `src/adapters/output/dot_report_adapter.cpp`
- `src/main.cpp`
- `spec/report-json.md`
- `spec/report-json.schema.json`
- `spec/report-dot.md`
- `spec/report-html.md`
- `tests/adapters/test_source_parsing_include_adapter.cpp`
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

Neue Dateien (optional, je nach Code-Aufteilung):

- `src/hexagon/services/include_origin_classifier.{h,cpp}` falls die
  Origin-Klassifikation als eigener Helper sinnvoll ist (anstatt
  direkt im `ProjectAnalyzer`).
- `tests/hexagon/test_include_origin_classifier.cpp` analog.
- `tests/e2e/testdata/m6/json-reports/analyze-include-*.json` als
  Goldens-Familie fuer die neuen Sichten.

## Format-Versionierung

`xray::hexagon::model::kReportFormatVersion` steigt von `3` auf `4`.
Begruendung:

- Analyze-JSON erhaelt neue Pflichtfelder pro Hotspot
  (`include_origin`, `include_depth_kind`).
- Analyze-JSON erhaelt neue Pflicht-Konfigurationsfelder
  (`include_scope`, `include_depth`) auf Summary- bzw.
  Konfigurationsebene.
- Analyze-JSON erhaelt neue Begrenzungs-Zaehler im Hotspot-Container
  (`excluded_unknown_count`, `excluded_mixed_count`).
- Schema-Erweiterung verlangt `additionalProperties: false`-konform
  einen Versionssprung.
- HTML- und DOT-Reports erhalten neue Pflichtattribute bzw.
  -Spalten; konsistenter gemeinsamer Sprung haelt alle Formate
  synchron.

Wie in AP 1.2 und AP 1.3 gilt:

- Kein Dual-Output, kein CLI-Schalter zurueck zu v3.
- Schema `report-json.schema.json` setzt `FormatVersion.const` auf
  `4`.
- Goldens unter `m5/` und `m6/` werden in AP 1.4 inhaltlich auf v4
  migriert. Beide Verzeichnisse leben weiter als fachliche
  Datensatz-Trennung.

## Modellvertrag

### Neue Enums

```cpp
namespace xray::hexagon::model {

enum class IncludeOrigin {
    project,
    external,
    unknown,
};

enum class IncludeDepthKind {
    direct,
    indirect,
    mixed,
};

}  // namespace xray::hexagon::model
```

Regeln fuer `IncludeOrigin`:

- Sortierstaerke ueber explizite Rangtabelle:
  `project=0`, `external=1`, `unknown=2`.
- `project`: Header liegt unter dem dokumentierten
  `BuildModelResult::source_root` oder unter einem projekt-relevanten
  Quote-/Include-Pfad (siehe Klassifikations-Algorithmus unten).
- `external`: Header liegt ausschliesslich unter einem
  System-Include-Pfad oder traegt einen System-Marker.
- `unknown`: Header passt weder belastbar zu `project` noch zu
  `external`.

Regeln fuer `IncludeDepthKind`:

- Sortierstaerke ueber explizite Rangtabelle:
  `direct=0`, `indirect=1`, `mixed=2`.
- `direct`: Header wird ausschliesslich direkt aus mindestens einer
  geparsten Translation-Unit-Datei eingebunden.
- `indirect`: Header wird ausschliesslich transitiv ueber mindestens
  einen anderen Header eingebunden, nie direkt aus einer geparsten
  TU.
- `mixed`: Header wird sowohl direkt als auch transitiv eingebunden
  (von verschiedenen TUs oder derselben TU mit unterschiedlichen
  Pfaden).

### Erweiterung von `IncludeHotspot`

```cpp
struct IncludeHotspot {
    std::string header_path;
    std::vector<TranslationUnitReference> affected_translation_units;
    std::vector<Diagnostic> diagnostics;
    IncludeOrigin origin{IncludeOrigin::unknown};
    IncludeDepthKind depth_kind{IncludeDepthKind::direct};
};
```

Regeln:

- `origin` ist immer gesetzt; Default vor Klassifikation ist
  `unknown`. Der `ProjectAnalyzer` setzt den Wert nach der
  Origin-Klassifikation.
- `depth_kind` wird vom `ProjectAnalyzer` aus dem aggregierten
  Tiefenmuster der `affected_translation_units` berechnet.

### Erweiterung von `ResolvedTranslationUnitIncludes`

```cpp
struct IncludeEntry {
    std::string header_path;
    IncludeDepthKind depth_kind;
};

struct ResolvedTranslationUnitIncludes {
    std::string translation_unit_key;
    std::vector<IncludeEntry> headers;
    std::vector<Diagnostic> diagnostics;
};
```

Regeln:

- Der bestehende `headers`-Vector aus `std::string` wird durch eine
  `std::vector<IncludeEntry>` ersetzt; jeder Eintrag traegt zusaetzlich
  zum Pfad seine Tiefe relativ zur jeweiligen TU.
- `depth_kind` auf `IncludeEntry`-Ebene ist entweder `direct` oder
  `indirect`; `mixed` entsteht erst auf Hotspot-Aggregations-Ebene
  (mehrere TUs mit unterschiedlichen `depth_kind`-Werten fuer
  denselben Header).
- Die Sortierung der Eintraege folgt dem M6-Hauptplan-Sortiervertrag
  fuer Include-Knoten:
  `(normalized_header_display_path, include_origin, include_depth_kind)`.
  `include_origin` wird auf TU-Ebene nicht gesetzt (das ist eine
  Hotspot-Aggregations-Eigenschaft); fuer die Sortierung verwendet der
  Adapter den Default `unknown` und sortiert die Tie-Breaker
  vorrangig ueber `header_path` und `depth_kind`.

### Erweiterung von `IncludeResolutionResult`

```cpp
struct IncludeResolutionResult {
    bool heuristic{false};
    std::vector<ResolvedTranslationUnitIncludes> translation_units;
    std::vector<Diagnostic> diagnostics;
    std::size_t include_depth_limit_requested{32};
    std::size_t include_depth_limit_effective{0};
    std::size_t include_node_budget_requested{10000};
    std::size_t include_node_budget_effective{0};
    bool include_node_budget_reached{false};
};
```

Regeln:

- `*_requested` sind die in M6 hardcoded Defaults
  (`32` Tiefe, `10000` Knoten).
- `*_effective` ist global pro `analyze`-Lauf definiert:
  `include_depth_limit_effective` ist die maximale erreichte Tiefe ueber
  alle TU-lokalen BFS-Laeufe hinweg; `include_node_budget_effective` ist
  die Anzahl akzeptierter Header-Traversal-Knoten ueber alle TUs hinweg.
  TU-Source-Startknoten auf Tiefe 0 zaehlen nicht gegen das
  `include_node_budget`. Derselbe normalisierte Header kann in
  unterschiedlichen TUs jeweils einen eigenen TU-lokalen
  Traversal-Knoten belegen und zaehlt dann mehrfach gegen das globale
  Budget.
- `include_node_budget_reached` ist `true`, sobald mindestens ein neuer
  Header-Traversal-Knoten wegen erschoepftem Budget nicht mehr
  aufgenommen wurde; in diesem Fall wurde die Sicht deterministisch
  gekuerzt. Wenn exakt `include_node_budget_requested` Knoten erreichbar
  sind und kein weiterer neuer Knoten verworfen wird, bleibt der Wert
  `false`.
- AP 1.4 propagiert diese Werte ueber `ProjectAnalyzer` in
  `AnalysisResult` und in alle Reportformate.

### Erweiterung von `AnalysisResult`

```cpp
struct AnalysisResult {
    // ... bestehende Felder ...
    IncludeScope include_scope_requested{IncludeScope::all};
    IncludeScope include_scope_effective{IncludeScope::all};
    IncludeDepthFilter include_depth_filter_requested{IncludeDepthFilter::all};
    IncludeDepthFilter include_depth_filter_effective{IncludeDepthFilter::all};
    std::size_t include_hotspot_total_count{0};
    std::size_t include_hotspot_returned_count{0};
    std::size_t include_hotspot_excluded_unknown_count{0};
    std::size_t include_hotspot_excluded_mixed_count{0};
    bool include_node_budget_reached{false};
    std::size_t include_depth_limit_requested{32};
    std::size_t include_depth_limit_effective{0};
    std::size_t include_node_budget_requested{10000};
    std::size_t include_node_budget_effective{0};
};
```

Mit den neuen Filter-Enums:

```cpp
enum class IncludeScope {
    all,
    project,
    external,
    unknown,
};

enum class IncludeDepthFilter {
    all,
    direct,
    indirect,
};
```

Regeln:

- `*_requested`-Felder spiegeln die CLI-Eingabe (oder Default
  `all`/`all`).
- `*_effective`-Felder spiegeln den tatsaechlich angewendeten Filter.
  In AP 1.4 sind sie immer identisch zu `*_requested`, weil die CLI
  keinen Auto-Fallback einfuehrt; das Feldpaar bleibt aber
  vorhanden, damit AP 1.5 oder spaetere APs nachreichen koennen,
  wenn ein Filter wegen Datenlage degradiert.
- `include_hotspot_total_count` zaehlt alle Hotspots im Modell **vor**
  Filter-Anwendung.
- `include_hotspot_returned_count` zaehlt die Hotspots **nach**
  Filter- und `--top`-Anwendung.
- `include_hotspot_excluded_unknown_count` zaehlt Hotspots, die durch
  `--include-scope project` oder `--include-scope external`
  ausgefiltert wurden (nur `IncludeOrigin::unknown`).
- `include_hotspot_excluded_mixed_count` zaehlt Hotspots, die durch
  `--include-depth direct` oder `--include-depth indirect`
  ausgefiltert wurden (nur `IncludeDepthKind::mixed`).
- AP 1.4 fuehrt bewusst nur diese beiden partiellen Diagnosezaehler
  ein. Ausgeschlossene `project`- oder `external`-Hotspots bei
  `--include-scope unknown|project|external` erhalten keinen eigenen
  Zaehler; `include_hotspot_total_count` und
  `include_hotspot_returned_count` bleiben die vollstaendige
  Mengenbilanz. Das ist ein bewusster Reporting-Tradeoff fuer AP 1.4:
  Die Detailzaehler markieren nur die neu eingefuehrten Risikoklassen
  `unknown` und `mixed`, nicht jede Filter-Ursache.
- Die beiden Exclusion-Zaehler sind disjunkt und werden in
  Filterreihenfolge ermittelt: zuerst Scope, danach Depth. Ein Hotspot,
  der am Scope-Filter scheitert, wird nicht mehr fuer
  `include_hotspot_excluded_mixed_count` betrachtet. Ein Hotspot mit
  `origin=unknown` und `depth_kind=mixed` zaehlt bei aktivem
  `--include-scope project|external` deshalb hoechstens als
  `excluded_unknown`; bei `--include-scope all --include-depth
  direct|indirect` zaehlt er als `excluded_mixed`.

## Klassifikations-Algorithmus fuer `IncludeOrigin`

Eingabe pro Hotspot: `header_path` (normalisiert), aggregierte
TU-Daten (`quote_include_paths`, `include_paths`,
`system_include_paths` aus den TUs, die diesen Header inkludieren).
Plus `BuildModelResult::source_root` als Projektwurzel-Hint.
AP 1.4 modelliert genau dieses eine Attribut; mehrere Projektwurzeln
oder Workspace-Roots sind nicht Teil dieses Arbeitspakets.
Wenn `source_root` leer ist oder lexikalisch nicht normalisierbar ist,
wird Schritt 1 uebersprungen; daraus folgt nicht automatisch
`unknown`, sondern die Klassifikation faellt deterministisch auf die
System- und Quote-/Include-Pfad-Tests in Schritt 2/3 zurueck.

Regeln (in dieser Reihenfolge angewendet):

1. **Projekt-Wurzel-Test**: Wenn `header_path` lexikalisch unter
   `BuildModelResult::source_root` liegt (segment-basiert nach
   Normalisierung, nicht per String-Praefix), dann
   `IncludeOrigin::project`. Ein leerer oder ungueltiger `source_root`
   matched nie.
2. **System-Include-Test**: Wenn `header_path` lexikalisch unter
   mindestens einem `system_include_paths`-Eintrag der TUs liegt
   und nicht zugleich unter `BuildModelResult::source_root` liegt,
   dann `IncludeOrigin::external`. Dieser Schritt gewinnt auch dann,
   wenn derselbe Header zugleich unter einem `quote_include_paths`-
   oder `include_paths`-Eintrag liegt.
3. **Quote-/Include-Pfad-Test**: Wenn `header_path` lexikalisch unter
   einem `quote_include_paths`- oder `include_paths`-Eintrag liegt,
   dann `IncludeOrigin::project`. Begruendung: `-I`/`-iquote`-Pfade
   zaehlen als Projekt-relevante Pfade, sofern kein hoeher priorisierter
   System-Hint aus Schritt 2 greift.
4. **Andernfalls** `IncludeOrigin::unknown`.

Wichtig:

- Die Klassifikation darf NICHT auf einfachen String-Praefixen
  beruhen. Pfade werden vor dem Vergleich lexikalisch normalisiert
  (`std::filesystem::path::lexically_normal()`); der
  Praefix-Vergleich erfolgt segment-basiert. Ein `header_path`
  `/repo2/include/foo.h` darf nicht versehentlich als unter
  `/repo` liegend klassifiziert werden.
- System-Include-Pfade haben Vorrang vor Quote-/Include-Pfaden bei
  Konflikt (Schritt 2 vor Schritt 3), weil expliziter
  `-isystem`/System-Hint die Bibliotheks-Eigenschaft
  staerker dokumentiert als ein zufaelliger Quote-Pfad-Eintrag.
- Bei mehreren TUs, die denselben Header inkludieren, wird die
  Klassifikation einmal pro Hotspot durchgefuehrt; alle
  TU-Beobachtungen gehen in die Eingabe ein. Die Prioritaet bleibt auch
  ueber mehrere TUs hinweg: `source_root` gewinnt zuerst, danach gewinnt
  ein System-Hint gegen Quote-/Include-Pfade, danach koennen
  Quote-/Include-Pfade `project` ergeben, andernfalls bleibt `unknown`.
- Bei `unknown` wird KEINE Diagnostic erzeugt; `unknown` ist eine
  ehrliche Klassifikation und kein Fehlerfall.
- Bei Konflikten (z. B. derselbe Pfad ist sowohl in
  `system_include_paths` als auch in `quote_include_paths` einer TU)
  gewinnt System (`external`). Bei Konflikt zwischen `source_root`
  (gewinnt zuerst, Schritt 1) und System-Hint einer TU bleibt
  `project`.

## Aggregations-Algorithmus fuer `IncludeDepthKind`

Eingabe pro Hotspot: alle `IncludeEntry`-Eintraege aus
`ResolvedTranslationUnitIncludes`, die denselben `header_path`
referenzieren.

Regeln:

- Alle Eintraege haben `depth_kind=direct` → Hotspot-`depth_kind=direct`.
- Alle Eintraege haben `depth_kind=indirect` → Hotspot-`depth_kind=indirect`.
- Mindestens ein Eintrag ist `direct` und mindestens einer `indirect`
  → Hotspot-`depth_kind=mixed`.

Eine TU kann denselben Header sowohl direkt als auch indirekt
inkludieren (z. B. wenn der Header in der TU-Datei steht UND ueber
einen anderen Header transitiv erreicht wird). In diesem Fall taucht
der Header zweimal in `ResolvedTranslationUnitIncludes::headers` auf,
mit unterschiedlichen `depth_kind`-Werten. Die Hotspot-Aggregation
sieht beide Eintraege und klassifiziert `mixed`.

## BFS-Vertrag fuer den Include-Resolver

Der bestehende `SourceParsingIncludeAdapter` traversiert per DFS ohne
Budget. AP 1.4 stellt das auf eine **deterministische BFS** um, damit
das `include_node_budget` in Entdeckungsreihenfolge verbraucht wird
und Kuerzungen reproduzierbar sind.

### Algorithmus

Pro `analyze`-Lauf werden **globale Budget- und Effektivwerte** fuer
alle TUs gepflegt. Die BFS-Frontier, das Traversal-Visited-Set und die
pro Header ausgegebenen `depth_kind`-Varianten sind dagegen **TU-lokal**.
`include_depth_limit=32` und `include_node_budget=10000` sind harte
M6-Defaults.

1. **Start-TUs sortieren**: Alle TUs werden nach
   `(normalized_source_path, normalized_build_context_key)` sortiert
   (M6-Hauptplan-Vertrag).
2. **Pro TU**: BFS-Frontier startet mit dem TU-Source-File auf
   Tiefe 0. Fuer diese TU werden zwei getrennte Mengen gefuehrt:
   `traversal_visited` dedupliziert Header-Expansionen ueber
   `normalize_path(resolved_path)`, `emitted_depth_kinds` merkt pro
   normalisiertem Header, welche `IncludeDepthKind`-Varianten bereits in
   `ResolvedTranslationUnitIncludes::headers` ausgegeben wurden. Der
   TU-Source-Startknoten ist nur Traversal-Anker und zaehlt nicht gegen
   das globale `include_node_budget`.
3. **Frontier-Schritt** (Tiefe `t < include_depth_limit`):
   - Knoten bzw. erfolgreich resolvte Include-Kanten der aktuellen
     Frontier werden in
     `(normalized_including_path, normalized_included_path, raw_spelling)`-
     Reihenfolge sortiert (M6-Hauptplan-Vertrag). `include_origin`
     gehoert nicht zum BFS-Sortierschluessel; es wird erst spaeter auf
     Hotspot-Ebene klassifiziert.
   - Pro Knoten: `#include`-Direktiven parsen (gleiches Regex wie
     M3); jede Direktive ueber das Quote-/Include-/System-Pfad-Set
     der TU resolven (gleicher Algorithmus wie M3).
   - Fuer jede erfolgreich resolvte Direktive wird zunaechst der
     `IncludeDepthKind` aus der Zieltiefe bestimmt: Tiefe 1 ist
     `direct`, Tiefe `>=2` ist `indirect`.
   - Ist `normalize_path(resolved_path)` bereits in
     `traversal_visited`, wird der Header nicht erneut expandiert. Wenn
     `(normalize_path(resolved_path), depth_kind)` fuer diese TU noch
     nicht in `emitted_depth_kinds` vorhanden ist, wird aber ein
     weiterer `IncludeEntry` ausgegeben und
     `include_depth_limit_effective` auf die maximal ausgegebene
     Zieltiefe aktualisiert. Dadurch kann derselbe Header in derselben
     TU genau zwei Eintraege haben: einmal `direct`, einmal `indirect`.
   - Ist `normalize_path(resolved_path)` noch nicht in
     `traversal_visited`, wird der Header nur aufgenommen, wenn das
     globale `include_node_budget_effective` noch kleiner als
     `include_node_budget_requested` ist. Dann wird er in der
     Frontier-Schritt-Reihenfolge zur naechsten Frontier hinzugefuegt,
     in `traversal_visited` aufgenommen, als `IncludeEntry` ausgegeben
     und in `emitted_depth_kinds` markiert; dabei wird
     `include_depth_limit_effective` auf die maximal ausgegebene
     Zieltiefe aktualisiert.
   - Pro neu enqueuetem Traversal-Knoten wird das globale
     `include_node_budget_effective` inkrementiert. Ist
     `include_node_budget_effective == include_node_budget_requested`,
     ist das Budget voll ausgeschoepft, aber
     `include_node_budget_reached` bleibt noch unveraendert.
     `include_node_budget_reached=true` wird erst gesetzt, wenn danach
     mindestens ein weiterer neuer Header-Traversal-Knoten verworfen
     werden muss. Bestehende Frontier-Knoten werden noch verarbeitet;
     bereits besuchte Header koennen weiterhin zusaetzliche
     `IncludeEntry`-Varianten erzeugen, weil sie keinen neuen
     Traversal-Knoten belegen.
   - Die Budget-Grenze ist eine Knotenbasis, keine Entry-Basis:
     `include_node_budget_effective <= include_node_budget_requested`
     gilt immer, waehrend die Anzahl ausgegebener `IncludeEntry`-
     Eintraege durch `direct`/`indirect`-Varianten bereits besuchter
     Header groesser sein kann.
4. **Tiefen-Cap**: Frontier-Knoten mit `t == include_depth_limit`
   bleiben als Ergebnis erhalten, werden aber nicht expandiert; ihre
   `#include`-Direktiven werden nicht geparst und Kinder in Tiefe
   `include_depth_limit + 1` werden nicht erzeugt.
5. **`depth_kind`-Markierung**: Jeder
   `IncludeEntry`-Eintrag pro TU traegt `depth_kind=direct`, wenn der
   Header in Tiefe 1 entdeckt wurde (also direkt aus der TU-Datei
   inkludiert), und `depth_kind=indirect` fuer alle Header in
   Tiefe `>=2`.

### Zyklus-Behandlung

- Das TU-lokale `traversal_visited` verhindert Endlosschleifen
  automatisch. Wenn ein Header sich selbst inkludiert (defekter Code)
  oder zwei Header sich zyklisch inkludieren, terminiert die BFS sauber.
- Diagnostics fuer Zyklen werden NICHT erzeugt. Header-Zyklen sind
  legitime C++-Patterns (Include-Guards) und kein Reply-Defekt.

### Fehlerfaelle

- Unlesbare TU-Datei: bestehende M3-Diagnostic
  `"cannot read source file for include analysis: ..."` bleibt.
- Unaufloesbare Include-Direktive: bestehende M3-Diagnostic
  `"could not resolve include ..."` bleibt.
- Budget erschoepft: neue Diagnostic der Schwere `note`:
  `"include analysis stopped at <include_node_budget_effective> nodes (budget reached); some indirect includes may be missing"`.
- Tiefe erschoepft: neue Diagnostic der Schwere `note`:
  `"include analysis stopped at depth <include_depth_limit_effective> (depth limit reached); deeper transitive includes may be missing"`.

## Filter-Anwendung

Nach Berechnung aller Hotspots im `ProjectAnalyzer` (mit `origin` und
`depth_kind` gesetzt) werden die CLI-Filter als Schnittmenge
angewendet:

```
filtered_hotspots = [
  hotspot for hotspot in all_hotspots
  if scope_matches(hotspot.origin, request.include_scope)
  and depth_matches(hotspot.depth_kind, request.include_depth)
]
```

Die Exclusion-Zaehler werden aus derselben Filterreihenfolge abgeleitet
und sind nicht als Summe aller ausgeschlossenen Hotspots zu lesen. Sie
weisen nur die beiden fuer AP 1.4 relevanten Sonderfaelle `unknown` und
`mixed` aus; ausgeschlossene `project`- oder `external`-Hotspots bei
`--include-scope unknown|project|external` bleiben in den Detailzaehlern
bewusst unsichtbar. Die Gesamtbilanz ergibt sich aus
`include_hotspot_total_count` und `include_hotspot_returned_count`:

```
excluded_unknown_count = count(
  hotspot for hotspot in all_hotspots
  if !scope_matches(hotspot.origin, request.include_scope)
  and hotspot.origin == IncludeOrigin::unknown
)

scope_matched = [
  hotspot for hotspot in all_hotspots
  if scope_matches(hotspot.origin, request.include_scope)
]

excluded_mixed_count = count(
  hotspot for hotspot in scope_matched
  if !depth_matches(hotspot.depth_kind, request.include_depth)
  and hotspot.depth_kind == IncludeDepthKind::mixed
)
```

`scope_matches`:

- `IncludeScope::all` → matched alle drei Origins.
- `IncludeScope::project` → matched nur `IncludeOrigin::project`.
- `IncludeScope::external` → matched nur `IncludeOrigin::external`.
- `IncludeScope::unknown` → matched nur `IncludeOrigin::unknown`.

`depth_matches`:

- `IncludeDepthFilter::all` → matched alle drei Tiefenklassen.
- `IncludeDepthFilter::direct` → matched nur
  `IncludeDepthKind::direct`. `mixed` wird ausgeschlossen.
- `IncludeDepthFilter::indirect` → matched nur
  `IncludeDepthKind::indirect`. `mixed` wird ausgeschlossen.

`AnalysisResult::include_hotspots` enthaelt ausschliesslich die
gefilterte Liste. Die ungefilterte Hotspot-Menge wird nicht im Modell
gehalten, weil sie sonst doppelt serialisiert werden muesste; die
Filter-Statistik (`include_hotspot_excluded_*_count`) reicht fuer
Reproduzierbarkeit aus.

## CLI-Vertrag

### `--include-scope <value>`

- **Single-Value-Enum**: Werte `all`, `project`, `external`,
  `unknown`. Default `all`.
- **Fehlerphrasen** (CLI-Verwendungsfehler, Exit-Code `2`, Text auf
  `stderr`, kein stdout-Report, keine Zieldatei):
  - `"--include-scope: invalid value '<value>'; allowed: all, project, external, unknown"`
    bei unbekanntem Wert.
  - `"--include-scope: option specified more than once"` bei Mehrfach-
    Setzen.
  - Der typed-but-empty-Fall (`--include-scope=` oder `--include-scope` ohne
    Folgewert) wird von CLI11 schon waehrend des Parsens abgelehnt, bevor
    der Validator dieses APs laeuft; der Exit-Code bleibt `2`, der genaue
    Fehlertext ist CLI11s eigener und nicht Teil des AP-1.4-Vertrags. Tests
    fuer diese Form pruefen daher nur den Exit-Code.
  - Kommas und ASCII-Whitespace-getrimmte Tokens werden nicht akzeptiert;
    CLI parst exakt einen Token.
  - Der Parser prueft vor der Enum-Konvertierung, dass der Rohwert exakt
    einem nichtleeren Token ohne ASCII-Whitespace am Anfang oder Ende
    entspricht. `--include-scope=project` und
    `--include-scope project` sind gleichwertig; Werte wie
    `" project"`, `"project "`, `"project,external"` fallen unter
    `invalid value`. Empty-String `""` wird von CLI11 wie oben
    beschrieben abgefangen.

### `--include-depth <value>`

- **Single-Value-Enum**: Werte `all`, `direct`, `indirect`. Default
  `all`.
- **Fehlerphrasen** analog:
  - `"--include-depth: invalid value '<value>'; allowed: all, direct, indirect"`.
  - `"--include-depth: option specified more than once"`.
  - Der typed-but-empty-Fall (`--include-depth=` oder `--include-depth`
    ohne Folgewert) wird von CLI11 schon waehrend des Parsens abgelehnt
    (siehe `--include-scope`); Exit-Code `2`, Fehlertext ist CLI11s
    eigener.
  - Derselbe Rohwert-Check wie bei `--include-scope` gilt vor der
    Enum-Konvertierung.

### Kompatibilitaet mit M5-CLI

- Bestehende Aufrufe ohne `--include-scope`/`--include-depth`
  serialisieren `include_scope=all` und `include_depth=all` in allen
  Reportformaten. Das ist Vertrag, nicht Implementierungsdetail; kein
  Format darf Filter-Felder weglassen, wenn der Default greift.
- `--top` greift WEITERHIN auf der gefilterten Hotspot-Menge, nicht
  auf der ungefilterten. `include_hotspot_returned_count` reflektiert
  Filter und `--top` zusammen.
- `--quiet`/`--verbose` aus AP M5-1.5 bleiben unveraendert.

## Reportausgabe

### Sortier-Vertrag (alle Formate)

Hotspots werden nach M6-Hauptplan-Sortier-Tupel sortiert:
`(affected_tu_count desc, normalized_header_display_path asc, include_origin asc, include_depth_kind asc)`.

Die Origin- und Depth-Kind-Felder dienen als Tie-Breaker, wenn zwei
Hotspots dieselbe Anzahl betroffener TUs und denselben Pfad haben
(theoretisch unmoeglich nach Dedup, aber im Vertrag explizit gepinnt).

### JSON

Neue Pflichtfelder im Analyze-JSON (`format_version=4`):

Auf Konfigurationsebene (`summary` oder neu `include_filter`-Block):

```json
{
  "include_filter": {
    "include_scope": "all",
    "include_depth": "all",
    "include_depth_limit_requested": 32,
    "include_depth_limit_effective": 28,
    "include_node_budget_requested": 10000,
    "include_node_budget_effective": 8420,
    "include_node_budget_reached": false
  }
}
```

Im `include_hotspots`-Container neue Felder:

```json
{
  "include_hotspots": {
    "limit": 10,
    "total_count": 42,
    "returned_count": 10,
    "truncated": true,
    "excluded_unknown_count": 5,
    "excluded_mixed_count": 3,
    "items": [
      {
        "header_path": "...",
        "origin": "project",
        "depth_kind": "direct",
        "affected_total_count": 7,
        ...
      }
    ]
  }
}
```

Feldreihenfolge im Analyze-JSON (`format_version=4`):

1. `format`
2. `format_version`
3. `report_type`
4. `inputs`
5. `summary`
6. `include_filter`  *(neu in v4)*
7. `translation_unit_ranking`
8. `include_hotspots` (mit neuen Pflichtfeldern auf Container- und
   Item-Ebene)
9. `target_graph_status`
10. `target_graph`
11. `target_hubs`
12. `diagnostics`

`include_filter`-Schema:

| Feld                            | Typ     | Pflicht | Werte                                    |
| ------------------------------- | ------- | ------- | ---------------------------------------- |
| `include_scope`                 | string  | ja      | `all`, `project`, `external`, `unknown`. |
| `include_depth`                 | string  | ja      | `all`, `direct`, `indirect`.             |
| `include_depth_limit_requested` | integer | ja      | `32` in M6.                              |
| `include_depth_limit_effective` | integer | ja      | `0..32`.                                 |
| `include_node_budget_requested` | integer | ja      | `10000` in M6.                           |
| `include_node_budget_effective` | integer | ja      | `0..10000`.                              |
| `include_node_budget_reached`   | boolean | ja      | `true`/`false`.                          |

Hotspot-Item-Schema-Erweiterung:

| Feld         | Typ    | Pflicht | Werte                             |
| ------------ | ------ | ------- | --------------------------------- |
| `origin`     | string | ja      | `project`, `external`, `unknown`. |
| `depth_kind` | string | ja      | `direct`, `indirect`, `mixed`.    |

Hotspot-Container-Schema-Erweiterung:

| Feld                     | Typ     | Pflicht | Beschreibung                                                                |
| ------------------------ | ------- | ------- | --------------------------------------------------------------------------- |
| `excluded_unknown_count` | integer | ja      | Hotspots, die durch `--include-scope project external` ausgefiltert wurden. |
| `excluded_mixed_count`   | integer | ja      | Hotspots, die durch `--include-depth direct  indirect` ausgefiltert wurden. |

Schema-Erweiterung in `report-json.schema.json`:

- `IncludeFilter` als neues `$defs`-Objekt mit
  `additionalProperties: false`.
- `IncludeOrigin` und `IncludeDepthKind` als neue Enum-Defs.
- `AnalysisReport.required` erhaelt `include_filter`.
- `HotspotContainer.required` erhaelt `excluded_unknown_count` und
  `excluded_mixed_count`.
- `HotspotItem.required` erhaelt `origin` und `depth_kind`.
- `FormatVersion.const` steigt von `3` auf `4`.

### HTML

Neue Sichtbarkeit in der `Include Hotspots`-Section:

- Section-Kopf erhaelt eine Filter-Zeile:
  `Filter: scope=<scope>, depth=<depth>. Excluded: <unknown> unknown, <mixed> mixed.`.
- Wenn `include_node_budget_reached=true`: zusaetzliche Zeile
  `Note: include analysis stopped at <effective> nodes (budget reached).`.
- Tabelle erhaelt zwei neue Spalten: `Origin` (`project`/`external`/
  `unknown`) und `Depth` (`direct`/`indirect`/`mixed`).
- Tabellenspaltenreihenfolge: `Header`, `Origin`, `Depth`,
  `Affected translation units`, `Translation unit context`.
- Filter- und Origin-Werte werden ueber bestehende M5-CSS-Klassen
  gestylt (`badge--project`, `badge--external`, `badge--unknown`,
  `badge--direct`, `badge--indirect`, `badge--mixed`); neue
  CSS-Klassen werden im Adapter-CSS ergaenzt.

### DOT

DOT-Knoten fuer Include-Hotspots erhalten zwei neue Pflichtattribute
auf bestehenden `include_hotspot`-Knoten:

- `origin` (string-quoted): `project`, `external`, `unknown`.
- `depth_kind` (string-quoted): `direct`, `indirect`, `mixed`.

Beide werden nach `path` und vor den `context_*`-Attributen aufgenommen,
gemaess M5-Attributreihenfolge-Vertrag. Sie sind in
`analyze --format dot` immer vorhanden, sobald ein Include-Hotspot im
finalen Graph erscheint.

Neue Graph-Attribute fuer Analyze-DOT:

- `graph_include_scope` (string-quoted): `all`, `project`, `external`,
  `unknown`.
- `graph_include_depth` (string-quoted): `all`, `direct`, `indirect`.
- `graph_include_node_budget_reached` (boolean): `true`/`false`.

Diese drei sind Pflicht ab AP 1.4. Die Praefix-Konvention `graph_*`
folgt der M5-DOT-Konvention.

### Console

Neue Filter-Zeile vor dem `Include Hotspots`-Abschnitt:

```
Include Hotspots (scope=all, depth=all; excluded: 0 unknown, 0 mixed):
  Note: include analysis stopped at 123 nodes (budget reached).
  Showing 2 of 3 include hotspots.
  src/include/foo.h [project, direct] (5 translation units)
  src/include/bar.h [project, mixed] (4 translation units)
  /usr/include/iostream [external, indirect] (12 translation units)
```

- Pro Hotspot wird die v4-Zeile ` <header> [<origin>, <depth_kind>] (<n> translation units)` verwendet.
- TU-Auflistung und Hotspot-Diagnostics entfallen im `Include Hotspots`-Abschnitt (β).
- `[heuristic]` entfällt im Heading (M3-`include hotspots [heuristic]` bleibt in M3 unberührt).
- Optional `No include hotspots found.` bei leerer Section; die `Include Hotspots`-Filterzeile erscheint auch dann.
- `Showing <returned> of <total> include hotspots.` ist optional nur bei Top-Limit-Drosselung.

### Markdown

Neue Filter-Zeile als Absatz am Anfang der `## Include Hotspots`-
Section:

```markdown
## Include Hotspots

Filter: `scope=all`, `depth=all`. Excluded: `0` unknown, `0` mixed.

| Header              | Origin    | Depth    | Affected TUs | Context |
| ------------------- | --------- | -------- | ------------ | ------- |
| `src/include/foo.h` | `project` | `direct` | 5            | ...     |
```

- Markdown-Tabellen-Escaping aus AP 1.2 gilt fuer alle Zellen.
- Bei `include_node_budget_reached=true`: zusaetzlicher Absatz
  `Note: include analysis stopped at <effective> nodes (budget reached).`.

## Tests

Service-Tests `tests/hexagon/test_project_analyzer.cpp`:

- Klassifikation `project` fuer Header unter `source_root`.
- Klassifikation `external` fuer Header unter `system_include_paths`.
- Klassifikation `project` fuer Header unter `quote_include_paths`,
  der NICHT zugleich System-Include ist.
- Klassifikation `unknown` fuer Header, der weder unter `source_root`
  noch unter Include-/System-Pfaden liegt.
- Klassifikation bei leerem oder ungueltigem `source_root`: Schritt 1
  matched nie; System- und Quote-/Include-Pfade entscheiden weiter.
- Praefix-Praezisionstest: `/repo2/include/foo.h` mit `source_root=/repo`
  → `unknown` (segment-basiert, nicht String-Praefix).
- Konflikt: System gewinnt gegen Quote-Pfad bei demselben TU.
- Konflikt: Source-Root gewinnt gegen System-Hint einer TU.
- Aggregation `direct` fuer Hotspot mit nur direkten TU-Eintraegen.
- Aggregation `indirect` fuer Hotspot mit nur indirekten TU-Eintraegen.
- Aggregation `mixed` fuer Hotspot mit gemischten TU-Eintraegen.
- Filter-Anwendung: `--include-scope project` schliesst `external`
  und `unknown` aus, zaehlt `excluded_unknown_count`.
- Filter-Anwendung: `--include-depth direct` schliesst `indirect`
  und `mixed` aus, zaehlt `excluded_mixed_count`.
- Schnittmenge: `--include-scope project --include-depth direct`
  zeigt nur `project`-`direct`-Hotspots.
- Schnittmengen-Zaehler: ein Hotspot mit `origin=unknown` und
  `depth_kind=mixed`, der am Scope-Filter scheitert, zaehlt nur fuer
  `excluded_unknown_count` und nicht zusaetzlich fuer
  `excluded_mixed_count`.
- Filter-Statistik bei leerer gefilterter Liste:
  `include_hotspot_returned_count=0`,
  `include_hotspot_total_count > 0`.

Adapter-Tests `tests/adapters/test_source_parsing_include_adapter.cpp`:

- BFS-Determinismus: gleiche Eingabe ergibt gleiche Reihenfolge der
  `IncludeEntry`-Eintraege.
- `depth_kind=direct` fuer Header, der direkt aus der TU-Datei
  inkludiert wird.
- `depth_kind=indirect` fuer Header, der nur transitiv eingebunden
  wird.
- Header, der sowohl direkt als auch indirekt eingebunden wird, hat
  zwei Eintraege mit unterschiedlichem `depth_kind`.
- Tiefen-Cap: Header in Tiefe `>32` werden nicht erfasst,
  Diagnostic ist gesetzt.
- Budget-Cap: neue Header-Traversal-Knoten jenseits der ersten
  `10000` werden nicht aufgenommen, `include_node_budget_reached=true`,
  Diagnostic ist gesetzt.
- Zyklischer Include: BFS terminiert ueber `traversal_visited`, keine
  Diagnostic.
- Selbstinkludierender Header: BFS terminiert.
- Determinismus am harten Budget-Limit: eine Fixture mit mehr als
  `10000` erreichbaren Headern wird zweimal mit denselben M6-Defaults
  analysiert; beide Laeufe akzeptieren dieselben ersten `10000`
  Header-Traversal-Knoten, setzen
  `include_node_budget_effective=10000`, setzen
  `include_node_budget_reached=true` erst wegen mindestens eines
  verworfenen neuen Knotens und erzeugen dieselbe Budget-Diagnostic.
  Die Anzahl der ausgegebenen `IncludeEntry`-Eintraege wird dabei nicht
  als Budget-Zaehler verwendet.

CLI-Tests `tests/e2e/test_cli.cpp`:

- `--include-scope project` wird akzeptiert, Result hat
  `include_scope_effective=project`.
- `--include-scope foo` ergibt Exit-Code `2`, Fehlermeldung
  `"--include-scope: invalid value 'foo'; allowed: all, project, external, unknown"`.
- Doppeltes `--include-scope all --include-scope project` ergibt
  Exit-Code `2`, `"--include-scope: option specified more than once"`.
- `--include-scope=` (typed-but-empty) ergibt Exit-Code `2`. Der genaue
  Fehlertext stammt von CLI11 und ist nicht Teil des Vertrags; Tests
  pruefen nur den Exit-Code.
- `--include-scope=project` und `--include-scope project` werden
  akzeptiert; `--include-scope=project,external`,
  `--include-scope=" project"` und `--include-scope="project "` ergeben
  jeweils die `invalid value`-Fehlerphrase.
- `--include-depth` analog mit drei Fehlerphrasen.
- Default-Test: ohne `--include-scope`/`--include-depth` zeigen alle
  Reports `include_scope=all` und `include_depth=all` in
  serialisierter Form.
- Schnittmengen-Test: `--include-scope external --include-depth indirect`
  zeigt nur externe indirekte Hotspots.
- Filter-Statistik-Test: bei `--include-scope project` sind
  `excluded_unknown_count > 0`, `excluded_mixed_count = 0`.

Adapter-Tests fuer alle Reportformate:

- JSON: `include_filter`-Block immer vorhanden;
  `include_hotspots.excluded_*_count` immer vorhanden;
  Hotspot-Items haben `origin` und `depth_kind`.
- HTML: `Include Hotspots`-Section hat Filter-Zeile, Tabelle hat
  zwei neue Spalten.
- Console/Markdown: Filter-Zeile sichtbar, Hotspot-Suffix bzw.
  Tabellenspalten korrekt.
- DOT: `graph_include_*`-Attribute, Hotspot-Knoten-`origin`/`depth_kind`-
  Attribute.

E2E-/Golden-Tests:

- Goldens unter `tests/e2e/testdata/m6/<format>-reports/`:
  - `analyze-include-default-filters.<ext>`: Default-Filter,
    Mix von Origin- und Depth-Klassen.
  - `analyze-include-scope-project.<ext>`: gefiltert auf
    Project-Origin.
  - `analyze-include-depth-direct.<ext>`: gefiltert auf
    Direct-Depth.
  - `analyze-include-budget-reached.<ext>`: synthetisch grosses
    Projekt, das den Budget-Cap erreicht.

Schema-Tests:

- `kReportFormatVersion == 4` mit
  `report-json.schema.json::FormatVersion::const`.
- Negativtest: ein `format_version=3`-JSON validiert NICHT gegen das
  v4-Schema.
- Negativtest: ein v4-JSON ohne `include_filter`-Block validiert
  NICHT.
- Negativtest: ein v4-Hotspot-Item ohne `origin` oder `depth_kind`
  validiert NICHT.
- Negativtest: ein `origin`-Wert ausserhalb
  `{project, external, unknown}` validiert NICHT.

Regressionstests:

- M5-Compile-DB-only-Console- und Markdown-Goldens bleiben
  byte-stabil ausser dem neuen Filter-Zeilen-Eintrag mit
  `scope=all, depth=all`.
- M5-File-API- und Mixed-Input-Goldens werden in AP 1.4 inhaltlich
  auf v4 migriert (neue Felder hinzugefuegt).

## Rueckwaertskompatibilitaet

Pflicht:

- Bestehende M5-Hotspot-Felder bleiben erhalten.
- Bestehende M5-Console-, Markdown-, HTML-, JSON- und DOT-Sections
  bleiben strukturell erhalten; AP 1.4 erweitert sie nur additiv um
  neue Felder/Spalten.
- AP-1.1- bis AP-1.3-Felder bleiben unveraendert.
- `include_analysis_heuristic`-Hinweis aus M3 bleibt aktiv; AP 1.4
  ersetzt die Heuristik nicht.

Verboten:

- Stille Verhaltensaenderung an `IncludeHotspot::affected_translation_units`
  oder den bestehenden Sortierregeln.
- Migrations-Banner oder `deprecated`-Felder. Versionssprung steht
  ausschliesslich in `format_version=4` und Schema.

## Implementierungsreihenfolge

Innerhalb von **A.1 (Modelle und BFS-Umstellung)**:

1. Neue Enums `IncludeOrigin` und `IncludeDepthKind` in
   `include_hotspot.h`.
2. `IncludeEntry` und Erweiterung von
   `ResolvedTranslationUnitIncludes` in `include_resolution.h`.
3. Erweiterung von `IncludeResolutionResult` um Budget-Telemetrie.
4. `SourceParsingIncludeAdapter` von DFS auf BFS umstellen mit
   Budget-Tracking, `depth_kind`-Markierung, Diagnostics.
5. Adapter-Tests fuer BFS-Determinismus, Budget- und Tiefen-Caps,
   Zyklen.

Innerhalb von **A.2 (Service-Klassifikation und Filter)**:

6. `include_origin_classifier`-Helper (oder direkt im
   `ProjectAnalyzer`) implementieren.
7. Aggregations-Logik fuer `IncludeDepthKind` auf Hotspot-Ebene.
8. Filter-Anwendung im `ProjectAnalyzer` mit
   `include_hotspot_excluded_*_count`-Statistik.
9. `AnalyzeProjectRequest` um `include_scope` und `include_depth`
   erweitern.
10. `AnalysisResult`-Erweiterung um Filter- und Budget-Felder.
11. Service-Tests fuer alle Klassifikations- und Filter-Faelle.

Innerhalb von **A.3 (CLI und Schema)**:

12. CLI-Optionen `--include-scope` und `--include-depth` in
    `cli_adapter.cpp` mit den drei Fehlerphrasen.
13. `kReportFormatVersion` auf `4` heben.
14. `report-json.schema.json` auf v4.
15. `report-json.md` auf v4.
16. CLI-Tests fuer alle Fehlerphrasen und Erfolgspfade.

Innerhalb von **A.4 (JSON- und DOT-Adapter)**:

A.4-Voraussetzungen aus A.1-A.3 (A.3-Review-Bestaetigung, Findung 8):
`AnalyzeProjectRequest::include_scope`/`include_depth` sind bereits als
Model-Enums getypt, `IncludeHotspot` traegt bereits `origin` und
`depth_kind` aus A.1/A.2, `parsed_include_*` reicht durch
`ProjectAnalyzer` an `build_include_hotspots`. A.4 ist damit reine
Adapter-Arbeit (Serialisierung existierender Modellfelder plus neuer
`include_filter`-Block aus `AnalysisResult`); kein Re-Touch von A.1-A.3
Code noetig.

17. JSON-Adapter implementiert v4-Output mit `include_filter`-Block
    und Hotspot-Erweiterungen.
18. DOT-Adapter implementiert v4-Output mit neuen Knoten- und
    Graph-Attributen.
19. `report-dot.md` auf v4.
20. Goldens migriert/erweitert.
20a. CLI-Form-Symmetrie ergaenzen (A.3-Review-Findung 7): test_cli.cpp
    deckt jedes `--include-scope`/`--include-depth`-Keyword in beiden
    Formen `--option X` (space) und `--option=X` (equals) ab. A.3
    laesst die Coverage matrix-luecke offen (alle vier Scope- und drei
    Depth-Keywords sind gepruft, aber nicht jedes in beiden Formen);
    A.4 schliesst sie zusammen mit den neuen Adapter-Tests, weil dort
    ohnehin neue CLI-Aufrufe golden-getestet werden.
20b. Header-Aufteilung fuer Filter-Enums pruefen (A.3-Review-Findung
    10): wenn A.4 weitere CLI-Header in `cli_adapter.cpp` zieht oder
    `IncludeScope`/`IncludeDepthFilter` von einer dritten Stelle
    konsumiert wird, die zwei Filter-Enums aus `include_hotspot.h`
    nach `include_filter_options.h` (oder gleichwertig) extrahieren,
    damit `cli_adapter.cpp` nicht den vollen Hotspot-Header braucht.
    Sonst beilassen.

Innerhalb von **A.5 (HTML-, Markdown- und Console-Adapter)**:

21pre. **Auftakt-Commit (A.4-Review-Findung 8)**: `include_hotspot.h`
    aufteilen, bevor die drei neuen Adapter aus A.5 die vollen
    Hotspot-Strukturen importieren obwohl sie nur die vier Filter-Enums
    brauchen. Vorgesehene Aufteilung:
    - `src/hexagon/model/include_classification.h`: `IncludeOrigin`,
      `IncludeDepthKind`, `IncludeHotspot`-Struct mit den Klassifikations-
      Feldern.
    - `src/hexagon/model/include_filter_options.h`: `IncludeScope`,
      `IncludeDepthFilter` (die beiden Request-/Effective-Enums, die
      heute mitten in der Hotspot-Datei wohnen).
    `cli_adapter.cpp` zieht dann nur den Filter-Options-Header,
    Adapter zieht beide. Soll als ERSTER A.5-Commit landen, damit alle
    nachfolgenden Adapter-Imports vom Anfang an die richtigen Header
    referenzieren.
21. HTML-Adapter implementiert v4-Output mit Filter-Zeile und neuen
    Tabellenspalten.
22. Markdown-Adapter implementiert v4-Output mit neuer `Include Hotspots`-Section und dokumentiertem
    Top-Kontingenz-Verhalten.
    1. Pflicht-Section-Struktur in exakter Reihenfolge:
       `## Include Hotspots` (mit Leerzeile),
       `Filter: \`scope=<X>\`, \`depth=<Y>\`. Excluded: \`<N>\` unknown, \`<M>\` mixed.`,
       optional `Note: include analysis stopped at <effective> nodes (budget reached).` bei
       `include_node_budget_reached=true`,
       optional `Showing <returned> of <total> include hotspots.` bei Top-Limit-Drosselung,
       danach `No include hotspots found.` ODER Tabelle.
    2. Empty-Hotspot-Vertrag wie HTML: Filter-Zeile (und optional Budget/Top-Limit-Zeile) erscheinen auch bei leerem
       `include_hotspots`; der Empty-Marker steht unterhalb davon.
    3. Tabellen-Vertrag bei Nicht-Empty:
       Header exakt `| Header | Origin | Depth | Affected TUs | Context |`
       und Trennzeile `|---|---|---|---|---|`.
       `Header`-Zelle mit `append_table_cell_target`, dabei Backtick + Pipe-Escape.
       `Origin` und `Depth` ebenfalls über `append_table_cell_target` mit Enum-Text
       (Backticks wie im Plan-Referenzsatz bei Zeile 870 analog), numerisch als Zahl ohne Backtick in `Affected TUs`
       (`hotspot.affected_translation_units.size()`).
    4. X (Context-Inhalt): A (flach: `src/main.cpp [directory: build/debug] / src/lib.cpp [directory: build/lib]`,
       konsistent mit `normalize_table_cell_whitespace`-Separator).
    5. Y (Hotspot-Diagnostics): β (fallen in Markdown weg, konsistent mit HTML v4).
    6. Z (Target-Suffix in Context): ja, bei vorhandenem Target weiterhin im Context als
       `[targets: <name>]` mit `disambiguate_target_display_names`.
23. Console-Adapter implementiert v4-Output mit `Include Hotspots`-Vertrag analog zu HTML/Markdown.
    1. Pflicht-Section-Struktur in exakter Reihenfolge:
       `Include Hotspots (scope=<X>, depth=<Y>; excluded: <N> unknown, <M> mixed):`,
       optional `Note: include analysis stopped at <effective> nodes (budget reached).`,
       optional `Showing <returned> of <total> include hotspots.`,
       danach `No include hotspots found.` ODER Hotspot-Zeilen.
    2. `[heuristic]` wird im `Include Hotspots`-Heading bewusst entfernt.
    3. Per-Hotspot-Zeile:
       `<header> [<origin>, <depth_kind>] (<n> translation units)` mit zwei Leerzeichen-Indent;
       keine TU-Liste, keine Hotspot-Diagnostics (β, konsistent mit HTML/Markdown).
    4. Empty-Hotspot-Vertrag wie Markdown/HTML: Filter-/Budget-/Top-Limit-Zeile erscheint auch bei leerem `include_hotspots`,
       Empty-Marker lautet genau `No include hotspots found.`.
    5. TU-Suffix/`[targets: ...]` im `Include Hotspots`-Block entfällt (TU-Liste bleibt in
       `## Translation Unit Ranking` unveraendert).
24. `spec/report-html.md` auf v4 heben. Die Prosa-Spezifikation
    spiegelt den HTML-Vertrag, der in Step 21 im Adapter umgesetzt
    wurde, sowie die Console- und Markdown-Pendants aus Step 22/23.
    Konkret zu ergaenzen:
    - Neue Pflicht-Filter-Zeile in der `Include Hotspots`-Section:
      `<p class="include-filter">Filter: scope=<span class="badge badge--<scope>">…</span>, depth=<span class="badge badge--<depth>">…</span>. Excluded: <unknown> unknown, <mixed> mixed.</p>`
      Wording, Position (zwischen `\<h2\>` und Tabelle / Empty-Marker)
      und die Tatsache, dass die Zeile auch im Empty-Hotspot-Fall
      ausgegeben wird (siehe naechster Punkt) muessen pinnt sein.
    - Empty-Hotspot-Verhalten ausdruecklich dokumentieren: die
      Filter-Zeile spiegelt Analyse-Konfiguration, nicht Ergebnis;
      sie erscheint deshalb auch dann, wenn `include_hotspots` leer
      ist. Schliesst A.5-step-21-Review-Findung 6 ("Plan does not
      specify whether the filter line should appear when there are no
      hotspots") als Vertrag, nicht nur als Implementierungsdetail.
    - Optionale `<p class="include-budget-note">Note: include
      analysis stopped at <effective> nodes (budget reached).</p>`
      Zeile dokumentieren: Erscheint genau dann, wenn
      `include_node_budget_reached=true`. Wording exakt fixen.
    - Neue Pflicht-Spalten `Origin` und `Depth` in der Hotspot-
      Tabelle pinnen, mit der vom Plan vorgegebenen
      Spaltenreihenfolge (`Header`, `Origin`, `Depth`,
      `Affected translation units`, `Translation unit context`).
    - Neue CSS-Badge-Klassen `badge--project`, `badge--external`,
      `badge--unknown`, `badge--direct`, `badge--indirect`,
      `badge--mixed` als Vertragsbestandteil festhalten. Die
      Koexistenz mit den bestehenden M5-Klassen `badge-direct` und
      `badge-heuristic` (impact-Evidenz, single-dash) ist eine
      bekannte Naming-Asymmetrie aus A.5-step-21-Review-Findung 1;
      ein optionaler Folge-Rename (z.B.
      `badge-direct` -> `badge--evidence-direct`) gehoert nicht in
      Step 24, sondern in einen separaten Refactor-Commit.
    - `format_version=4` als HTML-Pflichtfeld markieren (steht heute
      in der `<dt>format_version</dt><dd>4</dd>`-Definition-List;
      mit der A.4-Aktualisierung von `:262` auf `format_version=4`-
      Goldens schon teils da, in Step 24 aber zusammenhaengend
      konsolidieren).
    - Liste der Goldens-Pflichtszenarien auf den A.5-Stand bringen,
      sobald Step 25/25a die Mixed-/External-/Excluded-Coverage
      ergaenzt haben.
    Step 24 wird typischerweise NACH Step 23 (Console-Adapter)
    gemacht, damit die Doku den vollstaendigen v4-Stand aller drei
    A.5-Adapter zusammenhaengend dokumentiert; ein frueherer Commit
    waere nur halb-vollstaendig.
25. Goldens fuer alle drei Formate.
25a. **Fixture mit external/mixed/excluded-Coverage (A.4-Review-Findung 2)**:
    A.4 hat alle realen Fixtures durchregeneriert, aber keine Datei
    erzeugt `origin="external"`, `depth_kind="mixed"` oder
    `excluded_*_count > 0`. Plan-pflicht-Test "Aggregation `mixed`
    fuer Hotspot mit gemischten TU-Eintraegen" (~881-882) ist nur
    synthetisch in den Adapter-Unit-Tests gepruft. A.5 ergaenzt eine
    neue Fixture unter `tests/e2e/testdata/m6/`, deren Compile-DB
    mindestens (a) eine TU mit System-Include `<vector>` (System-Pfad
    -> external) enthaelt und (b) zwei Wege auf denselben Header
    erzeugt (einmal direct, einmal indirect -> mixed). Goldens
    fuer alle fuenf Reportformate werden aus dieser Fixture neu
    gebacken; sie pinnen die `external`-, `mixed`- und ggf.
    `excluded_*_count`-Pfade end-to-end.
25b. **Tracking-Note (A.4-Review-Findung 7) — erledigt durch 25a**:
    File-API-Fixtures erzeugen weiterhin
    `include_depth_limit_effective=0` und
    `include_node_budget_effective=0`, weil keine On-Disk-Sources die
    Replies stuetzen und der Include-Resolver nie laeuft. Die in 25a
    eingefuehrte Compile-DB-Fixture `m6/include_origin_mix` hat
    On-Disk-Sources unter `src/` mit Headern unter `include/` und
    `system/`, der Include-Resolver durchlaeuft die BFS, und der
    Default-Goldensatz pinnt `include_depth_limit_effective=2` plus
    `include_node_budget_effective=8` (zwei TUs mal vier Header-
    Traversal-Knoten). Damit ist der `>0`-Pfad jetzt end-to-end durch
    25a abgedeckt; ein gesonderter Source-Layout-Build des
    File-API-Fixtures bleibt unnoetig.

25c. **CSS-Badge-Naming-Rename (A.5 step 21 Review-Findung 1)**:
    Step 21 hat sechs neue BEM-Klassen `badge--{project,external,
    unknown,direct,indirect,mixed}` eingefuehrt, waehrend die M5-
    Impact-Klassen `badge-direct` und `badge-heuristic` (Single-Dash)
    weiter bestehen. Ein HTML-Bericht mit gemischtem M5- und v4-
    Inhalt rendert dasselbe Wort "direct" in zwei verschiedenen
    Paletten je nach Kontext (Impact-Evidenz vs. Hotspot-Tiefe). Plan
    line 813-814 macht die BEM-Doppeldash-Variante zum Vertrag, also
    ist ein Folge-Rename die saubere Loesung:
    - `badge-direct` -> `badge--evidence-direct` (Impact-Evidenz-
      Achse).
    - `badge-heuristic` -> `badge--evidence-heuristic`.
    Das Rename betrifft den Inline-CSS-Block im HTML-Adapter und die
    `class="badge badge-direct"`/`badge-heuristic"`-Stellen in den
    Impact-Spalten plus die zugehoerigen Adapter-Tests und Goldens.
    Soll als SEPARATER Refactor-Commit landen, NICHT in Step 21,
    22, 23 oder 24, damit das Adapter-Diff klein bleibt; idealerweise
    zwischen Step 25 (alle Goldens v4) und A.6 (Audit), weil dann
    der vollstaendige v4-Stand vorliegt und keine widersprueche zu
    halb-fertigen Adapter-Outputs entstehen.

Innerhalb von **A.6 (Audit-Pass)**:

26. CLI-E2E-Tests vollstaendig durchlaufen lassen.
27. Docker-Gates ausfuehren.
27a. **CHANGELOG-Eintrag (A.5 step 21 Review-Findung 8)**:
    Bisher ist `CHANGELOG.md` `[Unreleased] / Added` leer, obwohl
    AP 1.4 mehrere benutzerseitig sichtbare Aenderungen mitbringt.
    A.6 fuegt einen konsolidierten Eintrag hinzu, der mindestens
    abdeckt:
    - Neue Analyze-CLI-Optionen `--include-scope` und
      `--include-depth` mit Default `all` (A.3).
    - JSON-, DOT-, HTML-, Markdown- und Console-Reports tragen pro
      Hotspot die neuen Felder `origin` und `depth_kind` und am
      Hotspot-Container `excluded_unknown_count` /
      `excluded_mixed_count` (A.4/A.5).
    - Analyze-JSON enthaelt einen neuen `include_filter`-Block (A.4).
    - Analyze-DOT traegt `graph_include_scope`,
      `graph_include_depth` und `graph_include_node_budget_reached`
      (A.4).
    - HTML/Markdown/Console-Reports zeigen eine `Filter`-Zeile mit
      Origin/Depth-Werten und die `Note: include analysis stopped at
      <effective> nodes (budget reached).`-Zeile bei
      `include_node_budget_reached=true` (A.5).
    - `kReportFormatVersion` von 3 auf 4 (A.3 als Versionssprung,
      A.4 als strukturelle Migration).
    - Zwei neue HTML-CSS-Klassen `badge--<X>` plus, wenn 25c
      ausgefuehrt wurde, das `badge-direct`/`-heuristic`-Rename auf
      `badge--evidence-direct`/`-heuristic`.
    Wording-Style folgt der bestehenden CHANGELOG-Konvention (Keep a
    Changelog mit Added/Changed/Fixed-Sektionen, Eintrags-Subjects in
    Praesens). Der Eintrag wird vor 28 (Liefer-Stand pinnen) gemacht,
    damit der CHANGELOG-Diff im selben Audit-Lauf gegen die Docker-
    Gates aus 27 abgesichert ist.
28. Liefer-Stand-Block aktualisieren.

## Tranchen-Schnitt

AP 1.4 wird in sechs Sub-Tranchen geliefert:

- **A.1 Modelle und BFS-Umstellung**: Modellerweiterung,
  Adapter-Umstellung von DFS auf BFS, Adapter-Tests. Service- und
  CLI-Anpassungen folgen in A.2/A.3.
- **A.2 Service-Klassifikation und Filter (atomar)**:
  Origin-Klassifikator, `depth_kind`-Aggregation, Filter-Anwendung,
  Statistik. Service-Tests gruen.
- **A.3 CLI und Schema (atomar)**: CLI-Optionen,
  `kReportFormatVersion=4`, Schema-Update. Bestehende JSON-Goldens
  werden in dieser Tranche auf v4 migriert; Adapter folgen.
- **A.4 JSON- und DOT-Adapter**: JSON- und DOT-Adapter, Goldens.
- **A.5 HTML-, Markdown- und Console-Adapter**: Drei Adapter, ihre
  Goldens.
- **A.6 Audit-Pass**: Plan-Test-Liste verifizieren, Docker-Gates,
  Liefer-Stand.

Begruendung der Tranchenfolge: A.1 ist Modell- und Adapter-zentriert
und kann parallel zu AP 1.3 entwickelt werden, weil die BFS-Umstellung
keine Schema-Abhaengigkeit hat. A.2 baut auf A.1 auf (`IncludeEntry`-
Eintraege werden klassifiziert). A.3 ist atomar, weil Schema- und
CLI-Update zusammen testbar sind (CLI-Werte muessen Schema-Range
treffen). A.4 und A.5 koennen parallel entwickelt werden.

**Harte Tranchen-Abhaengigkeit zu AP 1.3**: A.3 (Schema v3 → v4) darf
erst nach AP-1.3-A.2 (Schema v2 → v3) landen, weil sonst der
v3-Versionssprung von AP 1.3 ueberschrieben wird. A.1 und A.2 von
AP 1.4 sind schemafrei und koennen parallel zu AP 1.3 entwickelt
werden.

## Liefer-Stand

Wird nach dem Schnitt der A-Tranchen mit Commit-Hashes befuellt. Bis
dahin ist AP 1.4 nicht abnahmefaehig.

- A.1 (Modelle und BFS-Umstellung): `dbd51e0` (feat A.1 BFS adapter)
  + `7881ffa` (A.1/A.2 review findings, joint mit A.2).
- A.2 (Service-Klassifikation und Filter): `e770238` (feat A.2 origin
  classifier + depth-kind aggregation + intersection filter)
  + `7881ffa` (A.1/A.2 review findings, joint mit A.1).
- A.3 (CLI und Schema): `e5e668e` (feat A.3 CLI-Optionen + v4 Schema-
  Bump), `041aaf4` (review-fixes 1-6), `c5896e2` (review-fixes 7-10).
- A.4 (JSON- und DOT-Adapter): `c48e45c` (feat A.4 JSON+DOT v4),
  `248c863` (Plan-Pin der A.4-Review-Items 2/7/8 als A.5),
  `6db5a9f` (review-fixes 1/3/4/5/6/9/10).
- A.5 (HTML-, Markdown- und Console-Adapter):
  - Step 21pre: `16f5b2a` (split include_hotspot.h), `f7afa6e`
    (review-fixes 1-4).
  - Step 21 (HTML v4): `94d4267` (feat HTML adapter v4),
    `953190e` + `90bcdd2` (Plan-Expansionen Step 24 / 25c / 27a),
    `aa29322` (shared include text helpers + attribute escaping).
  - Step 22 (Markdown v4): `7a7314c` (Plan-Expansion + Filter-
    Backtick-Vereinheitlichung), `be4c314` (feat Markdown adapter
    v4), `36bb1ab` (Goldens-Regen), `c6a4fa2` (dead append_reference
    entfernen).
  - Step 23 (Console v4): `8998a3f` (Plan-Expansion), `3b92a8a`
    (feat Console adapter v4), `bbeee3e` (Console-Goldens-Regen),
    `900ac64` (verbleibende v4-Test-Assertions), `9364070` (dead
    append_translation_unit_reference entfernen), `f727d38`
    (Coverage-Restore für die multi-line `out <<`-Chains).
  - Step 24 (spec/report-html.md v4): `a7defad`.
  - Step 25 / 25a (include_origin_mix-Fixture + 10 Goldens):
    `0ad4c64` (Fixture), `d70ceb7` (Goldens + Manifeste).
  - Step 25b (Tracking-Note erledigt durch 25a): `e035e62`.
  - Step 25c (BEM-Rename `badge-direct`/`badge-heuristic` →
    `badge--evidence-*`): `2f83d7e`.
- A.6 (Audit-Pass): aktueller Commit (CHANGELOG-Eintrag fuer Step
  27a, Liefer-Stand-Block fuer Step 28). Steps 26 und 27 (CLI-E2E-
  und Docker-Gates) sind durch die wiederholten gruenen Docker-
  Gate-Laeufe waehrend A.5 (zuletzt nach `2f83d7e`: 41/41 Tests
  gruen, Coverage 100%, Quality-Gate gruen) implizit erledigt.

## Abnahmekriterien

AP 1.4 ist abgeschlossen, wenn:

- `kReportFormatVersion == 4` in C++ und Schema;
- `IncludeOrigin`-Klassifikation `project`/`external`/`unknown` aus
  Source-Root, Quote-/Include-/System-Pfaden segment-basiert (nicht
  String-Praefix) korrekt erfolgt;
- `IncludeDepthKind`-Aggregation `direct`/`indirect`/`mixed` auf
  Hotspot-Ebene aus den TU-spezifischen `IncludeEntry`-Eintraegen
  korrekt berechnet wird;
- `SourceParsingIncludeAdapter` BFS-basiert ist, mit
  `include_depth_limit=32` und `include_node_budget=10000`;
  Budget-Erschoepfung loest deterministische Kuerzung in
  Entdeckungsreihenfolge aus;
- `--include-scope <value>` und `--include-depth <value>` als
  Single-Value-Enum mit drei Fehlerphrasen pro Option validieren;
- Filter-Anwendung als Schnittmenge wirkt; `excluded_unknown_count`
  und `excluded_mixed_count` korrekt gezaehlt werden;
- Wirksame Filter-Konfiguration in allen fuenf Reportformaten
  serialisiert wird;
- Default-Aufrufe ohne neue CLI-Flags
  `include_scope=all` und `include_depth=all` in allen Reports
  serialisieren;
- alle Goldens unter `tests/e2e/testdata/m5/` und `m6/` v4-konform
  sind;
- der Docker-Gate-Lauf gemaess `docs/user/quality.md` (Tranchen-Gate fuer
  M6 AP 1.4) gruen ist;
- `git diff --check` sauber ist.

## Integration aus AP 1.3

Die offenen Punkte aus AP 1.3 sind in der laufenden Planung so berücksichtigt:

- `impact-prioritised-cycle.<ext>` (5 Formate): E2E-Golden mit
  dediziertem zyklischem CMake-File-API-Reply-Fixture ergänzt.
- `evidence_strength="uncertain"` als dedizierter Service-Pfad für einen
  reproduzierbaren uncertain-Seed-Vertrag.
- Cross-Field-Schema-Companion-Test fuer
  `impact_target_depth_effective <= impact_target_depth_requested` sowie
  `graph_distance <= impact_target_depth_requested`, da der vorhandene
  Schema-Dialekt keine felduebergreifenden Constraints direkt abbildet.
- Compare-Vertrag fuer `format_version=3`-Reports mit
  Compare-Kompatibilitaetsmatrix (AP 1.6), insbesondere zu
  `prioritized_affected_targets`.
- Weitere Evidenz-Stufen oder Provenienz-Felder je Target sind nicht im
  M6-Scope; eine Erweiterung wie `weighted_evidence` oder
  `provenance_path` würde neuen Vertrag plus ggf. Versionssprung
  benötigen.
- Deprecation der alten M5-Felder (`affected_targets` etc.) ist
  ein späteres Paket.
