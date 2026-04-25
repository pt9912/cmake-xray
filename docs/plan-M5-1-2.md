# Plan M5 - AP 1.2 JsonReportAdapter fuer stabile maschinenlesbare Ausgaben umsetzen

Der JSON-Export ist fuer Automatisierung, CI-Auswertung und Folgewerkzeuge gedacht. Deshalb muss das Format stabiler und expliziter sein als die menschenlesbaren Textberichte.

Ein JSON-Bericht fuer `analyze` soll mindestens enthalten:

- `format`: fester Formatbezeichner `cmake-xray.analysis`
- `format_version`: Schema-/Formatversion, initial `1`
- `report_type`: `analyze`
- `inputs`: verwendete Eingabequellen aus dem strukturierten `ReportInputs`-Modell, mindestens `compile_database_path`, `compile_database_source`, `cmake_file_api_path`, `cmake_file_api_resolved_path` und `cmake_file_api_source`
- `summary`: Translation-Unit-Anzahl, Ranking-Anzahl, Hotspot-Anzahl, Top-Limit, Beobachtungsherkunft und Target-Metadatenstatus
- `translation_unit_ranking`: Objekt mit `limit`, `total_count`, `returned_count`, `truncated` und deterministisch sortierten `items` inklusive Metriken, Diagnostics und Target-Zuordnungen
- `include_hotspots`: Objekt mit `limit`, `total_count`, `returned_count`, `truncated` und deterministisch sortierten `items`
- `diagnostics`: reportweite Diagnostics

Ein JSON-Bericht fuer `impact` soll mindestens enthalten:

- `format`: fester Formatbezeichner `cmake-xray.impact`
- `format_version`: Schema-/Formatversion, initial `1`
- `report_type`: `impact`
- `inputs`: verwendete Eingabequellen aus dem strukturierten `ReportInputs`-Modell, mindestens `compile_database_path`, `compile_database_source`, `cmake_file_api_path`, `cmake_file_api_resolved_path`, `cmake_file_api_source`, `changed_file` und `changed_file_source`
- `summary`: Anzahl betroffener Translation Units, Klassifikation, Beobachtungsherkunft, Target-Metadatenstatus und Anzahl betroffener Targets
- `directly_affected_translation_units`
- `heuristically_affected_translation_units`
- `directly_affected_targets`
- `heuristically_affected_targets`
- `diagnostics`

Wichtig:

- `inputs` darf nur Felder enthalten, die stabil im fachlichen Ergebnis- oder Request-Modell verfuegbar sind; `cmake_file_api_path` ist als Feld fuer M5 verpflichtend, damit File-API- und Mixed-Input-Laeufe vollstaendig dokumentiert werden koennen
- JSON folgt bei `analyze` der CLI-`--top`-Begrenzung fuer Ranking- und Hotspot-Listen, muss aber ueber `limit`, `total_count`, `returned_count` und `truncated` eindeutig anzeigen, ob die Ausgabe gekuerzt wurde; eine unlimitierte JSON-Ausgabe waere nur nach expliziter Schema-Entscheidung zulaessig
- `impact`-JSON enthaelt in M5 keine `limit`-/`truncated`-Felder, solange der CLI- und Port-Vertrag keine Impact-Begrenzung kennt; alle betroffenen Translation Units und Targets aus dem `ImpactResult` werden ausgegeben
- Markdown, HTML und DOT folgen bei `analyze` ebenfalls der CLI-`--top`-Begrenzung; HTML und Markdown kennzeichnen begrenzte Abschnitte menschenlesbar
- DOT enthaelt bei `analyze --top N` die Top-N-Ranking- und Top-N-Hotspot-Knoten sowie begrenzte Kontextkanten; fuer einen Top-Hotspot werden hoechstens `context_limit` der in `IncludeHotspot` gespeicherten betroffenen Translation Units als Kontextknoten ausgegeben, auch wenn diese Translation Units nicht selbst im Top-N-Ranking liegen. `context_total_count` zaehlt pro Hotspot die eindeutigen betroffenen Translation Units vor `context_limit` und vor globalem Graph-Budget; `context_returned_count` zaehlt pro Hotspot die eindeutigen Kontext-Translation-Units, die nach `context_limit`, globalem `node_limit` und globalem `edge_limit` tatsaechlich als Knoten im Graph enthalten und durch eine Kontextkante mit diesem Hotspot verbunden sind. Wenn dieselbe Kontext-TU mehreren Hotspots zugeordnet ist, wird sie pro Hotspot in dessen `context_*`-Werten gezaehlt, aber als Graphknoten nur einmal gegen das globale `node_limit`. Nach Anwendung des `edge_limit` werden reine Kontext-TU-Knoten ohne verbleibende Kontextkante entfernt, sofern sie nicht zugleich primaere Ranking-Knoten oder Target-Kontextknoten sind; `graph_node_limit`, `graph_truncated` und `context_returned_count` beziehen sich auf diesen finalen Graphzustand.
- DOT fuer `impact` ist trotz unbegrenztem `ImpactResult` ebenfalls budgetiert; der Graph enthaelt betroffene Translation Units und Targets nur bis zum globalen `node_limit`-/`edge_limit`-Budget und kennzeichnet Kuerzungen ueber `graph_truncated`
- `context_limit` ist fuer M5 Teil des DOT-Formatvertrags, kleiner als oder gleich dem wirksamen `--top`-Limit bei `analyze` und nicht pro Hotspot zu einem unbeschraenkten Gesamtgraphen addierbar, weil zusaetzlich ein globales `node_limit`-/`edge_limit`-Budget gilt
- M5 legt die DOT-Grenzen deterministisch und als M5-Formatvertrag fest: Fuer `analyze` gilt `context_limit = min(top_limit, 5)`, `node_limit = max(25, 4 * top_limit + 10)`, `edge_limit = max(40, 6 * top_limit + 20)`. Ohne explizites `--top` wird der wirksame Standard-Top-Wert der CLI verwendet. Fuer `impact` gilt mangels Top-Limit fest `node_limit = 100` und `edge_limit = 200`. Eine spaetere Konfigurierbarkeit waere eine explizite Vertragsaenderung mit Dokumentation, Tests und gegebenenfalls DOT-Metadaten fuer die wirksamen Limits.
- Knotenkuerzung erfolgt deterministisch in stabiler Sortierreihenfolge: bei `analyze` zuerst primaere Top-Ranking-Knoten, dann Top-Hotspot-Knoten, dann Target-Knoten, dann Hotspot-Kontext-Translation-Units sortiert nach Anzeige-Pfad; bei `impact` zuerst geaenderte Datei, dann direkt betroffene Translation Units, heuristisch betroffene Translation Units und Targets, jeweils nach Anzeige-Pfad bzw. Target-Name sortiert
- Kanten werden nur ausgegeben, wenn beide Endknoten im Budget enthalten sind. Bei erreichtem `edge_limit` gilt eine globale Kantenprioritaet mit lexikografischem Tie-Breaker nach stabiler Quell-ID, Ziel-ID und Kantenart: fuer `analyze` zuerst Translation-Unit-zu-Include-Hotspot-Kanten, dann Translation-Unit-zu-Target-Kanten; fuer `impact` zuerst geaenderte-Datei-zu-direkt-betroffener-Translation-Unit-Kanten, dann geaenderte-Datei-zu-heuristisch-betroffener-Translation-Unit-Kanten, dann direkt-betroffene-Translation-Unit-zu-Target-Kanten, dann heuristisch-betroffene-Translation-Unit-zu-Target-Kanten
- DOT-Attributwerte verwenden fuer den M5-Metadatenvertrag eine exakte Lexik: Integer werden als unquoted ASCII-Dezimalzahlen ohne Vorzeichen geschrieben, Booleans als unquoted lowercase `true` oder `false`, Strings als quoted DOT-Strings mit deterministischem Escaping
- bei Analyze-Hotspot-Kontext enthaelt jeder betroffene Hotspot-Knoten verpflichtend die Node-Attribute `context_total_count` integer, `context_returned_count` integer und `context_truncated` boolean; bei ungekuerztem Kontext ist `context_truncated=false`, und diese `context_*`-Attribute gelten nicht fuer `impact`
- DOT enthaelt fuer `analyze` und `impact` immer die Graph-Attribute `graph_node_limit` integer, `graph_edge_limit` integer und `graph_truncated` boolean; bei ungekuerztem Graph ist `graph_truncated=false`, damit Consumer budgetierte M5-Ausgaben von aelteren oder unbudgetierten DOT-Ausgaben unterscheiden koennen. Kommentare duerfen nur zusaetzlich erscheinen und sind nicht Teil des Vertrags
- JSON-Ausgaben muessen gueltiges UTF-8 und syntaktisch gueltiges JSON sein
- Felder mit leerer Menge werden als leere Arrays ausgegeben, nicht weggelassen, sofern sie Teil des Formatvertrags sind
- optionale fachliche Informationen koennen `null` sein, wenn das Schema dies explizit dokumentiert
- numerische Metriken bleiben numerisch und werden nicht als lokalisierte Strings gerendert
- die Reihenfolge von Objektfeldern soll im Adapter stabil bleiben, auch wenn JSON semantisch keine Feldreihenfolge garantiert
- Pfade werden als Anzeige-Strings ausgegeben; kanonische Normalisierungsschluessel duerfen nur aufgenommen werden, wenn sie keinen instabilen Hostbezug in Goldens einfuehren
- `docs/report-json.md` ist der verbindliche lesbare M5-Vertrag und dokumentiert fuer alle Felder Typ, Pflichtstatus, erlaubte Enum-Werte, Nullability, leere-Array-Regeln und Aenderungsregeln fuer kuenftige `format_version`-Erhoehungen; zusaetzlich entsteht ein maschinenlesbares Schema-Artefakt `docs/report-json.schema.json`, gegen das Golden-Outputs validiert werden
- Tests pruefen nicht nur syntaktisch gueltiges JSON, sondern auch Pflichtfelder, Array-statt-Weglassen-Regeln, numerische Typen, bekannte Enum-Werte und `null` nur an dokumentierten Stellen

Vorgesehene Artefakte:

- `src/adapters/output/json_report_adapter.h`
- `src/adapters/output/json_report_adapter.cpp`
- `tests/adapters/test_json_report_adapter.cpp`
- JSON-Golden-Outputs unter `tests/e2e/testdata/m5/`
- Dokumentation `docs/report-json.md` als verbindlicher lesbarer Formatvertrag und `docs/report-json.schema.json` als maschinenlesbares Validierungsartefakt; `docs/guide.md` verweist darauf nur nutzungsorientiert

**Ergebnis**: `cmake-xray` besitzt einen versionierten, automatisierbaren Reportvertrag fuer Analyse- und Impact-Ergebnisse.
