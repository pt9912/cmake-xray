# Plan M6 - AP 1.3 Impact-Priorisierung ueber den Target-Graphen einfuehren

## Ziel

AP 1.1 hat den Target-Graphen im Kern modelliert; AP 1.2 macht ihn in
Reports sichtbar. AP 1.3 nutzt diesen Graphen erstmals **fachlich**, um
betroffene Targets nicht nur zu listen, sondern nach Naehe zur geaenderten
Datei zu **priorisieren**. Damit erfuellt M6 die Lastenheft-Kennung `F-25`
(priorisierte Target-Betroffenheit ueber den Target-Graphen).

Konkret: `cmake-xray impact` fuegt zu jedem betroffenen Target eine
`priority_class` (`direct`, `direct_dependent`, `transitive_dependent`),
eine `graph_distance` (`0`, `1`, `>=2`) und eine `evidence_strength`
(`direct`, `heuristic`, `uncertain`) hinzu. Eine deterministische
Reverse-BFS ueber `target_graph.edges` berechnet die `graph_distance`,
begrenzt durch ein konfigurierbares Tiefenbudget. CLI-Optionen
`--impact-target-depth <n>` und `--require-target-graph` machen das
Verhalten explizit steuerbar.

Nach AP 1.3 koennen Nutzer und CI-Werkzeuge zwischen "diese Datei trifft
direkt mein Target", "ein Target abhaengt von einem direkt betroffenen
Target" und "ein Target ist mehrere Reverse-Hops entfernt" unterscheiden.
Das ist die fachliche Basis fuer alle nachfolgenden M6-Schritte (insb.
AP 1.5 fuer Schwellenwerte und AP 1.6 fuer Compare).

AP 1.3 hebt `xray::hexagon::model::kReportFormatVersion` von `2`
(eingefuehrt in AP 1.2) auf `3`, weil neue Pflichtfelder im Impact-JSON
zugefuegt werden. Die Aktivierung von `format_version=3` erfolgt erst
in der Tranche, in der alle ausgelieferten Adapter die neuen
Pflichtfelder/-Abschnitte schreiben.

## Scope

Umsetzen:

- Neue Modelle `TargetPriorityClass`, `TargetEvidenceStrength` und
  `PrioritizedImpactedTarget` in `src/hexagon/model/impact_result.h`.
- Erweiterung von `ImpactResult` um `prioritized_affected_targets`,
  `impact_target_depth_requested` und `impact_target_depth_effective`.
- Erweiterung von `AnalyzeImpactRequest` um `impact_target_depth` und
  `require_target_graph`.
- Service-Logik fuer Reverse-BFS in
  `src/hexagon/services/impact_analyzer.cpp` (oder einem neuen
  `target_graph_traversal`-Helper, je nach Code-Komplexitaet).
- CLI-Optionen `--impact-target-depth <n>` (Default `2`, Range `0-32`)
  und `--require-target-graph` (Flag).
- Vier klare CLI-Fehlerphrasen fuer `--impact-target-depth`:
  `negative value`, `not an integer`, `value exceeds maximum 32`,
  `option specified more than once`.
- Reportausgabe der priorisierten Target-Sicht in Console, Markdown,
  HTML, JSON und DOT.
- `kReportFormatVersion` von `2` auf `3` heben, sobald alle Adapter
  v3-konform sind; Schema-Update.
- Aktualisierung von `docs/report-json.md`, `docs/report-json.schema.json`,
  `docs/report-dot.md` und `docs/report-html.md` auf v3.
- Goldens fuer alle fuenf Formate fuer die priorisierte Sicht.
- Diagnostics fuer fehlende Target-Graph-Daten, Budget-/Zyklus-Begrenzung.

Nicht umsetzen:

- CLI-Steuerung fuer die `evidence_strength`-Aggregation oder fuer
  Klassifikations-Schwellenwerte; das gehoert zu AP 1.5 (falls dann
  noetig).
- Compare-Diff fuer `prioritized_affected_targets`. M6 AP 1.6 ist
  ausdruecklich `analyze`-only; Impact-Compare ist ein eigener
  Folgeumfang.
- Reverse-BFS ueber `tu_target`-Kanten oder Include-Hotspot-Daten;
  AP 1.3 traversiert ausschliesslich `target_graph.edges`.
- Weiterleitung der Priorisierung an `analyze`. Hub-Hervorhebung in
  `analyze` bleibt unveraendert von AP 1.2.
- Aenderung der bestehenden M5-`ImpactKind`-Enum (`direct`/`heuristic`)
  fuer Translation-Units. AP 1.3 bringt die neue Achse nur an Targets
  an, nicht an TUs.

Diese Punkte folgen in spaeteren Arbeitspaketen oder bleiben bewusst
ausserhalb von M6.

## Voraussetzungen aus AP 1.1 und AP 1.2

AP 1.3 baut auf folgenden Vertraegen auf:

- `ImpactResult::target_graph`, `ImpactResult::target_graph_status` und
  die normalisierte `TargetGraph::edges`-Liste aus AP 1.1.
- `target_graph_support::sort_target_graph` als Single-Source-of-Truth
  fuer Identitaets-Normalisierung; AP 1.3 ruft den Helper bei Bedarf
  erneut, baut aber keine eigene Sortier-/Dedup-Logik.
- `kReportFormatVersion=2` aus AP 1.2 als Ausgangsbasis fuer den Sprung
  auf `3`.
- Bestehender `cmake-xray.impact`-JSON-Vertrag mit
  `directly_affected_translation_units`, `directly_affected_targets`,
  etc. AP 1.3 fuegt neue Felder hinzu, ohne die bestehenden Felder zu
  brechen.
- AP 1.2 hat `target_graph` und `target_graph_status` bereits in
  Console, Markdown, HTML und DOT Reports etabliert; AP 1.3 ergaenzt
  die priorisierte Sicht und nutzt die bestehende
  `target_dependency`-Kantendarstellung in DOT.

Falls AP 1.1 oder AP 1.2 nicht vollstaendig geliefert sind (insbesondere
`target_graph_status`-Defaults, Reverse-BFS-tauglicher
`TargetGraph::edges`, `format_version=2`), ist AP 1.3 nicht
abnahmefaehig.

## Dateien

Voraussichtlich zu aendern:

- `src/hexagon/model/impact_result.h`
- `src/hexagon/model/report_format_version.h`
- `src/hexagon/ports/driving/analyze_impact_port.h`
- `src/hexagon/services/impact_analyzer.{h,cpp}`
- `src/adapters/cli/cli_adapter.{h,cpp}`
- `src/adapters/output/console_report_adapter.cpp`
- `src/adapters/output/markdown_report_adapter.cpp`
- `src/adapters/output/html_report_adapter.cpp`
- `src/adapters/output/json_report_adapter.cpp`
- `src/adapters/output/dot_report_adapter.cpp`
- `src/adapters/output/impact_priority_text.h`
- `src/main.cpp`
- `docs/report-json.md`
- `docs/report-json.schema.json`
- `docs/report-dot.md`
- `docs/report-html.md`
- `tests/adapters/test_cmake_file_api_adapter.cpp`
- `tests/adapters/test_console_report_adapter.cpp`
- `tests/adapters/test_markdown_report_adapter.cpp`
- `tests/adapters/test_html_report_adapter.cpp`
- `tests/adapters/test_json_report_adapter.cpp`
- `tests/adapters/test_dot_report_adapter.cpp`
- `tests/hexagon/test_impact_analyzer.cpp`
- `tests/e2e/test_cli.cpp`
- `tests/e2e/testdata/m5/<format>-reports/manifest.txt` (Goldens-Migration auf v3)
- `tests/e2e/testdata/m6/<format>-reports/manifest.txt` (neue Priorisierungs-Goldens)
- `docs/examples/manifest.txt`
- `docs/examples/generation-spec.json`

Neue Dateien (optional, je nach Code-Aufteilung):

- `src/adapters/output/impact_priority_text.cpp`, falls der gemeinsame
  String-Helper nicht header-only umgesetzt wird.
- `src/hexagon/services/target_graph_traversal.{h,cpp}` falls Reverse-BFS
  als eigener Helper sinnvoll ist (anstatt direkt im
  `ImpactAnalyzer`-Body).
- `tests/hexagon/test_target_graph_traversal.cpp` analog.
- `tests/e2e/testdata/m6/json-reports/impact-prioritised-*.json` als
  neue Goldens-Familie fuer die priorisierte Sicht.

## Format-Versionierung

`xray::hexagon::model::kReportFormatVersion` steigt von `2` (AP 1.2) auf
`3`. Begruendung:

- Impact-JSON erhaelt drei neue Pflichtfelder `prioritized_affected_targets`,
  `impact_target_depth_requested` und `impact_target_depth_effective`.
- Der M5-Vertrag aus AP 1.1 sagt: "Hinzufuegen eines neuen Pflichtfelds
  erhoeht `format_version`". AP 1.3 folgt dem Vertrag konsequent.
- HTML, DOT und Console/Markdown bekommen ebenfalls neue Pflicht-Sections
  bzw. -Abschnitte; der gemeinsame Versionssprung haelt alle Formate
  synchron.

Wie in AP 1.2 gilt auch in AP 1.3:

- Kein Dual-Output, kein CLI-Schalter zurueck zu v2.
- Schema `report-json.schema.json` setzt `FormatVersion.const` auf `3`
  erst zusammen mit der globalen Aktivierung von
  `kReportFormatVersion=3` in A.4. Vorher duerfen Schema-Aenderungen
  nur als Zielvertrag dokumentiert oder in nicht-produktiven
  Draft-/Testpfaden vorbereitet werden; die produktive Schema-Const
  bleibt mit der produktiven Report-Version synchron.
- Bestehende Goldens unter `m5/` und `m6/` werden in AP-1.3 inhaltlich
  auf v3 migriert (Felder hinzugefuegt). Beide Verzeichnisse leben
  weiter als fachliche Datensatz-Trennung (M5-Datensatz vs.
  M6-Datensatz).
- Tranche-Invariant: Kein Commit darf Reports mit `format_version=3`
  erzeugen, solange ein produktiver Adapter noch nicht alle AP-1.3-
  Pflichtfelder bzw. Pflichtabschnitte rendert. Bis zu dieser
  Aktivierung bleiben E2E-/Golden-Gates auf v2 oder auf adapterlokale
  interne Tests beschraenkt.

Konsequenzen fuer AP 1.6 (Compare):

- Compare-Kompatibilitaetsmatrix in `docs/report-json.md` muss
  v2-vs-v3-Vergleich abdecken oder explizit ablehnen. AP 1.3 dokumentiert
  in der `report-json.md`-Aenderung, dass v2-Compare ohne
  Priorisierungs-Felder weiterhin moeglich ist, weil AP 1.6 ohnehin
  `analyze`-only ist und die Impact-Felder fuer Compare ausgeschlossen
  sind. Der Versionssprung ist damit fuer Compare unproblematisch.

## Modellvertrag

### Neue Enums

```cpp
namespace xray::hexagon::model {

enum class TargetPriorityClass {
    direct,                 // graph_distance == 0
    direct_dependent,       // graph_distance == 1
    transitive_dependent,   // graph_distance >= 2
};

enum class TargetEvidenceStrength {
    direct,
    heuristic,
    uncertain,
};

}  // namespace xray::hexagon::model
```

Regeln fuer `TargetPriorityClass`:

- Sortierstaerke ueber explizite Rangtabelle in
  `src/hexagon/services/impact_analyzer.cpp`:
  `direct=0`, `direct_dependent=1`, `transitive_dependent=2` (kleinster
  Rang gewinnt). Sortierung darf NICHT aus `enum class`-Ordinalwerten
  oder String-Werten abgeleitet werden, weil Reorders im Enum die
  Reportordnung sonst still brechen wuerden.
- `direct` korrespondiert exakt zu `graph_distance=0`,
  `direct_dependent` zu `graph_distance=1`, `transitive_dependent` zu
  `graph_distance>=2`. Diese Korrespondenz ist Vertragsbestandteil; ein
  Target mit `priority_class=direct` und `graph_distance!=0` ist ein
  Service-Defekt.

Regeln fuer `TargetEvidenceStrength`:

- Sortierstaerke ueber explizite Rangtabelle:
  `direct=0`, `heuristic=1`, `uncertain=2` (kleinster Rang gewinnt).
- `direct`: Das Target wurde aus mindestens einer direkt betroffenen TU
  (`ImpactKind::direct`) als Owner-Target erkannt, oder es ist in seiner
  minimalen Reverse-BFS-Distanz von einem `direct`-Seed erreichbar.
- `heuristic`: Das Target wurde nur aus heuristisch betroffenen TUs
  (`ImpactKind::heuristic`) als Owner erkannt, oder es ist in seiner
  minimalen Reverse-BFS-Distanz von einem `heuristic`-Seed erreichbar
  (und auf dieser Distanz nicht zugleich von einem `direct`-Seed
  erreichbar).
- `uncertain`: Das Target ist betroffen, aber weder eine direkte TU
  noch eine heuristische TU des Targets ist im
  `affected_translation_units`-Set; das Target wurde rein ueber
  Build-Metadaten (z. B. `target_assignments` mit indirekter
  Header-Beziehung) als betroffen klassifiziert. Auch Targets, die ueber
  Reverse-BFS erreicht werden, deren Quelle aber selbst `uncertain` ist,
  erben diese Stufe.
- Bei mehreren erreichbaren Wegen gewinnt zuerst die kuerzere Distanz.
  Nur unter Wegen derselben minimalen Distanz gewinnt die staerkere
  Evidenz: `direct` > `heuristic` > `uncertain`.

### `PrioritizedImpactedTarget`

```cpp
namespace xray::hexagon::model {

struct PrioritizedImpactedTarget {
    TargetInfo target;
    TargetPriorityClass priority_class;
    std::size_t graph_distance;
    TargetEvidenceStrength evidence_strength;
};

}  // namespace xray::hexagon::model
```

Regeln:

- `target` enthaelt `display_name`, `type` und `unique_key` aus dem
  jeweiligen Target.
- `graph_distance` ist die minimal erreichbare Reverse-BFS-Distanz vom
  Target zum naechsten Impact-Seed-Target. Impact-Seed-Targets sind
  Owner-Targets direkt oder heuristisch betroffener TUs sowie
  `uncertain`-Targets, die rein ueber Build-Metadaten als betroffen
  klassifiziert wurden. `uncertain`-Targets sind keine Owner-Targets
  eines betroffenen TU, werden fuer die Distanzberechnung aber als
  Seeds mit `graph_distance=0` behandelt.
- `priority_class` ist abhaengig vom `graph_distance` (siehe
  Korrespondenzregel oben).
- `evidence_strength` folgt der oben definierten Regel.

### Erweiterung von `ImpactResult`

```cpp
struct ImpactResult {
    // ... bestehende Felder ...
    std::vector<PrioritizedImpactedTarget> prioritized_affected_targets;
    std::size_t impact_target_depth_requested{2};
    std::size_t impact_target_depth_effective{0};
};
```

Regeln:

- `prioritized_affected_targets` ist die vollstaendige priorisierte
  Sicht; sie enthaelt jedes betroffene Target genau einmal, sortiert
  nach dem M6-Hauptplan-Sortier-Tupel
  `(priority_class, graph_distance, evidence_strength, stable_target_key, display_name, target_type)`.
- `impact_target_depth_requested` ist der normalisierte angeforderte
  Wert: `AnalyzeImpactRequest::impact_target_depth.value_or(2)`.
  Der `ImpactAnalyzer` setzt dieses Feld fuer jeden erfolgreich
  aufgebauten `ImpactResult`, auch fuer Nicht-CLI-Caller. Adapter
  duerfen deshalb niemals `null` ausgeben oder das Feld auslassen.
- `impact_target_depth_effective` ist die tatsaechlich angewendete
  maximale Reverse-BFS-Tiefe. Bei fehlendem Target-Graphen
  (`target_graph_status=not_loaded`) gilt `0` (M5-Fallback). Bei
  `--require-target-graph` ohne Target-Graph wird `impact` gar nicht
  bis zum Result-Aufbau ausgefuehrt; der Fehlerfall produziert keinen
  Reportoutput.
- Die bestehenden Felder `affected_targets`, `directly_affected_targets`
  und `heuristically_affected_targets` aus M5 bleiben **erhalten** in
  AP 1.3. Das vereinfacht Migration und vermeidet Doppel-Brueche; die
  neuen `prioritized_affected_targets` ergaenzen, ersetzen aber nicht.
  AP 1.7 oder ein Folge-AP entscheidet, ob die alten Felder nach M6
  deprecated werden.

### Erweiterung von `AnalyzeImpactRequest`

```cpp
struct AnalyzeImpactRequest {
    // ... bestehende Felder ...
    std::optional<std::size_t> impact_target_depth;
    bool require_target_graph{false};
};
```

Regeln:

- `impact_target_depth` ist ein optionaler Wert, der vom CLI-Adapter
  gesetzt wird. Der CLI-Default `2` wird im CLI-Adapter angewendet
  (siehe CLI-Vertrag); fehlt der Wert im Request, wendet der
  `ImpactAnalyzer` ebenfalls `2` als Fallback an.
- `require_target_graph` ist ein Flag, das im CLI-Adapter gesetzt
  wird. Bei `true` und fehlendem nutzbaren Target-Graphen wird
  `impact` bereits in der Service-Eingabevalidierung abgebrochen;
  siehe Validierungsvertrag und CLI-Vertrag.

### Service-Eingabevalidierung

`ImpactAnalyzer::analyze_impact()` validiert `AnalyzeImpactRequest`
selbst, unabhaengig davon, ob der Aufrufer CLI, Test-Builder oder ein
anderer Adapter ist:

- `impact_target_depth.value_or(2)` muss in `[0, 32]` liegen. Ein Wert
  `> 32` ist ein Service-Validierungsfehler mit Code
  `impact_target_depth_out_of_range` und Message
  `"impact_target_depth: value exceeds maximum 32"`.
- Negative Tiefenwerte sind kein Service-Fehlerfall dieses
  Request-Typs, weil `AnalyzeImpactRequest::impact_target_depth` als
  `std::size_t` modelliert ist und negative Werte nicht repraesentieren
  kann. Die CLI validiert negative Textwerte vor dem Request-Aufbau mit
  der CLI-Fehlerphrase `"--impact-target-depth: negative value"`.
  Kuenftige Nicht-CLI-Adapter mit signed oder textueller Eingabe
  muessen dieselbe Normalisierung vor Konstruktion des
  `AnalyzeImpactRequest` leisten.
- `require_target_graph=true` verlangt
  `target_graph_status=loaded` oder `target_graph_status=partial`.
  Bei `not_loaded` bricht der Service vor dem `ImpactResult`-Aufbau mit
  Code `target_graph_required` und Message
  `"target graph data is required but not available"` ab.
- Service-Validierungsfehler erzeugen keinen partiellen
  `ImpactResult`. CLI, Tests und kuenftige API-Adapter mappen diese
  Fehler auf ihr jeweiliges Fehlerformat; die CLI-Mapping-Regeln stehen
  im CLI-Vertrag.

## Service-Vertrag (Reverse-BFS)

`ImpactAnalyzer::analyze_impact()` wird so erweitert, dass nach der
bestehenden M5-Logik (Compile-DB-/File-API-Lade-Pfad, TU-Sammlung,
M4-Target-Zuordnung, M5-`affected_targets`-Aufbau) die Priorisierung
ueber den Target-Graphen folgt.

### Status-Verhalten des Target-Graphen

- `target_graph_status=loaded`: Reverse-BFS laeuft ueber die
  vollstaendige normalisierte `target_graph.edges`-Liste.
- `target_graph_status=partial`: Reverse-BFS laeuft deterministisch
  ueber die vorhandene normalisierte Teilmenge von
  `target_graph.edges`. Fehlende oder nicht aufloesbare Kanten werden
  nicht synthetisiert. `impact_target_depth_effective` wird wie bei
  `loaded` aus der tatsaechlich erreichten maximalen Distanz innerhalb
  dieses Teilgraphen berechnet; der Wert ist nicht pauschal `0` und
  nicht pauschal `requested`.
- Bei `partial` wird genau eine Diagnostic der Schwere `warning` an
  `result.diagnostics` angehaengt:
  `"target graph partially loaded; impact prioritisation uses available edges only"`.
  Diese Diagnostic ist unabhaengig davon, ob die BFS vorzeitig endet;
  eine zusaetzliche `stopped at depth N`-Note bleibt erlaubt, wenn der
  vorhandene Teilgraph vor dem angeforderten Tiefenbudget endet.
- `target_graph_status=not_loaded`: Es gilt der Fallback ohne
  Reverse-BFS, siehe unten.

### Algorithmus

1. **Impact-Seed-Targets sammeln**:
   - Direkt betroffene Owner-Targets sind Targets, die mindestens eine
     `ImpactKind::direct`-TU als Member haben.
     `evidence_strength=direct`, `graph_distance=0`,
     `priority_class=direct`.
   - Heuristisch betroffene Owner-Targets sind Targets, die nur
     `ImpactKind::heuristic`-TUs als Member haben.
     `evidence_strength=heuristic`, `graph_distance=0`,
     `priority_class=direct`.
   - Targets, die ueberhaupt keine TUs als Member haben (rare Faelle
     mit indirektem Header-Mapping ueber `target_assignments`):
     `evidence_strength=uncertain`, `graph_distance=0`,
     `priority_class=direct`.
   - Wenn dasselbe Target ueber mehrere Seed-Quellen erkannt wird, gibt
     es genau einen Seed-Eintrag mit `graph_distance=0`; die staerkste
     Evidenz gewinnt.
2. **Reverse-BFS**:
   - Frontier `F_0` = Vereinigung aller Impact-Seed-Targets.
   - Fuer jedes `step in {1..impact_target_depth_requested}`:
     - Sortiere die aktuelle Frontier `F_{step-1}` nach
       `(stable_target_key, edge_kind, display_name, target_type)`.
     - Sortiere die `target_graph.edges` deterministisch nach demselben
       Tupel.
     - Sammle alle Kanten mit `to_unique_key in F_{step-1}` und
       `kind=direct` (in M6 ist `direct` der einzige Kantentyp).
     - Pro Kante: Das `from_unique_key`-Target ist ein Reverse-BFS-Hop
       mit Kandidatendistanz `step`; `priority_class` folgt der
       Korrespondenzregel, `evidence_strength` wird vom
       Frontier-Vorgaenger geerbt.
     - Fuehre die Kandidaten dieses Schritts pro Target zusammen und
       gleiche sie gegen die bisher gespeicherten Resultate ab:
       - Wenn das Target noch nicht erreicht wurde, fuege es mit
         `graph_distance=step` hinzu.
       - Wenn das Target bereits mit kleinerer Distanz erreicht wurde,
         ignoriere den Kandidaten; kuerzeste Distanz gewinnt.
       - Wenn das Target bereits mit derselben Distanz erreicht wurde,
         aktualisiere nur `evidence_strength`, falls der Kandidat
         staerkere Evidenz liefert. `graph_distance` und
         `priority_class` bleiben unveraendert.
     - Die naechste Frontier `F_step` enthaelt alle Targets, deren
       minimale Distanz in diesem Schritt erstmals entdeckt wurde, mit
       der nach dem Merge staerksten Evidenz dieser Distanz.
     - Wenn `F_step` leer ist, beende die BFS vorzeitig; setze
       `impact_target_depth_effective` auf `step-1`, also die groesste
       tatsaechlich erreichte Distanz, nicht auf den `requested`-Wert.
   - Wenn `impact_target_depth_effective < impact_target_depth_requested`,
     fuege eine Diagnostic der Schwere `note` an `result.diagnostics`
     an: `"reverse target graph traversal stopped at depth N (no further reachable targets)"`.
3. **Externe und unresolved Kanten**:
   - `target_graph.edges` mit `resolution=external` haben `to_unique_key`
     der Form `<external>::<raw_id>`. Externe Targets sind keine
     Impact-Seed-Targets und tauchen daher nicht in `F_0` auf. Reverse-BFS
     erreicht externe Targets als Frontier-Member nur dann, wenn sie
     ueber andere interne Targets als `to`-Knoten verbunden sind, was
     per AP-1.1-Vertrag nicht passieren kann (externe sind nur
     `to`-Knoten von outgoing-Kanten interner Quellen). Konsequenz:
     `prioritized_affected_targets` enthaelt **keine** externen
     Targets.
   - AP 1.3 verlaesst sich nicht allein auf diese AP-1.1-Invariante:
     Kandidaten mit externem Target-Key (`<external>::...`) oder ohne
     interne `TargetInfo` werden vor dem Eintrag in
     `prioritized_affected_targets` explizit verworfen. Sollte AP 1.1
     spaeter externe Knoten auch als `from`-Knoten zulassen, bleibt der
     AP-1.3-Output dadurch unveraendert frei von externen Targets.
4. **Zyklen-Erkennung**:
   - Die erreichte-Targets-Tabelle verhindert, dass Zyklen die BFS in
     eine Endlosschleife treiben. Wenn die BFS einen Zyklus erkennt
     (ein Target wird erneut als Reverse-Kantenziel angetroffen), wird
     nur eine Erreichung mit groesserer Distanz ignoriert. Eine
     Erreichung in derselben Distanz darf die Evidenzstaerke nachziehen.
   - Wenn die BFS innerhalb eines Zyklus das Tiefenbudget erschoepft,
     wird eine Diagnostic der Schwere `warning` angehaengt:
     `"reverse target graph traversal hit depth limit N within a cycle; some transitively dependent targets may be missing"`.
5. **Sortierung**:
   - Die finale `prioritized_affected_targets`-Liste wird sortiert nach
     `(priority_class, graph_distance, evidence_strength, stable_target_key, display_name, target_type)`.
   - Sortier-Helper aus `target_graph_support` werden wiederverwendet,
     wo moeglich; ggf. neue freie Funktion
     `sort_prioritized_impacted_targets()` in
     `target_graph_traversal.{h,cpp}` (oder direkt im
     `impact_analyzer.cpp`).
6. **`impact_target_depth_effective`-Berechnung**:
   - `target_graph_status=not_loaded` → `0`.
   - Andernfalls `min(impact_target_depth_requested, max_reachable_depth)`,
     wobei `max_reachable_depth` durch das vorzeitige BFS-Ende
     bestimmt wird (siehe Schritt 2). Wenn die BFS bis zur angeforderten
     Tiefe Targets findet, ist `effective == requested`.

### Kompatibilitaets-Fallback ohne Target-Graph

Wenn `target_graph_status=not_loaded` (Compile-DB-only-Pfad oder
File-API ohne Target-Graph-Daten) und der Nutzer **nicht**
`--require-target-graph` gesetzt hat:

- `prioritized_affected_targets` enthaelt nur die Impact-Seed-Targets
  aus Schritt 1, also `graph_distance=0`/`priority_class=direct`.
  Keine Reverse-BFS-Hops.
- `impact_target_depth_requested` ist der CLI-/Default-Wert.
- `impact_target_depth_effective` ist `0`.
- Eine Diagnostic der Schwere `note` an `result.diagnostics`:
  `"target graph not loaded; impact prioritisation degrades to impact seed targets only"`.

Wenn der Nutzer `--require-target-graph` gesetzt hat und kein Target-
Graph verfuegbar ist:

- `ImpactAnalyzer` produziert kein `ImpactResult`. Der Service-Aufruf
  endet mit einem nonzero CLI-Exit; siehe CLI-Vertrag.

### Determinismus-Anforderungen

- Reverse-BFS-Ergebnisse muessen byte-stabil sein, unabhaengig von
  Container-/Map-Iterationsreihenfolge im C++-Standard. Helper-
  Funktionen verwenden `std::map` oder explizit sortierte
  `std::vector`-Listen, niemals `std::unordered_map` als Quelle der
  Iteration.
- Bei mehreren Wegen zu demselben Target gewinnt:
  - Zuerst die kuerzere Distanz.
  - Bei gleicher Distanz die staerkere Evidenz (`direct` > `heuristic`
    > `uncertain`).
  - Bei gleicher Distanz und Evidenz ist die `target`-Identitaet
    unique; mehrere Kandidaten werden zu genau einem Result-Eintrag
    gemerged.
- Aenderungen am Provenienz-Pfad (welcher konkrete BFS-Pfad das Target
  zuerst erreicht hat) duerfen `graph_distance` und `priority_class`
  NICHT veraendern; sie aendern nur Diagnostics-Provenienz und sind
  deshalb in M6 nicht Teil des Modellvertrags.

## CLI-Vertrag

### `--impact-target-depth <n>`

- **Default**: `2`.
- **Wertebereich**: ganze Zahl in `[0, 32]`.
- **Verhalten**:
  - `0` reduziert die Sicht auf Impact-Seed-Targets ohne
    Reverse-BFS-Hops.
  - Werte `>= 1` aktivieren entsprechend viele Reverse-BFS-Schritte.
- **Fehlerphrasen** (CLI-Verwendungsfehler, Exit-Code `2`, Text auf
  `stderr`, kein stdout-Report, keine Zieldatei):
  - `"--impact-target-depth: not an integer"` bei nicht-numerischen
    Werten wie `"abc"` oder `"1.5"`.
  - `"--impact-target-depth: negative value"` bei Werten `< 0`.
  - `"--impact-target-depth: value exceeds maximum 32"` bei Werten `> 32`.
  - Mehrfaches Setzen derselben Option: `"--impact-target-depth: option specified more than once"`.
  Die CLI prueft diese Faelle vor dem Request-Aufbau. Der Service
  validiert die normalisierte Tiefe trotzdem erneut und liefert fuer
  nicht-CLI-Aufrufer den Code `impact_target_depth_out_of_range`.

### `--require-target-graph`

- **Flag** (kein Wert).
- **Verhalten**:
  - Wenn gesetzt und ein nutzbarer Target-Graph (`target_graph_status`
    `loaded` oder `partial`) vorliegt: keine Auswirkung; die
    Priorisierung laeuft normal.
  - Wenn gesetzt und kein nutzbarer Target-Graph vorliegt: `impact`
    endet vor dem Result-Aufbau mit Exit-Code `1` (Eingabefehler) und
    der Fehlermeldung
    `"--require-target-graph: target graph data is required but not available"`.
    Kein stdout-Report, keine Zieldatei (`--output` wird ebenfalls
    leer gelassen).
- **Exit-Code-Begruendung**: `--require-target-graph` ist syntaktisch
  korrekt geparst; der Fehler entsteht erst bei der Eingabeanalyse,
  weil die angeforderte File-API-/Target-Graph-Datenbasis fehlt.
  Deshalb mappt die CLI den Service-Code `target_graph_required` auf
  Exit-Code `1`. Parser- und Nutzungsfehler wie ungueltige
  `--impact-target-depth`-Werte bleiben Exit-Code `2`.
- **Wechselwirkung mit `--impact-target-depth`**: Beide Optionen sind
  unabhaengig kombinierbar. `--impact-target-depth 0
  --require-target-graph` ist gueltig; der Service rendert
  `prioritized_affected_targets` mit nur Impact-Seed-Targets, aber das
  `--require-target-graph` haelt das Vertragsversprechen "Target-Graph
  ist verfuegbar".

### Wechselwirkung mit existierenden CLI-Optionen

- `--format`-Validierung aus M5 bleibt unveraendert. Die ausgewaehlten
  Formate unterstuetzen die neuen v3-Felder erst ab der globalen
  Version-Aktivierung in A.4; vorher bleiben produktive Reports v2
  gemaess Tranche-Invariant.
- `--output` aus M5 bleibt unveraendert; atomarer Schreibvertrag aus
  AP M5-1.1 gilt fuer v3-Outputs identisch.
- `--quiet`/`--verbose` aus AP M5-1.5 bleiben unveraendert.

## Reportausgabe

### Sortier-Vertrag (alle Formate)

Die priorisierte Sicht wird in allen fuenf Formaten in derselben
Reihenfolge ausgegeben, gemaess M6-Hauptplan-Sortiervertrag:
`(priority_class, graph_distance, evidence_strength, stable_target_key, display_name, target_type)`.

`priority_class` und `evidence_strength` sortieren ueber explizite
Rangtabellen (siehe Modellvertrag oben), nicht ueber Enum-Ordinalwerte.

### JSON

Neue Pflichtfelder im Impact-JSON (`format_version=3`):

```json
{
  "prioritized_affected_targets": [
    {
      "target": {
        "display_name": "common",
        "type": "STATIC_LIBRARY",
        "unique_key": "common::STATIC_LIBRARY"
      },
      "priority_class": "direct",
      "graph_distance": 0,
      "evidence_strength": "direct"
    }
  ],
  "impact_target_depth_requested": 2,
  "impact_target_depth_effective": 1
}
```

Feldreihenfolge im Impact-JSON (`format_version=3`):

1. `format`
2. `format_version`
3. `report_type`
4. `inputs`
5. `summary`
6. `directly_affected_translation_units`
7. `heuristically_affected_translation_units`
8. `directly_affected_targets`
9. `heuristically_affected_targets`
10. `target_graph_status`
11. `target_graph`
12. `prioritized_affected_targets`
13. `impact_target_depth_requested`
14. `impact_target_depth_effective`
15. `diagnostics`

`prioritized_affected_targets`-Item-Schema:

| Feld | Typ | Pflicht | Werte |
| --- | --- | --- | --- |
| `target` | object | ja | `TargetNode`-Schema aus AP 1.2. |
| `priority_class` | string | ja | `direct`, `direct_dependent`, `transitive_dependent`. |
| `graph_distance` | integer | ja | `0` bis `32`; zusaetzlich `<= impact_target_depth_requested`. |
| `evidence_strength` | string | ja | `direct`, `heuristic`, `uncertain`. |

`impact_target_depth_requested`/`effective`-Felder:

| Feld | Typ | Pflicht | Werte |
| --- | --- | --- | --- |
| `impact_target_depth_requested` | integer | ja | `0` bis `32` (CLI-Range). |
| `impact_target_depth_effective` | integer | ja | `0` bis `impact_target_depth_requested`. |

Schema-Erweiterung in `report-json.schema.json`:

- `ImpactReport.required` erhaelt die drei neuen Felder.
- `PrioritizedImpactedTarget` als neues `$defs`-Objekt mit
  `additionalProperties: false`.
- `TargetPriorityClass` und `TargetEvidenceStrength` als neue Enum-Defs.
- `FormatVersion.const` steigt von `2` auf `3`, aber erst in A.4
  zusammen mit `kReportFormatVersion=3` und den v3-faehigen Adaptern.

### HTML

Neue Pflicht-Section im Impact-HTML, eingefuegt zwischen `Target Graph
Reference` und `Diagnostics`:

```
8. Prioritised Affected Targets
9. Diagnostics
```

Section-Inhalt:

- `h2`: `Prioritised Affected Targets`.
- Sichtbare Tiefenangabe: `Requested depth: 2. Effective depth: 1.`.
- Tabelle mit Spalten `Display name`, `Type`, `Priority class`,
  `Graph distance`, `Evidence strength`, `Unique key`. Eindeutige
  Identifikation ueber `Unique key`-Spalte; `Display name`-Kollisionen
  werden nicht zusaetzlich diambiguiert (anders als in AP 1.2 Hubs),
  weil die `Unique key`-Spalte den Schluessel sowieso anzeigt.
- Wenn `prioritized_affected_targets` leer ist und
  `target_graph_status=loaded`/`partial`: Section bleibt mit `h2`,
  Inhalt ist Absatz `No prioritised targets.`.
- Wenn `target_graph_status=not_loaded`: Section bleibt mit `h2`,
  Inhalt ist Absatz `Target graph not loaded; prioritisation skipped.`,
  und Tiefenangabe wird durch `Requested depth: 2. Effective depth:
  0 (no graph).` ergaenzt.

### DOT

DOT-Vertragsregeln werden um optionale Knoten- und Kantenattribute
erweitert:

- Target-Knoten in Impact-DOT erhalten zusaetzlich:
  - `priority_class` (string-quoted): `direct`, `direct_dependent`,
    `transitive_dependent`.
  - `graph_distance` (integer): `0`, `1`, ...
  - `evidence_strength` (string-quoted): `direct`, `heuristic`,
    `uncertain`.
- Diese Attribute erscheinen ausschliesslich an Target-Knoten in der
  Impact-Sicht (nicht in der Analyze-Sicht), und nur wenn
  `target_graph_status != not_loaded`.
- Neue Graph-Attribute fuer Impact-DOT:
  - `graph_impact_target_depth_requested` (integer).
  - `graph_impact_target_depth_effective` (integer).
- Der `graph_`-Praefix ist bewusst DOT-spezifisch: Es sind
  Graph-Attribute, die 1:1 auf die Modellfelder
  `impact_target_depth_requested` und
  `impact_target_depth_effective` abgebildet werden.
- Beide Graph-Attribute sind Pflicht in Impact-DOT, unabhaengig von
  `target_graph_status`. Bei `not_loaded` uebernimmt
  `graph_impact_target_depth_requested` weiterhin den Wert aus
  `ImpactResult` (z. B. `2` beim Default);
  `graph_impact_target_depth_effective` ist `0`, weil ohne Graph keine
  Reverse-BFS-Hops ausgefuehrt werden.
- Die bestehende Knotenprioritaet im Impact-DOT aus AP 1.2 bleibt
  unveraendert; AP 1.3 fuegt nur neue Attribute auf bestehenden Knoten
  hinzu, keine neue Knotenart.
- Das `impact`-Attribut auf Target-Knoten aus M5 (`direct`/`heuristic`)
  bleibt erhalten und ergaenzt das neue `evidence_strength`. Beide
  Attribute haben unterschiedliche Bedeutung: `impact` ist die
  bestehende M5-Klassifikation (Klassifikation ueber `affected_targets`),
  `evidence_strength` ist die AP-1.3-Klassifikation aus der
  Reverse-BFS. Beide koexistieren in v3.

### Console

Neuer Abschnitt **Prioritised Affected Targets** im Impact-Output:

```
Prioritised Affected Targets (requested depth: 2, effective depth: 1):
  [direct, distance=0, evidence=direct] common
  [direct_dependent, distance=1, evidence=direct] app
  [direct_dependent, distance=1, evidence=heuristic] tool
```

- Eine Zeile pro Target, zwei Leerzeichen Einzug.
- Suffix-Format `[<priority_class>, distance=<n>, evidence=<strength>]`
  vor dem `display_name`. Bei Namens-Kollision haengt
  `disambiguate_target_display_names()` aus AP 1.2 `[key:
  <unique_key>]` an, nach dem `display_name`.
- Wenn `target_graph_status=not_loaded`: Abschnitt erscheint ohne
  Target-Zeilen als reine Metadaten-/Fallback-Zeile:
  `Prioritised Affected Targets (requested depth: 2, effective depth: 0, no graph).`
  Die Diagnostic
  `"target graph not loaded; impact prioritisation degrades to impact seed targets only"`
  macht den Grund ebenfalls sichtbar.
- Wenn `prioritized_affected_targets` nicht leer ist, erscheint der
  Abschnitt mit allen Eintraegen, sortiert nach dem 6-Tupel.
- Wenn `prioritized_affected_targets` leer ist und
  `target_graph_status=loaded`/`partial`: Abschnitt erscheint mit dem
  Leersatz `No prioritised targets.`.

### Markdown

Neuer Abschnitt im Impact-Markdown:

```markdown
## Prioritised Affected Targets

Requested depth: `2`. Effective depth: `1`.

| Display name | Type | Priority class | Graph distance | Evidence strength | Unique key |
|---|---|---|---|---|---|
| `common` | `STATIC_LIBRARY` | `direct` | 0 | `direct` | `common::STATIC_LIBRARY` |
| `app` | `EXECUTABLE` | `direct_dependent` | 1 | `direct` | `app::EXECUTABLE` |
```

- Markdown-Tabellen-Escaping aus AP 1.2 gilt fuer alle Zellen
  (`|` zu `\|`, Whitespace-Normalisierung).
- Bei `not_loaded`: Section bleibt mit Tiefenangabe erhalten:
  ``Requested depth: `2`. Effective depth: `0` (no graph).``,
  gefolgt vom Absatz `Target graph not loaded; prioritisation skipped.`.
- Bei leerem `prioritized_affected_targets`: Leersatz-Absatz `No
  prioritised targets.`.

## Adapter-Implementierungs-Hinweise

Gemeinsame Helfer:

- `evidence_strength` und `priority_class` werden als String-Konstanten
  aus einem gemeinsamen Helper (`impact_priority_text.h`) abgerufen,
  damit JSON, DOT und HTML denselben String-Wert verwenden. Console und
  Markdown nutzen denselben Helper fuer das Suffix-Format.
- Adapter rufen `target_graph_support::sort_target_graph` NICHT erneut
  auf; sie rendern aus `result.prioritized_affected_targets` und
  vertrauen der Service-Sortierung.

Adapter-Grenzen (unveraendert aus M5/AP 1.2):

- Adapter rendern ausschliesslich aus `ImpactResult`. AP 1.3 fuegt
  keinen CLI-Kontext in Adapter ein.
- Reverse-BFS-Logik lebt ausschliesslich im Hexagon (im
  `ImpactAnalyzer` oder `target_graph_traversal`-Helper). Adapter
  berechnen weder Distanzen noch Klassifikationen.

## Tests

Service-Tests `tests/hexagon/test_impact_analyzer.cpp`:

- Compile-DB-only-`impact`: `prioritized_affected_targets` enthaelt nur
  Impact-Seed-Targets (`graph_distance=0`);
  `impact_target_depth_effective=0`.
  Diagnostic `"target graph not loaded ..."` ist gesetzt.
- Service-Validierung: `AnalyzeImpactRequest::impact_target_depth=33`
  bricht mit Code `impact_target_depth_out_of_range` und ohne
  `ImpactResult` ab.
- Service-Validierung: `require_target_graph=true` mit
  `target_graph_status=not_loaded` bricht mit Code
  `target_graph_required` und ohne `ImpactResult` ab.
- File-API-`impact` mit Target-Graph, Default-Tiefe `2`: Reverse-BFS
  laeuft korrekt, alle erreichten Targets haben passende
  `priority_class` und `graph_distance`.
- File-API-`impact` mit `target_graph_status=partial`: Reverse-BFS
  laeuft ueber die vorhandenen Kanten, `impact_target_depth_effective`
  entspricht der im Teilgraph tatsaechlich erreichten maximalen
  Distanz, und die Diagnostic
  `"target graph partially loaded; impact prioritisation uses available edges only"`
  ist gesetzt.
- File-API-`impact` mit `--impact-target-depth 0`: keine Reverse-BFS,
  nur Impact-Seed-Targets; `impact_target_depth_effective=0`.
- File-API-`impact` mit `--impact-target-depth 1`: ein
  Reverse-BFS-Schritt, Targets mit `graph_distance=1` erscheinen mit
  `priority_class=direct_dependent`.
- File-API-`impact` mit Tiefe groesser als der erreichbare Maximum:
  BFS endet vorzeitig, `impact_target_depth_effective <
  impact_target_depth_requested`, Diagnostic `"reverse target graph
  traversal stopped at depth N ..."` ist gesetzt.
- File-API-`impact` mit Zyklus innerhalb der Tiefenbegrenzung:
  Reverse-BFS terminiert ueber die erreichte-Targets-Tabelle,
  kuerzeste Distanz gewinnt, keine Endlosschleife.
- File-API-`impact` mit Zyklus, der das Tiefenbudget erschoepft:
  Diagnostic `"reverse target graph traversal hit depth limit N within
  a cycle ..."` ist gesetzt.
- Mehrere Wege zum selben Target: kuerzere Distanz gewinnt; bei
  gleicher Distanz gewinnt staerkere Evidenz.
- `evidence_strength`-Klassifikation:
  - Owner mit nur direkten TUs: `direct`.
  - Owner mit nur heuristischen TUs: `heuristic`.
  - Owner ohne TUs (Header-Only-Library-Fall): `uncertain`.
  - Reverse-BFS-Hop von `direct`-Owner: `direct`.
  - Reverse-BFS-Hop von `heuristic`-Owner: `heuristic`.
  - Reverse-BFS-Hop, wenn dasselbe Target via `direct` und `heuristic`-
    Pfaden in derselben Distanz erreicht wird: `direct` gewinnt.
- Externe Targets erscheinen NICHT in `prioritized_affected_targets`,
  weil sie keine Owner-Targets sind und Reverse-BFS sie nicht erreicht.
- Sortier-Test: ein Result mit Targets in allen drei `priority_class`-
  Stufen und allen drei `evidence_strength`-Stufen wird in der
  korrekten 6-Tupel-Reihenfolge sortiert.

CLI-Tests `tests/e2e/test_cli.cpp`:

- `--impact-target-depth 0` wird akzeptiert; Result hat
  `impact_target_depth_effective=0`.
- `--impact-target-depth 32` wird akzeptiert.
- `--impact-target-depth 33` ergibt Exit-Code `2`, Fehlermeldung
  `"--impact-target-depth: value exceeds maximum 32"`.
- `--impact-target-depth -1` ergibt Exit-Code `2`, Fehlermeldung
  `"--impact-target-depth: negative value"`.
- `--impact-target-depth abc` ergibt Exit-Code `2`, Fehlermeldung
  `"--impact-target-depth: not an integer"`.
- `--impact-target-depth 1.5` ergibt Exit-Code `2`, Fehlermeldung
  `"--impact-target-depth: not an integer"`.
- Doppeltes `--impact-target-depth 2 --impact-target-depth 3` ergibt
  Exit-Code `2`, Fehlermeldung
  `"--impact-target-depth: option specified more than once"`.
- Ohne `--impact-target-depth` greift der Default `2`; Result hat
  `impact_target_depth_requested=2`.
- `--require-target-graph` mit File-API-Daten: laeuft normal.
- `--require-target-graph` ohne File-API-Daten (Compile-DB-only):
  Exit-Code `1`, Fehlermeldung
  `"--require-target-graph: target graph data is required but not available"`.
  Kein stdout-Report, keine Zieldatei. Test pinnt, dass dies das
  CLI-Mapping des Service-Codes `target_graph_required` ist und kein
  Parserfehler mit Exit-Code `2`.
- `--require-target-graph --impact-target-depth 0` mit File-API-Daten:
  laeuft, Result hat `impact_target_depth_effective=0`,
  `prioritized_affected_targets` enthaelt nur Impact-Seed-Targets.

Adapter-Tests:

- JSON-Adapter: `prioritized_affected_targets`-Array korrekt
  serialisiert; Pflichtfelder `priority_class`, `graph_distance`,
  `evidence_strength` und `target` immer gesetzt;
  `impact_target_depth_requested` und `impact_target_depth_effective`
  immer gesetzt.
- DOT-Adapter: Target-Knoten in Impact-DOT erhalten
  `priority_class`-, `graph_distance`- und `evidence_strength`-
  Attribute, sobald `target_graph_status != not_loaded`.
  `graph_impact_target_depth_requested` und
  `graph_impact_target_depth_effective` immer gesetzt; bei
  `not_loaded` bleibt `requested` der Result-Wert und `effective=0`.
  Ein Mapping-Test pinnt, dass die DOT-Graph-Attribute aus den
  Modellfeldern `impact_target_depth_requested` und
  `impact_target_depth_effective` stammen.
- HTML-Adapter: `Prioritised Affected Targets`-Section vorhanden mit
  Tabelle (loaded), Leersatz (loaded mit leerer Liste) oder
  not_loaded-Hinweis (status not_loaded); Tiefenangabe sichtbar.
- Console-Adapter: Suffix-Format
  `[<priority_class>, distance=<n>, evidence=<strength>]` byteweise
  gepinnt; bei `not_loaded` ist die Metadaten-/Fallback-Zeile mit
  `requested depth` und `effective depth: 0` sichtbar.
- Markdown-Adapter: Tabelle korrekt; Markdown-Tabellen-Escaping aus
  AP 1.2 wird angewendet; bei `not_loaded` bleibt die Section mit
  Tiefenangabe und Fallback-Absatz sichtbar.

E2E-/Golden-Tests:

- Goldens unter `tests/e2e/testdata/m6/<format>-reports/`:
  - `impact-prioritised-default.<ext>`: File-API-`impact`,
    Default-Tiefe `2`, alle drei `priority_class`-Stufen vertreten.
  - `impact-prioritised-depth-zero.<ext>`: `--impact-target-depth 0`,
    nur Impact-Seed-Targets.
  - `impact-prioritised-cycle.<ext>`: Reverse-BFS in zyklischem Graph,
    Diagnostic vorhanden.
  - `impact-prioritised-partial.<ext>`: File-API-`impact` mit
    `target_graph_status=partial`, vorhandene Teilgraph-Hops sichtbar,
    Partial-Diagnostic vorhanden.
  - `impact-not-loaded.<ext>`: Compile-DB-only, `not_loaded`-Pfad.
    Golden pinnt fuer alle fuenf Formate, dass `requested` sichtbar
    bleibt (Default `2`) und `effective=0` ist.
  - `impact-require-target-graph-error.txt`: Stderr-Output bei
    `--require-target-graph` ohne File-API.
- Manifeste werden um die neuen Goldens erweitert;
  `validate_doc_examples.py` prueft sie ebenfalls.

Schema-Tests:

- `kReportFormatVersion == 3` mit `report-json.schema.json::FormatVersion::const`.
- Negativtest: ein `format_version=2`-JSON validiert NICHT gegen das
  v3-Schema.
- Negativtest: ein `format_version=3`-Impact-JSON ohne
  `prioritized_affected_targets` validiert NICHT.
- Negativtest: ein `priority_class`-Wert ausserhalb
  `{direct, direct_dependent, transitive_dependent}` validiert NICHT.
- Boundary-Negativtests: `impact_target_depth_requested=-1`,
  `impact_target_depth_requested=33` und
  `impact_target_depth_effective=-1` validieren NICHT gegen das
  v3-Schema.
- Invariant-Negativtest: ein Impact-JSON mit
  `impact_target_depth_effective > impact_target_depth_requested`
  validiert NICHT bzw. wird, falls die verwendete JSON-Schema-Dialekt-
  Unterstuetzung keine felduebergreifende `<=`-Constraint ausdruecken
  kann, durch einen expliziten Schema-Companion-Test als ungueltig
  gepinnt.
- Boundary-/Invariant-Negativtest fuer Targets: ein
  `prioritized_affected_targets[]`-Eintrag mit `graph_distance=33` oder
  `graph_distance > impact_target_depth_requested` validiert NICHT
  bzw. wird bei fehlender felduebergreifender Schema-Unterstuetzung
  durch einen Schema-Companion-Test als ungueltig gepinnt.

Regressionstests:

- M5-`affected_targets`, `directly_affected_targets` und
  `heuristically_affected_targets` bleiben in v3-Output erhalten und
  zeigen dasselbe Verhalten wie in v2.
- Compile-DB-only-Console- und Markdown-Goldens bleiben in den
  bestehenden M5-Sections byte-stabil gegenueber v2. Ab v3 enthalten
  sie zusaetzlich die neue `Prioritised Affected Targets`-
  Fallback-Section mit Tiefen-Metadaten sowie den neuen Diagnostic-
  Eintrag (siehe Kompatibilitaets-Fallback oben).

## Rueckwaertskompatibilitaet

Pflicht:

- Bestehende M5-Felder im Impact-JSON bleiben erhalten.
- Bestehende M5-Console- und Markdown-Sections bleiben erhalten;
  AP 1.3 fuegt nur neue Sections hinzu.
- AP-1.2-Felder (`target_graph`, `target_hubs`, `target_graph_status`)
  bleiben unveraendert.

Verboten:

- Stille Verhaltensaenderung an `affected_targets` (M5) oder
  `directly_affected_targets`/`heuristically_affected_targets` (M5).
  Beide bleiben byte-identisch zu v2-Output.
- Migrations-Banner oder `deprecated`-Felder. Versionssprung steht
  ausschliesslich in `format_version=3` und Schema.

## Implementierungsreihenfolge

Innerhalb von **A.1 (Modelle und Service-Logik)**:

1. Neue Enums `TargetPriorityClass` und `TargetEvidenceStrength` in
   `impact_result.h`.
2. `PrioritizedImpactedTarget` und Erweiterung von `ImpactResult`.
3. `AnalyzeImpactRequest` um `impact_target_depth` und
   `require_target_graph` erweitern.
4. Service-Eingabevalidierung fuer `impact_target_depth` und
   `require_target_graph`.
5. Service-Logik fuer Owner-Target-Sammlung mit
   `evidence_strength`-Klassifikation.
6. Reverse-BFS-Implementierung mit deterministischer Sortierung,
   erreichter-Targets-Tabelle, Zyklus-Erkennung, Same-Distance-
   Evidence-Merge und Tiefenbudget.
7. Diagnostics fuer Compile-DB-only-Fallback, vorzeitiges BFS-Ende und
   Budget-Kappung.
8. Service-Tests fuer Validierung, BFS- und Klassifikations-Faelle.

Innerhalb von **A.2 (CLI und Schema)**:

9. CLI-Optionen `--impact-target-depth` und `--require-target-graph`
   in `cli_adapter.cpp` mit den vier
   `--impact-target-depth`-Fehlerphrasen.
10. CLI-Validation und Composition-Root-Verdrahtung in `main.cpp`;
    Service-Validierungsfehler auf CLI-Exit-Codes mappen.
11. CLI-Tests fuer alle Fehlerphrasen, Service-Error-Mappings und
    Erfolgspfade.
12. `report-json.md` als v3-Zielvertrag vorbereiten. Aenderungen an
    `report-json.schema.json` bleiben bis A.4 entweder aus oder auf
    nicht-produktive Draft-/Testpfade beschraenkt; insbesondere bleibt
    `FormatVersion.const` bis zur Adapter-Vollstaendigkeit auf `2`.

Innerhalb von **A.3 (JSON- und DOT-Adapter)**:

13. JSON-Adapter implementiert v3-faehige Ausgabe.
14. DOT-Adapter implementiert v3-faehige Ausgabe mit neuen Knoten- und
    Graph-Attributen.
15. `report-dot.md` auf v3.
16. Adapterlokale Tests fuer JSON und DOT erweitern; globale
    `format_version=3`-Goldens werden erst nach A.4 aktiviert.

Innerhalb von **A.4 (HTML-, Markdown- und Console-Adapter)**:

17. HTML-Adapter implementiert v3-Output mit
    `Prioritised Affected Targets`-Section.
18. Markdown-Adapter implementiert v3-Output.
19. Console-Adapter implementiert v3-Output.
20. `report-html.md` auf v3.
21. `kReportFormatVersion` auf `3` heben und
    `report-json.schema.json::FormatVersion::const` auf `3` setzen.
22. Goldens fuer alle fuenf Formate in `m5/` und `m6/`
    migrieren/erweitern.

Innerhalb von **A.5 (Audit-Pass)**:

23. CLI-E2E-Tests durchlaufen lassen, alle Goldens byte-stabil.
24. Docker-Gates ausfuehren.
25. Liefer-Stand-Block in diesem Dokument und in `docs/plan-M6.md`
    aktualisieren.

## Tranchen-Schnitt

AP 1.3 wird in fuenf Sub-Tranchen geliefert:

- **A.1 Modelle und Service-Logik (atomar)**: Neue Modelle,
  `AnalyzeImpactRequest`-Erweiterung, Reverse-BFS,
  `evidence_strength`-Klassifikation, Service-Eingabevalidierung und
  Service-Tests. Adapter werden noch nicht angepasst; bestehende
  Adapter sehen die neuen Felder einfach noch nicht und produzieren
  weiterhin v2-Output.
- **A.2 CLI und Schema-Zielvertrag**: CLI-Optionen, CLI-Mapping der
  Service-Validierungsfehler, Doc-Vorbereitung auf v3 und hoechstens
  nicht-produktive Schema-Drafts. `kReportFormatVersion` und die
  produktive `report-json.schema.json::FormatVersion::const` bleiben
  in dieser Tranche auf `2`; es gibt noch keine globalen v3-Goldens.
- **A.3 JSON- und DOT-Adapter**: JSON- und DOT-Adapter implementieren
  v3-faehige Ausgabe mit den neuen Feldern. Tests bleiben adapterlokal,
  solange nicht alle Formate v3-faehig sind.
- **A.4 HTML-, Markdown- und Console-Adapter plus Version-Aktivierung**:
  Drei Adapter, `kReportFormatVersion=3`, Schema-Const `3` und globale
  v3-Goldens fuer alle fuenf Formate.
- **A.5 Audit-Pass**: Plan-Test-Liste verifizieren, Docker-Gates,
  Liefer-Stand.

Begruendung der atomaren A.1-Tranche: Service-Logik ohne CLI- und
Adapter-Anbindung ist intern testbar (`ImpactAnalyzer` mit
synthetischem Request) und liefert die Modellfelder, die A.2-A.4
benoetigen. A.2 muss CLI und Schema-Zielvertrag buendeln, weil
CLI-Werte aus `[0,32]` und Service-Validierungsfehler mit dem
Schema-Range zusammenpassen muessen. Der globale Versionssprung bleibt
bis A.4 gesperrt, damit kein produktiver Report `format_version=3`
ausweist, bevor alle Adapter die v3-Pflichtausgabe liefern. A.3 und
A.4 koennen parallel entwickelt werden, solange dieser Tranche-
Invariant eingehalten wird.

## Liefer-Stand

- A.1 (Modelle und Service-Logik): `92c719a` feat: add M6 AP 1.3 A.1
  reverse-target-graph traversal models and service logic. Fuehrt die
  AP-1.3-Modelle (`TargetPriorityClass`, `TargetEvidenceStrength`,
  `PrioritizedImpactedTarget`, `ServiceValidationError`), die
  `AnalyzeImpactRequest`-Erweiterung um `impact_target_depth` und
  `require_target_graph`, den
  `target_graph_traversal`-Reverse-BFS-Helfer und die
  Service-Eingabevalidierung im `ImpactAnalyzer` ein. v2-Reports
  bleiben byte-stabil; die vier AP-1.3-Diagnostics werden bewusst
  zurueckgehalten.
- A.2 (CLI und Schema-Zielvertrag): `535194c` feat: add M6 AP 1.3 A.2
  --impact-target-depth + --require-target-graph CLI options. CLI
  validiert die Tiefe mit den vier dokumentierten Fehlerphrasen,
  mappt `target_graph_required` auf Exit-Code 1 mit klarer
  Fehlermeldung, und reicht die Werte unveraendert in den
  `AnalyzeImpactRequest` durch. `kReportFormatVersion` bleibt auf 2.
- A.3 (JSON- und DOT-Adapter): `dfc3cd5` feat: add M6 AP 1.3 A.3
  JSON+DOT v3 groundwork (priority text mappings, dormant DOT wiring).
  Fuehrt `src/adapters/output/impact_priority_text.h` als
  format-uebergreifende Stringquelle ein, ruestet den DOT-Adapter
  intern um und reserviert die JSON-Slots; produktive Reports bleiben
  v2 dank `if constexpr (kReportFormatVersion >= 3)`.
- A.4 (HTML/Markdown/Console v3 + globale Aktivierung): `87f3a25`
  feat: add M6 AP 1.3 A.4 v3 activation across all five report
  formats. `kReportFormatVersion` springt auf 3, die Schema-Const in
  `report-json.schema.json` zieht nach, alle fuenf Adapter rendern
  die priorisierte Sicht inklusive der Tiefen-Metadaten und der vier
  AP-1.3-Diagnostics. M3/M4/M5/M6-Goldens und `docs/examples/`
  inhaltlich auf v3 migriert; `make docker-binary` als neuer
  Makefile-Target hilft beim Goldens-Generieren ohne lokale Toolchain.
- A.5 (Audit-Pass + Liefer-Stand pinnen): dieser Commit (`docs: pin
  M6 AP 1.3 Liefer-Stand with A.1-A.5 commit hashes`); identifizierbar
  ueber `git log -- docs/plan-M6-1-3.md`. Audit-Befunde:
  Reverse-BFS-Determinismus, evidence-strength-Klassifikation,
  Tiefenbudget-Schnitte, Zyklen-Behandlung und externe Targets sind
  in `tests/hexagon/test_target_graph_traversal.cpp` und
  `tests/hexagon/test_impact_analyzer.cpp` abgedeckt; CLI-Vier-Phrasen-
  Fehlervertrag und `--require-target-graph`-Exit-Mapping sind in
  `tests/e2e/test_cli.cpp` gepinnt; alle fuenf Reportformate liefern
  `prioritized_affected_targets`, `impact_target_depth_requested` und
  `impact_target_depth_effective`, JSON-Schema-Const und C++-Konstante
  stimmen ueberein, die existierende M5-Felder bleiben byte-identisch
  in v3.

Docker-Gate-Lauf gemaess `docs/quality.md` auf dem A.4-Commit lokal
gruen: `make docker-gates` (Test 35/35, Coverage 100%, clang-tidy 0,
lizard 0).

## Abnahmekriterien

AP 1.3 ist abgeschlossen, wenn:

- `kReportFormatVersion == 3` in C++ und Schema;
- `prioritized_affected_targets`, `impact_target_depth_requested` und
  `impact_target_depth_effective` in allen fuenf Reportformaten
  ausgegeben werden, sobald das jeweilige Format das Modell
  unterstuetzt;
- die Reverse-BFS deterministisch ist (gleiche Eingabe → gleiche
  Ausgabe, unabhaengig von Container-Iteration), zyklensicher
  (erreichte-Targets-Tabelle greift) und budget-respektierend
  (`impact_target_depth_effective <= impact_target_depth_requested`);
- der `ImpactAnalyzer` `AnalyzeImpactRequest` unabhaengig von der CLI
  validiert und fuer ungueltige Tiefe bzw. fehlenden erforderlichen
  Target-Graphen klare Service-Fehlercodes ohne partielles
  `ImpactResult` liefert;
- `--impact-target-depth <n>` mit vier klaren Fehlerphrasen
  (`negative value`, `not an integer`, `value exceeds maximum 32`,
  `option specified more than once`) und Default `2`, Range `[0, 32]`
  validiert;
- `--require-target-graph` mit fehlendem Target-Graph zu Exit-Code `1`,
  klarer Fehlermeldung und keinem Reportoutput fuehrt; Exit-Code `1`
  ist das bewusste CLI-Mapping des Service-Eingabefehlers
  `target_graph_required`, nicht ein Parserfehler;
- der Compile-DB-only-Fallback ohne `--require-target-graph`
  M5-kompatibles Verhalten plus eine klare Diagnostic liefert;
- `target_graph_status=partial` als nutzbarer Teilgraph behandelt wird,
  Reverse-BFS ueber vorhandene Kanten ausfuehrt, `effective` aus der
  tatsaechlich erreichten Tiefe berechnet und eine klare
  Partial-Diagnostic liefert;
- `evidence_strength`-Klassifikation `direct`, `heuristic`, `uncertain`
  in Service-Tests fuer Impact-Seed-Targets und Reverse-BFS-Hops gepinnt
  ist;
- Mehrweg-Targets die kuerzere Distanz und staerkere Evidenz
  korrekt anwenden;
- Externe Targets nicht in `prioritized_affected_targets` erscheinen;
- bestehende M5-Felder (`affected_targets`,
  `directly_affected_targets`, `heuristically_affected_targets`)
  byte-identisch zu v2-Output bleiben;
- alle Goldens unter `tests/e2e/testdata/m5/` und `m6/` v3-konform
  sind;
- der Docker-Gate-Lauf gemaess `docs/quality.md` (Tranchen-Gate fuer
  M6 AP 1.3) gruen ist;
- `git diff --check` sauber ist.

## Offene Folgearbeiten

Diese Punkte werden bewusst in spaeteren M6-Arbeitspaketen umgesetzt:

- CLI-Steuerung der Hub- und Priorisierungs-Schwellenwerte (AP 1.5).
  AP 1.5 darf den `target_hubs.thresholds`-Block aus AP 1.2 nutzen
  und ergaenzt voraussichtlich keine neuen Pflichtfelder im
  Impact-JSON.
- Compare-Vertrag fuer `format_version=3`-Reports mit
  Compare-Kompatibilitaetsmatrix (AP 1.6). AP 1.6 ist `analyze`-only;
  Impact-Compare wuerde die `prioritized_affected_targets`-Sicht
  betreffen, ist aber explizit kein Bestandteil von M6.
- Weitere Evidenz-Stufen oder Provenienz-Felder pro Target. M6
  verwendet bewusst die drei festen Werte; eine Erweiterung auf
  `weighted_evidence` oder `provenance_path` braeuchte einen eigenen
  Vertrag und Versionssprung.
- Deprecation der alten M5-Felder `affected_targets` etc. M6 behaelt
  sie. Eine spaetere Deprecation-Welle waere ein eigenes M7- oder
  M8-Thema.
