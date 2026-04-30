# Plan M6 - AP 1.2 Target-Graph-Ausgaben und Hub-Hervorhebung umsetzen

## Ziel

AP 1.1 hat den Target-Graphen, die Hub-Klassifikation und die zugehoerigen
Diagnostics in `AnalysisResult` und `ImpactResult` gefuellt, ohne sie in
Reports sichtbar zu machen. AP 1.2 macht diese Sicht in allen fuenf
M5-Reportformaten nutzbar: Console, Markdown, HTML, JSON und DOT. Damit
erfuellt M6 die Lastenheft-Kennung `F-18` (textuelle Darstellung direkter
Target-Abhaengigkeiten) und `F-20` (Hub-Hervorhebung).

Nach AP 1.2 koennen Nutzer ueber `cmake-xray analyze --format <format>`
direkte Target-Abhaengigkeiten und Target-Hubs nachvollziehen, sobald
File-API-Daten geladen wurden. `cmake-xray impact` liefert die gleiche
Target-Graph-Sicht als Lesedaten, wendet sie aber noch nicht fuer
Priorisierung an; das uebernimmt AP 1.3.

AP 1.2 versioniert den maschinenlesbaren JSON-Output explizit:
`xray::hexagon::model::kReportFormatVersion` steigt in diesem AP von `1`
auf `2`. Alle Format-Vertrags-Dokumente (`docs/report-json.md`,
`docs/report-json.schema.json`, `docs/report-dot.md`, `docs/report-html.md`)
werden synchron aktualisiert.

## Scope

Umsetzen:

- Console- und Markdown-Adapter erhalten im `analyze`-Pfad einen neuen
  Abschnitt fuer direkte Target-Abhaengigkeiten (`from -> to`) und einen
  Abschnitt fuer Target-Hubs. Im `impact`-Pfad gibt es nur einen Abschnitt
  "Target Graph Reference" als Lesedaten-Sicht; Target Hubs sind im Impact-
  Pfad ausdruecklich nicht vorgesehen, analog zum JSON-Vertrag (siehe
  unten).
- HTML-Adapter erhaelt eine neue Section `Target Graph` (Knoten- und
  Kantentabellen) und `Target Hubs` (eingehende und ausgehende Hubs).
- JSON-Adapter erhaelt neue Pflichtfelder `target_graph_status`,
  `target_graph` (mit `nodes` und `edges`) und `target_hubs` (mit `inbound`
  und `outbound`); `format_version` steigt auf `2`.
- DOT-Adapter fuer `analyze` erweitert seine Knoten- und Kantenmenge um
  Target-Knoten ueber den vollstaendigen Target-Graphen und um eine neue
  `target_dependency`-Kantenart.
- DOT-Adapter fuer `impact` propagiert Target-Knoten und
  `target_dependency`-Kanten in den `impact`-Graphen, ohne Impact-
  Priorisierung anzuwenden (das gehoert zu AP 1.3 und beeinflusst die
  Knoten-/Kantenattribute, nicht die hier eingefuehrten Strukturen).
- Disambiguierung kollidierender `display_name`-Werte: Console, Markdown
  und HTML zeigen `<display_name> [key: <unique_key>]` nur dann, wenn der
  `display_name` in der jeweiligen Liste nicht eindeutig ist. JSON und DOT
  geben weiterhin `unique_key` und `display_name` parallel aus.
- Einheitlicher Empty-Section-Vertrag pro Format-Familie bei
  `target_graph_status=not_loaded` (siehe eigenen Abschnitt unten).
- Aktualisierung der Format-Vertrags-Dokumente.
- Aktualisierung von Goldens fuer Console, Markdown, HTML, JSON und DOT.
- Schema-Validierung fuer den neuen `format_version=2`-JSON-Vertrag.
- Aktualisierung des `report-json.schema.json`-Schemas, sodass v2 das
  geschlossene Schema bleibt (`additionalProperties: false`).

Nicht umsetzen:

- Impact-Priorisierung ueber den Target-Graphen (gehoert zu AP 1.3).
- CLI-Steuerung fuer Hub-Schwellenwerte und Analyseauswahl
  (`--target-hub-in-threshold`, `--target-hub-out-threshold`, `--analysis`)
  (gehoert zu AP 1.5).
- Compare-Vertrag fuer `format_version=2`-JSON-Reports (gehoert zu AP 1.6,
  inklusive Compare-Kompatibilitaetsmatrix).
- Backwards-Kompatibilitaets-Shim fuer `format_version=1`-Konsumenten;
  AP 1.2 ist ein klarer Versionssprung, kein Dual-Output.

Diese Punkte folgen in spaeteren M6-Arbeitspaketen.

## Voraussetzungen aus AP 1.1

AP 1.2 baut auf folgenden AP-1.1-Vertraegen auf:

- `AnalysisResult::target_graph`, `AnalysisResult::target_graph_status`,
  `AnalysisResult::target_hubs_in` und `AnalysisResult::target_hubs_out`
  sind gesetzt.
- `ImpactResult::target_graph` und `ImpactResult::target_graph_status` sind
  gesetzt.
- `TargetGraph::nodes` und `TargetGraph::edges` sind durch
  `sort_target_graph` normalisiert (sortiert und dedupliziert).
- `TargetDependency::to_unique_key` verwendet bei `external` den Praefix
  `<external>::<raw_id>`; AP 1.2 nimmt diesen Praefix in JSON und DOT
  unveraendert auf.
- Compile-Database-only-Pfade liefern leeren `target_graph` und
  `target_graph_status=not_loaded`. AP 1.2 hat fuer diesen Fall einen
  expliziten Leersatz-/Leerabschnitt-Vertrag pro Format.

Falls AP 1.1 nicht vollstaendig geliefert ist (insbesondere `<external>::`-
Praefix-Vertrag, `target_graph_status`-Defaults und `target_hubs_*`-
Berechnung), ist AP 1.2 nicht abnahmefaehig.

## Dateien

Voraussichtlich zu aendern:

- `src/hexagon/model/report_format_version.h`
- `src/adapters/output/console_report_adapter.{h,cpp}`
- `src/adapters/output/markdown_report_adapter.{h,cpp}`
- `src/adapters/output/html_report_adapter.{h,cpp}`
- `src/adapters/output/json_report_adapter.{h,cpp}`
- `src/adapters/output/dot_report_adapter.{h,cpp}`
- `docs/report-json.md`
- `docs/report-json.schema.json`
- `docs/report-dot.md`
- `docs/report-html.md`
- `tests/adapters/test_console_report_adapter.cpp`
- `tests/adapters/test_markdown_report_adapter.cpp`
- `tests/adapters/test_html_report_adapter.cpp`
- `tests/adapters/test_json_report_adapter.cpp`
- `tests/adapters/test_dot_report_adapter.cpp`
- `tests/e2e/testdata/m5/json-reports/manifest.txt`
- `tests/e2e/testdata/m5/dot-reports/manifest.txt`
- `tests/e2e/testdata/m5/html-reports/manifest.txt`
- `docs/examples/manifest.txt`
- `docs/examples/generation-spec.json`

Neue Dateien:

- `tests/e2e/testdata/m6/json-reports/` mit M6-Goldens fuer Analyze und
  Impact, inklusive Target-Graph-Daten und leerer Target-Graph-Faelle
- `tests/e2e/testdata/m6/dot-reports/` mit M6-DOT-Goldens
- `tests/e2e/testdata/m6/html-reports/` mit M6-HTML-Goldens
- `tests/e2e/testdata/m6/markdown-reports/` mit M6-Markdown-Goldens
- `tests/e2e/testdata/m6/console-reports/` mit M6-Console-Goldens
- `docs/examples/analyze-target-graph.{txt,md,html,json,dot}` und
  `docs/examples/impact-target-graph.{txt,md,html,json,dot}` mit
  repraesentativen M6-Beispielen

Hinweis zur Verzeichnisstruktur: Goldens leben unter `m6/` parallel zu
`m5/`, damit AP 1.7 (Doku- und Abnahme) keine Drift zwischen Pre-M6- und
Post-M6-Beispielen riskiert. Der `validate_doc_examples.py`-Generator-
Spec wird um die neuen `m6/`-Pfade erweitert.

## Format-Versionierung

`xray::hexagon::model::kReportFormatVersion` steigt in diesem AP von `1`
auf `2`. Begruendung:

- JSON erhaelt neue Pflichtfelder (`target_graph_status`, `target_graph`,
  `target_hubs`). Der M5-AP-1.1-Vertrag legt fest:
  > Hinzufuegen eines neuen Pflichtfelds erhoeht `format_version`, weil
  > bestehende Konsumenten das Feld noch nicht erwarten.
- HTML-Geruest erhaelt neue Pflichtsections und ein veraendertes
  `meta xray-report-format-version`-Attribut.
- DOT erhaelt eine neue Knotenart (alle Targets aus `TargetGraph::nodes`,
  nicht nur die direkt aus M4-`target_assignments` ableitbaren) und eine
  neue Kantenart (`target_dependency`). Der M5-AP-1.3-Vertrag legt fest:
  > `format_version` wird erhoeht bei Hinzufuegen einer neuen Knoten- oder
  > Kantenart.
- Console und Markdown haben keine `format_version`-Identitaet, aber ihre
  Goldens aendern sich; der gemeinsame Versionssprung haelt alle Formate
  synchron.

`format_version=1`-Output wird ab AP 1.2 nicht mehr produziert. Es gibt
keinen CLI-Schalter, um das alte Format zu erzwingen. Konsumenten, die auf
`format_version=1` festsitzen, muessen mit der Compare-Kompatibilitaets-
matrix aus AP 1.6 arbeiten oder ihre Werkzeuge auf `format_version=2`
heben.

Begleitende Schema-Aenderung: `report-json.schema.json` setzt
`FormatVersion.const` auf `2`. Schema-Tests pinnen den Wert gegen die
C++-Konstante; ein abweichender Wert laesst den Schema-Gate-Test
fehlschlagen.

## Empty-Section-Vertrag bei `target_graph_status=not_loaded`

AP 1.2 vereinheitlicht das Verhalten der fuenf Reportformate, wenn keine
Target-Graph-Daten geladen wurden. Die Strategie unterscheidet sich
bewusst entlang der Format-Familie:

| Format | `Target Graph`-Sektion bei `not_loaded` | `Target Hubs`-Sektion bei `not_loaded` (nur Analyze) |
|---|---|---|
| Console | Komplett weggelassen, kein Header | Komplett weggelassen, kein Header |
| Markdown | Komplett weggelassen, kein Heading | Komplett weggelassen, kein Heading |
| HTML | Section mit `h2` bleibt; Inhalt ist Absatz `Target graph not loaded.` | Section mit `h2` bleibt; Inhalt ist Absatz `Target hubs not available.` |
| JSON | Pflichtfelder `target_graph_status`, `target_graph` mit `nodes=[]`, `edges=[]` (Analyze und Impact) | Nur Analyze-JSON: Pflichtfeld `target_hubs` mit `inbound=[]`, `outbound=[]`, `thresholds`-Objekt mit Defaults. Impact-JSON kennt das Feld gar nicht (Schema verbietet es ueber `additionalProperties: false`). |
| DOT | `graph_target_graph_status="not_loaded"`-Attribut bleibt; keine Target-Knoten, keine `target_dependency`-Kanten, kein synthetischer `external_target` | (entfaellt: DOT hat keine Hub-Sicht in AP 1.2) |

Begruendung der Asymmetrie:

- **HTML, JSON und DOT** sind strukturierte Formate. Schema-Konsumenten,
  CSS-Layouts und Graphviz-Tooling profitieren davon, dass die Section
  bzw. das Pflichtfeld immer existiert; Datennichtvorhanden wird ueber
  Status-/Leersatz-Inhalt explizit gemacht.
- **Console und Markdown** sind narrative Text-Reports. Ein leerer
  Abschnitt mit "No data."-Lippenbekenntnis liest sich als Lärm und
  signalisiert nicht den Unterschied zu "kein Datenpfad konfiguriert".
  Stattdessen sagt die bereits vorhandene Summary-Zeile bei beiden
  Formaten `Target metadata: not_loaded`, was den Status klar macht.

HTML-Sonderfall: Beide Sections behalten Header und Leersatz, analog zur
M5-Section `Target Metadata`, die bei `target_metadata=not_loaded`
ebenfalls einen Leersatz `No target metadata loaded.` zeigt statt
weggelassen zu werden. Die HTML-Konvention "Section bleibt mit Leersatz"
ist damit M5-erbe und gilt fuer alle M6-HTML-Sections symmetrisch.

Die Empty-Section-Regeln in den nachfolgenden formatspezifischen
Vertragsregeln verweisen auf diesen zentralen Vertrag und duerfen ihn
nicht widerspruechlich erweitern.

## JSON-Vertragsregeln

Neue Felder im JSON-Output, gemeinsam fuer Analyze und Impact:

```json
{
  "target_graph_status": "not_loaded",
  "target_graph": {
    "nodes": [],
    "edges": []
  }
}
```

Felder im Detail:

| Feld | Typ | Pflicht | Werte / Nullability |
| --- | --- | --- | --- |
| `target_graph_status` | string | ja | `not_loaded`, `loaded`, `partial`. |
| `target_graph` | object | ja | Container mit `nodes` und `edges`. Bei `not_loaded` beide leer. |
| `target_graph.nodes` | array | ja | `TargetNode`-Objekte, sortiert nach `(unique_key, display_name, type)` gemaess M6-Hauptplan-Sortiervertrag. |
| `target_graph.edges` | array | ja | `TargetEdge`-Objekte, sortiert nach `(from_unique_key, to_unique_key, kind, from_display_name, to_display_name)` gemaess M6-Hauptplan-Sortiervertrag. |

`TargetNode`-Schema:

| Feld | Typ | Pflicht | Beschreibung |
| --- | --- | --- | --- |
| `display_name` | string | ja | `TargetInfo::display_name`. |
| `type` | string | ja | `TargetInfo::type`. |
| `unique_key` | string | ja | `TargetInfo::unique_key`. |

`TargetEdge`-Schema:

| Feld | Typ | Pflicht | Beschreibung |
| --- | --- | --- | --- |
| `from_display_name` | string | ja | Anzeigename des konsumierenden Targets. |
| `from_unique_key` | string | ja | Stabiler Schluessel des konsumierenden Targets. |
| `to_display_name` | string | ja | Anzeigename des Ziel-Targets oder `raw_id` bei `external`. |
| `to_unique_key` | string | ja | Stabiler Schluessel; `<external>::<raw_id>` bei `external`. |
| `kind` | string | ja | `direct`. |
| `resolution` | string | ja | `resolved` oder `external`. |

Felder fuer Hubs (nur `analyze`-JSON):

```json
{
  "target_hubs": {
    "inbound": [],
    "outbound": [],
    "thresholds": {
      "in_threshold": 10,
      "out_threshold": 10
    }
  }
}
```

| Feld | Typ | Pflicht | Beschreibung |
| --- | --- | --- | --- |
| `target_hubs` | object | ja | Pflichtcontainer im Analyze-Output. |
| `target_hubs.inbound` | array | ja | `TargetNode`-Objekte mit `in_count >= in_threshold`, sortiert nach `(unique_key, display_name, type)`. |
| `target_hubs.outbound` | array | ja | `TargetNode`-Objekte mit `out_count >= out_threshold`, sortiert nach `(unique_key, display_name, type)`. |
| `target_hubs.thresholds.in_threshold` | integer | ja | Wirksamer Schwellenwert; in AP 1.2 hardcoded `10`. |
| `target_hubs.thresholds.out_threshold` | integer | ja | Wirksamer Schwellenwert; in AP 1.2 hardcoded `10`. |

`target_hubs` ist im Impact-JSON bewusst nicht enthalten. AP 1.2 fokussiert
Impact-JSON auf den reinen Target-Graphen ohne Hub-Sicht; AP 1.3 entscheidet
ueber Hub-Sichtbarkeit im Impact-Kontext zusammen mit der Priorisierung.

**Schema-Erzwingung der report_type-Asymmetrie**: Das bestehende
`oneOf`-Konstrukt in `report-json.schema.json` (siehe M5-Schema mit
`AnalysisReport` vs. `ImpactReport`) wird in AP 1.2 so erweitert:

- `AnalysisReport.required` enthaelt `target_graph_status`,
  `target_graph` und `target_hubs`. `AnalysisReport.properties` listet
  alle drei Felder; `additionalProperties: false` bleibt aktiv.
- `ImpactReport.required` enthaelt `target_graph_status` und
  `target_graph`, aber NICHT `target_hubs`. `ImpactReport.properties`
  listet `target_hubs` ueberhaupt nicht; in Kombination mit dem bereits
  aktiven `additionalProperties: false` lehnt das Schema `target_hubs`
  im Impact-JSON hart ab.
- Damit unterscheidet das Schema strict zwischen analyze und impact;
  eine fehlerhafte Adapter-Implementierung, die `target_hubs` im
  Impact-Output serialisieren wuerde, schlaegt am Schema-Validations-
  Gate `json_schema_check` sofort fehl.

Schema-Negativtests in A.1 fuer die Asymmetrie:

- Ein synthetisches Impact-JSON mit `target_hubs`-Feld muss am Schema
  fehlschlagen.
- Ein synthetisches Analyze-JSON ohne `target_hubs`-Feld muss am Schema
  fehlschlagen.
- Beide Tests laufen im selben CTest-Gate wie die Goldens-Validation
  und sind Pflichtbestandteil der A.1-Abnahme.

Feldreihenfolge im Analyze-JSON (`format_version=2`):

1. `format`
2. `format_version`
3. `report_type`
4. `inputs`
5. `summary`
6. `translation_unit_ranking`
7. `include_hotspots`
8. `target_graph_status`
9. `target_graph`
10. `target_hubs`
11. `diagnostics`

Feldreihenfolge im Impact-JSON (`format_version=2`):

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
12. `diagnostics`

Wichtig:

- `target_hubs.thresholds` wird in `format_version=2` als Objekt
  serialisiert, weil AP 1.5 voraussichtlich CLI-konfigurierbare Werte
  einfuehrt und das Schema nicht erneut brechen soll. AP 1.2 setzt feste
  Werte `10`/`10` ueber die `kDefaultTargetHub*Threshold`-Konstanten aus
  AP 1.1.
- `external::*`-Targets erscheinen NICHT in `target_graph.nodes`, weil sie
  keine echten File-API-Targets sind. Sie erscheinen aber als
  `to_unique_key="<external>::<raw_id>"` in `target_graph.edges`.
- `target_graph_status="not_loaded"` impliziert leeren `target_graph` und
  leere `target_hubs`-Listen, aber das Schema verlangt die Felder
  trotzdem (nicht weglassen).

## DOT-Vertragsregeln

Neue Knotenregel fuer `analyze`-DOT:

- Zusaetzlich zu den bisherigen Knoten (Translation-Units, Include-Hotspots,
  Target-Knoten aus M4-Zuordnungen) werden alle Knoten aus
  `AnalysisResult::target_graph.nodes` als Target-Knoten ausgegeben, sofern
  das `node_limit` das zulaesst.
- Dedup auf Target-`unique_key`: ein Target, das bereits als
  M4-Zuordnungs-Target im Graph steht, bekommt keinen zweiten Knoten.
- `external::*`-Ziele werden NICHT als eigene Target-Knoten ausgegeben.
  Eine `target_dependency`-Kante mit `external` wird mit der
  `external_target_id` als zusaetzliches Edge-Attribut serialisiert (siehe
  unten).

Neue Kantenart `target_dependency` fuer Analyze und Impact:

- `kind="target_dependency"`.
- `style="solid"` fuer `resolution=resolved`, `style="dashed"` fuer
  `resolution=external`.
- Pflichtattribute fuer `target_dependency`-Kanten:
  - `kind` (immer `target_dependency`).
  - `style` (`solid` oder `dashed`).
  - `resolution` (`resolved` oder `external`).
- Optionales Attribut bei `resolution=external`:
  - `external_target_id`: rohe `raw_id`-Zeichenkette ohne `<external>::`-
    Praefix. Fuer `resolution=resolved` entfaellt das Attribut.
- Quelle und Ziel sind die jeweiligen DOT-Node-IDs der File-API-Targets.
  Bei `resolution=external` wird das Ziel ueber einen synthetischen
  Knoten der Form `external_<index>` mit Pflichtattributen
  `kind="external_target"`, `label=<raw_id>` und
  `external_target_id=<raw_id>` erzeugt; AP 1.2 verbietet das Erfinden
  eines `unique_key` fuer externe Ziele im DOT-Knotengraphen.
- **`raw_id`-Escaping und Truncation**: Sowohl `label` als auch
  `external_target_id` folgen den bestehenden M5-DOT-Vertragsregeln aus
  `docs/report-dot.md`. Konkret:
  - **Stringescaping** (gilt fuer beide Attribute): Backslash zu `\\`,
    Anfuehrungszeichen zu `\"`, Newline zu `\n`, Carriage Return zu `\r`,
    Tab zu `\t`. Sonstige ASCII-Steuerzeichen unter `0x20` werden
    deterministisch durch `\xHH`-Sequenzen ersetzt. UTF-8-Bytes ueber
    `0x7F` bleiben rohe Bytes; Graphviz akzeptiert UTF-8 in quoted
    Strings. AP 1.2 fuegt keine eigenen Escaping-Regeln hinzu.
  - **Label-Truncation** (gilt nur fuer `label`): Wenn `raw_id` mehr als
    48 Zeichen hat, wird die Middle-Truncation aus dem M5-DOT-Vertrag
    angewendet (22 Zeichen + `...` + 23 Zeichen, ASCII-`...`).
    `external_target_id` bleibt **unverkuerzt** und enthaelt den
    vollstaendigen `raw_id`, analog zum M5-Vertrag fuer `unique_key`-
    Attribute.
  - **Begruendung**: `raw_id` kann beliebige UTF-8-Zeichen enthalten,
    weil CMake-File-API-IDs nicht auf das CMake-Target-Namen-Vokabular
    eingeschraenkt sind (z. B. interne IMPORTED-Target-Hashes mit `@`,
    `#` oder Unicode-Pfadkomponenten). Ohne explizite Anbindung an den
    M5-Escaping-Vertrag wuerden DOT-Parser/-Renderer bei `"`, `\`,
    Newline oder Tab im `raw_id` brechen.
- **Dedup-Vertrag fuer `external_target`-Knoten (Geltungsbereich:
  Analyze UND Impact)**: Pro `to_unique_key` (= `<external>::<raw_id>`)
  wird genau EIN synthetischer Knoten erzeugt. Alle `target_dependency`-
  Kanten mit demselben `to_unique_key` referenzieren denselben
  `external_<index>`-Knoten als Ziel. Der Vertrag gilt sowohl fuer
  `analyze --format dot` als auch fuer `impact --format dot`; der
  DOT-Adapter implementiert beide Pfade ueber denselben
  Dedup-Helper, damit kein Pfad versehentlich pro Quelle einen eigenen
  externalen Knoten erzeugt. Der Dedup-Vertrag ist verbindlich, damit:
  - mehrere interne Targets gegen dieselbe externe Bibliothek nicht zu
    `n` separaten externalen Knoten fuehren,
  - das `node_limit` nicht durch redundante synthetische Knoten verbraucht
    wird,
  - die DOT-Ausgabe byte-stabil bleibt, auch wenn die Reihenfolge der
    Roh-Reply-Kanten variiert.
- ID-Vergabe fuer `external_<index>` (gilt fuer Analyze und Impact):
  Der Adapter sammelt zuerst die Menge aller `to_unique_key`-Werte mit
  `resolution=external` aus den finalen `target_dependency`-Kanten
  (also nach Anwendung des Kantenbudgets), sortiert sie nach
  `to_unique_key` aufsteigend und vergibt `external_1`, `external_2`,
  ... in dieser Sortierreihenfolge. Der Index ist damit reproduzierbar
  und unabhaengig von der Reply-Reihenfolge oder davon, welche interne
  Quelle die Kante zuerst produziert hat.
- **Mapping ist eine reine Funktion vom finalen `to_unique_key`-Satz**:
  Gegeben dieselbe sortierte Menge externer Ziele, vergibt der gemeinsame
  Dedup-Helper aus AP 1.2 dieselben `external_<index>`-IDs, unabhaengig
  davon, ob der Aufrufer `analyze --format dot` oder
  `impact --format dot` ist. Wenn Analyze- und Impact-DOT denselben
  externen Ziel-Satz rendern, sind die `external_<index>`-IDs in
  beiden Reports byte-identisch zugeordnet. Wenn die Saetze
  unterschiedlich sind (z. B. weil Impact-DOT einen kleineren
  Subgraphen rendert oder weil Budget-Druck unterschiedlich greift),
  unterscheiden sich die Mappings entsprechend ihrem Eingangs-Satz;
  der Index bleibt aber innerhalb eines Report-Aufrufs deterministisch
  und ueber identische Eingangsmengen reproduzierbar.
- Wenn das `node_limit` keine `external_target`-Knoten mehr zulaesst,
  werden zugehoerige `target_dependency`-Kanten ueber die bestehende M5-
  Regel "Kanten werden nur ausgegeben, wenn beide Endknoten im finalen
  Graph enthalten sind" automatisch verworfen, und `graph_truncated=true`
  wird gesetzt.

Anpassung der Statement-Reihenfolge im Analyze-DOT (Knotenprioritaet,
absteigend; bei Budget-Druck weicht jeweils die niedrigste noch nicht
aufgenommene Prioritaet zuerst):

1. primaere Top-Ranking-Translation-Units (unveraendert aus M5).
2. Top-Hotspot-Knoten (unveraendert).
3. Target-Knoten **fuer primaere Top-Ranking-Translation-Units** aus
   `AnalysisResult::target_assignments` (unveraendert aus M5; rangiert
   weiterhin vor Hotspot-Kontext, weil diese Targets direkt zu Top-
   Ranking-TUs gehoeren).
4. Hotspot-Kontext-Translation-Units (unveraendert aus M5).
5. **Neu in AP 1.2**: weitere Target-Knoten aus
   `AnalysisResult::target_graph.nodes`, die noch nicht ueber Prioritaet 3
   aufgenommen wurden. Sortierung folgt der `target_graph_support::sort_target_graph`-
   Reihenfolge `(unique_key, display_name, type)`.
6. **Neu in AP 1.2**: synthetische `external_target`-Knoten, nur erzeugt,
   wenn mindestens eine `target_dependency`-Kante mit `resolution=external`
   im finalen Graph erscheint UND beide Endknoten der Kante (Quelle aus
   Prioritaet 3 oder 5, Ziel als synthetischer Knoten) noch im Budget
   liegen.

Anpassung der Kantenprioritaet im Analyze-DOT:

1. `tu_include_hotspot` (unveraendert).
2. `tu_target` (unveraendert).
3. `target_dependency` (neu in AP 1.2). Innerhalb dieser Kantenart wird
   nach `(from_unique_key, to_unique_key)` priorisiert; bei
   `node_limit`-/`edge_limit`-Druck werden zuerst die alphabetisch
   spaeteren Kanten verworfen.

Anpassung im Impact-DOT:

- `target_graph.nodes` und `target_dependency`-Kanten werden NACH den
  bestehenden Impact-Knoten (geaenderte Datei, betroffene
  Translation-Units, betroffene Targets) und NACH den bestehenden
  Impact-Kanten ausgegeben. Ein bereits ueber Impact geladener Target-
  Knoten bekommt keinen zweiten Knoteneintrag; das `impact`-Attribut
  bleibt am ersten Eintrag.
- AP 1.2 fuehrt fuer Impact-DOT KEIN `impact`-Attribut auf
  `target_dependency`-Kanten ein. Die Kantenfarbe/-form folgt nur
  `resolution`. AP 1.3 entscheidet, ob `target_dependency`-Kanten ein
  Impact-Attribut bekommen.

Budget-Aenderungen:

- Analyze-DOT: `node_limit = max(25, 4 * top_limit + 10)` bleibt
  unveraendert. Target-Graph-Knoten teilen sich dasselbe Budget mit allen
  anderen Analyze-Knotenarten. Bei Knoten-Druck weicht zuerst die
  niedrigste Prioritaet aus der oben definierten sechsstufigen
  Knotenprioritaet, also synthetische `external_target`-Knoten zuerst,
  danach weitere Target-Knoten aus `target_graph.nodes` (Prioritaet 5),
  danach Hotspot-Kontext-TUs (Prioritaet 4), danach Targets der Top-
  Ranking-TUs (Prioritaet 3) und so weiter. Top-Ranking-TUs (Prioritaet 1)
  und Top-Hotspot-Knoten (Prioritaet 2) bleiben Teil des Graphen, solange
  ueberhaupt Knoten ausgegeben werden.
- Analyze-DOT: `edge_limit = max(40, 6 * top_limit + 20)` bleibt
  unveraendert. `target_dependency`-Kanten teilen sich das Budget mit
  `tu_include_hotspot` und `tu_target` ueber die Kantenprioritaet.
- Impact-DOT: `node_limit = 100` und `edge_limit = 200` bleiben
  unveraendert. Target-Graph-Knoten und `target_dependency`-Kanten teilen
  sich diese Budgets.
- `graph_truncated=true` wird gesetzt, sobald mindestens ein
  Kandidat-Target-Knoten oder eine Kandidat-`target_dependency`-Kante
  wegen Budget weggefallen ist.

DOT-Graph-Attribute (zusaetzlich zu den bestehenden in M5):

- `graph_target_graph_status` (string-quoted): `not_loaded`, `loaded`,
  `partial`. Pflicht ab AP 1.2 fuer Analyze- und Impact-DOT. Der Name folgt
  der etablierten M5-Konvention `graph_*` fuer Graph-Level-Attribute
  (siehe `docs/report-dot.md` `graph_node_limit`, `graph_edge_limit`,
  `graph_truncated`). Die Redundanz zwischen `graph_*` und `target_graph_*`
  ist akzeptiert, weil sie Adapter-Code ueber eine einheitliche
  Praefix-Konvention vereinfacht.
- AP 1.2 fuegt KEINE Hub-Attribute zur DOT-Graphschicht hinzu. Hubs sind
  eine textuelle/strukturierte Sicht; eine Hub-Hervorhebung im DOT-Graphen
  waere eine eigene Knoten-Attribut-Erweiterung mit Versionssprung. AP 1.5
  entscheidet, ob das in M6 Folge-AP wird oder nach M6 verschoben wird.

## Console- und Markdown-Vertragsregeln

Neue Abschnitte in `analyze`-Output (Console und Markdown):

- **Direct Target Dependencies**: erscheint, sobald
  `target_graph_status != not_loaded`. Auch leerer `target_graph.edges` mit
  `target_graph_status=loaded` erzeugt den Abschnitt mit dem Leersatz
  `No direct target dependencies.`. Bei `target_graph_status=not_loaded`
  wird der Abschnitt komplett weggelassen (siehe Empty-Section-Vertrag);
  stattdessen steht in der Summary-Zeile bereits
  `Target metadata: not_loaded`.
- **Target Hubs**: erscheint, sobald `target_graph_status != not_loaded`.
  Leerer `inbound`/`outbound`-Vector erzeugt jeweils den Leersatz
  `No incoming hubs.` / `No outgoing hubs.`. Bei
  `target_graph_status=not_loaded` wird der Abschnitt komplett weggelassen
  (siehe Empty-Section-Vertrag).

Disambiguierung bei Namens-Kollision:

- Wenn ein `display_name` in der jeweiligen Liste (Direct Target
  Dependencies oder Target Hubs) nicht eindeutig ist, ergaenzt der Adapter
  das Suffix ` [key: <unique_key>]`. Eindeutige `display_name`-Werte
  bleiben unveraendert.
- Eindeutigkeitspruefung wird pro Liste durchgefuehrt: ein Target kann in
  Direct Target Dependencies eindeutig sein und in Target Hubs nicht. Beide
  Adapter halten die Pruefung in einem gemeinsamen Helper in
  `console_report_adapter.cpp`/`markdown_report_adapter.cpp`, damit die
  Regel byteweise konsistent bleibt.

Abschnitts-Format Console (`analyze`):

```
Direct Target Dependencies (target_graph_status: loaded):
  mylib -> common
  mylib -> nlohmann_json::INTERFACE_LIBRARY [external]
  app -> mylib
```

- Ueberschrift in deutscher CMake-Xray-Konvention bleibt englisch (analog zu
  bestehenden M5-Sections).
- Eine Kante pro Zeile, zwei Leerzeichen Einzug.
- `to`-Element ist `to_display_name` aus dem Modell: `display_name` fuer
  `resolution=resolved` und `raw_id` fuer `resolution=external`. Damit
  arbeitet der Disambiguierungs-Helper auf demselben `display_text` wie
  in den anderen Formaten und Mischfaelle (intern `foo` vs. extern
  `raw_id=foo`) werden ueber `[key: <unique_key>]` aufgeloest.
- Suffix `[external]` erscheint genau dann, wenn `resolution=external`.
  Bei `resolved` entfaellt das Suffix komplett (kein redundantes
  `[resolved]`-Suffix, weil das Lärm waere).
- Reihenfolge der Suffixe bei kombiniertem Mischfall:
  `<to_display_name> [external] [key: <to_unique_key>]`. `[external]`
  kommt zuerst, weil es semantisch zur Kante gehoert; `[key: ...]`
  kommt zuletzt, weil es ein Disambiguierungs-Marker ist. Doppelte
  Suffixe sind im Kollisionsfall akzeptiert; der `<external>::`-Praefix
  in `[key: <external>::foo]` und das separate `[external]`-Suffix
  bleiben beide sichtbar, damit ein Console-Leser nicht erst aus dem
  Schluessel-Praefix die Resolution rekonstruieren muss.

Abschnitts-Format Console (`analyze`, Hubs):

```
Target Hubs (in_threshold: 10, out_threshold: 10):
  Inbound (>= 10 incoming): common, utility
  Outbound (>= 10 outgoing): app, integration_test
```

- Eine Zeile fuer `Inbound`, eine Zeile fuer `Outbound`.
- Komma-getrennte Liste der Hub-Targets, jeweils `display_name`. Bei
  Namens-Kollision innerhalb der Liste: `display_name [key: <unique_key>]`.
- Leerer Vector: `No incoming hubs.` / `No outgoing hubs.`.

Abschnitts-Format Markdown (`analyze`):

```markdown
## Direct Target Dependencies

Status: `loaded`.

| From | To | Resolution | External Target |
|---|---|---|---|
| `mylib` | `common` | resolved | |
| `mylib` | `nlohmann_json::INTERFACE_LIBRARY` | external | `nlohmann_json::INTERFACE_LIBRARY` |
| `app` | `mylib` | resolved | |

## Target Hubs

| Direction | Threshold | Targets |
|---|---|---|
| Inbound | 10 | `common`, `utility` |
| Outbound | 10 | `app`, `integration_test` |
```

- Leere Tabellen werden NICHT als Tabelle ausgegeben; Markdown-Adapter
  ersetzt sie durch den jeweiligen Leersatz als Absatz.

`impact`-Output (Console und Markdown) erhaelt einen neuen Abschnitt
**Target Graph Reference** als reine Lesedaten-Sicht:

```
Target Graph Reference (target_graph_status: loaded):
  mylib -> common
  app -> mylib
```

- Same Format wie der `analyze`-Direct-Target-Dependencies-Abschnitt.
- Es gibt KEINEN Hub-Abschnitt im `impact`-Output. Diese Asymmetrie
  spiegelt den JSON-Vertrag: `target_hubs` existiert nur im
  `cmake-xray.analysis`-JSON, nicht im `cmake-xray.impact`-JSON. AP 1.3
  entscheidet ueber Hub-Sichtbarkeit im Impact-Kontext zusammen mit der
  Priorisierung.
- AP 1.3 darf den `Target Graph Reference`-Abschnitt durch eine
  priorisierte Sicht ersetzen, ohne einen weiteren Versionssprung zu
  brauchen, weil Console und Markdown kein `format_version` tragen;
  Goldens werden trotzdem in AP 1.3 byte-stabil aktualisiert.
- Bei `target_graph_status=not_loaded` wird auch der `Target Graph
  Reference`-Abschnitt komplett weggelassen, gemaess dem zentralen
  Empty-Section-Vertrag.

Wichtig:

- Target-Display-Namen werden in Console und Markdown UTF-8-stabil
  ausgegeben. Sonderzeichen werden NICHT escaped (Markdown-Backticks
  bewahren das Aussehen, Console gibt den Rohstring aus). Bei
  Markdown-Sonderzeichen wie ``` ` ``` im Target-Namen verwendet der
  Markdown-Adapter doppelte Backticks `` `` `` als Inline-Code-Wrapper,
  analog zu existierenden M5-Code-Spans.

Sortier-Vertrag fuer Console und Markdown (alle Listen und Tabellen):

- `Direct Target Dependencies`-Liste (Console) und Tabelle (Markdown):
  sortiert nach
  `(from_unique_key, to_unique_key, kind, from_display_name, to_display_name)`
  gemaess M6-Hauptplan-Sortiervertrag (identisch zu
  `target_graph_support::sort_target_graph`).
- `Target Hubs`-Inbound-Liste: sortiert nach
  `(unique_key, display_name, type)`, identisch zur JSON-
  `target_hubs.inbound`-Sortierung.
- `Target Hubs`-Outbound-Liste: sortiert nach
  `(unique_key, display_name, type)`, identisch zur JSON-
  `target_hubs.outbound`-Sortierung.
- `Target Graph Reference`-Liste (Impact): sortiert nach demselben
  5-Stufen-Tupel wie der Analyze-`Direct Target Dependencies`-Abschnitt.
- Adapter rufen `sort_target_graph` NICHT erneut auf; sie verlassen sich
  auf die im `AnalysisResult`/`ImpactResult` bereits normalisierten
  Datenstrukturen aus AP 1.1. Wenn die Modellsortierung nicht stabil
  waere, wuerden Goldens zwischen Laeufen flackern; das ist ein
  AP-1.1-Verstoss und nicht durch Adapter-Logik abzufangen.

## HTML-Vertragsregeln

Neue Pflichtsections fuer Analyze-HTML (in dokumentierter Reihenfolge):

1. Header / Summary
2. Inputs
3. Translation Unit Ranking
4. Include Hotspots
5. Target Metadata (unveraendert aus M5)
6. **Target Graph** (neu in AP 1.2)
7. **Target Hubs** (neu in AP 1.2)
8. Diagnostics

Neue Pflichtsections fuer Impact-HTML:

1. Header / Summary
2. Inputs
3. Directly Affected Translation Units
4. Heuristically Affected Translation Units
5. Directly Affected Targets
6. Heuristically Affected Targets
7. **Target Graph Reference** (neu in AP 1.2)
8. Diagnostics

Section `Target Graph` (Analyze) Inhalt:

- `h2`: `Target Graph`.
- Sichtbarer Statuswert: `Status: <target_graph_status>`.
- Wenn `target_graph_status=not_loaded`: nur ein Absatz mit dem Leersatz
  `Target graph not loaded.`. Keine Tabelle.
- Wenn `target_graph_status=loaded` oder `partial` und `target_graph.edges`
  leer: Tabelle entfaellt, Leersatz `No direct target dependencies.`.
- Wenn `target_graph_status=loaded` oder `partial` und `target_graph.edges`
  nicht leer: Tabelle mit Spalten `From`, `To`, `Resolution`,
  `External target`. `External target`-Zelle bleibt leer fuer
  `resolution=resolved` und enthaelt den `raw_id` fuer
  `resolution=external`.

Section `Target Hubs` (Analyze) Inhalt:

- `h2`: `Target Hubs`.
- Sichtbare Schwellenwerte: `Incoming threshold: 10. Outgoing threshold: 10.`.
- Tabelle mit Spalten `Direction`, `Threshold`, `Targets`. Eine Zeile fuer
  `Inbound`, eine fuer `Outbound`. `Targets`-Zelle: kommagetrennte
  Display-Namen, bei Namens-Kollision innerhalb der Liste mit ` [key:
  <unique_key>]`-Suffix.
- Wenn `target_graph_status=not_loaded`: Section bleibt mit `h2` und Absatz
  `Target hubs not available.`, analog zum Empty-Section-Vertrag oben.
  Die Begruendung "scheinbar leerer Erfolg" gilt fuer Console und Markdown,
  nicht fuer HTML; HTML-Sektionen tragen ueber Heading + Leersatz selbst
  die Aussage "kein Erfolg, kein Laerm".

Section `Target Graph Reference` (Impact) Inhalt:

- Identischer Tabellen-Vertrag wie `Target Graph` im Analyze-HTML.
- AP 1.3 darf den Section-Titel auf `Target Graph (prioritised)` aendern
  und die Tabelle um Priorisierungs-Spalten erweitern; AP 1.2 schreibt nur
  die Lesedaten-Sicht.

Sortier-Vertrag fuer HTML (alle Tabellen und Listen):

- `Target Graph`-Tabelle (Analyze) und `Target Graph Reference`-Tabelle
  (Impact): Zeilenreihenfolge nach
  `(from_unique_key, to_unique_key, kind, from_display_name, to_display_name)`,
  identisch zu JSON `target_graph.edges` und Console/Markdown.
- `Target Hubs`-Tabelle (Analyze): Zeilenreihenfolge fest
  `Inbound, Outbound`. Innerhalb jeder `Targets`-Zelle ist die kommaweise
  Reihenfolge nach `(unique_key, display_name, type)` sortiert,
  identisch zu JSON `target_hubs.inbound`/`target_hubs.outbound`.
- HTML-Adapter rufen `sort_target_graph` nicht erneut auf; gleicher
  Vertrag wie Console und Markdown.

CSS:

- Die Status-Badge-Klasse aus M5 (`badge--loaded`, `badge--partial`,
  `badge--not-loaded`) wird auch fuer `target_graph_status` wiederverwendet.
- Keine neuen CSS-Klassen.
- Keine neuen Druck-Regeln; `tr { break-inside: avoid }` aus M5 deckt die
  neuen Tabellen ab.

Escaping:

- `<external>::<raw_id>`-Strings enthalten spitze Klammern; das HTML-
  Escaping aus M5 (`<` -> `&lt;`, `>` -> `&gt;`) erzeugt automatisch das
  korrekte Markup `&lt;external&gt;::raw_id`. Adapter-Tests pinnen das
  Verhalten als Regression gegen versehentliche Roh-HTML-Ausgabe.
- `raw_id` kann beliebige UTF-8-Zeichen enthalten. Whitespace-
  Normalisierung aus M5 wird auch auf `raw_id` angewendet.

`meta xray-report-format-version`:

- Der Wert steigt auf `2`.
- `main[data-format-version]` steigt entsprechend auf `2`.

## Adapter-Implementierungs-Hinweise

Gemeinsame Helfer:

- AP 1.2 fuegt einen Helper `disambiguate_target_display_names()` in einer
  geeigneten Stelle (`analysis_support.h` oder einem neuen
  `target_display_support.h`) hinzu. Der Helper arbeitet auf einer Liste
  von `(display_text, identity_key)`-Paaren, NICHT direkt auf
  `TargetInfo`-Verweisen. Damit deckt derselbe Helper drei Faelle ab:
  - File-API-Targets (sowohl als Hub-Eintraege als auch als
    `from_display_name`-/`to_display_name`-Werte): `display_text` ist der
    `TargetInfo::display_name`, `identity_key` ist `TargetInfo::unique_key`
    der Form `name::type`.
  - Externe Ziele in Edge-Listen: `display_text` ist der `raw_id`,
    `identity_key` ist `<external>::<raw_id>`.
  - Mischfaelle: ein internes Target `foo` und ein externes Ziel mit
    `raw_id=foo` haben gleichen `display_text=foo`, aber unterschiedlichen
    `identity_key` (`foo::STATIC_LIBRARY` vs. `<external>::foo`). Der
    Helper erkennt die Kollision und ergaenzt beiden Eintraegen das Suffix
    ` [key: <identity_key>]`. Das `<` und `>` werden in HTML escaped (s.u.).
- Eindeutigkeitspruefung erfolgt anhand `display_text`-Kollisionen
  innerhalb der uebergebenen Liste; Eintraege mit gleichem `identity_key`
  zaehlen als ein Eintrag (Adapter sollten ohnehin keine doppelten
  `identity_key`-Werte uebergeben, weil `sort_target_graph` das im Modell
  ausschliesst).
- Console- und Markdown-Adapter rufen denselben Helper. HTML-Adapter ruft
  ihn ebenfalls fuer Hub-Listen und Tabellenspalten. Bei HTML wird das
  Suffix nach dem Helper-Aufruf HTML-escaped, weil `<` und `>` Bestandteil
  von `<external>::*`-Schluesseln sind.
- JSON- und DOT-Adapter rufen den Helper NICHT auf; sie geben weiterhin
  `display_name`/`raw_id` und `unique_key` parallel aus (JSON ueber
  separate Felder, DOT ueber separate Attribute), weil dort kein
  Lesbarkeitsproblem besteht.

Adapter-Grenzen (unveraendert aus M5):

- Adapter rendern ausschliesslich aus `AnalysisResult` und `ImpactResult`.
  AP 1.2 fuegt keinen CLI-Kontext in Adapter ein.
- Hub-Schwellen kommen aus `AnalysisResult` (vorberechnete `target_hubs_in`
  und `target_hubs_out`-Listen plus die wirksamen `kDefaultTargetHub*Threshold`-
  Werte). Adapter berechnen Hubs nicht selbst.
- DOT- und HTML-Adapter rufen `target_graph_support::sort_target_graph`
  NICHT auf. Die Sortierung wird bereits in `ProjectAnalyzer` und
  `ImpactAnalyzer` durchgefuehrt; Adapter setzen darauf auf.

## Tests

Adapter-Unit-Tests:

- JSON-Adapter:
  - Compile-DB-only-`analyze`: `format_version=2`, `target_graph_status="not_loaded"`,
    `target_graph={"nodes":[],"edges":[]}`, `target_hubs.inbound=[]`,
    `target_hubs.outbound=[]`, `target_hubs.thresholds={"in_threshold":10,"out_threshold":10}`.
  - File-API-`analyze` ohne Hubs: `target_graph_status="loaded"`,
    `target_graph.nodes` und `target_graph.edges` korrekt sortiert,
    `target_hubs.inbound=[]`, `target_hubs.outbound=[]`.
  - File-API-`analyze` mit zehn eingehenden Kanten gegen `common`:
    `target_hubs.inbound` enthaelt `common`, `target_hubs.outbound` ist
    leer.
  - File-API-`analyze` mit `external`-Kante: Kante in
    `target_graph.edges` mit `to_unique_key="<external>::<raw_id>"`,
    `to_display_name=<raw_id>`, `resolution="external"`.
  - File-API-`analyze` mit `target_graph_status="partial"`: Status korrekt
    serialisiert, Diagnostics aus `AnalysisResult.diagnostics` korrekt
    nach `diagnostics`-Array uebernommen.
  - Mixed-Input-`analyze`: gleiche Erwartung wie File-API-`analyze`,
    `inputs.compile_database_path` und `inputs.cmake_file_api_path` beide
    gesetzt.
  - File-API-`impact`: `target_graph` korrekt, `target_hubs` ist NICHT im
    Output (Schema verbietet das Feld in `cmake-xray.impact`).
  - Compile-DB-only-`impact`: `target_graph_status="not_loaded"`,
    `target_graph={"nodes":[],"edges":[]}`.
- DOT-Adapter:
  - `analyze` mit File-API: `graph_target_graph_status="loaded"`-Attribut,
    Target-Knoten aus `target_graph.nodes` ausgegeben, `target_dependency`-
    Kanten korrekt erzeugt.
  - `analyze` mit `external`-Kante: synthetischer `external_target`-Knoten
    mit `kind="external_target"` und `external_target_id`-Attribut,
    `target_dependency`-Kante mit `style="dashed"` und
    `external_target_id`-Attribut.
  - **M4/M6-Knoten-Dedup**: `analyze`, in dem ein Target `mylib` sowohl
    als M4-Zuordnung einer Top-Ranking-TU (also Knotenprioritaet 3)
    als auch in `target_graph.nodes` (Knotenprioritaet 5) auftritt:
    Im finalen DOT-Graph erscheint genau EIN `target`-Knoten fuer
    `mylib`, nicht zwei. Der Test pinnt das byteweise und prueft, dass
    der ueberzaehlige Prioritaet-5-Kandidat sauber unterdrueckt wird.
    Begruendung: Die DOT-Vertragsregel "Dedup auf Target-`unique_key`:
    ein Target, das bereits als M4-Zuordnungs-Target im Graph steht,
    bekommt keinen zweiten Knoten" ist zentral und braucht eine
    explizite Regression. Ohne Test kann eine kuenftige Refaktorierung
    der Knotenprioritaeten-Sammelschleife unbemerkt Duplikate erzeugen.
  - `analyze` mit drei internen Targets `mylib`, `app`, `test`, die alle
    gegen dasselbe externe Ziel `boost` linken: genau EIN
    `external_target`-Knoten mit `external_target_id="boost"` und drei
    `target_dependency`-Kanten, die alle diesen Knoten als Ziel
    referenzieren. Der Test pinnt den Dedup-Vertrag byteweise.
  - `analyze` mit zwei externen Zielen `boost` und `abseil` ueber zwei
    interne Targets: zwei `external_target`-Knoten in deterministischer
    Reihenfolge `external_1` (raw_id `abseil`) und `external_2`
    (raw_id `boost`), weil die ID-Vergabe sortiert nach `to_unique_key`
    erfolgt.
  - `impact` mit drei internen Targets, die alle gegen `boost` linken:
    genau EIN `external_target`-Knoten, drei `target_dependency`-
    Kanten zeigen darauf. Test pinnt den Impact-Pfad gegen
    versehentliche pro-Quelle-Knotenerzeugung byteweise.
  - `impact` mit zwei externen Zielen `boost` und `abseil`, deren
    `to_unique_key`-Satz identisch zum Analyze-Pendant ist (also keine
    Budget-Kuerzung, gleiche externe Ziele): identische ID-Vergabe-
    Reihenfolge wie im Analyze-Test (`external_1` = `abseil`,
    `external_2` = `boost`). Damit ist sowohl der gemeinsame
    Dedup-Helper als auch das Mapping-als-Funktion-vom-Eingangs-Satz
    byteweise gepinnt.
  - `analyze` und `impact` mit unterschiedlichen externen Ziel-Saetzen
    (z. B. Impact rendert nur einen Subgraphen, der `boost` enthaelt,
    aber nicht `abseil`): Impact-`external_1` = `boost`, kein
    `external_2`. Analyze unveraendert. Test pinnt, dass das Mapping
    NICHT konfusion-stabil ueber Reporttypen hinweg ist, sondern strikt
    eine Funktion des jeweiligen Eingangs-Satzes.
  - `analyze` mit einem `raw_id` `quote\"and\\backslash\nnewline`: das
    `label`-Attribut enthaelt die korrekt escapte Form
    (`\"`, `\\`, `\n`), das `external_target_id`-Attribut enthaelt
    dieselbe escapte Form unverkuerzt. Test pinnt sowohl Label-Output
    als auch `external_target_id` byteweise und validiert zusaetzlich
    ueber `dot -Tsvg` (Docker-Pfad) bzw. `tests/validate_dot_reports.py`,
    dass Graphviz die Ausgabe akzeptiert.
  - `analyze` mit einem `raw_id`, der mehr als 48 Zeichen lang ist (zum
    Beispiel ein vollstaendiger CMake-IMPORTED-Target-Hash wie
    `nlohmann_json::INTERFACE_LIBRARY::abcdef0123456789`): das
    `label`-Attribut wird per Middle-Truncation auf 48 Zeichen gekuerzt,
    waehrend `external_target_id` den vollstaendigen Wert behaelt.
  - `analyze` mit einem `raw_id`, der UTF-8-Bytes ueber `0x7F` enthaelt
    (zum Beispiel `älib` als Library-Name): rohe UTF-8-Bytes
    bleiben in beiden Attributen erhalten; Graphviz akzeptiert die
    Ausgabe.
  - `analyze` mit `node_limit`-Druck: `target_graph.nodes` werden zuletzt
    aufgenommen; bei erreichten Budget wird `graph_truncated="true"`
    gesetzt und ueberzaehlige `target_dependency`-Kanten weggelassen.
  - `impact`: Target-Graph wird NACH bestehenden Impact-Knoten und -Kanten
    ausgegeben. `target_dependency`-Kanten haben kein `impact`-Attribut.
  - Compile-DB-only-`analyze`: `graph_target_graph_status="not_loaded"`,
    keine Target-Knoten ausserhalb der bisherigen M4-`tu_target`-Knoten,
    keine `target_dependency`-Kanten.
- HTML-Adapter:
  - Compile-DB-only-`analyze`: `Target Graph`-Section enthaelt `h2` und
    Absatz `Target graph not loaded.`, KEINE Tabelle. `Target Hubs`-Section
    enthaelt `h2` und Absatz `Target hubs not available.`, KEINE Tabelle.
    Beide Sections bleiben im Output (nicht weggelassen).
  - File-API-`analyze` mit Hubs: `Target Graph`-Tabelle gefuellt,
    `Target Hubs`-Tabelle mit zwei Zeilen, Schwellen sichtbar,
    Display-Namen in Hub-Liste byte-stabil.
  - `<external>::<raw_id>`-Strings werden HTML-escaped als
    `&lt;external&gt;::<raw_id>` ausgegeben. Adapter-Test verwendet
    `raw_id="evil<script>"` und prueft, dass keine Roh-HTML-Tags
    rauslecken.
  - Disambiguierung bei Namens-Kollision in Hub-Liste: zwei Targets mit
    gleichem `display_name=foo` und unterschiedlichem `unique_key`
    bekommen ` [key: ...]`-Suffix. Test pinnt den Output byteweise.
  - Disambiguierung bei Mischfall in Direct Target Dependencies: eine
    Kante mit Ziel-Target `foo` (intern, `unique_key="foo::STATIC_LIBRARY"`)
    und eine `external`-Kante mit `raw_id=foo` (extern,
    `unique_key="<external>::foo"`) im selben Abschnitt erzeugen beide ein
    Suffix mit dem jeweiligen `identity_key`; HTML-Adapter escaped `<` und
    `>` im externen Suffix korrekt zu `&lt;external&gt;::foo`.
  - File-API-`impact`: `Target Graph Reference`-Section gefuellt; keine
    Hub-Section.
- Console-Adapter:
  - Compile-DB-only-`analyze`: kein Target-Graph- und kein Target-Hub-
    Abschnitt.
  - File-API-`analyze` mit Hubs: beide Abschnitte gefuellt, Schwellen
    sichtbar, Listen kommagetrennt.
  - Disambiguierung wie HTML.
  - `external`-Kante: `to`-Element ist die `raw_id`, gefolgt von
    `[external]`-Suffix; kein literales `external`-Wort als Ziel.
  - Mischfall mit Disambiguierungs-Kollision plus External: Suffix-
    Reihenfolge `<raw_id> [external] [key: <external>::<raw_id>]`
    byteweise gepinnt.
- Markdown-Adapter:
  - Wie Console; zusaetzlich Test, dass leere Tabellen NICHT als leere
    Markdown-Tabelle ausgegeben werden, sondern als Leersatz-Absatz.
  - Test fuer Backtick-im-Display-Namen mit doppelten-Backticks-Inline-Code-
    Wrapping.

E2E-/Golden-Tests:

- Goldens unter `tests/e2e/testdata/m6/<format>-reports/` mit mindestens:
  - `analyze-compile-db-only.<ext>`: keine Target-Graph-Daten,
    `target_graph_status=not_loaded`. Pflicht fuer alle fuenf Formate.
  - `analyze-file-api-loaded.<ext>`: `loaded`, mehrere Kanten, mindestens
    einen Hub. Pflicht fuer alle fuenf Formate.
  - `analyze-file-api-partial.<ext>`: `partial`, mindestens eine
    `external`-Kante, mindestens eine `partial`-Diagnostic. Pflicht fuer
    alle fuenf Formate. Fuer DOT muss dieses Golden zusaetzlich:
    - mindestens einen synthetischen `external_target`-Knoten enthalten,
    - das `graph_target_graph_status="partial"`-Graph-Attribut tragen,
    - mindestens eine `target_dependency`-Kante mit `style="dashed"` und
      `resolution="external"` ausgeben.
    Fuer JSON muss das Golden zusaetzlich `target_graph_status="partial"`
    serialisieren und mindestens eine Kante mit `resolution="external"`
    in `target_graph.edges` enthalten.
    Fuer Console und Markdown muss das Golden den `Direct Target
    Dependencies`-Abschnitt mit mindestens einer External-Kante zeigen
    (Console: `<raw_id> [external]`-Suffix gemaess Console-Vertrag;
    Markdown: `External Target`-Spalte gefuellt).
  - `analyze-dot-raw-id-special-chars.dot`: ein DOT-spezifisches Golden
    mit einem `raw_id`, der Anfuehrungszeichen, Backslash und Newline
    enthaelt. Pflicht nur fuer DOT, weil JSON, HTML, Console und
    Markdown ihre eigenen Escape- und Whitespace-Vertraege haben, die
    bereits durch Sonderzeichen-Tests in den Adapter-Tests abgesichert
    sind. Das DOT-Golden pinnt das Zusammenspiel von M5-Escape-Vertrag
    und M6-`raw_id`-Quelle byteweise und laeuft zusaetzlich durch
    `dot -Tsvg`.
  - `impact-compile-db-only.<ext>`: keine Target-Graph-Daten,
    `target_graph_status=not_loaded`. Pflicht fuer alle fuenf Formate.
  - `impact-file-api-loaded.<ext>`: `loaded`, mehrere Kanten. Pflicht fuer
    alle fuenf Formate.
  - `impact-file-api-partial.<ext>`: `partial`, mindestens eine
    `external`-Kante. Pflicht fuer alle fuenf Formate. Fuer Console und
    Markdown wurde die ursprueglich angedachte Optionalitaet verworfen,
    weil ein Strukturidentitaets-Adapter-Test kuenftige Pfadabweichungen
    (Praefix-Aenderungen, Label-Anpassungen, neue Spalten) nicht
    zuverlaessig auffaengt; ein eigenes Golden ist die billigere und
    robustere Absicherung.
- Manifeste werden um die neuen Goldens erweitert; `validate_doc_examples.py`
  prueft sie ebenfalls.
- JSON-Schema-Validation gegen `format_version=2`-Schema laeuft als
  CTest-Gate.
- DOT-Syntax-Validation (`dot -Tsvg`) laeuft fuer alle neuen DOT-Goldens.

Schema-Tests:

- Test, dass `xray::hexagon::model::kReportFormatVersion == 2` mit
  `report-json.schema.json::FormatVersion::const` uebereinstimmt.
- Negativtest: ein `format_version=1`-JSON-Eingang validiert NICHT gegen
  das M6-Schema; das ist erwartet und schuetzt vor versehentlichem
  Mischen.

Regressionstests:

- Compile-DB-only-Goldens fuer Console und Markdown bleiben byte-stabil
  (es gibt keinen Target-Graph-Abschnitt).
- M5-File-API- und Mixed-Input-Goldens **veraendern sich** durch AP 1.2.
  Die Aenderung ist erwartet und wird im Tranchen-Liefer-Stand-Block
  dokumentiert; Regressionstests vergleichen gegen die NEUEN Goldens, nicht
  gegen die M5-Versionen.
- Bestehende M5-DOT-/JSON-/HTML-Goldens werden vollstaendig durch
  M6-Goldens ersetzt; AP 1.2 fuehrt KEINE doppelten Goldens fuer beide
  Versionen.

## Rueckwaertskompatibilitaet

Pflicht:

- Compile-DB-only-Pfade in Console und Markdown bleiben byte-stabil
  gegenueber M5; AP 1.2 fuegt fuer diese Pfade keine neuen Abschnitte
  hinzu.
- Schema und Goldens fuer `format_version=2` sind in AP 1.2 vollstaendig
  und gepinnt.

Verboten:

- Dual-Output: weder JSON, HTML noch DOT geben gleichzeitig
  `format_version=1` und `format_version=2` aus.
- Stille Zusatzfelder: AP 1.2 fuegt keine Felder ein, die nicht im
  `report-json.schema.json` mit `additionalProperties: false` erlaubt sind.
- Konsumenten-Migrations-Hinweise im Report-Output: keine `deprecated`-
  Felder, keine Migration-Banner. Versionssprung steht ausschliesslich in
  `format_version` und Schema.

## Implementierungsreihenfolge

Die Reihenfolge ist auf die atomaren Tranchen A.1-A.4 abgestimmt; jede
Tranche umfasst Schema/Adapter/Goldens fuer einen kohaerenten
Format-Cluster, damit das CTest-Gate `json_schema_check` und die DOT-/
HTML-Validations-Gates niemals zwischen Schritten rot werden.

Innerhalb von **A.1**:
1. `kReportFormatVersion` auf `2` heben; Schema-Konstante synchron auf `2`.
2. `report-json.schema.json` um `target_graph_status`, `target_graph`
   und `target_hubs` erweitern.
3. `docs/report-json.md` auf v2 aktualisieren.
4. JSON-Adapter implementieren; Adapter-Tests gruen.
5. M5-JSON-Goldens unter `tests/e2e/testdata/m5/json-reports/` auf v2
   migrieren; M6-JSON-Goldens unter `tests/e2e/testdata/m6/json-reports/`
   anlegen; Schema-Validation und E2E-Tests gruen.

Innerhalb von **A.2**:
6. `docs/report-dot.md` auf v2 aktualisieren.
7. DOT-Adapter implementieren; Adapter-Tests gruen.
8. M5-DOT-Goldens migrieren; M6-DOT-Goldens anlegen; DOT-Syntax-Gate und
   E2E-Tests gruen.

Innerhalb von **A.3**:
9. `docs/report-html.md` auf v2 aktualisieren.
10. HTML-Adapter implementieren; Adapter-Tests gruen.
11. Markdown-Adapter implementieren; Adapter-Tests gruen.
12. Console-Adapter implementieren; Adapter-Tests gruen.
13. M5-Goldens fuer HTML, Markdown, Console migrieren; M6-Goldens anlegen;
    `validate_doc_examples.py` aktualisieren.

Innerhalb von **A.4**:
14. CLI-E2E-Tests vollstaendig durchlaufen lassen, byte-stabil gegen
    Goldens vergleichen.
15. Docker-Gates ausfuehren (siehe `docs/quality.md`); Tranche gilt erst
    nach gruenem Gate als geliefert.

## Tranchen-Schnitt

AP 1.2 wird in vier Sub-Tranchen geliefert, analog zum M5-Liefer-Stand-
Vertrag:

- **A.1 Schema, JSON-Adapter und JSON-Goldens (atomar)**:
  `kReportFormatVersion=2`, `report-json.schema.json` v2,
  `docs/report-json.md` auf v2 aktualisiert, JSON-Adapter implementiert
  v2-Output (mit `target_graph_status`, `target_graph`, `target_hubs`),
  bestehende M5-JSON-Goldens unter `tests/e2e/testdata/m5/json-reports/`
  werden auf v2 migriert (Pfad bleibt `m5/`, weil sie historisch dort
  leben und in M5-Tests referenziert sind), neue M6-Goldens unter
  `tests/e2e/testdata/m6/json-reports/` fuer Target-Graph-Daten
  hinzugefuegt. JSON-Adapter-Tests, Schema-Validation-Gate und E2E-Tests
  fuer JSON gruen.
- **A.2 DOT-Adapter und DOT-Goldens**: DOT-Adapter implementiert
  v2-Output (`graph_target_graph_status`, `target_dependency`-Kanten,
  synthetische `external_target`-Knoten), `docs/report-dot.md` auf v2
  aktualisiert, M5-DOT-Goldens migriert, M6-DOT-Goldens erzeugt.
  Adapter-Tests, DOT-Syntax-Gate und E2E-Tests gruen.
- **A.3 HTML-, Markdown- und Console-Adapter**: HTML-Adapter,
  Markdown-Adapter und Console-Adapter implementieren v2-Output.
  `docs/report-html.md` auf v2 aktualisiert. Goldens fuer alle drei
  Formate erzeugen. Adapter- und E2E-Tests gruen.
- **A.4 Audit-Pass**: Plan-Test-Liste aus `Tests`-Sektion gegen Ist-Stand
  verifizieren. Docker-Gates gruen. Liefer-Stand-Block in diesem Dokument
  und in `docs/plan-M6.md` aktualisieren.

Begruendung der atomaren A.1-Tranche: Die urspruenglich angedachte
Aufteilung in "A.1 Schema only" + "A.2 JSON-Adapter" wurde verworfen.
Schema v2 setzt `additionalProperties: false` zusammen mit neuen
Pflichtfeldern (`target_graph_status`, `target_graph`, `target_hubs`); ein
Schema-Update ohne gleichzeitige Adapter- und Goldens-Migration wuerde
das CTest-Gate `json_schema_check` rot werden lassen, weil bestehende
M5-Goldens das verschaerfte Schema nicht mehr erfuellen. Die saubere
Loesung ist, alle drei Aenderungen in einem Liefer-Schritt zu buendeln.

A.3 darf parallel zu A.2 entwickelt werden, weil HTML, Markdown und
Console keine Schema-Abhaengigkeit haben. A.1 ist Voraussetzung fuer
A.2 und A.3, weil `kReportFormatVersion=2` als zentrale Konstante in
beiden Tranchen verwendet wird.

Scope von `json_schema_check` in A.1: Das Gate validiert in A.1 und
allen Folge-Tranchen (a) das Schema selbst (Schema-Selbst-Validation,
`FormatVersion.const == 2`), (b) alle JSON-Goldens unter `m5/json-reports/`
und `m6/json-reports/` gegen das v2-Schema, und (c) die durch E2E-Tests
generierten Live-JSON-Outputs gegen dasselbe Schema. Es gibt keinen
Validator-Pfad, der Goldens gegen ein altes v1-Schema validiert; v1
existiert nach A.1 weder als Schema-Datei noch als Goldens-Inhalt.

## Liefer-Stand

Wird nach dem Schnitt der A-Tranchen mit Commit-Hashes befuellt. Bis dahin
ist AP 1.2 nicht abnahmefaehig. Format analog zu `docs/plan-M5-1-8.md`
Z. 151ff.:

- A.1 (Schema, JSON-Adapter und JSON-Goldens): noch nicht ausgeliefert.
- A.2 (DOT-Adapter und DOT-Goldens): noch nicht ausgeliefert.
- A.3 (HTML-, Markdown- und Console-Adapter): noch nicht ausgeliefert.
- A.4 (Audit-Pass): noch nicht ausgeliefert.

## Abnahmekriterien

AP 1.2 ist abgeschlossen, wenn:

- `kReportFormatVersion == 2` in C++ und Schema;
- alle fuenf Reportformate Target-Graph-Daten korrekt ausgeben, sobald
  `target_graph_status != not_loaded`;
- Console und Markdown den Target-Graph-Abschnitt bei
  `target_graph_status=not_loaded` weglassen, ohne "scheinbar leerer
  Erfolg" zu erzeugen;
- HTML beide Sections (`Target Graph` und `Target Hubs`) auch bei
  `target_graph_status=not_loaded` mit `h2` und Leersatz behaelt
  (`Target graph not loaded.` bzw. `Target hubs not available.`),
  konsistent mit der M5-`Target Metadata`-Section;
- Analyze-JSON und Impact-JSON beide den `target_graph_status`- und
  `target_graph`-Block immer enthalten, auch bei `not_loaded`; das Schema
  laesst die Felder nicht weg;
- nur Analyze-JSON zusaetzlich den `target_hubs`-Block immer enthaelt
  (auch bei `not_loaded`); Impact-JSON enthaelt `target_hubs` ausdruecklich
  nicht, und das Schema verbietet das Feld dort;
- DOT eine neue `target_dependency`-Kantenart, einen synthetischen
  `external_target`-Knoten fuer `<external>::*`-Ziele und ein
  `target_graph_status`-Graph-Attribut ausgibt;
- die Disambiguierung kollidierender `display_name`-Werte ueber
  `disambiguate_target_display_names()` byte-stabil in Console, Markdown
  und HTML wirkt;
- alle relevanten Goldens unter `tests/e2e/testdata/m6/` gepinnt sind;
- Schema-Validation, DOT-Syntax-Validation und Doc-Examples-Drift-Gate
  gruen sind;
- Compile-DB-only-Goldens fuer Console und Markdown byte-stabil gegenueber
  M5 bleiben;
- der Docker-Gate-Lauf gemaess `docs/quality.md` (Tranchen-Gate fuer M6
  AP 1.2) gruen ist;
- `git diff --check` sauber ist.

## Offene Folgearbeiten

Diese Punkte werden bewusst in spaeteren M6-Arbeitspaketen umgesetzt:

- Impact-Priorisierung mit `priority_class`, `graph_distance`,
  `impact_target_depth_*`-Feldern in JSON, HTML, DOT, Console und Markdown
  (AP 1.3). AP 1.3 darf den Output von AP 1.2 erweitern, aber nicht
  brechen, weil das `format_version` erneut anheben wuerde.
- CLI-Optionen `--target-hub-in-threshold`, `--target-hub-out-threshold`,
  `--analysis target-graph,target-hubs` und Provenienz-Aufnahme der
  wirksamen Schwellen in `target_hubs.thresholds` (AP 1.5). AP 1.2 setzt
  feste Defaults; das JSON-Feld ist bereits ein Objekt, damit AP 1.5
  keinen weiteren Versionssprung braucht.
- Compare-Vertrag fuer `format_version=2`-Reports mit
  Compare-Kompatibilitaetsmatrix (AP 1.6).
- Hub-Hervorhebung im DOT-Graphen ueber zusaetzliche Knoten-Attribute
  (`is_inbound_hub`, `is_outbound_hub`); AP 1.5 oder eine spaetere Version
  entscheidet, ob das Teil von M6 wird.
