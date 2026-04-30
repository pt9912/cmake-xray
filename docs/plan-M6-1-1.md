# Plan M6 - AP 1.1 Target-Graph-Vertraege im Kern schaerfen

## Ziel

AP 1.1 schafft die Kernmodelle und Adapter-Vertraege, auf denen die uebrigen
M6-Arbeitspakete aufbauen. M4 hat Translation-Unit-Beobachtungen ueber
`TargetAssignment` an Targets gebunden; AP 1.1 fuehrt erstmals direkte
Target-zu-Target-Abhaengigkeiten als eigenstaendige Kernsicht ein, ohne die
bestehenden TU- und Include-Modelle mit CMake-File-API-spezifischen
JSON-Strukturen zu koppeln.

Nach AP 1.1 existiert noch keine sichtbare Ausgabe der Target-Graph-Daten und
keine Impact-Priorisierung. AP 1.1 stellt nur die belastbaren Modell-, Port-,
Adapter- und Service-Vertraege her, auf denen AP 1.2 (Reportausgaben), AP 1.3
(Impact-Priorisierung) und AP 1.5 (CLI-Schwellenwerte) direkt aufsetzen.

Das Hexagon-Prinzip aus M4/M5 bleibt bindend: Die Fachlogik fuer Target-Graph,
Hub-Klassifikation und spaetere Impact-Priorisierung lebt im Kern, nicht in
Console-, Markdown-, HTML-, JSON- oder DOT-Adaptern.

## Scope

Umsetzen:

- Neue Modelle `TargetDependency`, `TargetGraph` und `TargetGraphStatus` im
  Kern.
- Erweiterung von `BuildModelResult` um `target_dependencies` und
  `target_graph_status`.
- `CmakeFileApiAdapter` extrahiert direkte Target-zu-Target-Abhaengigkeiten aus
  vorhandenen Reply-Daten (`dependencies`-Array der Target-Reply-Dateien).
- `ProjectAnalyzer` propagiert die Target-Graph-Sicht in `AnalysisResult`,
  inklusive `target_graph_status`-Diagnostics.
- `ImpactAnalyzer` propagiert dieselbe Sicht in `ImpactResult`, ohne in AP 1.1
  bereits Impact-Priorisierung zu berechnen.
- Hub-Klassifikation ueber `target_hub_in_threshold` und
  `target_hub_out_threshold` als Kernfunktion mit harten M6-Default-Schwellen
  (`10`/`10`); CLI-Steuerung folgt in AP 1.5.
- Deterministische Sortierung von Knoten und Kanten nach den in
  `docs/plan-M6.md` Abschnitt 1.2 fixierten Tupeln.
- Behandlung von Selbstkanten, fehlenden Target-Definitionen und externen
  Abhaengigkeitszielen ueber Diagnostics, nicht ueber erratene Kanten.
- Allgemeine Zyklen-Erkennung uebersteigt AP 1.1, weil AP 1.1 keine
  Graph-Traversierung durchfuehrt; sie wird in AP 1.3 ueber die
  Reverse-BFS abgedeckt.
- Kern- und Adapter-Tests, die das neue Modell, Sortierung, Selbstkanten,
  Mehrfach- und externe Kanten, defekte `dependencies`-Eintraege ohne `id`
  sowie den Compile-Database-only-Fallback absichern.

Nicht umsetzen:

- Sichtbarkeit der Target-Graph-Daten in Console, Markdown, HTML, JSON oder DOT
  (gehoert zu AP 1.2).
- Impact-Priorisierung ueber den Target-Graphen (gehoert zu AP 1.3).
- CLI-Optionen `--target-hub-in-threshold`, `--target-hub-out-threshold` und
  Schwellen-Provenienz in Reports (gehoeren zu AP 1.5).
- Eigene neue JSON-Formatversion, JSON-Schema-Aenderungen oder
  `format_version`-Erhoehung; AP 1.1 fuehrt keine maschinenlesbaren
  Reportfelder ein.
- Eine vollstaendige transitive Hull oder reverse-Adjazenzliste; AP 1.1 stellt
  nur direkte Kanten und Hub-Klassifikation bereit. Reverse-BFS lebt in AP 1.3
  und nutzt die in AP 1.1 gespeicherte direkte Kantenliste.
- Aenderungen an `compile_commands.json`-only-Pfaden ueber das
  `target_graph_status=not_loaded`-Default hinaus.

Diese Punkte folgen in spaeteren M6-Arbeitspaketen.

## Voraussetzungen aus M4/M5

AP 1.1 baut auf folgenden bereits stabilen Vertraegen auf:

- `BuildModelResult` mit `compile_database`, `target_assignments`,
  `target_metadata`, `source_root` und `cmake_file_api_resolved_path` aus M4
  und M5 AP 1.1.
- `TargetInfo` mit `display_name`, `type` und `unique_key` aus M4
  (`src/hexagon/model/target_info.h`).
- `CmakeFileApiAdapter` als einziger Produzent von `BuildModelResult` mit
  Target-Daten; `CompileCommandsJsonAdapter` liefert weiterhin keinen
  Target-Graphen.
- `ReportInputs` und `kReportFormatVersion` aus M5 AP 1.1; AP 1.1 aendert beide
  nicht.
- `ProjectAnalyzer`/`ImpactAnalyzer` als kanonische Producer von
  `AnalysisResult` und `ImpactResult`.

## Dateien

Voraussichtlich zu aendern:

- `src/hexagon/model/build_model_result.h`
- `src/hexagon/model/analysis_result.h`
- `src/hexagon/model/impact_result.h`
- `src/hexagon/model/target_info.h` (nur zur kommentierten Schluesselverwendung,
  keine Felderaenderung; Hauptaenderungen liegen in den neuen Dateien)
- `src/adapters/input/cmake_file_api_adapter.cpp`
- `src/hexagon/services/project_analyzer.cpp`
- `src/hexagon/services/impact_analyzer.cpp`
- `tests/adapters/test_cmake_file_api_adapter.cpp`
- `tests/hexagon/test_project_analyzer.cpp`
- `tests/hexagon/test_impact_analyzer.cpp`

Neue Dateien:

- `src/hexagon/model/target_graph.h`
- `src/hexagon/services/target_graph_support.h`
- `src/hexagon/services/target_graph_support.cpp`
- `tests/hexagon/test_target_graph_support.cpp`

## Modellvertrag

### `TargetDependency`

Neues Value-Object in `src/hexagon/model/target_graph.h`. Die Richtung folgt
exakt der M6-Vertragsregel: `from` ist das konsumierende Target, `to` ist das
Target, von dem es direkt abhaengt.

```cpp
namespace xray::hexagon::model {

enum class TargetDependencyKind {
    direct,
};

enum class TargetDependencyResolution {
    resolved,
    external,
};

struct TargetDependency {
    std::string from_unique_key;
    std::string from_display_name;
    std::string to_unique_key;
    std::string to_display_name;
    TargetDependencyKind kind{TargetDependencyKind::direct};
    TargetDependencyResolution resolution{TargetDependencyResolution::resolved};
};

}  // namespace xray::hexagon::model
```

Regeln:

- `from_unique_key` verwendet dieselbe `TargetInfo::unique_key`-Bildungsregel
  wie `target_assignments` (`name + "::" + type`); Targets in unterschiedlichen
  Build-Kontexten duerfen nicht versehentlich zusammenfallen.
- `to_unique_key` ist immer ein nicht leerer String und folgt einer von zwei
  Bildungsregeln, abhaengig von `resolution`:
  - bei `resolved` ist es die `TargetInfo::unique_key` des Ziel-Targets im
    selben File-API-Reply;
  - bei `external` ist es `"<external>::" + raw_id`, wobei `raw_id` die
    woertlich aus dem `dependencies[*].id`-Feld der File-API-Target-Reply
    uebernommene Zeichenkette ist. Damit kollabieren unterschiedliche
    externe Ziele nicht in einer Kante, und die Hub-Zaehlung bleibt
    eindeutig.
- Echte File-API-Target-`unique_key`-Werte folgen dem Schema `name::type`
  und enthalten nur die fuer CMake-Target-Namen zulaessigen Zeichen
  (alphanumerisch, `_`, `-`, `+`, `.`, `::`-Namespace-Trenner) gefolgt
  vom CMake-Target-Typ. Die Zeichen `<` und `>` sind in CMake-Target-Namen
  nicht zulaessig, damit kann der Praefix `<external>::` mit
  keiner moeglichen `TargetInfo::unique_key` kollidieren.
- AP 1.1 reserviert den Praefix `<external>::` ausschliesslich fuer
  `to_unique_key`-Werte mit `resolution=external` und vergibt ihn nicht an
  `from_unique_key` oder `TargetInfo::unique_key`. Adapter und Services
  duerfen aus einem `to_unique_key` mit diesem Praefix nicht zurueck auf
  ein File-API-Target schliessen; der Praefix ist eine reine Schluessel-
  Disambiguierung, kein Lookup-Pfad.
- `from_display_name` und `to_display_name` verwenden den menschenlesbaren
  Namen aus dem File-API-Reply (`raw_id` bei `external`). Sie sind reine
  Anzeigedaten; Sortierung und Vergleichsidentitaet basieren ausschliesslich
  auf den `unique_key`-Werten und `kind`.
- `TargetDependencyKind::direct` ist in AP 1.1 der einzige zulaessige Wert.
  Spaetere transitive oder generierte Kanten brauchen einen eigenen
  Vertragsschritt.
- `TargetDependencyResolution::resolved` bedeutet, dass das Ziel-Target
  innerhalb desselben File-API-Reply als bekanntes Target existiert.
  `external` bedeutet, dass die `dependencies`-Liste einen vorhandenen
  `id`-Wert enthaelt, dessen Target-Reply in derselben Reply nicht aufloesbar
  ist (zum Beispiel ein durch CMake exportiertes IMPORTED-Target ohne
  sichtbares JSON in der aktuellen Reply).
- Eintraege in `dependencies` ohne `id`-Feld oder mit leerer `id` sind in der
  CMake File API v1 ein Reply-Defekt. AP 1.1 verwirft solche Eintraege und
  haengt eine Diagnostic an, statt eine erratene Kante zu erzeugen
  (siehe Adaptervertrag).
- Mehrfachkanten zwischen demselben `(from_unique_key, to_unique_key, kind)`-
  Tripel werden dedupliziert; doppelte Eintraege im File-API-Reply fuehren
  nicht zu Mehrfachkanten im Modell.

### `TargetGraphStatus`

```cpp
namespace xray::hexagon::model {

enum class TargetGraphStatus {
    not_loaded,
    loaded,
    partial,
};

}  // namespace xray::hexagon::model
```

Regeln:

- `not_loaded` ist der Default fuer alle Compile-Database-only-Pfade und fuer
  File-API-Pfade, die keine Target-Reply enthalten.
- `loaded` bedeutet: alle Targets aus dem Reply wurden ohne
  graphspezifische Diagnostics ausgewertet, alle `dependencies`-Eintraege
  liegen als `TargetDependencyResolution::resolved` vor.
- `partial` bedeutet: die Target-Graph-Sicht ist ladbar, aber mindestens
  eines der folgenden graphspezifischen Probleme tritt auf:
  - mindestens eine Kante hat `TargetDependencyResolution::external`;
  - mindestens ein `dependencies`-Eintrag ohne `id` oder mit leerer `id`
    wurde verworfen;
  - mindestens eine Selbstkante wurde verworfen;
  - mindestens eine `id`-Kollision in Phase 1 wurde erkannt;
  - das `dependencies`-Subobjekt eines Target-Replies konnte nicht oder nur
    teilweise geparst werden.
- `target_metadata` aus M4 (Source-Coverage von Targets) und
  `target_graph_status` (Vollstaendigkeit der Target-zu-Target-Sicht) leben
  unabhaengig voneinander. Eine `target_metadata=partial`-Bedingung ist
  ausdruecklich kein Trigger fuer `target_graph_status=partial`, und
  umgekehrt; Targets ohne kompilierbare Sourcen koennen einen vollstaendig
  aufgeloesten Target-Graphen liefern.
- AP 1.1 fuehrt keinen separaten Fehlerwert ein; harte File-API-Ladefehler
  bleiben ueber `BuildModelResult::compile_database.error` sichtbar und der
  Status bleibt `not_loaded`.

### `TargetGraph`

```cpp
namespace xray::hexagon::model {

struct TargetGraph {
    std::vector<TargetInfo> nodes;
    std::vector<TargetDependency> edges;
};

}  // namespace xray::hexagon::model
```

Regeln:

- `nodes` enthaelt jedes im Reply sichtbare Target genau einmal. Die
  Sortierung folgt dem M6-Sortiervertrag: `(unique_key, display_name, type)`.
- `edges` enthaelt jede `(from_unique_key, to_unique_key, kind)`-Kombination
  hoechstens einmal. Die Sortierung folgt
  `(from_unique_key, to_unique_key, kind, from_display_name, to_display_name)`.
- AP 1.1 berechnet keine Adjazenzlisten oder Indizes; spaetere APs bauen
  diese bei Bedarf in eigenen Helpern auf der gespeicherten Kantenliste auf.
- Externe Kanten (`TargetDependencyResolution::external`) bleiben Teil von
  `edges`. Adapter und Reports duerfen sie nicht verwerfen, weil sie
  fachliche Provenienz transportieren. Verworfene Defekte (Eintraege ohne
  `id`, Selbstkanten) erscheinen ausschliesslich in
  `BuildModelResult::diagnostics` und nicht in `edges`.

### Erweiterung von `BuildModelResult`

```cpp
struct BuildModelResult {
    // ... bestehende Felder ...
    TargetGraph target_graph;
    TargetGraphStatus target_graph_status{TargetGraphStatus::not_loaded};
};
```

Regeln:

- Compile-Database-only-Pfade lassen `target_graph` leer und
  `target_graph_status=not_loaded`.
- File-API-Pfade ohne `dependencies`-Array setzen `target_graph_status` auf
  `loaded` (Knoten sind bekannt, es gibt nur keine Kanten) oder `partial`
  (wenn andere Diagnostics die Reply als unvollstaendig markieren). AP 1.1
  pinnt diese Unterscheidung in einem Adaptertest.
- Diagnostics ueber Target-Graph-Probleme werden in der bestehenden
  `BuildModelResult::diagnostics`-Liste fortgefuehrt; AP 1.1 fuehrt keinen
  separaten Diagnostic-Slot fuer Target-Graph-Probleme ein.

### Erweiterung von `AnalysisResult`

```cpp
struct AnalysisResult {
    // ... bestehende Felder ...
    TargetGraph target_graph;
    TargetGraphStatus target_graph_status{TargetGraphStatus::not_loaded};
    std::vector<TargetInfo> target_hubs_in;
    std::vector<TargetInfo> target_hubs_out;
};
```

Regeln:

- `target_graph` und `target_graph_status` werden vom `ProjectAnalyzer`
  direkt aus `BuildModelResult` uebernommen, sobald Target-Daten geladen
  wurden.
- `target_hubs_in` und `target_hubs_out` sind die Hub-Auswahllisten nach den
  Default-Schwellenwerten dieses APs (`10`/`10`). Sie sind ein Untermenge der
  Knoten in `target_graph` und nach `(unique_key, display_name, type)`
  sortiert.
- AP 1.1 schreibt diese Felder, gibt sie aber nicht in Reports aus. Die
  Sichtbarkeit folgt in AP 1.2; CLI-gesteuerte Schwellen folgen in AP 1.5.

### Erweiterung von `ImpactResult`

```cpp
struct ImpactResult {
    // ... bestehende Felder ...
    TargetGraph target_graph;
    TargetGraphStatus target_graph_status{TargetGraphStatus::not_loaded};
};
```

Regeln:

- `ImpactResult` erhaelt in AP 1.1 keine neue Hub- oder Priorisierungssicht.
  AP 1.3 fuehrt `priority_class`, `graph_distance`,
  `impact_target_depth_requested` und `impact_target_depth_effective` ein.
- AP 1.1 stellt sicher, dass `target_graph` und `target_graph_status` fuer
  `impact` denselben Inhalt wie der zugrunde liegende `BuildModelResult`
  haben, damit AP 1.3 keinen zweiten File-API-Lauf braucht.

## Adaptervertrag (`CmakeFileApiAdapter`)

Der Adapter liest in M4 bereits Target-Reply-Dateien fuer
`target_assignments`. AP 1.1 erweitert diese bestehende Schleife um die
`dependencies`- und `nameOnDisk`-/`name`-Felder, ohne den bestehenden
Sourcen- und Compile-Group-Pfad zu veraendern.

Regeln:

- Fuer jedes Target im Codemodel-Reply registriert der Adapter genau einen
  Knoten in `TargetGraph::nodes` mit dem `TargetInfo`, das auch in
  `target_assignments` verwendet wird.
- Der Adapter liest die `dependencies`-Liste der Target-Reply (CMake File API
  v1: Array von Objekten mit `id` und optional `backtrace`). Pro Eintrag wird
  versucht, das Ziel-Target in der vollstaendig vorab aufgebauten Target-
  Menge des Replies anhand der `id` aufzuloesen.
- Die `id` in der File-API-Reply ist nicht der menschenlesbare Name, sondern
  ein adapterinterner Schluessel (z. B. `cmake-xray::STATIC_LIBRARY-Debug`).
- Die Auswertung verlaeuft strikt zweistufig, damit Forward-Referenzen
  innerhalb des Replies nicht faelschlich als `external` klassifiziert
  werden:
  - **Phase 1 (Index-Aufbau)**: Der Adapter iteriert ueber alle
    Target-Reply-Dateien des Codemodels, liest `name`, `type` und `id`,
    befuellt `TargetGraph::nodes` mit den entsprechenden `TargetInfo`-
    Werten und baut dabei eine `std::unordered_map<std::string, TargetInfo>`
    von File-API-`id` auf das zugehoerige `TargetInfo`. In dieser Phase
    werden `dependencies`-Felder noch nicht ausgewertet. Sourcen,
    Compile-Groups und `target_assignments` aus M4 werden in dieser Phase
    zusammen mit dem Index aufgebaut, damit der bestehende M4-Pfad nicht
    in einen dritten Lauf gespalten werden muss.
  - **`id`-Kollision in Phase 1**: Trifft der Adapter auf eine `id`, die
    bereits in der Map liegt, gewinnt der erste Eintrag in Codemodel-
    Iteration-Reihenfolge. Folgende Targets mit derselben `id` werden
    weiterhin als Knoten in `TargetGraph::nodes` aufgenommen, aber nicht
    in die `id`-Map eingetragen. Pro kollidierender `id` wird genau eine
    Diagnostic der Schwere `warning` an `BuildModelResult::diagnostics`
    angehaengt:
    `"file api reply contains duplicate target id '<raw_id>'; first occurrence '<winner_unique_key>' wins, conflicting targets: <list_of_displaced_unique_keys>"`.
    Die Liste der verdraengten `unique_key`-Werte wird lexikografisch
    sortiert, damit der Diagnostic-Text reproduzierbar bleibt.
    `target_graph_status` wird auf `partial` gesetzt. Kanten, die auf eine
    kollidierende `id` verweisen, werden gegen den first-wins-Eintrag
    aufgeloest und gelten als `resolved`; verdraengte Targets sind ueber
    diese Kanten nicht erreichbar.
  - **Phase 2 (Dependencies-Aufloesung)**: Der Adapter iteriert ein zweites
    Mal ueber dieselben Target-Reply-Dateien (oder ueber die in Phase 1
    zwischengespeicherten Roh-`dependencies`-JSON-Knoten, je nach
    Implementierung) und loest jede Kante gegen die jetzt vollstaendige
    `id -> TargetInfo`-Map auf. Erst in dieser Phase werden Kanten an
    `TargetGraph::edges` und Diagnostics fuer `external`, defekte
    `id`-Eintraege und Selbstkanten an `BuildModelResult::diagnostics`
    angehaengt.
- Diese Trennung ist Teil des Adaptervertrags, nicht nur eine Implementier-
  ungsempfehlung. Der Plan erlaubt Implementierungen, Phase 2 ueber
  zwischengespeicherte JSON-Knoten oder ueber einen zweiten Disk-Read
  abzuwickeln, solange das beobachtbare Verhalten "Forward-Referenzen werden
  als `resolved` klassifiziert" erfuellt ist.
- Wird eine `id` nicht gefunden, wird die Kante als
  `TargetDependencyResolution::external` aufgenommen,
  `to_unique_key` als `"<external>::" + raw_id` und `to_display_name` als
  `raw_id` gesetzt; eine Diagnostic der Schwere `note` wird an
  `BuildModelResult::diagnostics` angehaengt:
  `"target '<from_display_name>' references unknown target id '<raw_id>'"`.
- Hat ein Eintrag keine `id` oder eine leere `id`, wird die Kante komplett
  verworfen und eine Diagnostic der Schwere `warning` an
  `BuildModelResult::diagnostics` angehaengt:
  `"target '<from_display_name>' has a dependency entry without a target id; entry skipped"`.
  AP 1.1 erfindet keinen synthetischen Schluessel fuer solche Eintraege,
  damit Hub-Zaehlung, Impact-Traversierung und Compare-Identitaeten in
  spaeteren APs nicht ueber instabile Reply-Indizes verzerrt werden.
- Mehrfache Eintraege fuer dasselbe
  `(from_unique_key, to_unique_key, kind)`-Tripel werden in Phase 2 vor dem
  Anhaengen dedupliziert. Da `sort_target_graph` Dedup ohnehin garantiert
  (siehe `target_graph_support`-Vertrag), darf der Adapter die Phase-2-
  Dedup als blosse Effizienzmassnahme implementieren; korrekt bleibt der
  finale Output durch den `sort_target_graph`-Aufruf unten.
- Selbstkanten (`from_unique_key == to_unique_key`) werden uebersprungen und
  als Diagnostic der Schwere `note` vermerkt; sie sind in der CMake File API
  nicht erwartbar, der Adapter haelt sie aber explizit aus dem Modell heraus,
  damit Hub-Klassifikation und spaetere Impact-Traversierung saubere Daten
  bekommen.
- Der Adapter setzt `BuildModelResult::target_graph_status` ausschliesslich
  anhand graphspezifischer Beobachtungen, unabhaengig von `target_metadata`:
  - `loaded`, wenn alle Kanten `resolved` sind und weder Selbstkanten noch
    fehlende `id`-Eintraege noch `id`-Kollisionen noch `dependencies`-
    Parsefehler aufgetreten sind.
  - `partial`, wenn mindestens eine Kante `external` ist, mindestens eine
    Selbstkante verworfen wurde, mindestens ein Eintrag ohne `id` verworfen
    wurde, mindestens eine `id`-Kollision auftrat oder das `dependencies`-
    Subobjekt eines Target-Replies nicht vollstaendig geparst werden konnte.
  - `not_loaded` bleibt erhalten, wenn der Codemodel-Reply ueberhaupt keine
    Target-Reply-Dateien enthaelt; das ist der File-API-Fehlerpfad und wird
    durch bestehende M4-Pfade abgedeckt.
- `target_metadata=partial` (Targets ohne kompilierbare Sourcen aus M4)
  wird vom Adapter nicht in `target_graph_status` propagiert. Beide Status
  laufen unabhaengig.
- Vor der Rueckgabe ruft der Adapter `sort_target_graph()` auf den befuellten
  `TargetGraph`, damit `BuildModelResult::target_graph` deterministisch
  sortiert ist und Adapter-Tests nicht von der Reply-Reihenfolge abhaengen.
  Diagnostics werden in der Reihenfolge angehaengt, in der die
  Reply-Eintraege gelesen wurden; Adapter-Tests akzeptieren diese
  Reihenfolge oder pruefen die Diagnostic-Menge ueber stabile Sortier-
  Helper, um nicht ueber Reply-Iteration zu fluktuieren.

Wichtig:

- Adapter-Tests pruefen den rohen Reply-Inhalt deterministisch; `display_name`-
  Reihenfolge im Reply darf das Ergebnis nicht beeinflussen, weil Sortierung
  ueber den `unique_key` lebt.
- Die bestehende Source-Sortierung und das `target_metadata`-Verhalten aus
  M4 bleiben unveraendert. AP 1.1 erweitert nur den Result-Aufbau in
  `build_success_result()`.

## Servicevertrag (`target_graph_support`)

Hub-Klassifikation und Knoten-/Kantensortierung leben als freie Funktionen im
Hexagon, nicht in den Reportadaptern. Die neue Datei
`src/hexagon/services/target_graph_support.{h,cpp}` enthaelt:

```cpp
namespace xray::hexagon::services {

struct TargetHubThresholds {
    std::size_t in_threshold;
    std::size_t out_threshold;
};

inline constexpr std::size_t kDefaultTargetHubInThreshold = 10;
inline constexpr std::size_t kDefaultTargetHubOutThreshold = 10;

void sort_target_graph(model::TargetGraph& graph);

std::pair<std::vector<model::TargetInfo>, std::vector<model::TargetInfo>>
compute_target_hubs(const model::TargetGraph& graph,
                    TargetHubThresholds thresholds);

}  // namespace xray::hexagon::services
```

Regeln fuer `sort_target_graph`:

- Setzt die in `docs/plan-M6.md` Abschnitt 1.2 fixierte Sortierung fuer
  Knoten und Kanten um. Adapter und Services rufen diese Funktion ueber alle
  Pfade hinweg, statt eigene Sortierregeln zu schreiben.
- Dedupliziert zusaetzlich auf Identitaetsschluessel: Knoten kollabieren
  ueber `unique_key`, Kanten ueber `(from_unique_key, to_unique_key, kind)`.
  Der erste in Sortier-Reihenfolge auftretende Eintrag gewinnt; Anzeige- und
  Provenienzdaten gleicher Identitaet werden nicht gemerged. Mehrfache
  `external`-Kanten mit unterschiedlichem `raw_id` haben unterschiedliche
  `to_unique_key`-Werte und kollabieren deshalb nicht.
- Ist idempotent: Ein erneuter Aufruf auf einem bereits sortierten und
  dedupizierten `TargetGraph` aendert weder Reihenfolge noch Inhalt. Damit
  ist der redundante Adapter+Analyzer-Aufruf billig und beobachtbar
  nebenwirkungsfrei.
- Ist die Single-Source-of-Truth fuer Identitaets-Normalisierung. Aufrufer
  duerfen sich darauf verlassen, dass nach `sort_target_graph` keine
  doppelten Knoten oder Kanten mehr existieren; eigene Dedup-Logik in
  Aufrufern ist verboten, weil sie Identitaets-Regeln duplizieren wuerde.
Regeln fuer `compute_target_hubs`:

- Zaehlt eingehende und ausgehende Kanten pro `unique_key` und liefert zwei
  Listen `(in_hubs, out_hubs)`. Ein Knoten erscheint in `in_hubs`, wenn
  `in_count >= thresholds.in_threshold`, und in `out_hubs`, wenn
  `out_count >= thresholds.out_threshold`. Externe Kanten zaehlen mit, weil
  sie fachliche Konsumbeziehungen transportieren.
- Hub-Listen enthalten nur Knoten aus `TargetGraph::nodes`, also
  `TargetInfo`-Werte echter File-API-Targets. Externe Kanten erhoehen den
  In-Grad ihres synthetischen `<external>::*`-Ziels, aber dieses Ziel ist
  kein Knoten in `nodes` und kann daher nicht selbst zum In-Hub werden.
  Damit bleibt die Hub-Sicht auf Projekt-eigene Targets fokussiert.
- **Defensives Verhalten in Release und Debug**: `compute_target_hubs`
  liefert auch dann korrekte Hub-Listen, wenn der uebergebene
  `TargetGraph` nicht durch `sort_target_graph` normalisiert wurde. Dazu
  baut der Helper intern zwei Identitaets-Sets auf, ohne den Input zu
  modifizieren oder zu kopieren:
  - ein `std::unordered_set<std::string>` ueber `unique_key`-Werte aller
    Knoten in `nodes`, damit nur projekt-eigene Targets als Hub-Kandidaten
    auftauchen und doppelte Knoten den Hub-Kandidatenpool nicht aufblaehen;
  - ein `std::unordered_set<EdgeIdentity>` ueber
    `(from_unique_key, to_unique_key, kind)`, damit doppelte Kanten den
    Grad nicht doppelt erhoehen.
  Damit ist der Helper auch dann robust, wenn ein zukuenftiger
  `BuildModelPort`, ein synthetischer Test-Builder oder ein partiell
  korrupter Adapter-Output unsortierte oder doppelte Eintraege liefert. Die
  Defensiv-Logik ist O(N+E) und damit fuer realistische Projektgroessen
  vernachlaessigbar; es lohnt nicht, sie hinter einen Build-Flag zu legen.
- **Verhaeltnis zu `sort_target_graph`**: Defensiv heisst nicht "ersetzt
  Sortierung". `sort_target_graph` bleibt die Single-Source-of-Truth fuer
  die Modell-Modifikation, weil `AnalysisResult::target_graph` und
  `ImpactResult::target_graph` als sortiert in Reports landen.
  `compute_target_hubs` macht ausschliesslich Lese-Operationen und liefert
  deterministische Hub-Listen, unabhaengig vom Input-Zustand.
- **Determinismus der Hub-Liste**: Die zurueckgegebenen Listen sind nach
  `(unique_key, display_name, type)` sortiert. Wenn ein `unique_key` in
  `nodes` mehrfach mit unterschiedlichen `display_name`/`type`-Werten
  vorkommt (Defekt-Fall), gewinnt der lexikografisch erste
  `(unique_key, display_name, type)`-Eintrag fuer die Hub-Liste; alle
  anderen werden zusammen mit ihm als ein einziger Hub-Kandidat gezaehlt.
- Die Defaults `kDefaultTargetHubInThreshold` und
  `kDefaultTargetHubOutThreshold` betragen jeweils `10` und entsprechen dem
  M6-Hauptplan AP 1.5.
- AP 1.1 verdrahtet diese Defaults direkt in `ProjectAnalyzer`. Die
  CLI-Steuerung folgt in AP 1.5; dafuer wird der Default in einem spaeteren
  Schritt durch einen Wert aus dem Request ersetzt, ohne dass die Helper-
  Signatur sich aendert.

### Service-Anbindung in `ProjectAnalyzer`

`ProjectAnalyzer::analyze_project()` wird so erweitert, dass nach dem
erfolgreichen `BuildModelResult`-Laden die Target-Graph-Sicht in das
`AnalysisResult` uebernommen wird:

- `result.target_graph = build_model.target_graph;`
- `result.target_graph_status = build_model.target_graph_status;`
- `sort_target_graph(result.target_graph);`
- `auto [in_hubs, out_hubs] = compute_target_hubs(result.target_graph,`
  `{kDefaultTargetHubInThreshold, kDefaultTargetHubOutThreshold});`
- `result.target_hubs_in = std::move(in_hubs);`
- `result.target_hubs_out = std::move(out_hubs);`

Der `sort_target_graph()`-Aufruf im Analyzer ist Defense-in-Depth: Der
M6-Adaptervertrag verlangt bereits sortierten Output, aber der
`BuildModelPort`-Vertrag schreibt das nicht zwingend vor, und zukuenftige
Adapter (zum Beispiel synthetische Test-Doubles oder eigene Build-System-
Adapter) duerfen den Analyzer nicht durch unsortierte `TargetGraph`-Werte
nicht-deterministisch machen. `compute_target_hubs` setzt auf einen
sortierten Graphen auf, damit die Hub-Listen ohne weitere Sortierung
deterministisch sind.

Wichtig:

- Im Mixed-Input-Pfad bleibt die Compile-Database autoritativ fuer Ranking,
  Hotspots und TU-basierte Impact-Ableitung. Die Target-Graph-Sicht ergaenzt
  ausschliesslich `target_graph`, `target_graph_status`, `target_hubs_in` und
  `target_hubs_out`. Die bestehende `apply_target_enrichment`-Logik wird
  nicht veraendert.
- Im Compile-Database-only-Pfad bleibt `target_graph` leer und
  `target_graph_status=not_loaded`. AP 1.1 fuehrt fuer diesen Fall keine
  zusaetzliche Diagnostic ein, weil Compile-Database-only-Laeufe in M4
  bereits `target_metadata=not_loaded` melden und Nutzer dadurch wissen,
  dass keine Target-Daten verfuegbar sind.

### Service-Anbindung in `ImpactAnalyzer`

`ImpactAnalyzer::analyze_impact()` propagiert dieselbe Sicht in
`ImpactResult` mit derselben Defense-in-Depth-Begruendung wie im
`ProjectAnalyzer`:

- `result.target_graph = build_model.target_graph;`
- `result.target_graph_status = build_model.target_graph_status;`
- `sort_target_graph(result.target_graph);`

In AP 1.1 wird die Sicht nicht weiter verarbeitet. AP 1.3 nutzt
`result.target_graph` als Ausgangspunkt fuer die reverse-BFS und setzt
`priority_class`, `graph_distance` sowie `impact_target_depth_*` neu.

## Sortier- und Identitaetsregeln

- Knoten in `TargetGraph::nodes` werden in `(unique_key, display_name, type)`-
  Reihenfolge sortiert.
- Kanten in `TargetGraph::edges` werden in
  `(from_unique_key, to_unique_key, kind, from_display_name, to_display_name)`-
  Reihenfolge sortiert. `kind` rangiert ueber eine explizite Rangtabelle in
  `target_graph_support.cpp`, nicht ueber Enum-Ordinalwerte oder Stringwerte.
- Hub-Listen `target_hubs_in` und `target_hubs_out` sind nach
  `(unique_key, display_name, type)` sortiert; ein Knoten kann gleichzeitig
  in beiden Listen erscheinen.
- Ein-/Ausgangsgrad zaehlt jede in `edges` enthaltene Kante einmal,
  unabhaengig von ihrer `resolution`. Nur dedupizierte Kanten zaehlen;
  verworfene Defekte (siehe Adaptervertrag) zaehlen nicht.
- Vergleichsidentitaet eines Knotens ist `unique_key`. Vergleichsidentitaet
  einer Kante ist `(from_unique_key, to_unique_key, kind)`. Andere Felder
  sind reine Anzeige- bzw. Provenienzdaten.

## Diagnostics

AP 1.1 verwendet die bestehende `BuildModelResult::diagnostics`-Liste mit
`Diagnostic{severity, message}`. Es gibt keine neuen Diagnostic-Typen.

Pflichten:

- Pro `external`-Kante genau eine Diagnostic mit Schwere `note` und Text
  `"target '<from_display_name>' references unknown target id '<raw_id>'"`,
  damit dieselbe externe Referenz nicht mehrfach gemeldet wird, wenn sie in
  `dependencies` doppelt auftaucht (Dedup-Vertrag fuer Diagnostics: pro
  `(from_unique_key, raw_id)` hoechstens eine Diagnostic).
- Pro verworfenem `dependencies`-Eintrag ohne `id` oder mit leerer `id`
  genau eine Diagnostic der Schwere `warning` mit Text
  `"target '<from_display_name>' has a dependency entry without a target id; entry skipped"`.
  Mehrere defekte Eintraege im selben Target erzeugen mehrere Diagnostics,
  weil sie unterschiedlichen Reply-Inhalt repraesentieren.
- Selbstkanten produzieren genau eine Diagnostic der Schwere `note` pro
  betroffenem Target und `(from_unique_key, to_unique_key)`-Paar.
- Pro kollidierender File-API-`id` genau eine Diagnostic der Schwere
  `warning` mit dem im Adaptervertrag definierten Text. Mehrfache
  Verdraenger derselben `id` werden in der Liste der verdraengten
  `unique_key`-Werte zusammengefasst, nicht in mehrere Diagnostics
  gespalten.
- Wenn ein File-API-Reply eine `target`-Datei enthaelt, deren `dependencies`
  weder gelesen noch geparst werden konnte (zum Beispiel wegen JSON-Fehler
  innerhalb dieses Subobjekts), wird der Target-Reply weiter verarbeitet,
  aber `target_graph_status=partial` gesetzt und eine Diagnostic der Schwere
  `warning` angehaengt.
- AP 1.1 fuehrt keine Diagnostics fuer Compile-Database-only-Laeufe ein, weil
  diese Pfade keine Target-Graph-Erwartung haben.

## Rueckwaertskompatibilitaet

Pflicht:

- Bestehende M4/M5-Compile-Database-only-Goldens fuer Console und Markdown
  bleiben byte-stabil.
- Bestehende M4/M5-File-API- und Mixed-Input-Goldens bleiben byte-stabil,
  weil AP 1.1 die Target-Graph-Sicht zwar produziert, aber noch nicht in
  Reports rendert.
- Bestehende JSON-, HTML- und DOT-Goldens fuer `analyze` und `impact` bleiben
  byte-stabil, weil weder neue Felder serialisiert noch bestehende
  Feldwerte veraendert werden.
- `format_version` aus M5 AP 1.1 bleibt unveraendert auf `1`. AP 1.1 fuehrt
  keine M6-Formatversion ein.

Verboten:

- Reportadapter veraendern, damit sie `target_graph` lesen oder schreiben.
  Diese Migration ist AP 1.2.
- Hub-Schwellen oder Default-Werte aus dem Service-Helper in einen
  Reportadapter kopieren.
- `target_assignments` aus M4 mit dem neuen `TargetGraph` zusammenzulegen.
  Beide Sichten leben parallel: `target_assignments` bindet Translation
  Units an Targets, `target_graph` bindet Targets aneinander.

## Implementierungsreihenfolge

1. `src/hexagon/model/target_graph.h` mit `TargetDependency`,
   `TargetDependencyKind`, `TargetDependencyResolution` (Werte `resolved`
   und `external`), `TargetGraphStatus` und `TargetGraph` einfuehren.
2. `BuildModelResult`, `AnalysisResult` und `ImpactResult` um die neuen
   Felder erweitern. Compile- und Linker-Sauberkeit pruefen.
3. `target_graph_support.{h,cpp}` mit `sort_target_graph`,
   `compute_target_hubs` und den `kDefault*Threshold`-Konstanten anlegen.
4. Hexagon-Service-Tests fuer `target_graph_support` schreiben (leerer Graph,
   sortierte Knoten/Kanten, Hubs unterhalb/auf/ueber Schwelle, externe
   Kanten ohne Hub-Eintrag fuer `<external>::*`-Ziele, dedupizierte
   Mehrfachkanten).
5. `CmakeFileApiAdapter` so erweitern, dass die `id -> TargetInfo`-Map in
   einer Phase 1 vollstaendig ueber alle Target-Reply-Dateien aufgebaut
   wird (mit first-wins-Diagnostic-Vertrag bei `id`-Kollisionen), bevor in
   Phase 2 `dependencies` ausgewertet werden und der `TargetGraph`
   befuellt wird. `sort_target_graph` wird vor Rueckgabe aufgerufen.
6. Adapter-Tests fuer `loaded`, `partial`, externe Kanten, defekte
   Eintraege ohne `id` (verworfen), Selbstkanten und doppelte Eintraege
   schreiben.
7. `ProjectAnalyzer` und `ImpactAnalyzer` so erweitern, dass die
   Target-Graph-Sicht aus `BuildModelResult` uebernommen, sortiert und im
   `ProjectAnalyzer` zusaetzlich in Hub-Listen klassifiziert wird.
8. Service-Tests fuer `ProjectAnalyzer` und `ImpactAnalyzer` aktualisieren:
   Compile-Database-only-Pfade, File-API-only-Pfade, Mixed-Input-Pfade,
   Fehlerergebnisse.
9. Goldens und CLI-E2E-Tests laufen lassen, dass keine bestehenden Goldens
   driften (Pflicht-Voraussetzung fuer den Liefer-Stand).

## Tests

Service-Tests `tests/hexagon/test_target_graph_support.cpp`:

- leerer Graph: `sort_target_graph` ist no-op, `compute_target_hubs` liefert
  zwei leere Listen.
- ein Knoten ohne Kanten: `compute_target_hubs` liefert leere Listen mit
  Default-Schwellen.
- zehn eingehende Kanten gegen einen Knoten bei `in_threshold=10` machen ihn
  zum In-Hub; bei `in_threshold=11` nicht.
- zehn ausgehende Kanten gegen einen Knoten bei `out_threshold=10` machen ihn
  zum Out-Hub.
- Knoten mit `in_count >= 10` und `out_count >= 10` erscheint in beiden
  Listen.
- Sortierung: Eingabe in zufaelliger Reihenfolge ergibt gleiches Ergebnis
  wie Eingabe in lexikografisch aufsteigender Reihenfolge.
- Externe Kanten zaehlen fuer Hub-Schwellen mit; das `<external>::*`-Ziel
  taucht nicht selbst als Hub auf, weil es kein Knoten in
  `TargetGraph::nodes` ist.
- Mehrfacheintraege werden nicht doppelt gezaehlt (Voraussetzung: Adapter
  hat dedupliziert; Service-Test pinnt, dass `compute_target_hubs` keine
  zusaetzliche Deduplikation macht und daher jeden Listeneintrag genau
  einmal zaehlt).
- Sortier-Tie-Breaker `(unique_key, display_name, type)` durch zwei Targets
  mit gleichem `unique_key`-Praefix abdecken.
- Sortier-Tie-Breaker fuer Kanten durch zwei Kanten mit gleichem
  `(from_unique_key, to_unique_key)`, aber unterschiedlichen `display_name`-
  Werten abdecken.
- Idempotenz: `sort_target_graph` zweimal aufrufen liefert dasselbe
  Ergebnis wie ein einfacher Aufruf.
- Praefix-Disambiguierung: ein Target mit `display_name="external"` und
  `type="STATIC_LIBRARY"` (also `unique_key="external::STATIC_LIBRARY"`)
  taucht im Knotenset auf und kollidiert nicht mit einer externen Kante,
  deren `to_unique_key="<external>::STATIC_LIBRARY"` ist; beide existieren
  parallel, Hub-Zaehlung bleibt korrekt.
- Dedup-Vertrag von `sort_target_graph`: ein Eingangsgraph mit drei
  Knoten `(unique_key=X, X, X)` und drei Kanten `(A,B,direct)` liefert
  einen Ausgang mit genau einem Knoten und einer Kante. Der erste in
  Sortier-Reihenfolge auftretende Eintrag gewinnt fuer Anzeigedaten.
- Dedup-Vertrag fuer `external`-Kanten: zwei Eingangskanten
  `to_unique_key="<external>::libfoo"` und `to_unique_key="<external>::libbar"`
  bleiben getrennt, weil sich der Identitaetsschluessel unterscheidet.
- Robustheit von `compute_target_hubs`: ein Test uebergibt einen
  vorsaetzlich unsortierten und mit doppelten Kanten beladenen
  `TargetGraph` und erwartet dieselben Hub-Listen wie der Aufruf auf einem
  zuvor mit `sort_target_graph` normalisierten Vergleichsgraphen mit
  identischer logischer Identitaetsmenge. Damit ist der Defensiv-Vertrag
  ueber `unordered_set`-basierte Identitaets-Dedup im Helper testbar
  gepinnt, ohne dass die Test-Suite an Build-Konfiguration (Debug vs.
  Release) gebunden ist.
- Doppelte Knoten: ein `TargetGraph`, dessen `nodes`-Vector denselben
  `unique_key` zweimal enthaelt, fuehrt zu hoechstens einem Hub-Eintrag
  pro `unique_key` in der Ausgabeliste.

Adapter-Tests `tests/adapters/test_cmake_file_api_adapter.cpp`:

- Reply ohne `dependencies`-Felder: `target_graph_status=loaded`, leere
  `edges`, alle Targets als Knoten vorhanden.
- Reply mit voll aufgeloesten `dependencies`: `target_graph_status=loaded`,
  Kanten sortiert, keine Diagnostic.
- Forward-Referenz im Reply: Target `A` listet in `dependencies` die `id`
  von Target `B`, das im Codemodel-`targets`-Array nach `A` steht. Erwartet:
  Kante `(A, B)` mit `resolution=resolved`, `target_graph_status=loaded`,
  keine `external`-Diagnostic. Der Test pinnt explizit den zweistufigen
  Adapter-Ablauf, weil ein einstufiger Pass diese Kante als `external`
  klassifizieren wuerde.
- `id`-Kollision in Phase 1: zwei Targets `A` und `B` im Codemodel haben
  dieselbe `id="dup-id"`, `A` steht im Codemodel vor `B`. Erwartet: beide
  Targets erscheinen in `nodes`, die Diagnostic-Liste enthaelt genau eine
  Diagnostic der Schwere `warning` mit dem definierten Text und
  `winner_unique_key=A::<type>`, `target_graph_status=partial`. Eine
  weitere Kante eines dritten Targets `C`, die auf `id="dup-id"` verweist,
  loest gegen `A` auf (`resolution=resolved`, `to_unique_key=A::<type>`).
- Drei-Wege-`id`-Kollision: drei Targets `A`, `B`, `C` mit derselben
  `id`. Genau eine Diagnostic, deren `list_of_displaced_unique_keys` `B`
  und `C` lexikografisch sortiert enthaelt, unabhaengig von der
  Codemodel-Reihenfolge der drei Targets.
- Reply mit `dependencies`-Eintrag, dessen `id` zu keinem Target passt:
  Kante als `external`, `to_unique_key="<external>::<raw_id>"`, Diagnostic
  der Schwere `note`, `target_graph_status=partial`.
- Reply mit zwei `external`-Kanten desselben Targets, deren `raw_id`-Werte
  sich unterscheiden: zwei Kanten im Modell, zwei Diagnostics, keine
  Kollision in `to_unique_key`.
- Adapter-Output ist sortiert: zwei Targets, deren Reply-Reihenfolge
  invertiert wurde (z. B. via geschickter Codemodel-Fixture-Reihenfolge),
  liefern denselben sortierten `TargetGraph` wie die kanonische Reihenfolge.
  Der Test prueft Knoten- und Kantenreihenfolge byteweise gegen das
  erwartete Sortier-Tupel, nicht nur den Inhalt.
- Reply mit doppelter `external`-Kante (gleiches `raw_id` in `dependencies`
  zweimal): genau eine Kante im Modell, genau eine Diagnostic.
- Reply mit `dependencies`-Eintrag ohne `id` oder mit leerer `id`: Kante
  wird komplett verworfen, Diagnostic der Schwere `warning`,
  `target_graph_status=partial`. `TargetGraph::edges` enthaelt fuer diesen
  Eintrag keinen Platzhalter.
- Reply mit Selbstkante: Kante wird verworfen, Diagnostic der Schwere
  `note`, `target_graph_status=partial`.
- Reply mit doppelter Kante zwischen `(A, B, direct)`: nur eine Kante im
  Modell, keine Diagnostic.
- Reply mit Target ohne kompilierbare Sourcen behaelt das M4-Verhalten
  (`target_metadata=partial`) und liefert dennoch korrekt aufgeloeste
  Kanten, sofern dieses Target an Kanten beteiligt ist; insbesondere muss
  `target_graph_status=loaded` bleiben, sofern keine graphspezifischen
  Probleme vorliegen.
- File-API-only-Pfad ohne Codemodel-Reply behaelt
  `target_graph_status=not_loaded` und leeren `TargetGraph`.

Service-Tests `tests/hexagon/test_project_analyzer.cpp`:

- Compile-Database-only-Pfad: `result.target_graph_status=not_loaded`,
  `result.target_graph` leer, `target_hubs_in/out` leer.
- File-API-only-Pfad mit voller Reply: `result.target_graph_status=loaded`,
  `result.target_graph` vollstaendig, Hub-Listen auf Default-Schwelle
  geprueft.
- Mixed-Input-Pfad: `result.target_graph_status=loaded`,
  `result.target_assignments` weiter aus dem File-API-Mixed-Pfad,
  `result.target_graph` korrekt befuellt.
- File-API-Pfad mit `partial` Reply: `result.target_graph_status=partial`,
  Diagnostics aus `BuildModelResult` korrekt nach `result.diagnostics`
  uebernommen, ohne dass M4-Diagnostic-Texte driften.
- File-API-Pfad mit `target_metadata=partial` (Targets ohne kompilierbare
  Sourcen) bei vollstaendig aufgeloestem Target-Graphen: explizit
  `result.target_graph_status=loaded`, weil beide Status unabhaengig
  laufen.
- Hub-Schwellen: ein Test deckt einen Graph mit genau einem Knoten an der
  Default-Schwelle ab und einen Test mit einem Knoten knapp unterhalb der
  Schwelle.

Service-Tests `tests/hexagon/test_impact_analyzer.cpp`:

- Compile-Database-only-Impact: `result.target_graph_status=not_loaded`,
  `result.target_graph` leer.
- Mixed-Input-Impact: `result.target_graph_status=loaded`,
  `result.target_graph` propagiert.
- File-API-only-Impact: gleiche Erwartung wie Mixed-Input, sofern die
  File-API-Reply Target-Daten enthaelt.
- AP 1.1 testet keine Impact-Priorisierung; ein expliziter Negativtest pinnt,
  dass `result.affected_targets` weiterhin nach M4/M5-Regeln befuellt wird.

Regressionstests:

- bestehende Console-, Markdown-, HTML-, JSON- und DOT-Goldens fuer
  `analyze` und `impact` bleiben byte-stabil. Drift in Goldens ist ein
  Liefer-Stand-Showstopper.
- `tests/e2e/test_cli.cpp` wird nicht erweitert; AP 1.1 fuehrt keine neuen
  CLI-Optionen ein.

## Abnahmekriterien

AP 1.1 ist abgeschlossen, wenn:

- die neuen Modelltypen kompilieren und in `BuildModelResult`,
  `AnalysisResult` und `ImpactResult` genutzt werden;
- `CmakeFileApiAdapter` direkte Target-zu-Target-Abhaengigkeiten zweistufig
  (Index-Aufbau, danach Dependencies-Aufloesung) extrahiert, externe Kanten
  ueber `<external>::<raw_id>` eindeutig kodiert, defekte `dependencies`-
  Eintraege ohne `id` verwirft, `id`-Kollisionen first-wins behandelt und
  passende Diagnostics anhaengt;
- `target_graph_status` und `target_metadata` unabhaengig voneinander gesetzt
  werden und Tests beide Achsen explizit pinnen;
- `ProjectAnalyzer` und `ImpactAnalyzer` die Target-Graph-Sicht aus
  `BuildModelResult` propagieren und `ProjectAnalyzer` zusaetzlich Hub-Listen
  unter den M6-Default-Schwellen `(10, 10)` aus dem Service-Helper setzt;
- `target_graph_support`-Helper Sortierung, Identitaets-Deduplikation und
  Hub-Klassifikation deterministisch und in einem unabhaengig getesteten
  Modul liefern; `compute_target_hubs` ist defensiv robust gegen
  unsortierte oder duplizierte Eingaben und liefert in Release- und
  Debug-Builds dieselben Hub-Listen;
- bestehende M5-Goldens fuer Console, Markdown, HTML, JSON und DOT byte-stabil
  bleiben;
- `format_version` aus M5 AP 1.1 unveraendert bleibt;
- `git diff --check` sauber ist;
- der Docker-Gate-Lauf gemaess `docs/quality.md` (Tranchen-Gate fuer M6 AP
  1.1) gruen ist.

## Liefer-Stand

Diese Sektion wird nach dem ersten Tranchenschnitt analog zu
`docs/plan-M5-1-8.md` Z. 151ff. mit den Commit-Hashes der A-Schritte
befuellt. Bis dahin bleibt sie leer; ein leeres Liefer-Stand-Feld bedeutet,
dass AP 1.1 noch nicht abnahmefaehig ist.

- A.1 (Modelle): noch nicht ausgeliefert.
- A.2 (`target_graph_support`-Helper und Tests): noch nicht ausgeliefert.
- A.3 (`CmakeFileApiAdapter`-Erweiterung und Adapter-Tests): noch nicht
  ausgeliefert.
- A.4 (Service-Anbindung in `ProjectAnalyzer`/`ImpactAnalyzer` und
  Service-Tests): noch nicht ausgeliefert.
- A.5 (Audit-Pass: Plan-Test-Liste gegen Ist-Stand verifiziert): noch nicht
  ausgeliefert.

## Offene Folgearbeiten

Diese Punkte werden bewusst in spaeteren M6-Arbeitspaketen umgesetzt:

- Reportausgabe der Target-Graph- und Hub-Daten in Console, Markdown, HTML,
  JSON und DOT (AP 1.2).
- Impact-Priorisierung ueber den Target-Graphen (AP 1.3).
- CLI-Optionen `--target-hub-in-threshold`, `--target-hub-out-threshold` und
  Provenienz der wirksamen Schwellen (AP 1.5).
- JSON-Vertragserweiterung fuer `target_graph`, `target_hubs` und
  `target_graph_status` (AP 1.2 entscheidet, ob das ueber `format_version=2`
  oder additive Felder unter `format_version=1` laeuft).
- DOT-Vertrag fuer Target-zu-Target-Kanten (AP 1.2).
- Allgemeine Zyklen-Diagnostics fuer Reports, sobald die Reverse-BFS aus
  AP 1.3 Zyklen beim Traversieren erkennt. AP 1.1 erkennt nur Selbstkanten
  als trivialen Zyklus-Sonderfall und verwirft sie; alle weiteren
  Zyklus-Klassen brauchen eine Traversierung und gehoeren damit zu AP 1.3.
