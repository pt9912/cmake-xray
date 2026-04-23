# Plan M4 AP 1.2 - `CmakeFileApiAdapter` fuer CMake-Reply-Daten umsetzen

## 0. Kontext

AP 1.1 hat `BuildModelPort` als einheitlichen Vertrag etabliert. AP 1.5 hat `--cmake-file-api <path>` als CLI-Option eingefuehrt und die Driving Ports um `cmake_file_api_path` erweitert. AP 1.2 implementiert nun den ersten konkreten zweiten Adapter, der diesen Vertrag ueber CMake File API v1 Reply-Daten bedient.

### 0.1 Eingabe

Der Nutzer uebergibt einen Pfad. Akzeptiert werden:

- ein Build-Verzeichnis, das `.cmake/api/v1/reply/` enthaelt
- direkt das Reply-Verzeichnis `.cmake/api/v1/reply/`

### 0.2 Ausgabe

Ein `BuildModelResult` mit:

- `source = ObservationSource::derived`
- `target_metadata = TargetMetadataStatus::loaded` oder `partial`
- `compile_database` mit synthetischen `CompileEntry`-Objekten aus Compile-Gruppen
- `target_assignments` fuer jede Beobachtung mit zugeordneten Targets
- `source_root` = Top-Level-Source-Verzeichnis aus dem Codemodel
- `diagnostics` fuer partielle oder fehlende Daten

## 1. CMake File API v1 Reply-Struktur

### 1.1 Verzeichnisstruktur

```
<build>/.cmake/api/v1/reply/
  index-<hash>.json               # Einstiegspunkt
  codemodel-v2-<hash>.json        # Build-Struktur mit Target-Referenzen
  target-<name>-<hash>.json       # Pro Target: Sources, Compile-Gruppen
```

### 1.2 Relevante JSON-Felder

**Index** (`index-*.json`):
- `objects[].kind` — Typ des Reply-Objekts (fuer M4 relevant: `"codemodel"`)
- `objects[].jsonFile` — relativer Pfad zur eigentlichen Reply-Datei

**Codemodel** (`codemodel-v2-*.json`):
- `paths.source` — absolutes Top-Level-Source-Verzeichnis
- `paths.build` — absolutes Top-Level-Build-Verzeichnis
- `configurations[].targets[]` — Target-Referenzen:
  - `name` — Target-Name
  - `id` — eindeutige Target-ID
  - `jsonFile` — Pfad zur Target-Detaildatei

**Target** (`target-*-*.json`):
- `name` — menschenlesbarer Name
- `type` — bekannte Werte: `EXECUTABLE`, `STATIC_LIBRARY`, `SHARED_LIBRARY`, `MODULE_LIBRARY`, `OBJECT_LIBRARY`, `INTERFACE_LIBRARY`, `UNKNOWN_LIBRARY`, `UTILITY`; der Adapter verwendet keine Allowlist, sondern akzeptiert jeden `type`-String und speichert ihn als Anzeige-Typ im `TargetInfo`; damit bleiben auch kuenftige CMake-Versionen mit neuen Target-Typen kompatibel
- `sources[]`:
  - `path` — Quelldateipfad; relativ zu `codemodel.paths.source` fuer Dateien innerhalb des Source-Trees, absolut fuer generierte oder externe Quellen
  - `compileGroupIndex` — Index in `compileGroups[]` (fehlt bei nicht-kompilierten Quellen)
- `compileGroups[]`:
  - `sourceIndexes[]` — welche Quellen diese Compile-Gruppe verwenden
  - `language` — `"C"`, `"CXX"` etc.
  - `compileCommandFragments[]` — `{ "fragment": "-std=c++20" }` etc.
  - `includes[]` — `{ "path": "/abs/include", "isSystem": false }`
  - `defines[]` — `{ "define": "NDEBUG" }`

## 2. Adapter-Architektur

### 2.1 Oeffentliche Schnittstelle

```cpp
// src/adapters/input/cmake_file_api_adapter.h
class CmakeFileApiAdapter final : public xray::hexagon::ports::driven::BuildModelPort {
public:
    xray::hexagon::model::BuildModelResult load_build_model(
        std::string_view path) const override;
};
```

`load_build_model(path)` nimmt den File-API-Pfad entgegen (Build-Verzeichnis oder Reply-Verzeichnis). Jede Adapter-Instanz bedient genau eine Eingabequelle ueber denselben `BuildModelPort`-Vertrag.

### 2.1.1 Mischpfad-Integration (Vorgriff auf AP 1.3)

Im Mischpfad (beide Eingabequellen angegeben) arbeiten `CompileCommandsJsonAdapter` und `CmakeFileApiAdapter` als zwei getrennte `BuildModelPort`-Instanzen. Die Services erhalten in AP 1.3 eine optionale zweite Port-Referenz:

```cpp
ProjectAnalyzer(const BuildModelPort& compile_database_port,
                const IncludeResolverPort& include_resolver_port,
                const BuildModelPort* file_api_port = nullptr);
```

Der Service ruft jeden Port mit seinem jeweiligen Pfad auf und fuehrt die Ergebnisse zusammen. Das bestehende `BuildModelPort`-Interface bleibt unveraendert; der Mischpfad entsteht durch Komposition, nicht durch Interface-Erweiterung.

### 2.2 Interne Verarbeitungsschritte

```
1. resolve_reply_directory(path)
   → Pfad endet auf /reply → direkt verwenden
   → sonst → path / .cmake/api/v1/reply anhaengen
   → nicht lesbar → BuildModelError (exit 3)

2. find_reply_entry(reply_dir)
   → alle index-*.json und error-*.json im Reply-Verzeichnis auflisten
   → fuer den Vergleich wird nur der Suffix nach dem Praefix verwendet:
     index-<suffix>.json → suffix; error-<suffix>.json → suffix
     (der volle Dateiname darf nicht verglichen werden, weil "index"
     lexikographisch nach "error" kommt und damit ein aelteres
     index-* faelschlich ein neueres error-* verdraengen wuerde)
   → die Datei mit dem lexikographisch groessten Suffix ist die aktuellste
     (CMake verwendet Zeitstempel-basierte Suffixe)
   → wenn die aktuellste Datei ein error-*.json ist → BuildModelError (exit 4);
     ein aelteres index-*.json wird ignoriert, weil die Error-Reply den
     aktuellen Zustand repraesentiert (seit CMake 4.1)
   → wenn die aktuellste Datei ein index-*.json ist → als Index verwenden
   → keine der beiden Dateitypen gefunden → BuildModelError (exit 4)
   → JSON parsen; syntaktisch ungueltig → BuildModelError (exit 4)

3. find_codemodel_reference(index)
   → objects[] nach kind=="codemodel" durchsuchen
   → nicht gefunden → BuildModelError (exit 4)

4. parse_codemodel(reply_dir, jsonFile)
   → codemodel JSON laden
   → paths.source extrahieren → source_root
   → Konfigurationsauswahl:
     wenn configurations.size() == 1 → diese verwenden
     wenn configurations.size() > 1 → file_api_invalid mit Fehlermeldung:
       "codemodel contains <n> configurations; multi-config generators
       are not supported in this version"
     (eine stille Auswahl von configurations[0] wuerde im Mischpfad
     zu einem Konfigurations-Mismatch fuehren koennen, weil seit
     CMake 3.26 auch compile_commands.json Multi-Config-Daten
     enthalten kann; ein harter Fehler ist sicherer als eine
     potentiell falsche Zuordnung)
   → configurations[0].targets[] iterieren

5. parse_targets(reply_dir, target_refs)
   → fuer jede Target-Referenz: target JSON laden
   → wenn ein referenziertes target-JSON nicht ladbar ist (fehlend oder
     syntaktisch ungueltig):
     ein fehlendes, vom Index referenziertes Reply ist laut
     cmake-file-api(7) das erwartete Signal fuer einen konkurrierenden
     CMake-Lauf; der Client soll den neuen Reply-Index erneut lesen
   → Retry: zurueck zu Schritt 2 (find_reply_entry), maximal ein
     Wiederholungsversuch; wenn der neue Index denselben oder einen
     aelteren Suffix hat oder der zweite Versuch ebenfalls ein
     fehlendes Target-Reply antrifft → file_api_invalid
   → sources[], compileGroups[] extrahieren
   → TargetInfo erzeugen (display_name, type, unique_key)

6. build_observations(targets, source_root, codemodel)
   → pro Target, pro Source mit compileGroupIndex:
     - resolved_source_path:
       wenn source.path absolut → normalize(source.path)
       wenn source.path relativ → normalize(source_root / source.path)
     - resolved_directory = normalize(target_build_directory)
       wobei target_build_directory = paths.build / directories[target.directoryIndex].build
       (directories[] hat Felder source und build, nicht path;
       build gibt den relativen Pfad innerhalb des Build-Trees an
       und entspricht dem directory-Feld in compile_commands.json)
     - CompileEntry synthetisieren aus compileGroup-Daten
     - unique_key = resolved_source_path + "|" + resolved_directory
     → dieselbe Normalisierung wie CompileCommandsJsonAdapter

7. build_target_assignments(observations, targets)
   → pro Beobachtung (identifiziert ueber unique_key = source|directory):
     alle Targets sammeln, die genau diese Beobachtung erzeugt haben
   → Zuordnung laeuft ueber den Beobachtungsschluessel, nicht ueber den
     reinen Quellpfad; dieselbe Quelldatei in verschiedenen
     Build-Kontexten erhaelt getrennte Assignments
   → TargetAssignment { observation_key, targets[] }

8. assemble_result()
   → BuildModelResult befuellen
```

### 2.3 Match-Key-Konsistenz

Der Match-Key muss identisch sein mit dem aus `CompileCommandsJsonAdapter`:

```
unique_key = normalized_absolute_source_path + "|" + normalized_absolute_directory
```

Fuer File-API-Beobachtungen:
- `source_path`: wenn `sources[].path` absolut ist, direkt normalisieren; wenn relativ, gegen `codemodel.paths.source` aufloesen
- `directory`: `codemodel.paths.build` + `directories[target.directoryIndex].build` aufloesen; `directories[]` hat die Felder `source` (relativ zum Source-Root) und `build` (relativ zum Build-Root), nicht `path`; `directoryIndex` verweist auf den CMake-Verzeichniskontext (die `CMakeLists.txt`, die das Target definiert), nicht auf eine Build-Konfiguration; Konfigurationen (Debug/Release) liegen unter `configurations[]`

Wichtig: Die Identitaet darf nicht auf `paths.build` allein basieren, weil damit Targets aus verschiedenen CMake-Verzeichnissen auf denselben Key kollabieren wuerden. `directories[].build` differenziert pro CMakeLists.txt-Scope und bildet damit das Aequivalent zum `directory`-Feld in `compile_commands.json`.

Bekannte Einschraenkung (M4): Wenn dieselbe Quelldatei in zwei Targets mit demselben `directoryIndex` aber unterschiedlichen Compile-Gruppen (andere Defines, andere Flags, anderes Output) kompiliert wird, erzeugt der File-API-Adapter pro (Target, Source)-Paar eine eigene `CompileEntry` mit den jeweiligen Compile-Group-Daten. Alle resultierenden Beobachtungen teilen aber denselben `unique_key = source|directory`, weil der Key auf Verzeichnisebene differenziert, nicht auf Compile-Group-Ebene.

Der Downstream-Code behandelt diese Doppelbeobachtungen unterschiedlich:
- `build_ranked_translation_units()` (`analysis_support.cpp:349`) erzeugt fuer jede Beobachtung einen Ranking-Eintrag ohne Deduplizierung — alle Metriken bleiben erhalten
- `index_observations()` im Impact-Analyzer (`impact_analyzer.cpp:29`) verwendet `emplace()` auf dem Key und behaelt damit nur die erste Beobachtung; die Metriken der weiteren gehen fuer die Impact-Zuordnung verloren

Das ist eine neue Situation: im Compile-Database-Pfad erzeugt CMake fuer dieselbe Source in verschiedenen Targets typischerweise verschiedene `directory`-Werte, sodass die Keys unterschiedlich sind. Der bestehende Regressionstest (`tests/e2e/testdata/m2/duplicate_tu_entries`) deckt genau diesen Fall mit `build/debug` vs. `build/release` ab. Im File-API-Pfad koennen dagegen zwei Targets im selben CMake-Verzeichnis dieselbe Source mit demselben Directory-Key kompilieren.

M4 akzeptiert diesen Zustand bewusst als Einschraenkung. Eine Erweiterung des Match-Key um einen Compile-Group-Hash oder Target-Namen wuerde einen adapteruebergreifenden Vertragswechsel erfordern und ist ein moeglicher spaeterer Ausbau.

### 2.4 CompileEntry-Synthese

Die File API liefert keine Kommandozeile, sondern bereits strukturierte Daten: `includes[]`, `defines[]` und `compileCommandFragments[]` sind getrennte Arrays. Der Adapter nutzt primaer die strukturierten Felder, weil sie zuverlaessig als einzelne Tokens vorliegen. `compileCommandFragments` enthalten dagegen opake Shell-Strings, die nicht garantiert 1:1 argv-Tokens sind.

Die Synthese unterscheidet drei Kategorien:

**Strukturierte Includes**: `includes[]` liefert absolute Pfade mit `isSystem`-Flag. Die CMake File API unterscheidet allerdings nicht zwischen `-I` und `-iquote`; beide nicht-System-Include-Typen erscheinen als `isSystem: false`. Damit geht die `-iquote`-Semantik (Quote-Include-Pfade, die der Include-Resolver fuer `#include "..."` nutzt) im File-API-Pfad verloren. Das ist ein bekannter Genauigkeitsverlust: der Include-Resolver behandelt alle nicht-System-Includes als regulaere `-I`-Pfade.

**Strukturierte Defines**: `defines[]` liefert Defines im Format `NAME` oder `NAME=VALUE`, die als `-DNAME` bzw. `-DNAME=VALUE` eingefuegt werden.

**Fragments**: `compileCommandFragments` werden bewusst nicht in die arguments-Liste aufgenommen. Begruendung:

- Fragments sind in nativer Shell-Syntax kodiert und nicht garantiert 1:1 argv-Tokens.
- CMake legt dieselben `-I`- und `-D`-Informationen sowohl in den strukturierten Feldern (`includes[]`, `defines[]`) als auch in den Fragments ab. Wuerden Fragments als Argumente eingefuegt, wuerde der nachgelagerte Parser in `analysis_support.cpp` Inline-Flags wie `-I/path` und `-DNAME` erkennen und `include_path_count` bzw. `define_count` doppelt zaehlen. Das verfaelscht Ranking und Hotspots.
- `-iquote`-Information geht verloren (siehe bekannter Genauigkeitsverlust unten), waere aber auch ueber Fragments nicht zuverlaessig rueckgewinnbar, weil die File API `-iquote`-Pfade in `includes[]` als `isSystem: false` fuehrt und Fragments denselben Pfad redundant enthalten koennen.

```cpp
std::vector<std::string> arguments;
arguments.push_back(language == "CXX" ? "c++" : "cc");

// Strukturierte Daten als korrekt getrennte Tokens:
for (const auto& inc : includes) {
    arguments.push_back(inc.isSystem ? "-isystem" : "-I");
    arguments.push_back(inc.path);   // immer als separates Token
}
for (const auto& def : defines) {
    arguments.push_back("-D" + def.define);
}

// compileCommandFragments werden NICHT aufgenommen:
// sie wuerden include_path_count und define_count doppelt zaehlen.

arguments.push_back("-c");
arguments.push_back(source_path);
```

Damit basiert `arg_count` im File-API-Pfad ausschliesslich auf den strukturierten Feldern (Compiler-Platzhalter, Includes, Defines, Source), waehrend `include_path_count` und `define_count` korrekt bleiben.

Bewusst akzeptierte Ranking-Abweichung: Die Scoring-Formel `score = arg_count + include_path_count + define_count` und der erste Tiebreaker `arg_count` (`analysis_support.cpp:168`) sind direkt von `arg_count` abhaengig. Da im File-API-Pfad Fragments (z.B. `-std=c++20`, `-Wall`, `-O2`) nicht gezaehlt werden, ist `arg_count` systematisch niedriger als im Compile-Database-Pfad. Das kann die Rangfolge von `derived`-Beobachtungen veraendern, wenn zwei TUs sich nur in der Anzahl sonstiger Compiler-Flags unterscheiden. Diese Abweichung wird akzeptiert, weil:

- `include_path_count` und `define_count` — die fachlich aussagekraeftigeren Metriken — sind korrekt
- die Alternative (Fragments aufnehmen) wuerde `include_path_count` und `define_count` doppelt zaehlen, was die fachlich wichtigeren Metriken verfaelscht
- `derived`-Beobachtungen werden im Report als solche gekennzeichnet; der Nutzer sieht, dass die Datengrundlage eine andere ist
- innerhalb des File-API-Pfads ist die Rangfolge konsistent, weil alle Beobachtungen denselben Informationsverlust haben

Weitere bekannte Genauigkeitsverluste:
- `-iquote`-Include-Pfade sind im File-API-Pfad nicht von regulaeren `-I`-Pfaden unterscheidbar. Der Include-Resolver behandelt alle nicht-System-Includes als `-I`.

## 3. Fehlerbehandlung

### 3.1 Erweiterung von `CompileDatabaseError`

Die bestehenden Fehlerwerte (`file_not_accessible`, `invalid_json` etc.) tragen Compile-Database-spezifische Semantik und loesen im CLI-Adapter Hinweise wie "check the path or generate the compilation database" aus. Fuer File-API-Fehler sind diese Hinweise irrefuehrend.

Deshalb wird `CompileDatabaseError` um zwei File-API-spezifische Werte erweitert:

```cpp
enum class CompileDatabaseError {
    none,
    file_not_accessible,
    invalid_json,
    not_an_array,
    empty_database,
    invalid_entries,
    // M4: File-API-spezifische Fehler
    file_api_not_accessible,   // Reply-Pfad nicht lesbar → exit 3
    file_api_invalid,          // ungueltige oder unzureichende Reply-Daten → exit 4
};
```

Die Erweiterung von `CompileDatabaseError` ist Bestandteil von AP 1.2, weil der Adapter diese Werte produzieren muss. Die Anpassung von `map_error_to_exit_code()` und `format_error()` in `cli_adapter.cpp` gehoert ebenfalls zu AP 1.2, damit die neuen Fehlerwerte korrekt auf Exit-Codes und Hinweise abgebildet werden:
- `file_api_not_accessible` → exit 3, Hinweis auf Reply-Pfad und cmake-Konfiguration
- `file_api_invalid` → exit 4, Hinweis auf fehlende oder ungueltige Reply-Daten

### 3.2 Fehlerzuordnung

| Situation | Ergebnis | Exit-Code |
|---|---|---|
| Reply-Verzeichnis nicht lesbar | `error = file_api_not_accessible` | 3 |
| Kein `index-*.json` gefunden | `error = file_api_invalid` | 4 |
| Index-JSON syntaktisch ungueltig | `error = file_api_invalid` | 4 |
| Kein Codemodel in `objects[]` | `error = file_api_invalid` | 4 |
| Codemodel-JSON syntaktisch ungueltig | `error = file_api_invalid` | 4 |
| Codemodel ohne `paths.source` | `error = file_api_invalid` | 4 |
| Keine auswertbaren Targets/Sources | `error = file_api_invalid` | 4 |
| Codemodel mit `configurations.size() > 1` | `error = file_api_invalid`; Multi-Config wird in M4 nicht unterstuetzt | 4 |
| Referenziertes Target-JSON nicht ladbar (erster Versuch) | Retry: neuen Index suchen und erneut parsen (max. ein Versuch); spezifikationskonformes Verhalten bei konkurrierendem CMake-Lauf | — |
| Referenziertes Target-JSON nicht ladbar (nach Retry) | `error = file_api_invalid` | 4 |
| Source ohne `compileGroupIndex` | Source wird uebersprungen (kein Compile-Kontext) | 0 |
| Targets ohne kompilierbare Sources (z.B. reine INTERFACE_LIBRARY) | Target wird uebersprungen; kein Eintrag in `target_assignments`, weil `TargetAssignment` zwingend einen `observation_key` benoetigt und ohne Beobachtung kein Key existiert; das Target erscheint nicht in den Analyseergebnissen | 0 |

Bestehende Compile-Database-Fehlerwerte und ihre Hinweise bleiben unveraendert.

## 4. Referenzdaten und Fixtures

### 4.1 Minimales Fixture (`tests/e2e/testdata/m4/file_api_only/`)

```
build/
  .cmake/api/v1/reply/
    index-2025-01-01.json
    codemodel-v2-abc123.json
    target-app-def456.json
    target-core-ghi789.json
include/
  common/
    config.h
src/
  main.cpp
  core.cpp
```

Dieses Fixture deckt ab:
- File-API-Only-Pfad fuer `analyze` und `impact`
- Zwei Targets (`app` = EXECUTABLE, `core` = STATIC_LIBRARY)
- `main.cpp` gehoert nur zu `app`
- `core.cpp` gehoert nur zu `core`
- `config.h` wird von beiden Targets per Include eingebunden

### 4.2 Fixture fuer Mehrfachzuordnung (`tests/e2e/testdata/m4/multi_target/`)

- `shared.cpp` gehoert zu `app` und `core` (im selben CMake-Verzeichnis, also gleicher `directoryIndex`)
- Der Adapter erzeugt zwei `CompileEntry`-Objekte (eine pro Target) mit demselben `unique_key`
- Im Ranking erscheinen beide Beobachtungen (keine Deduplizierung)
- `target_assignments` enthaelt einen Eintrag fuer den gemeinsamen Key mit beiden Targets im `targets`-Vektor

### 4.3 Fixture fuer fehlendes Target-Reply (`tests/e2e/testdata/m4/missing_target_reply/`)

- Codemodel referenziert zwei Targets, aber ein Target-JSON fehlt → `file_api_invalid`
- Stellt sicher, dass partielle Momentaufnahmen nicht als Erfolg behandelt werden

### 4.3.1 Fixture fuer Mischpfad (`tests/e2e/testdata/m4/partial_targets/`)

- Vollstaendige File-API-Daten, aber nicht alle TUs haben Target-Zuordnungen (z.B. INTERFACE_LIBRARY ohne kompilierbare Sources)
- Braucht `compile_commands.json` fuer den Mischpfad-Test

### 4.4 Fixture fuer mehrere Index-Dateien (`tests/e2e/testdata/m4/multiple_indexes/`)

- Zwei `index-*.json` mit unterschiedlichen Zeitstempeln im Dateinamen
- Nur der lexikographisch letzte zeigt auf ein gueltiges Codemodel
- Stellt sicher, dass die Auswahlregel deterministisch und korrekt ist

### 4.5 Fixture fuer ungueltige Daten (`tests/e2e/testdata/m4/invalid_file_api/`)

- `index-*.json` mit ungueltigem JSON
- Braucht `compile_commands.json` damit der Mischpfad-Fehlerfall testbar ist

## 5. Tests

### 5.1 Adapter-Unit-Tests (`tests/adapters/test_cmake_file_api_adapter.cpp`)

Erfolgsfaelle:
- Valides Reply-Verzeichnis → `is_success()`, korrekte Entries, Targets, source_root
- Build-Verzeichnis als Eingabe → gleiche Ergebnisse wie Reply-Verzeichnis direkt
- `source` ist `ObservationSource::derived`
- `target_metadata` ist `loaded` bei vollstaendigen Daten
- Mehrfachzuordnung: Source in zwei Targets mit gleichem `directoryIndex` → zwei `CompileEntry`-Objekte mit demselben `unique_key`; Ranking enthaelt beide Eintraege; `target_assignments` enthaelt einen Eintrag mit zwei Targets (siehe bekannte Einschraenkung in 2.3 zum Impact-Pfad)
- Source ohne `compileGroupIndex` wird uebersprungen, keine Beobachtung erzeugt

Fehlerfaelle:
- Nicht lesbarer Pfad → `file_api_not_accessible`
- Kein Index-File → `file_api_invalid`
- Ungueltige Index-JSON → `file_api_invalid`
- Kein Codemodel in Index → `file_api_invalid`
- Ungueltige Codemodel-JSON → `file_api_invalid`
- Leeres Codemodel (keine Targets) → `file_api_invalid`
- Referenziertes Target-JSON fehlt, kein neuerer Index verfuegbar → `file_api_invalid` nach Retry

Reply-Auswahl:
- Mehrere `index-*.json` im Reply-Verzeichnis → deterministisch den lexikographisch letzten waehlen; Ergebnis bleibt stabil
- `error-*.json` ist juenger als `index-*.json` → `file_api_invalid`, aelteres Index wird ignoriert
- Nur `index-*.json` vorhanden, kein `error-*.json` → normaler Erfolgspfad

Reproduzierbarkeit:
- Permutierte Target-Reihenfolge in der Codemodel-Datei → identische Ergebnisse

### 5.2 CLI-Tests fuer File-API-Fehlercode-Mapping (`tests/e2e/test_cli.cpp`)

Die neuen Fehlerwerte `file_api_not_accessible` und `file_api_invalid` werden in AP 1.2 ueber Stub-Ports getestet, analog zum bestehenden Muster in `test_cli.cpp:398ff` (StubAnalyzeProjectPort mit vorab konstruiertem AnalysisResult). Damit wird das Fehlercode-Mapping im CLI-Adapter geprueft, ohne dass der CmakeFileApiAdapter verdrahtet sein muss:

- StubAnalyzeProjectPort liefert `file_api_not_accessible` → exit 3, Hinweis erwaehnt File API Reply-Pfad
- StubAnalyzeProjectPort liefert `file_api_invalid` → exit 4, Hinweis erwaehnt File API Reply-Daten

Volle E2E-Tests, die `--cmake-file-api` auf echten Fixtures mit dem verdrahteten Adapter ausfuehren, gehoeren zu AP 1.3, weil sie die Integration in `main.cpp` und die Analyser voraussetzen.

### 5.3 Kern-Tests fuer Match-Key-Konsistenz

- Gleiche Source-Datei, gleicher Build-Kontext → identischer `unique_key` aus beiden Adaptern
- Verschiedene Build-Kontexte (verschiedene Target-Verzeichnisse) → verschiedene `unique_key`s
- Includes und Defines aus strukturierten File-API-Feldern werden zuverlaessig als Ranking-Metriken erkannt (arg_count, include_path_count, define_count)

## 6. Reihenfolge

| Schritt | Aufgabe |
|---|---|
| 1 | Fixtures unter `tests/e2e/testdata/m4/` anlegen (minimale Reply-JSON-Dateien) |
| 2 | `CmakeFileApiAdapter` Header und Grundstruktur (`resolve_reply_directory`, `find_and_parse_index`) |
| 3 | Codemodel-Parsing und Target-Parsing implementieren |
| 4 | Observation-Aufbau und Target-Assignment-Logik |
| 5 | Fehlerbehandlung und Diagnostics |
| 6 | Adapter-Tests schreiben |
| 7 | `CMakeLists.txt` erweitern, Build verifizieren |

## 7. Abgrenzung

Nicht Bestandteil von AP 1.2:

- Verdrahtung des Adapters in `main.cpp` und Aufruflogik in `cli_adapter.cpp` (→ AP 1.3); die Fehlercode-Erweiterung in `cli_adapter.cpp` ist dagegen Bestandteil von AP 1.2
- Integration in `ProjectAnalyzer` und `ImpactAnalyzer` (→ AP 1.3)
- Reporter-Aenderungen (→ AP 1.4)
- Query-Writing oder CMake-Ausfuehrung
- Target-Abhaengigkeiten (`dependencies[]`)
- Multi-Config-Generatoren: M4 behandelt `configurations.size() > 1` als harten Fehler (`file_api_invalid`), nicht als Diagnostic; seit CMake 3.26 kann auch `compile_commands.json` Multi-Config-Daten enthalten, sodass eine stille Auswahl von `configurations[0]` zu falschem Konfigurations-Matching im Mischpfad fuehren wuerde; Multi-Config-Unterstuetzung ist ein moeglicher spaeterer Ausbau mit expliziter `--configuration`-Option
