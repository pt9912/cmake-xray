# Plan M5 - AP 1.7 Erweiterte Plattformunterstuetzung bewerten

## Ziel

AP 1.7 macht die Plattformlage fuer M5 sichtbar und testbar. Ziel ist keine
vollstaendige Produktfreigabe fuer macOS oder Windows, sondern eine belastbare
Bewertung:

- Linux bleibt die offiziell freigegebene Primaerplattform fuer M5.
- macOS und Windows bauen in einer nativen Matrix oder werden mit dokumentierten
  Smoke-Checks bewertet.
- Pfad-, Zeilenenden- und Atomic-Replace-Risiken werden nicht nur beschrieben,
  sondern mit konkreten Gates abgesichert.
- Bekannte Einschraenkungen werden in Nutzer- und Release-Dokumentation
  nachvollziehbar festgehalten.

## Scope

Umsetzen:

- Native Build-/Test-Matrix fuer Linux, macOS und Windows pruefen und
  gegebenenfalls haerten.
- Verpflichtende CI-Matrix fuer die plattformsichere atomare Replace-Semantik
  auf Linux, macOS und Windows.
- CLI-Smokes fuer `--help`, `analyze` und `impact` auf macOS und Windows.
- Bewertung von Pfadnormalisierung, Laufwerksbuchstaben, Backslashes und
  Zeilenenden in Tests, Goldens und Report-Ausgaben.
- Dokumentierte Mindestversionen fuer CMake und Compiler bestaetigen.
- Mindestversionen fuer CMake und Compiler in CI oder verpflichtend
  validierten Smoke-Reports fail-fast pruefen.
- Mindestens eine aktuelle CMake- und Compiler-Kombination in CI oder
  verpflichtend validierten Smoke-Checks ausfuehren.
- Bekannte CMake-File-API-Einschraenkungen aus M4 und M5 in der
  Plattformbewertung erhalten.
- `docs/quality.md` und `docs/releasing.md` um Plattformstatus,
  Smoke-Abdeckung und bekannte Einschraenkungen ergaenzen.
- Kleine Portabilitaetsfixes an Pfad-, Test- und Skriptlogik, falls sie fuer
  die Bewertung notwendig sind.

Nicht umsetzen:

- Offizielle macOS-/Windows-Releasefreigabe als gleichwertige M5-Zielplattform.
- Verpflichtende macOS-/Windows-Release-Artefakte im finalen M5-Releaseumfang.
- Cross-Compilation-Strategie fuer alle Zielarchitekturen.
- Produktive Installer, Signierung, Notarisierung oder Windows-Code-Signing.
- Neue fachliche Analysefeatures, neue Reportformate oder Target-Graph-Semantik.
- Automatische CMake-Ausfuehrung zur Erzeugung von File-API-Reply-Daten.

Nicht Bestandteil von AP 1.7:

- Veraenderung des offiziellen M5-Releaseumfangs aus AP 1.6.
- Multi-Arch-OCI-Strategie.
- Vollstaendige Performance-Baseline je Betriebssystem.
- Langfristiger Plattform-Supportvertrag ueber M5 hinaus.

## Voraussetzungen aus AP 1.1 bis AP 1.6

AP 1.7 baut auf folgenden M5-Vertraegen auf:

- gemeinsamer Atomic-Writer und `--output`-Schreibvertrag aus AP 1.1,
- JSON-, DOT- und HTML-Goldens inklusive plattformrobuster Pfadfaelle aus
  AP 1.2 bis AP 1.4,
- command-lokaler `--quiet`-/`--verbose`-Vertrag aus AP 1.5,
- Release-Plattformklassifizierung und Preview-Sperren aus AP 1.6.

Falls AP 1.1 den Atomic-Replace-Wrapper nicht plattformspezifisch trennt, ist
AP 1.7 fuer die Windows-Aussage blockiert. Falls AP 1.6 macOS-/Windows-Artefakte
als Preview klassifiziert, darf AP 1.7 diese Klassifizierung nur bestaetigen
oder enger fassen, aber nicht stillschweigend zur offiziellen Freigabe erweitern.

## Dateien

Voraussichtlich zu aendern:

- `.github/workflows/build.yml`
- `.github/workflows/release.yml`
- `tests/adapters/test_atomic_report_writer.cpp`
- `tests/e2e/run_e2e_lib.sh`
- `tests/e2e/run_e2e_artifacts.sh`
- `tests/e2e/run_e2e_smoke.sh`
- `tests/e2e/run_e2e_verbosity.sh`
- `tests/e2e/test_cli.cpp`
- `tests/e2e/test_cli_verbosity.cpp`
- `docs/quality.md`
- `docs/releasing.md`
- `docs/guide.md`
- `README.md`

Neue Dateien, falls die bestehende Struktur nicht ausreicht:

- `tests/platform/test_atomic_writer_platform.cpp`
- `tests/platform/toolchain-minimums.json`
- `tests/platform/platform-smoke-report.schema.json`
- `tests/platform/validate_platform_smoke_report.py`
- `tests/platform/README.md`
- `scripts/platform-smoke.sh`
- `scripts/platform-smoke.ps1`

## Plattformstatus-Vertrag

M5 unterscheidet drei Statusklassen:

- `supported`: offiziell freigegeben, dokumentiert und releasefaehig.
- `validated_smoke`: Build, Pflicht-Smokes und verpflichtende Plattformgates
  laufen gruen, aber ohne vollstaendige Releasefreigabe.
- `known_limited`: Build, Pflicht-Smokes oder Plattformgates sind nicht
  vollstaendig gruen; die akzeptierten Ausfaelle, Umgehungen oder nicht
  abgedeckten Flaechen sind dokumentiert und priorisierbar.

Fuer M5 gilt:

- Linux ist `supported`.
- macOS darf hoechstens `validated_smoke` erreichen.
- Windows darf hoechstens `validated_smoke` erreichen.
- `validated_smoke` fuer macOS oder Windows ist nur zulaessig, wenn alle in
  AP 1.7 als Pflicht definierten Build-, Atomic-Replace- und CLI-Smoke-Gates
  auf dieser Plattform gruen sind.
- `validated_smoke` braucht einen objektiven Nachweis im PR- oder Release-
  Kontext: entweder verpflichtende CI-Jobs pro Plattform oder ein
  reproduzierbarer Smoke-Report als verpflichtendes Review-Artefakt mit
  nonzero Fail-Verhalten bei Abweichungen.
- Der Nachweispfad muss als Required Check in der Branch-Protection oder als
  nicht ueberspringbares Release-Gate konfiguriert sein, bevor
  `validated_smoke` dokumentiert werden darf.
- `known_limited` ist zu verwenden, sobald ein Pflichtgate rot ist, nur
  manuell statt in CI laeuft, bewusst uebersprungen wird oder nur mit einer
  dokumentierten Einschraenkung aussagekraeftig ist.
- Ein nur lokal ausgefuehrtes oder frei editierbares Smoke-Protokoll reicht
  nicht fuer `validated_smoke`; ohne automatischen oder artefaktbasiert
  verifizierbaren Nachweis bleibt der Status `known_limited`.
- Jeder Status muss in `docs/releasing.md` und `docs/quality.md` konsistent
  benannt werden.
- Ein roter macOS- oder Windows-Job darf nicht durch implizites Ignorieren zu
  einem gruenen Plattformstatus fuehren. Entweder wird der Job als
  verpflichtendes Gate gefuehrt, oder die Einschraenkung wird als
  `known_limited` dokumentiert.

## CI- und Smoke-Vertrag

### Native Build-Matrix

Die native CI-Matrix prueft mindestens:

- Linux mit `ubuntu-latest`.
- macOS mit `macos-latest`.
- Windows mit `windows-latest`.
- Python-Bootstrap fuer JSON-Schema-Gates.
- `cmake -B build -DCMAKE_BUILD_TYPE=Release`.
- `cmake --build build --config Release --parallel`, soweit vom Host
  unterstuetzt.
- `ctest --test-dir build --output-on-failure -C Release --parallel`, soweit
  vom Host unterstuetzt.

Regeln:

- `fail-fast: false` bleibt gesetzt, damit Plattformfehler sichtbar bleiben.
- Platform-Jobs muessen sprechende Namen tragen, zum Beispiel
  `linux-x86_64`, `macos-arm64`, `windows-x86_64`.
- Wenn ein Plattformjob bewusst nicht als Gate gilt, muss der Jobname oder die
  Doku diesen Preview-/Smoke-Status ausdruecken.
- Native CI darf nicht auf Docker als Ersatz fuer macOS-/Windows-Aussagen
  verweisen.
- Fuer `validated_smoke` muss pro macOS-/Windows-Plattform mindestens einer
  dieser Nachweispfade verpflichtend sein:
  - ein nicht ueberspringbarer Matrixjob, der Build, Atomic-Replace-Tests und
    CLI-Smokes ausfuehrt;
  - ein reproduzierbarer Smoke-Report-Upload mit Host-, Toolchain-,
    CMake-Version, Kommando-Log, Exit-Codes und Checksummen der erzeugten
    Reports, der als verpflichtender Review-Check ausgewertet wird.
- Smoke-Reports muessen gegen `tests/platform/platform-smoke-report.schema.json`
  validieren und zusaetzlich durch
  `tests/platform/validate_platform_smoke_report.py` fachlich gegen erwartete
  Plattform, `checkout_sha`, `head_sha`, Pflichtkommandos und Checksummen
  geprueft werden.
- Der gewaehlte Nachweispfad ist fuer PRs ein Required Check und fuer
  taggesteuerte Releases ein zwingendes `needs`-Gate vor Publish-Jobs.
- Fehlt dieser Nachweispfad, darf die Plattform nur als `known_limited`
  dokumentiert werden.

### Normative CLI-Smokes

Auf macOS und Windows muessen mindestens folgende echte Binary-Aufrufe laufen.
Diese Liste ist die einzige normative Pflichtkommandoliste fuer AP 1.7; Tests,
Smoke-Reports und Doku verweisen auf diese Liste statt eigene Kurzformen zu
definieren.

- `cmake-xray --help`
- `cmake-xray analyze --compile-commands <fixture> --format console`
- `cmake-xray analyze --compile-commands <fixture> --format json`
- `cmake-xray analyze --compile-commands <fixture> --format json --output <path>`
- `cmake-xray impact --compile-commands <fixture> --changed-file <path> --format console`
- `cmake-xray impact --compile-commands <fixture> --changed-file <path> --format json`
- `cmake-xray impact --compile-commands <fixture> --changed-file <path> --format json --output <path>`

Soweit AP 1.3 und AP 1.4 abgeschlossen sind, werden DOT und HTML in denselben
Smoke-Pfad aufgenommen:

- `cmake-xray analyze --compile-commands <fixture> --format dot`
- `cmake-xray impact --compile-commands <fixture> --changed-file <path> --format dot`
- `cmake-xray analyze --compile-commands <fixture> --format html`
- `cmake-xray impact --compile-commands <fixture> --changed-file <path> --format html`
- `cmake-xray analyze --compile-commands <fixture> --format dot --output <path>`
- `cmake-xray impact --compile-commands <fixture> --changed-file <path> --format dot --output <path>`
- `cmake-xray analyze --compile-commands <fixture> --format html --output <path>`
- `cmake-xray impact --compile-commands <fixture> --changed-file <path> --format html --output <path>`

Die Smokes duerfen stdout/stderr nur nach den dokumentierten Report- und
Verbosity-Vertraegen bewerten. Zeilenenden werden fuer Golden-Vergleiche
normalisiert, aber die fachliche Ausgabe darf nicht plattformspezifisch
umgeschrieben werden.

Die `--output`-Smokes sind bewusst Teil des Plattformvertrags: Sie muessen auf
macOS und Windows eine echte Datei im Zielverzeichnis schreiben, leeres stdout
bei erfolgreicher Dateiausgabe pruefen und den geschriebenen Report gegen das
jeweilige Formatgate oder Golden validieren. Sie ersetzen nicht die
Atomic-Writer-Unit-Tests, sondern sichern den End-to-End-Pfad durch die echte
CLI-Binary.

### Smoke-Skripte

Wenn die Workflow-Matrix nicht alle CLI-Smokes direkt enthaelt, muss ein
plattformfaehiges Smoke-Skript existieren:

- Bash-Pfad fuer Linux/macOS.
- PowerShell-Pfad oder MSYS-kompatibler Bash-Pfad fuer Windows.
- Explizite Ausgabe von Host, Compiler, CMake-Version und Binary-Pfad.
- Nonzero Exit bei fehlgeschlagenem Build, fehlgeschlagener CLI-Ausfuehrung
  oder fehlender Testfixture.
- Keine Netzwerknutzung zur Laufzeit.
- Wenn Smoke-Skripte statt direkter Matrixschritte den Plattformstatus
  begruenden, muessen sie einen maschinenlesbaren Report erzeugen, der in CI
  hochgeladen und durch einen verpflichtenden Check validiert wird.
- Diese Smoke-Report-Variante ersetzt einen direkten Matrix-Smoke nur dann,
  wenn der Report deterministisch aus dem aktuellen Commit erzeugt wird, nicht
  manuell editierbar ist und der validierende Check in Branch-Protection oder
  Release-Gates verpflichtend ist.

### Smoke-Report-Vertrag

Der maschinenlesbare Smoke-Report ist nur dann ein zulaessiger Nachweis fuer
`validated_smoke`, wenn sein Format versioniert und automatisiert validiert
wird.

Pflichtfelder:

- `format`: fester Wert `cmake-xray.platform-smoke`.
- `format_version`: Integer `1`.
- `checkout_sha`: exakt der Commit, der im CI-Arbeitsverzeichnis ausgecheckt
  und gebaut wurde.
- `head_sha`: bei Pull Requests der Quell-Branch-Head-Commit, bei Push-,
  Tag- und manuellen Runs identisch zu `checkout_sha`.
- `event_name`: zum Beispiel `pull_request`, `push`, `workflow_dispatch`.
- `platform`: `macos` oder `windows`.
- `runner_image`, `architecture`, `generator`, `build_type`.
- `cmake.version`, `cmake.minimum_version`, `cmake.minimum_satisfied`.
- `compiler.id`, `compiler.version`, `compiler.minimum_version`,
  `compiler.minimum_satisfied`.
- `msys_path_mode` fuer Windows: `disabled`, `native_powershell`,
  `conversion_enabled_control`, oder `not_applicable`.
- `commands[]` mit Name, Argumentliste, Exit-Code, stdout-/stderr-Checksumme
  und erzeugten Report-Checksummen.
- `required_commands_satisfied`: Boolean.
- `atomic_replace_satisfied`: Boolean.
- `path_cases_satisfied`: Boolean.

Validierungsregeln:

- Schema-Verletzungen fuehren zu nonzero Exit.
- `format_version` muss exakt `1` sein.
- `checkout_sha` muss exakt zum lokalen CI-Checkout passen. In GitHub Actions
  ist das der tatsachlich ausgecheckte SHA, also fuer Standard-PR-Workflows
  typischerweise der Merge-Commit aus `github.sha`/`GITHUB_SHA`, sofern der
  Checkout nicht explizit auf den Head-Commit umgestellt wurde.
- `head_sha` muss bei `pull_request`-Runs dem PR-Head entsprechen, also
  `github.event.pull_request.head.sha`; bei `push`, `workflow_dispatch` und
  taggesteuerten Release-Runs muss `head_sha == checkout_sha` gelten.
- Der Verifier prueft immer `checkout_sha` gegen das Arbeitsverzeichnis und
  prueft `head_sha` nur gegen den Event-Kontext. Ein PR-Report darf nicht
  fehlschlagen, nur weil `checkout_sha` der Merge-SHA und `head_sha` der
  Branch-Head-SHA ist.
- `cmake.minimum_satisfied`, `compiler.minimum_satisfied`,
  `required_commands_satisfied`, `atomic_replace_satisfied` und
  `path_cases_satisfied` muessen `true` sein.
- Alle Pflichtkommandos aus dem CLI-Smoke-Vertrag muessen vorhanden sein und
  Exit-Code `0` haben.
- Report-Checksummen muessen gegen die im CI-Arbeitsverzeichnis vorhandenen
  Dateien neu berechnet werden.
- Windows-Smoke-Reports muessen den verwendeten `msys_path_mode` explizit
  ausweisen; ein fehlender oder unbekannter Modus ist ein Validierungsfehler.

Ein Smoke-Report darf einen direkten Matrix-Smoke nur ersetzen, wenn derselbe
Workflow den Report erzeugt, validiert und als Artefakt hochlaedt. Vorgefertigte
Reports aus frueheren Runs oder manuell hochgeladene Dateien sind fuer
`validated_smoke` nicht zulaessig.

Versionierungspolitik:

- `format_version=1` bleibt fuer M5 stabil.
- Pflichtfeldentfernung, Pflichtfeldumbenennung oder inkompatible
  Bedeutungswechsel erfordern eine neue Major-Version.
- Additive optionale Felder duerfen in Version `1` eingefuehrt werden, solange
  der Verifier unbekannte optionale Felder ignoriert und alle Pflichtregeln
  unveraendert bleiben.

## Atomic-Replace-Matrix

AP 1.7 macht die atomare Dateiausgabe zur verbindlichen Plattformpruefung.

Zu pruefen:

- neue Zieldatei wird vollstaendig geschrieben,
- vorhandene Zieldatei wird bei Erfolg ersetzt,
- vorhandene Zieldatei bleibt bei simuliertem Renderfehler unveraendert,
- vorhandene Zieldatei bleibt bei simuliertem Schreib-/Replace-Fehler
  unveraendert,
- temporaere Datei wird im Zielverzeichnis exklusiv reserviert,
- Kollisionen bei temporaeren Dateinamen werden deterministisch wiederholt
  oder sauber abgebrochen,
- keine Implementierung loescht den Zielpfad vor dem Replace-Schritt.

Plattformvertrag:

- POSIX nutzt `rename` oder `renameat` fuer atomaren Replace.
- Windows nutzt `ReplaceFileW` oder `MoveFileExW` mit
  `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`.
- Ein nacktes `std::filesystem::rename(temp, target)` darf nur dort verwendet
  werden, wo die Plattformsemantik den Replace-Vertrag erfuellt oder der Code
  in eine Plattformoperation gekapselt ist, deren Tests die Zielinvariante
  pruefen.
- Tests duerfen nicht nur Mock-Operationen pruefen; mindestens ein Testpfad
  muss die `DefaultAtomicFilePlatformOps` des jeweiligen Hosts ausfuehren.

CI-Anforderung:

- Atomic-Writer-Tests laufen in der nativen Linux-/macOS-/Windows-Matrix.
- Windows-spezifische Replace-Semantik wird nicht als reiner POSIX-Test
  akzeptiert.
- Ein roter Atomic-Writer-Test blockiert AP 1.7 unabhaengig davon, ob die
  restlichen CLI-Smokes gruen sind.

## macOS-Bewertung

macOS wird fuer M5 mindestens mit einem gaengigen Clang-/CMake-Setup bewertet.

Zu dokumentieren:

- Runner-Image und Architektur.
- CMake-Version.
- Compiler-ID und Compiler-Version.
- Build-Typ.
- Ergebnis von `ctest`.
- Ergebnis der CLI-Smokes.
- Abweichungen bei Pfadnormalisierung, Symlinks, Case-Sensitivity und
  Zeilenenden.

macOS-spezifische Risiken:

- Case-insensitive Dateisysteme koennen Pfadvergleichstests anders wirken
  lassen.
- `/private/var/...`-Aufloesungen duerfen keine byte-stabilen Goldens
  kontaminieren.
- Shell- und BSD-Tooling weichen in Details von GNU/Linux ab.

Regeln:

- Goldens duerfen keine lokalen macOS-Temp- oder Checkout-Praefixe enthalten.
- Tests, die absolute Hostpfade brauchen, muessen diese vor dem Vergleich
  normalisieren oder per Fixture synthetisch stabilisieren.
- macOS-Preview-Artefakte duerfen nur gemaess AP 1.6 klassifiziert werden.

## Windows-Bewertung

Windows wird fuer M5 mindestens mit einem gaengigen MSVC- oder ClangCL-/CMake-
Setup bewertet.

Zu dokumentieren:

- Runner-Image und Architektur.
- Generator und Toolchain, zum Beispiel Visual Studio oder Ninja.
- CMake-Version.
- Compiler-ID und Compiler-Version.
- Build-Typ und Konfiguration (`Release`).
- Ergebnis von `ctest`.
- Ergebnis der CLI-Smokes.
- Abweichungen bei Laufwerksbuchstaben, Backslashes, UNC-/Extended-Length-
  Pfaden und CRLF.

Windows-spezifische Risiken:

- POSIX-artige Pfade wie `/project/...` sind nicht automatisch absolut.
- MSYS2/Git-Bash kann CLI-Argumente umschreiben, wenn `MSYS_NO_PATHCONV` und
  `MSYS2_ARG_CONV_EXCL` nicht gesetzt sind.
- Backslash kann Pfadtrenner oder literal fachlicher Zeichenbestandteil sein.
- CRLF darf Golden-Vergleiche nicht brechen.
- Zielpfade koennen waehrend Replace-Tests durch Scanner oder offene Handles
  blockiert sein.

Regeln:

- Der Standardmodus fuer Windows-E2E- und Plattform-Smokes ist
  konvertierungsfrei: `MSYS_NO_PATHCONV=1` und `MSYS2_ARG_CONV_EXCL=*` sind
  gesetzt, oder der Smoke laeuft unter PowerShell mit nativen Windows-Pfaden.
- Zusaetzliche MSYS-Konvertierungs-Smokes sind erlaubt, muessen aber als
  eigener Modus `conversion_enabled_control` gekennzeichnet sein und duerfen
  nicht die konvertierungsfreien Pflicht-Smokes ersetzen.
- Jeder Windows-Smoke dokumentiert im Log und, falls genutzt, im Smoke-Report
  den Modus `disabled`, `native_powershell` oder `conversion_enabled_control`.
- Windows-Goldens fuer absolute Pfade sind eigene Varianten, wenn POSIX- und
  Windows-Semantik fachlich verschieden sind.
- Laufwerksbuchstaben werden in Display-Pfaden stabil und dokumentiert
  behandelt.
- UNC-Pfade (`\\server\share\...`) und Extended-Length-Pfade
  (`\\?\C:\...`) sind verpflichtende Windows-Testfaelle fuer Pfadanzeige,
  Escaping und mindestens einen CLI- oder Golden-Pfad.
- UNC-Tests duerfen in GitHub-hosted Windows-Runs nicht von externer
  Netzwerkfreigabe abhaengen. Ein synthetischer UNC-Pfadfall fuer Display-,
  Escape- und Normalisierungslogik ist immer verpflichtend und kommt ohne
  SMB-Share aus.
- Ein echter lokaler UNC-Dateisystem-Smoke nutzt nur eine im Job provisionierte
  Freigabe wie `\\localhost\cmake-xray-smoke`; Einrichtung und Cleanup sind
  Teil des Jobs.
- Wenn die lokale `\\localhost\...`-Freigabe auf dem GitHub-Runner wegen
  Rechte- oder Policy-Limits nicht anlegbar ist, wird nur der echte
  Dateisystem-Smoke als `known_limited` markiert. Der synthetische UNC-
  Pflichtfall bleibt required und muss gruen sein.
- Extended-Length-Tests verwenden ein lokal erzeugtes Temp-Verzeichnis unter
  dem Runner-Workspace und pruefen vorab die Windows-Long-Path-Faehigkeit.
  Wenn der Host Extended-Length-Dateiziele nicht zulaesst, wird der konkrete
  Test als `known_limited` dokumentiert; synthetische Parser-/Golden-Faelle
  bleiben trotzdem verpflichtend.
- Atomic-Writer-Tests muessen mindestens einen Extended-Length-Zielpfad oder
  einen dokumentierten Windows-API-Skip mit `known_limited`-Folge pruefen.
- Test-Helfer muessen Pfade ueber `native_path` oder einen aequivalenten
  Mechanismus an Windows-native Tools uebergeben.
- CRLF wird in Vergleichen normalisiert, ohne Report-Adapter zu
  plattformspezifischem Output zu zwingen.

## CMake-/Compiler-Kompatibilitaet

M5 haelt die dokumentierte Mindestversion:

- `cmake_minimum_required` bleibt bei der im Projekt dokumentierten
  Mindestversion.
- `README.md` und `docs/guide.md` nennen dieselbe Mindestversion.
- `docs/quality.md` nennt die in CI tatsaechlich verwendeten Versionen oder
  verweist auf die Workflow-Matrix.

Kompatibilitaetsregeln:

- `tests/platform/toolchain-minimums.json` ist die einzige autoritative,
  maschinenlesbare Quelle fuer CMake- und Compiler-Mindestversionen in AP 1.7.
- Die Datei enthaelt mindestens `cmake.minimum_version` sowie je Compiler-ID
  `compiler_minimums.MSVC`, `compiler_minimums.AppleClang`,
  `compiler_minimums.Clang` und `compiler_minimums.GNU`.
- Native Matrix, Smoke-Skripte, Smoke-Report-Verifier und Dokumentationschecks
  duerfen Mindestversionen nicht duplizieren, sondern muessen diese Datei lesen
  oder aus ihr generierte CI-Outputs verwenden.
- Jeder Plattformjob protokolliert `cmake --version` vor der Konfiguration und
  bricht vor dem Build ab, wenn die gefundene Version unter
  `tests/platform/toolchain-minimums.json` liegt.
- Jeder Plattformjob protokolliert Compiler-ID und Compiler-Version vor der
  Konfiguration und bricht vor dem Build ab, wenn die gefundene Version unter
  dem zugehoerigen Eintrag in `tests/platform/toolchain-minimums.json` liegt.
- Mindestens ein Matrixjob pro Hostfamilie nutzt eine explizit eingerichtete
  aktuelle CMake-/Compiler-Kombination statt nur implizit vorinstallierter
  Runner-Defaults; die konkreten Versionen werden in `docs/quality.md`
  dokumentiert.
- Compiler-Mindestanforderungen werden nicht implizit angehoben; jede
  Anhebung aendert zuerst `tests/platform/toolchain-minimums.json`; Doku,
  Matrix und Fail-Fast-Checks werden daraus nachgezogen.
- Falls ein Plattformfix eine neuere C++- oder CMake-Funktion braucht, wird die
  Mindestversion bewusst angepasst und in Doku, CI und Changelog nachgezogen.
- CMake-File-API-v1 bleibt der einzige M5-Vertrag; neuere CMake-Versionen
  duerfen keine stillschweigende Abhaengigkeit auf andere Reply-Formate
  einfuehren.

## Pfad- und Zeilenenden-Vertrag

Pfad- und Golden-Regeln:

- Fachliche Display-Pfade folgen den Regeln aus AP 1.1 bis AP 1.4.
- CLI-relative Eingaben bleiben normalisierte relative Anzeige-Pfade.
- Absolute Eingaben bleiben absolute Anzeige-Strings.
- Synthetische Fixture-Pfade werden bevorzugt, wenn byte-stabile Goldens
  noetig sind.
- Host-abhaengige Checkout- oder Temp-Pfade werden vor dem Golden-Vergleich
  normalisiert oder nicht in Goldens aufgenommen.
- Per-Plattform-Goldens sind zulaessig, wenn die Plattformsemantik fachlich
  unterschiedlich ist.

Zeilenenden-Regeln:

- Golden-Vergleiche duerfen CRLF zu LF normalisieren.
- Reportadapter muessen dadurch keine unterschiedlichen Plattformausgaben
  erzeugen.
- JSON-, DOT- und HTML-Syntaxvalidierung prueft den normalisierten Inhalt oder
  den vom Host geschriebenen Inhalt so, dass CRLF keine Scheindifferenzen
  erzeugt.
- Dokumentation sagt klar, dass byte-stabile Goldens intern normalisiert
  verglichen werden koennen.

## Release- und Artefaktgrenzen

AP 1.7 darf den Releaseumfang aus AP 1.6 nicht ausweiten.

Regeln:

- Offiziell freigegebene M5-Artefakte bleiben Linux-CLI-Archiv und OCI-Image,
  solange AP 1.6 nicht explizit erweitert wird.
- macOS-/Windows-Builds aus der Matrix sind Bewertungs- oder Preview-Ergebnis,
  keine automatische Releasefreigabe.
- Release-Workflow und Release-Doku muessen verhindern, dass macOS-/Windows-
  Artefakte unmarkiert als offizielle Assets erscheinen.
- Wenn bestehende Workflows macOS-/Windows-Pakete erzeugen, werden sie
  entweder deaktiviert, als Preview separiert oder ueber die Allowlist aus
  AP 1.6 vom finalen Publish ausgeschlossen.
- Der finale Release-Publish-Pfad braucht einen expliziten Guard, der die
  heruntergeladene oder erzeugte Assetliste vor dem finalen Release-Upload
  gegen die erlaubten offiziellen M5-Assetnamen prueft und bei jedem macOS-/
  Windows-Archiv ohne Preview-Klassifizierung nonzero abbricht.
- Der Guard muss unabhaengig vom konkreten Upload-Mechanismus gelten, also
  sowohl fuer `gh release upload` als auch fuer GitHub Actions, eigene
  Workflow-Schritte oder spaetere Publisher-Skripte.
- Der Guard ist ein nicht ueberspringbarer Release-Job oder ein verpflichtender
  Schritt im finalen Release-Job und muss vor jedem extern sichtbaren
  Release-Upload laufen.
- Der Release-Dry-Run muss denselben Guard ausfuehren und einen Negativfall
  enthalten, in dem ein macOS-/Windows-Archiv in der offiziellen Assetliste
  den Publish-Pfad vor Upload abbricht.
- Ein lokaler Ad-hoc-Check ohne Einbindung in den Release-Lauf reicht nicht;
  reine Review- oder Dokumentationspflicht reicht ebenfalls nicht.

## Dokumentation

`docs/quality.md` ergaenzt:

- native Plattformmatrix und ihr Gate-Status,
- Atomic-Replace-Matrix,
- CLI-Smoke-Abdeckung je Plattform,
- Umgang mit CRLF und per-Plattform-Goldens,
- benoetigte lokale Tools fuer Plattform-Smokes.

`docs/releasing.md` ergaenzt:

- offizieller M5-Plattformstatus,
- Linux als freigegebene Primaerplattform,
- macOS-/Windows-Status als `validated_smoke` oder `known_limited`,
- klare Abgrenzung von Preview-Artefakten,
- bekannte Einschraenkungen und Recovery-Hinweise fuer Plattformjobs.

`README.md` ergaenzt nur nutzerrelevante Aussagen:

- Mindestversionen fuer CMake und Compiler bleiben sichtbar,
- Plattformstatus wird kurz und konsistent beschrieben,
- detaillierte Gate-Erklaerungen verweisen auf `docs/quality.md` und
  `docs/releasing.md`.

`docs/guide.md` wird mitgepflegt, sobald AP 1.7 Mindestversions- oder
Plattformvoraussetzungen beruehrt:

- CMake-Mindestversion ist identisch zu `README.md` und
  `cmake_minimum_required`,
- Compiler-Mindestversionen und CMake-Mindestversion stammen aus
  `tests/platform/toolchain-minimums.json`,
- lokale Plattform-Smokes oder Verweise darauf widersprechen nicht
  `docs/quality.md`,
- Beispiele versprechen keine weitergehende macOS-/Windows-Freigabe als
  `docs/releasing.md`.

## Implementierungsreihenfolge

### Tranche A - Ist-Zustand erfassen und Matrix festziehen

1. Bestehende `build.yml`- und `release.yml`-Matrizen gegen den
   Plattformstatusvertrag pruefen.
2. CMake-, Compiler- und Runner-Versionen in CI-Ausgaben sichtbar machen und
   CMake-/Compiler-Versionen gegen `tests/platform/toolchain-minimums.json`
   fail-fast pruefen.
3. Entscheiden, ob macOS-/Windows-Jobs verpflichtende Gates sind oder ob ein
   verpflichtend validierter Smoke-Report-Upload den Nachweis liefert; dieser
   Nachweis wird als Required Check oder Release-`needs`-Gate verankert.
4. `docs/quality.md` mit dem vorlaeufigen Plattformstatus ergaenzen.

Tranche A ist fertig, wenn Build- und Testjobs fuer alle vorgesehenen Hosts
sichtbar laufen oder ihre Einschraenkung explizit dokumentiert ist.

### Tranche B - Atomic-Replace plattformverbindlich pruefen

1. Bestehende `test_atomic_report_writer.cpp`-Faelle auf Host-Ausfuehrung der
   `DefaultAtomicFilePlatformOps` pruefen.
2. Fehlende Windows-Replace-Faelle ergaenzen.
3. Sicherstellen, dass bestehende Zielinhalte bei Replace-Fehlern erhalten
   bleiben.
4. Native Matrix so konfigurieren, dass diese Tests auf Linux, macOS und
   Windows laufen.
5. `docs/quality.md` um die Atomic-Replace-Matrix ergaenzen.

Tranche B ist fertig, wenn ein Fehler im Atomic-Writer auf jeder Plattform den
CI-Job bricht.

### Tranche C - CLI-Smokes und Pfad-/Golden-Haertung

1. E2E-Smoke-Skripte auf Windows-MSYS- und macOS-Kompatibilitaet pruefen.
2. `native_path`, CRLF-Normalisierung und per-Plattform-Golden-Auswahl fuer
   alle M5-Artefaktformate absichern.
3. CLI-Smokes fuer `--help`, `analyze` und `impact` auf macOS und Windows in
   CI oder verpflichtend validierte Smoke-Report-Skripte aufnehmen.
4. Laufwerksbuchstaben-, UNC-, Extended-Length-, Backslash- und absolute-Pfad-
   Faelle pruefen.
5. `docs/releasing.md` und `README.md` mit finalem Plattformstatus fuer M5
   aktualisieren.
6. `docs/guide.md` gegen README, CMake-Mindestversion und Plattformstatus
   abgleichen.
7. Nicht ueberspringbaren Release-Asset-Allowlist-Guard im finalen
   Release-Lauf und passenden Dry-Run-Negativtest fuer macOS-/Windows-
   Preview-Abgrenzung ergaenzen.

Tranche C ist fertig, wenn macOS-/Windows-Risiken aus den Hauptplanpunkten
konkret gruen, rot oder dokumentiert begrenzt sind.

### Tranche D - Optionale Haertung

Optionale Folgehaertung innerhalb von AP 1.7:

- separater Windows-PowerShell-Smoke, falls MSYS-Bash zu viel Pfadmagie
  verdeckt,
- zusaetzlicher Ninja-Generator-Smoke auf Windows,
- zusaetzlicher `macos-13`-/Intel-Smoke, falls arm64 nicht repraesentativ
  genug ist,
- dokumentierte manuelle Smoke-Checkliste fuer lokale Plattformnachweise.

Tranche D darf nur umgesetzt werden, wenn A bis C nicht verzoegert werden.

## Entscheidungen

Diese Entscheidungen sind vor Umsetzungsbeginn getroffen und in die Tranchen
eingearbeitet:

- Linux bleibt M5-Releaseplattform. Begruendung: AP 1.6 definiert Linux-Archiv
  und OCI als offizielle Artefakte; AP 1.7 bewertet weitere Plattformen.
- macOS und Windows werden als Build-/Smoke-Ziele bewertet. Begruendung:
  `NF-08` und `NF-09` verlangen Sichtbarkeit der Risiken, nicht zwingend
  vollstaendige Freigabe.
- Atomic-Replace ist ein verpflichtendes Plattformgate. Begruendung:
  `--output` darf vorhandene Reports bei Fehlern nicht zerstoeren.
- CRLF-Normalisierung ist Testinfrastruktur, kein Reportformatmerkmal.
  Begruendung: Artefaktadapter sollen stabile fachliche Ausgabe liefern; die
  Host-Zeilenenden duerfen Golden-Vergleiche nicht verfaelschen.
- Per-Plattform-Goldens sind fuer fachlich unterschiedliche absolute Pfade
  erlaubt. Begruendung: `/project/...` und `C:/project/...` haben
  unterschiedliche Plattformsemantik.
- Preview-Artefakte bleiben vom offiziellen Release getrennt. Begruendung:
  Nutzer duerfen Smoke-Ergebnisse nicht als vollstaendige Plattformfreigabe
  missverstehen.

## Tests

CI- und Build-Tests:

- Native Matrix baut Linux, macOS und Windows.
- Native Matrix fuehrt `ctest` auf allen drei Plattformen aus.
- `validated_smoke` fuer macOS oder Windows ist durch verpflichtende CI-Jobs
  oder durch einen verpflichtend validierten Smoke-Report-Upload belegt.
- Diese CI-Jobs oder Smoke-Report-Checks sind als Required Checks fuer PRs
  beziehungsweise als nicht ueberspringbare Release-Gates fuer Tags
  konfiguriert.
- Smoke-Report-Artefakte enthalten Host, Toolchain, CMake-Version,
  ausgefuehrte Kommandos, Exit-Codes und Checksummen erzeugter Reports.
- Smoke-Report-Artefakte validieren gegen das versionierte JSON-Schema und
  den fachlichen Verifier; beide Gates muessen nonzero fehlschlagen, wenn
  Pflichtfelder, Pflichtkommandos, `checkout_sha`, `head_sha`, Checksummen
  oder Mindestversionen nicht passen.
- CI-Logs zeigen CMake- und Compiler-Versionen oder machen sie aus
  Workflow-Kontext eindeutig nachvollziehbar.
- CMake-Versionen werden pro Plattform gegen
  `tests/platform/toolchain-minimums.json` geprueft; Unterschreitung fuehrt vor
  dem Build zu nonzero Exit.
- Compiler-Versionen werden pro Plattform gegen
  `tests/platform/toolchain-minimums.json` fuer den jeweiligen Compiler
  geprueft; Unterschreitung fuehrt vor dem Build zu nonzero Exit.
- `fail-fast: false` bleibt fuer Plattformmatrizen gesetzt.
- Fehlende Python-Validator-Abhaengigkeit fuehrt zu konkretem Fehler statt zu
  stillem Skip.

Atomic-Writer-Tests:

- `DefaultAtomicFilePlatformOps::create_temp_exclusive` wird auf jedem Host
  getestet.
- `DefaultAtomicFilePlatformOps::replace_existing` wird auf jedem Host
  getestet.
- `DefaultAtomicFilePlatformOps::move_new` wird auf jedem Host getestet.
- Ersetzen einer vorhandenen Datei erhaelt bei simuliertem Fehler den alten
  Inhalt.
- Schreibfehler entfernen temporaere Dateien.
- Kollisionen bei temporaeren Dateinamen werden getestet.
- Windows-Hostpfad prueft die gewaehlt gekapselte `ReplaceFileW`- oder
  `MoveFileExW`-Semantik.
- Tests pruefen, dass der Zielpfad vor dem Replace nicht geloescht wird.

CLI-Smoke-Tests:

- Alle Kommandos aus `Normative CLI-Smokes` laufen auf macOS und Windows.
- JSON-Kommandos aus `Normative CLI-Smokes` erzeugen gueltiges JSON.
- `--output`-Kommandos aus `Normative CLI-Smokes` lassen stdout leer und
  schreiben gueltigen Dateiinhalt.
- DOT-Smokes aus `Normative CLI-Smokes` laufen, sobald AP 1.3 produktiv ist.
- HTML-Smokes aus `Normative CLI-Smokes` laufen, sobald AP 1.4 produktiv ist.
- `--quiet` und `--verbose`-Smokes laufen auf macOS und Windows, sobald
  AP 1.5 produktiv ist.

Pfad- und Golden-Tests:

- Windows-absolute `--changed-file`-Pfade mit Laufwerksbuchstaben werden
  getestet.
- Windows-UNC-Pfade wie `\\server\share\project\src\main.cpp` werden in
  mindestens einem Adapter-, CLI- oder Golden-Test abgedeckt.
- Ein synthetischer UNC-Pfadfall ist required; ein echter lokaler
  `\\localhost\...`-Share-Smoke ist nur required, wenn die Runner-Policy seine
  Provisionierung erlaubt.
- Windows-Extended-Length-Pfade wie `\\?\C:\project\src\main.cpp` werden in
  mindestens einem Pfad- oder Atomic-Writer-Test abgedeckt.
- Windows-UNC-Tests nutzen keine externe Netzwerkabhaengigkeit; sie sind
  synthetisch oder ueber eine lokale `\\localhost\...`-Freigabe reproduzierbar.
- Windows-Extended-Length-Tests dokumentieren Long-Path-Provisionierung und
  fallen bei fehlender Host-Unterstuetzung auf `known_limited` statt auf einen
  stillen Skip.
- POSIX-absolute Pfade werden auf Windows nicht faelschlich als absoluter
  Windows-Pfad erwartet.
- Backslash-Faelle werden in DOT, HTML und JSON stabil behandelt.
- Per-Plattform-Goldens werden nur dort eingesetzt, wo die fachliche
  Plattformsemantik unterschiedlich ist.
- CRLF wird in E2E-Golden-Vergleichen normalisiert.
- Host-abhaengige Checkout-Pfade erscheinen nicht unnormalisiert in Goldens.
- MSYS-Pfadkonvertierung ist fuer CLI-Argumente explizit kontrolliert.
- Windows-Pfadszenarien trennen konvertierungsfreie Pflicht-Smokes von
  optionalen MSYS-Konvertierungs-Kontrollsmokes.

Dokumentationstests oder Review-Gates:

- `docs/quality.md` nennt Plattformmatrix, Atomic-Replace-Gate und
  CLI-Smokes.
- `docs/releasing.md` nennt offiziellen und nicht offiziellen
  Plattformstatus.
- `README.md` widerspricht `docs/releasing.md` nicht.
- `docs/guide.md` nennt dieselbe CMake-Mindestversion wie `README.md` und
  `cmake_minimum_required`.
- Release-Workflow veroeffentlicht keine unmarkierten macOS-/Windows-Artefakte
  als offizielle M5-Assets.
- Ein nicht ueberspringbarer Release-Guard im finalen Release-Lauf bricht ab,
  wenn ein macOS-/Windows-Archiv in der offiziellen M5-Assetliste auftaucht.
- Der Release-Dry-Run deckt denselben Guard mit einem Negativfall ab.

Rueckwaertskompatibilitaets-Tests:

- Linux-Docker-Gates bleiben unveraendert gruen.
- Bestehende Console-, Markdown-, JSON-, DOT- und HTML-Goldens bleiben
  fachlich unveraendert.
- Plattformhaertung fuehrt keine neuen Reportfelder ein.
- Plattformhaertung aendert keine Exit-Codes, CLI-Optionen oder
  Verbosity-Vertraege.

## Abnahmekriterien

AP 1.7 ist abnahmefaehig, wenn:

- Linux, macOS und Windows in der nativen CI-Matrix oder ueber verpflichtend
  validierte Smoke-Report-Artefakte sichtbar bewertet sind.
- Der M5-Plattformstatus fuer Linux, macOS und Windows eindeutig dokumentiert
  ist.
- `validated_smoke` fuer macOS oder Windows nur vergeben wird, wenn der
  zugehoerige CI-Job oder Smoke-Report-Check gruen ist.
- macOS und Windows nicht stillschweigend als offiziell freigegebene
  Releaseplattformen erscheinen.
- Atomic-Replace-Tests auf Linux, macOS und Windows laufen.
- Vorhandene Zieldateien bei Render-, Schreib- und Replace-Fehlern
  unveraendert bleiben.
- Windows-Replace-Semantik ueber `ReplaceFileW` oder `MoveFileExW` mit
  dokumentierten Flags umgesetzt oder als blockierende Einschraenkung
  dokumentiert ist.
- CLI-Smokes fuer `--help`, `analyze` und `impact` auf macOS und Windows
  laufen oder als `known_limited` mit konkretem Fehler dokumentiert sind.
- CLI-`--output`-Smokes fuer mindestens JSON laufen auf macOS und Windows und
  pruefen leeres stdout sowie gueltigen Dateiinhalt.
- Pfadnormalisierung fuer Laufwerksbuchstaben, UNC-Pfade,
  Extended-Length-Pfade, Backslashes und POSIX-absolute Testpfade abgesichert
  ist.
- Windows-Smokes den konvertierungsfreien MSYS-/PowerShell-Pflichtmodus klar
  von optionalen MSYS-Konvertierungs-Kontrollsmokes trennen.
- CRLF keine Golden-Scheindifferenzen erzeugt.
- CMake- und Compiler-Mindestversionen in Doku und CI konsistent sind und die
  CI bei zu alter CMake- oder Compiler-Version vor dem Build abbricht.
- `validated_smoke`-Nachweise fuer macOS und Windows als Required Checks oder
  nicht ueberspringbare Release-Gates konfiguriert sind.
- Smoke-Report-Nachweise ein versioniertes JSON-Schema und einen fachlichen
  Verifier verwenden, die Pflichtkommandos, `checkout_sha`, `head_sha`,
  Checksummen, Mindestversionen und Windows-Path-Modus pruefen.
- CMake-File-API-Einschraenkungen aus M4/M5 weiterhin dokumentiert sind.
- `docs/quality.md` die Plattform-Gates und Smoke-Abdeckung beschreibt.
- `docs/releasing.md` die Release- und Preview-Grenzen beschreibt.
- `README.md` keine weitergehende Plattformfreigabe verspricht als
  `docs/releasing.md`.
- `docs/guide.md` dieselbe CMake-Mindestversion und denselben Plattformstatus
  wie README, Quality- und Release-Dokumentation verwendet.
- Release-Workflow und Release-Doku keine unmarkierten macOS-/Windows-
  Artefakte in den offiziellen M5-Releaseumfang aufnehmen.
- ein nicht ueberspringbarer Release-Asset-Allowlist-Guard im finalen
  Release-Lauf unmarkierte macOS-/Windows-Artefakte vor der Veroeffentlichung
  blockiert.
- Coverage-, Lizard-, Clang-Tidy- und bestehende Docker-Gates nach AP 1.7
  gruen bleiben.

## Offene Folgearbeiten

Folgearbeiten ausserhalb von AP 1.7:

- Ein spaeterer Meilenstein kann macOS und/oder Windows offiziell freigeben,
  muss dann aber Release-Artefakte, Signierung, Installer-/Archivvertrag,
  Supportstatus und Recovery-Pfade neu spezifizieren.
- Multi-Arch-OCI-Images bleiben ein eigenes Release-Arbeitspaket.
- Eine plattformspezifische Performance-Baseline fuer macOS und Windows bleibt
  ausserhalb von M5.
- Zusaetzliche Toolchains wie MinGW, ClangCL neben MSVC oder Intel/macOS
  koennen als eigene Smoke-Erweiterung folgen.
- Signierung, Notarisierung und Windows-Code-Signing gehoeren nicht zu M5.

**Ergebnis**: Der Projektstand ist nicht mehr nur implizit Linux-zentriert;
macOS- und Windows-Risiken sind sichtbar, testbar und fuer spaetere
Freigabeentscheidungen priorisierbar.
