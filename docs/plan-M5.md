# Plan M5 - Weitere Reportformate, Release-Artefakte und CLI-Ausgabemodi (`v1.2.0`)

## 0. Dokumenteninformationen

| Feld | Wert |
|---|---|
| Dokument | Plan M5 `cmake-xray` |
| Version | `0.2` |
| Stand | `2026-04-25` |
| Status | Entwurf |
| Referenzen | [Lastenheft](./lastenheft.md), [Design](./design.md), [Architektur](./architecture.md), [Phasenplan](./roadmap.md), [Plan M4](./plan-M4.md), [Qualitaet](./quality.md), [Releasing](./releasing.md) |

### 0.1 Zweck
Dieser Plan beschreibt die konkreten Schritte fuer Milestone M5 (`v1.2.0`). Ziel ist der Ausbau nach M4: `cmake-xray` soll die vorhandenen Analyseergebnisse in weiteren Ausgabeformaten bereitstellen, maschinenlesbare Formate versionieren, einen offiziellen Release-Pfad mit Linux-Artefakt und OCI-Image absichern, erste Portabilitaetsnachweise fuer macOS und Windows liefern und die CLI um detailreiche bzw. reduzierte Ausgabemodi erweitern.

M5 erweitert primaer die Nutzbarkeit und Auslieferbarkeit des bereits vorhandenen Analyseumfangs. Neue fachliche Target-Graph-Analysen bleiben bewusst ausserhalb dieses Meilensteins und werden in Phase 6 geplant.

### 0.2 Abschlusskriterium
M5 gilt als erreicht, wenn:

- `cmake-xray analyze` und `cmake-xray impact` neben `console` und `markdown` auch `html`, `json` und `dot` als Ausgabeformate akzeptieren
- `--output <path>` fuer `markdown`, `html`, `json` und `dot` bei `analyze` und `impact` freigeschaltet ist und Reportdateien atomar schreibt, sodass bei Fehlern keine halb geschriebenen Zielartefakte als Erfolg zurueckbleiben
- der HTML-Export eigenstaendige, deterministische Berichte fuer Projektanalyse und Impact-Analyse erzeugt, ohne externe Laufzeitressourcen zu benoetigen
- der JSON-Export fuer Projektanalyse und Impact-Analyse ein dokumentiertes Schema mit Formatversion, Pflichtfeldern, Typen, Enums, Nullability-Regeln und Array-Regeln ausgibt und fuer Automatisierung stabil nutzbar ist
- der DOT-Export fuer die in M5 vorhandene Datenlage sinnvolle Graphviz-Diagramme erzeugt, ohne noch nicht implementierte Target-Graph-Semantik vorwegzunehmen
- `--verbose` diagnoseorientierte Zusatzinformationen ausgibt, ohne maschinenlesbare Standardausgaben unbrauchbar zu machen
- `--quiet` fuer erfolgreiche Aufrufe reduzierte Ausgabe liefert und Fehler weiterhin verstaendlich auf `stderr` meldet
- ein tag-basierter Release-Workflow ein versioniertes Linux-CLI-Artefakt mit Pruefsumme erzeugt
- ein OCI-kompatibles Container-Image fuer lokale Nutzung und CI erzeugt und dokumentiert wird
- README, Release-Dokumentation und Beispiele die Nutzung ohne interne Build-Pfade wie `./build/cmake-xray` beschreiben
- macOS- und Windows-Builds mindestens in CI oder dokumentierten Smoke-Checks bewertet sind; bekannte Einschraenkungen werden explizit dokumentiert
- automatisierte Adapter-, CLI-, Golden-Output- und Release-Smoke-Tests die neuen Formate, Modi und Bereitstellungspfade absichern

Relevante Kennungen: `F-28`, `F-29`, `F-30`, `F-39`, `F-40`, `F-43`, `F-44`, `NF-08`, `NF-09`, `NF-20`, `NF-21`, `S-12`, `S-13`

### 0.3 Abgrenzung
Bestandteil von M5 sind:

- `HtmlReportAdapter` fuer Projektanalyse und Impact-Analyse
- `JsonReportAdapter` fuer Projektanalyse und Impact-Analyse mit dokumentierter Formatversion
- `DotReportAdapter` fuer graphische Sicht auf die vorhandenen Analyseergebnisse
- CLI-Formatwahl fuer `html`, `json` und `dot`
- CLI-Dateiausgabe ueber `--output` fuer alle artefaktorientierten Formate `markdown`, `html`, `json` und `dot`
- CLI-Modi `--verbose` und `--quiet`
- tag-basierter Release-Workflow fuer Linux-CLI-Artefakt und OCI-Image
- Installations- und Nutzungsdokumentation fuer Release-Artefakte und Container
- Portabilitaetspruefung fuer macOS und Windows auf Build- und Smoke-Test-Niveau
- Referenzdaten, Golden-Outputs und Beispiele fuer die neuen Ausgabeformate

Nicht Bestandteil von M5 sind:

- direkte Target-Graph-Analysen und textuelle Darstellung direkter Target-Abhaengigkeiten (`F-18`)
- Hervorhebung von Targets mit vielen ein- oder ausgehenden Abhaengigkeiten (`F-20`)
- Priorisierung betroffener Targets ueber den Target-Graphen hinweg (`F-25`)
- verfeinerte Include-Sicht jenseits des MVP (`F-16`, `F-17`)
- erweiterte Analysekonfiguration, Schwellenwerte und Vergleichsansichten (`F-10`, `F-11`, `F-37`, `F-38`)
- automatische Erzeugung von CMake-File-API-Query-Dateien oder implizite CMake-Ausfuehrung
- vollstaendige funktionale Freigabe fuer alle macOS- und Windows-Varianten; M5 liefert zunaechst belastbare Build-/Smoke-Aussagen

M5 baut auf der M4-Target-Sicht auf. Wenn File-API-Daten vorhanden sind, duerfen HTML, JSON und DOT dieselben Target-Zuordnungen darstellen wie Konsole und Markdown. Ohne File-API-Daten bleiben Target-Abschnitte leer oder entfallen nach klar dokumentierter Formatregel.

## 1. Arbeitspakete

### 1.1 Report-Vertraege fuer neue Formate und Formatversionierung schaerfen

Die M3-/M4-Reportstrecke ist textorientiert. Fuer M5 muessen weitere Formate angebunden werden, ohne Fachlogik in einzelne Adapter zu verschieben oder bestehende Konsolen- und Markdown-Vertraege zu destabilisieren.

Fuer M5 benoetigt der Report-Pfad mindestens:

- eine erweiterte Formatwahl in CLI und Composition Root fuer `console`, `markdown`, `html`, `json` und `dot`
- eine erweiterte `--output`-Validierung fuer `markdown`, `html`, `json` und `dot` bei `analyze` und `impact`; `console` bleibt standardmaessig stdout-orientiert
- ein gemeinsamer Schreibpfad fuer Reportdateien, der Zielartefakte atomar ueber temporaere Datei und Rename ersetzt und Fehler sauber an die CLI meldet
- klare Adaptergrenzen: Jeder Report-Adapter rendert ausschliesslich vorhandene `AnalysisResult`- bzw. `ImpactResult`-Modelle
- gemeinsame Hilfsfunktionen fuer stabile Sortierung, Text-Escaping, Pfadanzeige und Diagnostics, soweit dadurch Dopplung zwischen Adaptern reduziert wird
- eine dokumentierte Formatversion fuer maschinenlesbare JSON-Ausgaben
- eine explizite Regel, welche Ergebnisfelder fuer DOT graphisch modelliert werden und welche nur als Label oder Kommentar erscheinen

Wichtig:

- `console` und `markdown` duerfen sich fuer bestehende M3-/M4-Eingaben ohne neue Optionen nicht aendern
- Formatadapter duerfen keine neuen Impact- oder Ranking-Entscheidungen treffen
- JSON ist der einzige maschinenlesbare Vertragsausdruck in M5; HTML und DOT bleiben menschen- bzw. visualisierungsorientiert
- stdout bleibt der Standard, wenn kein `--output` angegeben ist; mit `--output` wird der vollstaendige Reportinhalt in die Datei geschrieben und nicht zusaetzlich fachlich nach stdout dupliziert
- alle neuen Formate muessen deterministisch sein, damit Golden-Outputs sinnvoll diffbar bleiben
- Datums-, Laufzeit- oder Hostinformationen duerfen nicht automatisch in Reports erscheinen

Vorgesehene Artefakte:

- Anpassung der CLI-Formatvalidierung in `src/adapters/cli/`
- Anpassung der CLI-Output-Validierung und der atomaren Dateiausgabe fuer Reportartefakte
- Anpassung der Composition-Root-Verdrahtung in `src/main.cpp`
- neue Adapter unter `src/adapters/output/`
- Erweiterung von `src/adapters/CMakeLists.txt`
- Dokumentation der JSON-Formatversion in `docs/`

**Ergebnis**: Der Report-Pfad ist fuer weitere Ausgabeformate offen, ohne die fachliche Verantwortung aus dem Hexagon in Adapter zu verlagern.

### 1.2 `JsonReportAdapter` fuer stabile maschinenlesbare Ausgaben umsetzen

Der JSON-Export ist fuer Automatisierung, CI-Auswertung und Folgewerkzeuge gedacht. Deshalb muss das Format stabiler und expliziter sein als die menschenlesbaren Textberichte.

Ein JSON-Bericht fuer `analyze` soll mindestens enthalten:

- `format`: fester Formatbezeichner, zum Beispiel `cmake-xray.analysis`
- `format_version`: Schema-/Formatversion, initial `1`
- `report_type`: `analyze`
- `inputs`: verwendete Eingabequellen und Anzeige-/Normalisierungspfade
- `summary`: Translation-Unit-Anzahl, Ranking-Anzahl, Hotspot-Anzahl, Top-Limit, Beobachtungsherkunft und Target-Metadatenstatus
- `translation_unit_ranking`: deterministisch sortierte Ranking-Eintraege inklusive Metriken, Diagnostics und Target-Zuordnungen
- `include_hotspots`: deterministisch sortierte Include-Hotspot-Eintraege
- `diagnostics`: reportweite Diagnostics

Ein JSON-Bericht fuer `impact` soll mindestens enthalten:

- `format`: fester Formatbezeichner, zum Beispiel `cmake-xray.impact`
- `format_version`: Schema-/Formatversion, initial `1`
- `report_type`: `impact`
- `inputs`: verwendete Eingabequellen und geaenderte Datei
- `summary`: Anzahl betroffener Translation Units, Klassifikation, Beobachtungsherkunft, Target-Metadatenstatus und Anzahl betroffener Targets
- `directly_affected_translation_units`
- `heuristically_affected_translation_units`
- `directly_affected_targets`
- `heuristically_affected_targets`
- `diagnostics`

Wichtig:

- JSON-Ausgaben muessen gueltiges UTF-8 und syntaktisch gueltiges JSON sein
- Felder mit leerer Menge werden als leere Arrays ausgegeben, nicht weggelassen, sofern sie Teil des Formatvertrags sind
- optionale fachliche Informationen koennen `null` sein, wenn das Schema dies explizit dokumentiert
- numerische Metriken bleiben numerisch und werden nicht als lokalisierte Strings gerendert
- die Reihenfolge von Objektfeldern soll im Adapter stabil bleiben, auch wenn JSON semantisch keine Feldreihenfolge garantiert
- Pfade werden als Anzeige-Strings ausgegeben; kanonische Normalisierungsschluessel duerfen nur aufgenommen werden, wenn sie keinen instabilen Hostbezug in Goldens einfuehren
- `docs/report-json.md` ist der verbindliche M5-Vertrag und dokumentiert fuer alle Felder Typ, Pflichtstatus, erlaubte Enum-Werte, Nullability, leere-Array-Regeln und Aenderungsregeln fuer kuenftige `format_version`-Erhoehungen
- Tests pruefen nicht nur syntaktisch gueltiges JSON, sondern auch Pflichtfelder, Array-statt-Weglassen-Regeln, numerische Typen, bekannte Enum-Werte und `null` nur an dokumentierten Stellen

Vorgesehene Artefakte:

- `src/adapters/output/json_report_adapter.h`
- `src/adapters/output/json_report_adapter.cpp`
- `tests/adapters/test_json_report_adapter.cpp`
- JSON-Golden-Outputs unter `tests/e2e/testdata/m5/`
- Dokumentation `docs/report-json.md` als verbindlicher Formatvertrag; `docs/guide.md` verweist darauf nur nutzungsorientiert

**Ergebnis**: `cmake-xray` besitzt einen versionierten, automatisierbaren Reportvertrag fuer Analyse- und Impact-Ergebnisse.

### 1.3 `DotReportAdapter` fuer Graphviz-Ausgaben umsetzen

Der DOT-Export soll vorhandene Analyseergebnisse visualisieren, ohne M6-Target-Graph-Funktionen vorwegzunehmen. M5 darf deshalb nur Graphen aus bereits vorhandenen Daten bilden.

Fuer `analyze` ist in M5 ein Graph sinnvoll, der mindestens folgende Knotenarten abbildet:

- Translation Units
- Include-Hotspots bzw. Include-Dateien, sofern sie im Ergebnis vorhanden sind
- Targets, sofern M4-Target-Zuordnungen geladen wurden

Kanten in `analyze`:

- Translation Unit zu Include-Hotspot, wenn der Hotspot fuer diese Sicht belastbar aus dem Ergebnis ableitbar ist
- Translation Unit zu Target, wenn eine Target-Zuordnung vorhanden ist

Fuer `impact` ist in M5 ein Graph sinnvoll, der mindestens folgende Knotenarten abbildet:

- geaenderte Datei
- direkt betroffene Translation Units
- heuristisch betroffene Translation Units
- betroffene Targets, sofern Target-Zuordnungen vorhanden sind

Kanten in `impact`:

- geaenderte Datei zu direkt betroffener Translation Unit
- geaenderte Datei zu heuristisch betroffener Translation Unit mit unterscheidbarem Style
- betroffene Translation Unit zu Target

Wichtig:

- es werden keine Target-zu-Target-Kanten erzeugt, solange `F-18` nicht umgesetzt ist
- DOT-IDs muessen deterministisch und korrekt escaped sein
- Labels duerfen fuer Lesbarkeit gekuerzt werden, wenn der vollstaendige Pfad als Attribut erhalten bleibt
- der Graph muss auch fuer leere Ergebnisse gueltiges DOT ausgeben
- die Ausgabe ist ein Graphviz-Quelltext, keine gerenderte SVG-/PNG-Datei
- Adapter- und Golden-Tests enthalten Sonderzeichenfaelle fuer IDs und Labels, zum Beispiel Leerzeichen, Anfuehrungszeichen, Backslashes und plattformtypische Pfadtrenner

Vorgesehene Artefakte:

- `src/adapters/output/dot_report_adapter.h`
- `src/adapters/output/dot_report_adapter.cpp`
- `tests/adapters/test_dot_report_adapter.cpp`
- DOT-Golden-Outputs unter `tests/e2e/testdata/m5/`
- Validierung der DOT-Goldens mit `dot -Tsvg` in CI, sofern Graphviz verfuegbar ist; andernfalls dokumentierter Parser-/Syntax-Smoke mit demselben Escaping-Fokus
- Nutzungsbeispiel fuer `dot -Tsvg` in der Dokumentation

**Ergebnis**: Nutzer koennen vorhandene Analyse- und Impact-Ergebnisse in Graphviz-Werkzeuge ueberfuehren, ohne dass M5 eine neue fachliche Graphanalyse einfuehrt.

### 1.4 `HtmlReportAdapter` fuer eigenstaendige HTML-Berichte umsetzen

Der HTML-Export ist der wichtigste menschenlesbare Ausbau in M5. Er soll die Markdown-Informationen komfortabler lesbar machen, aber keine dynamische Webanwendung einfuehren.

Fuer HTML gilt:

- jeder Bericht ist eine vollstaendige HTML-Datei
- CSS wird inline eingebettet, damit Artefakte in CI und Releases ohne Begleitdateien nutzbar sind
- es werden keine externen Fonts, Skripte oder CDN-Ressourcen geladen
- Tabellen, Zusammenfassungen und Diagnostics muessen ohne JavaScript nutzbar sein
- Target-Zuordnungen aus M4 werden sichtbar dargestellt, wenn vorhanden
- leere Ergebnisabschnitte erhalten klare Leersaetze analog zu Markdown

Wichtig:

- alle nutzergelieferten Inhalte und Pfade muessen HTML-escaped werden
- HTML-Ausgaben duerfen keine Hostnamen, Zeitstempel oder absoluten Build-Umgebungsdetails ergaenzen, die nicht bereits Teil der fachlichen Ergebnisdaten sind
- die visuelle Struktur soll stabil genug fuer Golden-Tests sein; kosmetische Zufallselemente sind nicht zulaessig
- Barrierearme Basisstruktur ist Pflicht: sinnvolle Ueberschriftenhierarchie, Tabellenueberschriften und ausreichender Kontrast

Vorgesehene Artefakte:

- `src/adapters/output/html_report_adapter.h`
- `src/adapters/output/html_report_adapter.cpp`
- `tests/adapters/test_html_report_adapter.cpp`
- HTML-Golden-Outputs unter `tests/e2e/testdata/m5/`
- Beispielberichte unter `docs/examples/`

**Ergebnis**: `cmake-xray` kann Analyse- und Impact-Ergebnisse als portables HTML-Artefakt fuer Reviews, CI und Dokumentation bereitstellen.

### 1.5 CLI-Modi `--verbose` und `--quiet` einfuehren

M5 erweitert die Bedienbarkeit um zwei Ausgabemodi. Beide Modi duerfen die bestehende Fehlersemantik nicht verwischen.

`--verbose` soll mindestens:

- reportweite Diagnostics vollstaendig ausgeben
- Eingabequellen, Beobachtungsherkunft und Target-Metadatenstatus sichtbar machen, sofern nicht ohnehin im Format enthalten
- bei CLI-Fehlern zusaetzliche Kontextinformationen liefern, ohne Stacktraces oder interne Implementierungsdetails auszugeben
- fuer `json` keine unstrukturierte Zusatztextausgabe auf `stdout` erzeugen; falls noetig, gehen Diagnosehinweise nach `stderr` oder in strukturierte JSON-Felder

`--quiet` soll mindestens:

- bei erfolgreichem `console`-Aufruf die Ausgabe auf eine knappe Zusammenfassung reduzieren
- bei Dateiausgabe nur notwendige Erfolgsmeldungen ausgeben oder ganz schweigen
- Warnungen und Fehler weiterhin auf `stderr` ausgeben
- fuer `json`, `html`, `markdown` und `dot` die Reportdatei bzw. den Reportinhalt nicht fachlich kuerzen, wenn das Format als Ergebnisartefakt angefordert wurde

Entscheidungen fuer M5:

- `--verbose` und `--quiet` sind gegenseitig exklusiv
- beide Optionen gelten fuer `analyze` und `impact`
- Standardverhalten ohne beide Optionen bleibt rueckwaertskompatibel
- quiet beeinflusst nicht Exit-Codes
- verbose darf Goldens nur fuer explizite Verbose-Testfaelle aendern

Vorgesehene Artefakte:

- Anpassung von `src/adapters/cli/cli_adapter.*`
- Anpassung von `src/main.cpp`
- gegebenenfalls kleines CLI-Modell fuer `OutputVerbosity`
- Tests in `tests/e2e/test_cli.cpp`
- Golden-Outputs fuer quiet/verbose unter `tests/e2e/testdata/m5/`

**Ergebnis**: Die CLI kann sowohl diagnoseorientiert als auch automationsfreundlich knapp betrieben werden.

### 1.6 Release-Artefakte und OCI-Image absichern

M5 macht die Bereitstellung aus dem Lastenheft verbindlich. Ziel ist nicht nur ein lokaler Docker-Build, sondern ein reproduzierbarer, tag-basierter Release-Pfad.

Der Release-Pfad muss mindestens:

- bei Tag-Push mit erlaubtem Semver-Tag einen Release-Workflow starten: regulaere Releases verwenden `vMAJOR.MINOR.PATCH`, Vorabversionen verwenden `vMAJOR.MINOR.PATCH-PRERELEASE`
- Tags vor dem Build fail-fast validieren und unbekannte Muster wie `vfoo`, unvollstaendige Versionen oder Build-Metadaten ohne dokumentierte Freigabe abbrechen
- Test-, Coverage-, Quality- und Runtime-Pfade vor dem Veroeffentlichen ausfuehren
- ein Linux-CLI-Artefakt als `.tar.gz` erzeugen
- im Archiv mindestens die ausfuehrbare Datei, Lizenz-/Hinweisdateien und kurze Nutzungsdokumentation enthalten
- eine SHA-256-Pruefsumme erzeugen
- ein OCI-kompatibles Runtime-Image bauen
- das Image mit dem Versions-Tag veroeffentlichen
- fuer regulaere Releases ohne Prerelease-Suffix zusaetzlich `latest` pflegen und Vorabversionen davon ausnehmen
- GitHub-Release-Notes aus dem Changelog verwenden

Wichtig:

- Release-Artefakte duerfen nicht von lokalen Build-Pfaden abhaengen
- der Runtime-Container muss `cmake-xray --help` ohne weitere Toolchain ausfuehren koennen
- die Dokumentation muss lokale Nutzung und CI-Nutzung des Containers zeigen
- fehlgeschlagene Release-Schritte muessen klar sichtbar sein und keine Teilveroeffentlichung als Erfolg melden
- die Release-Dokumentation muss die erlaubten Tag-Muster, Prerelease-Behandlung und `latest`-Regel explizit nennen

Vorgesehene Artefakte:

- Pruefung und ggf. Anpassung von `.github/workflows/release.yml`
- Pruefung und ggf. Anpassung von `Dockerfile`
- Aktualisierung von `docs/releasing.md`
- Aktualisierung von `README.md`
- Release-Smoke-Tests oder dokumentierte manuelle Pruefschritte

**Ergebnis**: Nutzer koennen `cmake-xray` als versioniertes Linux-Artefakt oder Container-Image nutzen, ohne ein lokales Build-Verzeichnis zu kennen.

### 1.7 Erweiterte Plattformunterstuetzung bewerten

M5 soll macOS und Windows nicht vollstaendig produktisieren, aber belastbar feststellen, welche Teile funktionieren und welche Einschraenkungen bestehen.

Fuer macOS:

- Build mit einem gaengigen Clang-/CMake-Setup
- Ausfuehrung der Unit-Tests, soweit ohne Linux-spezifische Annahmen moeglich
- CLI-Smoke-Test fuer `--help`, `analyze` und `impact`
- Bewertung von Pfadnormalisierung und Zeilenenden in Goldens

Fuer Windows:

- Build mit einem gaengigen MSVC- oder ClangCL-/CMake-Setup
- Ausfuehrung der Unit-Tests, soweit ohne POSIX-spezifische Annahmen moeglich
- CLI-Smoke-Test fuer `--help`, `analyze` und `impact`
- Bewertung von Laufwerksbuchstaben, Backslashes und CRLF in Report-Ausgaben

Fuer CMake-/Compiler-Kompatibilitaet:

- dokumentierte Mindestversionen bleiben explizit
- mindestens eine aktuelle CMake-Version wird in CI oder Smoke-Checks verwendet
- bekannte Einschraenkungen der CMake File API aus M4 bleiben dokumentiert

Vorgesehene Artefakte:

- CI-Matrix oder dokumentierte Smoke-Check-Skripte fuer macOS und Windows
- Aktualisierung von `docs/quality.md`
- Aktualisierung von `docs/releasing.md`
- falls noetig kleine Portabilitaetsfixes an Pfad- und Testlogik

**Ergebnis**: Der Projektstand ist nicht mehr nur implizit Linux-zentriert; macOS- und Windows-Risiken sind sichtbar und priorisierbar.

### 1.8 Referenzdaten, Dokumentation und Abnahme aktualisieren

Die neuen Formate und Release-Pfade sind nur dann nutzbar, wenn Beispiele, Goldens und Dokumentation konsistent sind.

M5 aktualisiert mindestens:

- `docs/examples/` mit repraesentativen HTML-, JSON- und DOT-Ausgaben
- `README.md` mit Formatwahl, quiet/verbose und Release-/Container-Nutzung
- `docs/guide.md` mit praktischen Aufrufen fuer alle M5-Formate
- `docs/releasing.md` mit finalem Artefaktumfang und Release-Verifikation
- `docs/quality.md` mit M5-Testumfang und Plattform-Smokes
- `CHANGELOG.md` fuer `v1.2.0`

Tests und Abnahme muessen mindestens abdecken:

- Adapter-Unit-Tests fuer HTML, JSON und DOT
- CLI-Tests fuer Formatwahl, ungueltige Formatwerte und `--format html|json|dot --output <path>` fuer `analyze` und `impact`
- CLI-Tests fuer atomare Dateiausgabe: erfolgreiche Writes erzeugen vollstaendige Reports, Fehlerfaelle hinterlassen kein halb geschriebenes Zielartefakt
- CLI-Tests fuer `--verbose`, `--quiet` und gegenseitigen Ausschluss
- Golden-Output-Tests fuer `analyze` und `impact` in allen neuen Formaten
- Smoke-Test fuer Docker-Runtime-Image
- Smoke-Test fuer Linux-Release-Artefakt nach Entpacken
- Release-Test oder Workflow-Schritt fuer erlaubte Semver-Tags, Prerelease-Tags und Ablehnung ungueltiger Tags
- Validierung, dass JSON syntaktisch gueltig ist, `format_version` enthaelt und den Pflichtfeld-, Typ-, Enum-, Nullability- und Array-Regeln aus `docs/report-json.md` entspricht
- Validierung, dass DOT syntaktisch durch Graphviz `dot -Tsvg` oder einen gleichwertigen DOT-Parser akzeptiert wird und Escaping-Goldens korrekt verarbeitet werden

**Ergebnis**: M5 ist nicht nur implementiert, sondern ueber Beispiele, Referenzdaten und Release-Dokumentation nachvollziehbar abnehmbar.

## 2. Reihenfolge und Abhaengigkeiten

Empfohlene Reihenfolge:

1. Report-Vertraege und Formatversionierung festlegen
2. JSON-Adapter umsetzen, weil er den stabilsten Datenvertrag erzwingt
3. DOT-Adapter auf Basis der vorhandenen Ergebnisdaten umsetzen
4. HTML-Adapter umsetzen
5. CLI-Formatwahl und quiet/verbose final verdrahten
6. Golden-Outputs und Beispiele fuer neue Formate erzeugen
7. Release-Workflow und Container-Verifikation absichern
8. macOS-/Windows-Smokes einrichten oder dokumentiert ausfuehren
9. README, Guide, Releasing, Quality und Changelog finalisieren

Abhaengigkeiten:

- `JsonReportAdapter`, `DotReportAdapter` und `HtmlReportAdapter` haengen an stabilen `AnalysisResult`- und `ImpactResult`-Modellen aus M4
- `DotReportAdapter` darf nur Target-Zuordnungen verwenden, die bereits in M4-Ergebnissen vorhanden sind
- `--quiet` und `--verbose` muessen vor Golden-Finalisierung feststehen
- Release-Workflow und Dokumentation muessen nach finaler CLI-Syntax aktualisiert werden

## 3. Risiken und Gegenmassnahmen

| Risiko | Auswirkung | Gegenmassnahme |
|---|---|---|
| JSON wird ohne klaren Vertrag eingefuehrt | Folgewerkzeuge brechen bei jeder Report-Aenderung | `format_version` dokumentieren und Golden-Tests fuer zentrale Felder pflegen |
| `--output` bleibt nur teilweise fuer neue Formate nutzbar | Nutzer koennen neue Artefaktformate nicht verlaesslich in CI speichern | `--output` fuer `markdown`, `html`, `json` und `dot` explizit freischalten und atomare Schreibtests aufnehmen |
| DOT suggeriert Target-Graph-Semantik, die M5 noch nicht besitzt | Nutzer interpretieren Graphen falsch | keine Target-zu-Target-Kanten erzeugen und Legende/Labels klar formulieren |
| DOT-Escaping ist syntaktisch fehlerhaft | Graphviz-Berichte brechen erst bei Nutzern | Sonderzeichen-Goldens mit Graphviz oder Parser validieren |
| HTML-Goldens werden durch kosmetische Details instabil | Tests werden teuer und rauschanfaellig | keine Zeitstempel, keine Zufalls-IDs, deterministische Struktur |
| `--verbose` schreibt Zusatztext in maschinenlesbare Ausgaben | JSON-/DOT-Nutzung in CI bricht | stdout fuer maschinenlesbare Reports sauber halten; Zusatzdiagnostik nach `stderr` oder strukturiert ausgeben |
| Release-Trigger akzeptiert ungueltige Tags | fehlerhafte Releases oder falsche `latest`-Tags entstehen | Semver-/Prerelease-Tags fail-fast validieren und `latest` nur fuer regulaere Releases setzen |
| Release-Workflow funktioniert nur fuer lokale Pfade | Artefakte sind nicht real nutzbar | entpacktes Linux-Archiv und Container-Image separat smoke-testen |
| Windows-Pfade brechen Golden-Tests | Portabilitaetsziel bleibt theoretisch | Pfadanzeige und Normalisierung explizit testen; Goldens plattformrobust gestalten |

## 4. Rueckverfolgbarkeit

| Kennung | Umsetzung in M5 |
|---|---|
| `F-28` | `HtmlReportAdapter` fuer `analyze` und `impact` |
| `F-29` | `JsonReportAdapter` fuer `analyze` und `impact` |
| `F-30` | `DotReportAdapter` fuer vorhandene Analyse- und Impact-Daten |
| `F-39` | `--verbose` fuer diagnoseorientierte CLI-Ausgabe |
| `F-40` | `--quiet` fuer reduzierte CLI-Ausgabe |
| `F-43` | versioniertes Linux-CLI-Artefakt mit Pruefsumme |
| `F-44` | OCI-kompatibles Container-Image |
| `NF-08` | macOS-/Windows-Build- und Smoke-Bewertung |
| `NF-09` | dokumentierte Compiler-/CMake-Kompatibilitaet und Smoke-Checks |
| `NF-20` | dokumentierte JSON-Formatversion |
| `NF-21` | Release- und Container-Dokumentation ohne interne Build-Pfade |
| `S-12` | Linux-Release-Archiv |
| `S-13` | Container-Image fuer lokale Nutzung und CI |
