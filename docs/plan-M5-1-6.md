# Plan M5 - AP 1.6 Release-Artefakte und OCI-Image absichern

## Ziel

AP 1.6 macht die Bereitstellung aus dem Lastenheft verbindlich. Ziel ist ein reproduzierbarer, auditierbarer Release-Pfad:

- M5 wird als versioniertes Linux-CLI-Artefakt und als OCI-Image veröffentlicht.
- Alle Schritte sind deterministisch, versionserkennbar und durch einheitliche Gates abgesichert.
- Die Release-Publication darf keinen Zwischenzustand ohne vollstaendige Validierung ausweisen.

## Scope

Umsetzen:

- Taggesteuerte Ausloesung des Release-Workflows mit klaren Musterregeln.
- Saubere Trennung von Versionsnummer in CMake-Projektversion und veroeffentlichter App-/Release-Version.
- Einheitliche `--version`-Ausgabe auf Top-Level.
- Erstellung, Packaging und Pruefsummen fuer Linux-Archive.
- Aufbau, Tagging und Veroeffentlichung eines OCI-Runtime-Images.
- Strukturierter Release-Workflow mit Draft-Priorisierung vor oeffentlicher Veroeffentlichung.
- Integrierter Release-Dry-Run mit Fake-Publishern und lokaler Registry.
- Explizite Behandlung von macOS-/Windows-Artefakten auf Releaseniveau.
- Dokumentierte Recovery-/Rollback-Schritte bei partiellen Failures.

Nicht umsetzen:

- Release-Schritte ausserhalb des Workflows (manuelle Notfallpfade nur dokumentiert).
- Neue fachliche Analysefeatures, Target-Graph-Features oder CLI-Report-Aenderungen in diesem AP.
- Vollstaendige Plattformfreigabe fuer macOS und Windows als Standard-M5-Auslieferung.
- Umfassende Security/Hardening-Audits ausserhalb der Release-Pipeline-Gates.

Nicht Bestandteil von AP 1.6:

- vollstaendige M5-uebergreifende Release-Plattformstrategie jenseits klarer M5-Aussagen.
- Multi-Arch-Image-Strategien ausserhalb des im Projekt bereits vorgesehenen Releasestacks.
- Langfristige Produkt-Key-Management- oder Signaturinfrastruktur.

## Voraussetzungen aus AP 1.1 bis AP 1.5

AP 1.6 kann auf folgenden Vertraegen aufsetzen:

- stabile CLI-Dispatch- und CLI-Test-Infrastruktur (AP 1.5+),
- `--output`- und Artifact-Schreibpfade (AP 1.1+),
- dokumentierte Release-Dokumente (`README`, `docs/releasing.md`) aus den prioren APs.

Falls die Artefakt- oder Container-Ausfuehrbarkeit von AP 1.5 noch nicht vorliegt, ist AP 1.6 blockiert; ein teilweiser Freigabestatus nur fuer Linux-Archive ist nicht ausreichend.

## Dateien

Voraussichtlich zu aendern:

- `.github/workflows/release.yml`
- `scripts/release-dry-run.sh`
- `src/adapters/cli/cli_adapter.{h,cpp}`
- `src/hexagon/model/application_info.h`
- `Dockerfile`
- `docs/releasing.md`
- `README.md`
- `CMakeLists.txt`

Neue Dateien, falls noch nicht vorhanden:

- `tests/release/test_release_dry_run.sh`
- `tests/release/test_release_smoke.sh`
- `tests/release/README.md` (nur falls eine separierte E2E-Beschreibung gebraucht wird)

## Tag- und Versionsvertrag

### SemVer-Tag-Muster

- Trigger nur bei Push auf erlaubte Tags.
- Erlaubte Muster:
  - `vMAJOR.MINOR.PATCH` fuer reguläre Releases
  - `vMAJOR.MINOR.PATCH-PRERELEASE` fuer Vorabversionen
- Die harte Validierung nutzt diese Regex:
  - `^v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(-((0|[1-9][0-9]*)|[0-9A-Za-z-]*[A-Za-z-][0-9A-Za-z-]*)(\.((0|[1-9][0-9]*)|[0-9A-Za-z-]*[A-Za-z-][0-9A-Za-z-]*))*)?$`
- Unerlaubt und hart fehlerhaft:
  - `vfoo`
  - unvollstaendige Versionen
  - Build-Metadaten ohne freigegebene Semantik

Regeln:

- Die Tag-Pruefung muss vor Build/Publish laufen und den Workflow mit nonzero Ergebnis beenden.
- Build-Metadaten mit `+...` sind in M5 nicht erlaubt.
- Prerelease-Identifier sind nicht leer; numerische Identifier duerfen keine fuehrenden Nullen enthalten.
- Die normalisierte App-Version ist exakt der Tag ohne fuehrendes `v`.

### Projekt- und App-Version

- Die `project(... VERSION ...)` in der Root-`CMakeLists.txt` bleibt numerisch (`MAJOR.MINOR.PATCH`).
- Veroeffentliche Version fuer App/CLI/Release darf Prerelease-Suffixe enthalten.
- Release-Builds verwenden genau `XRAY_APP_VERSION` als kanonische Quelle fuer die ausgegebene CLI-/Release-Version; der Wert wird aus dem validierten Tag ohne fuehrendes `v` gebildet.
- `XRAY_VERSION_SUFFIX` ist nur fuer lokale oder nicht veroeffentlichende Builds zulaessig, wenn `XRAY_APP_VERSION` nicht gesetzt ist; dann wird die App-Version aus numerischer CMake-Projektversion plus Suffix gebildet.
- Wenn `XRAY_APP_VERSION` und `XRAY_VERSION_SUFFIX` gleichzeitig gesetzt sind, muss die Konfiguration fail-fast abbrechen, damit keine konkurrierenden Versionsquellen entstehen.
- Release-Gates pruefen, dass `XRAY_APP_VERSION`, `cmake-xray --version`, Paketmetadaten und Release-Tag dieselbe Version ohne fuehrendes `v` ergeben.
- `v1.2.0-rc.1` auf Tag-Ebene entspricht ausgegebener App-Version `1.2.0-rc.1`; interne Projektversion bleibt `1.2.0`.

## `cmake-xray --version` Vertrag

Top-Level-Verhalten:

- `cmake-xray --version` ist direkt auf der Root-CLI ausfuehrbar.
- Die Option ist ein globales CLI-Flag und kein Subcommand-Alias.
- Ausgabe auf `stdout`: die veroeffentlichte App-Version ohne fuehrendes `v`.
- Keine Initialisierung der Analyse-Pipeline.
- Ausgabe enthaelt keine Analyseartefakte oder Host-/Build-Kontext.
- Alte statische Strings mit fuehrendem `v` müssen entfernt oder normalisiert werden.
- Release-Prueflogik validiert die Ausgabe gegen die Tag- und Versionserwartung ohne fuehrendes `v`.

## Release-Artefakt-Vertrag

### Linux-Archiv

- Build erzeugt ein `.tar.gz`-Artefakt mit stabiler Namenskonvention.
- Muss mindestens enthalten:
  - lauffaehige `cmake-xray`-Binary,
  - Lizenz-/Hinweisdatei(en),
  - kurze Nutzungshinweise.
- SHA-256-Checksumme ist Teil des Releasepakets.
- SHA-256 wird vor jedem Publish als Gate verifiziert: Archiv entpacken, erwartete Datei pruefen, Checksumme gegen das erzeugte Archiv und die enthaltene Binary beziehungsweise die dokumentierte Binary-Checksumme abgleichen.
- Artefakte duerfen nicht an lokale Build-Pfade koppeln.
- Das Archiv muss reproduzierbar gebaut werden: feste Dateireihenfolge, stabile Pfadnamen ohne lokale Workspace-Praefixe, numerische Owner-/Group-Werte, stabile Dateimodi und ein einheitlicher Zeitstempel aus dem Release-Kontext, zum Beispiel `SOURCE_DATE_EPOCH`.
- Ein Release-Smoke baut das Linux-Archiv zweimal aus gleichem Commit und gleicher Version und vergleicht mindestens Archiv-Checksumme, entpackte Dateiliste, Dateimodi und Binary-Checksumme.

### Veroeffentlichungskette

- Build- und Verify-Jobs duerfen `release`-Artefakte erzeugen und smoke testen.
- Veroeffentlichen in Registry oder GitHub Release darf nur im finalen Release-Job nach allen Gates erfolgen.
- Kein vorzeitiges GHCR-Push aus Verify/Pre-Release-Schritten.
- Der finale Release-Job darf nur die offiziell freigegebenen Artefakte publishen: Linux-Archiv, zugehoerige Checksums und OCI-Image. macOS-/Windows-Preview-Artefakte duerfen nicht als normale GitHub-Release-Assets hochgeladen und nicht in GHCR-Tags oder OCI-Manifeste aufgenommen werden.
- GitHub-Release-Re-Runs muessen idempotent sein: Ein existierender Draft oder oeffentlicher Release zum Tag wird zuerst gelesen und gegen erwartete Release-Metadaten, Asset-Namen, Asset-SHA-256-Werte und Checksums verglichen.
- Existierende GitHub-Assets duerfen nicht blind ueberschrieben werden. Identische Assets werden wiederverwendet; fehlende Assets werden hochgeladen; abweichende Assets oder Checksums fuehren vor weiterer Mutation zum Abbruch.
- Ein existierender oeffentlicher Release darf bei einem Re-Run nur bestaetigt werden, wenn Release Notes, Asset-Liste und Checksums exakt zum erwarteten Manifest passen. Jede Abweichung ist ein Recovery-Fall und kein automatischer Publish-Pfad.
- Falls Preview-Artefakte fuer macOS/Windows explizit erlaubt werden, laufen sie ueber einen separaten Preview-Job nach den offiziellen M5-Gates und mit eigener Zielklasse, zum Beispiel Workflow-Artefakt oder separat markierter GitHub-Prerelease. Sie duerfen nicht vom finalen M5-Release-Job als normale Release-Assets hochgeladen werden.
- Jeder Publish-Schritt verwendet eine explizite Allowlist fuer offizielle Asset-Namen und OCI-Tags. Dateien ausserhalb dieser Allowlist, insbesondere Preview-Artefakte, fuehren im finalen Release-Job zum Abbruch statt zu Upload oder implizitem Ignorieren.

### OCI-Image-Vertrag

- OCI-kompatibles Runtime-Image baut lauffaehig auf Basis der CLI.
- Versioniertes Tagging mit der veroeffentlichten Version.
- Fuer regelmaessige Releases zusaetzlich ein `latest`-Tag.
- `latest` ist fuer Vorabversionen nicht zulässig.
- `latest` wird erst nach erfolgreichem Push und Manifest-Check des versionierten Tags aktualisiert.
- Re-Runs muessen idempotent sein: Wenn der versionierte Tag bereits existiert, muss dessen Digest mit dem neu gebauten Image uebereinstimmen; bei Abweichung bricht der Workflow vor `latest` und vor oeffentlicher Release-Publikation ab.
- Ein bestehendes `latest` darf automatisch nur dann bestaetigt oder gesetzt werden, wenn er auf den Digest des validierten regulaeren Versions-Tags zeigt. Bei Digest-Mismatch bricht der Workflow hart ab; ein Update von `latest` auf einen anderen Digest ist ausschliesslich ein dokumentierter manueller Recovery-/Nachpublish-Schritt und kein automatischer Re-Run-Pfad.
- Fuer denselben Release-Tag darf `latest` nie automatisch auf einen anderen Digest umgeschrieben werden.
- Runtime-Container muss mindestens `cmake-xray --help` ohne externe Toolchain ausfuehren koennen.
- Image-Publish nur nach erfolgreicher Artefakt- und Smoke-Validierung.

## Plattformartefakte macOS/Windows

- Bestehende macOS-/Windows-Jobs im Workflow sind explizit zu klassifizieren:
  - deaktiviert, oder
  - als experimentelle Preview-Artefakte benannt.
- Wenn ausgegeben, muessen sie in Release Notes und Doku klar als nicht vollstaendig freigegeben stehen.
- Deaktivierte oder nicht explizit preview-freigegebene macOS-/Windows-Jobs duerfen keine GitHub-Release-Assets, keine Checksums und keine OCI-Beitraege erzeugen.
- Preview-Artefakte duerfen nur nach erfolgreichem Preview-Gate in einem separaten Preview-Publish-Pfad erscheinen und bleiben ausserhalb des offiziellen M5-Releaseumfangs.
- Der offizielle M5-Release-Pfad muss technisch verhindern, dass Preview-Artefakte in die normale Asset-Liste, in Checksums, in Release-Manifeste oder in GHCR/OCI-Verweise aufgenommen werden.
- Diese Sperre ist als technische Upload-Allowlist in jedem finalen Publish-Schritt umzusetzen, nicht nur als Dokumentations- oder Namenskonvention.

## Draft-/Release-Reihenfolge

- Release-Notes werden aus Changelog generiert.
- Zuerst Draft/Interim-Release-Zustand erzeugen.
- `draft_release_created` muss vor jedem oeffentlichen Release-Schritt stehen.
- OCI-Image-Publish vor oeffentlicher Release-Publikation.
- Oeffentlicher Release ist der letzte Publish-Schritt des Workflows.
- Bei Fehlern darf kein Zwischenstatus als Erfolg kommuniziert werden.

## Recovery- und Rollback-Vertrag

- Bei Fehlschlag in Draft-Erstellung oder OCI-Publish:
  - Automatismus raeumt auf, **oder**
  - dokumentierte manuelle Reihenfolge mit mind. Draft-Loeschung, Tag-Korrektur und Release-Notes-Nachfuehrung.
- Bei Fehlschlag nach teils erfolgtem Publish:
  - GHCR-Tag-/Digest-Cleanup ist dokumentiert.
  - Retag-/Nachpublish-Verhalten ist definiert.
  - GitHub-Release-Cleanup ist dokumentiert, inklusive Assets, Pruefsummen und Release Notes.
  - Ein bereits oeffentlicher GitHub Release wird nicht abstrakt "nicht oeffentlich" gemacht. Der Runbook-Pfad muss einen konkreten API-Pfad waehlen: entweder oeffentlichen Release inklusive Assets loeschen und aus validierten Artefakten neu erstellen, oder Release sichtbar als fehlerhaft markieren und einen dokumentierten Nachpublish mit korrigierten Assets ausfuehren.
  - Wenn der Fehler nach `release_published` auftritt, muss der Runbook-Pfad explizit festlegen, ob geloescht und neu veroeffentlicht oder sichtbar nachpubliziert wird; die Entscheidung und die betroffenen Release-, Asset- und Digest-IDs muessen auditierbar bleiben.
  - Automatische Recovery darf keine neuen Assets oder Tags veroeffentlichen, bevor sie den aktuellen GitHub-Release-Zustand und die GHCR-Digests erneut gelesen und gegen die erwartete Publish-Sequenz validiert hat.

## Release-Dry-Run-Vertrag

- `scripts/release-dry-run.sh` ruft Publish-Operationen auf Fake-Publisher.
- Testziel ist lokal (keine externen Plattformen notwendig).
- Dry-Run setzt explizit eine `DRY_RUN`-/`FAKE_PUBLISHER`-Betriebsart und verweigert echte GitHub- oder Registry-Endpunkte. Fehlende Fake-Endpunkte, echte GHCR-/GitHub-URLs oder echte Publish-Tokens fuehren zum Abbruch.
- Assertions sind explizit:
  - `draft_release_created`
  - danach `oci_image_published`
  - danach `release_published`
  - oeffentlicher Release-Status zuletzt.
- Docker-Pushes im Dry-Run gegen lokale Registry, inklusive Tag-/`latest`-Verhalten und Manifest-Check.
- Dry-Run-Faelle muessen Release-Re-Run-Szenarien abdecken: existierender Draft mit identischen Assets wird wiederverwendet, existierender oeffentlicher Release mit identischem Manifest wird bestaetigt, abweichende Assets oder Checksums brechen vor `release_published` ab, und ein OCI-Digest-Mismatch bricht vor `latest` ab.

## Implementierungsreihenfolge

Die Umsetzung erfolgt in sechs verbindlichen Tranchen. Jede Tranche endet mit einem vollstaendigen Lauf der Docker-Gates aus `README.md` ("Tests und Quality Gates") und `docs/quality.md`. Die globalen Abnahmekriterien dieses Plans gelten zusaetzlich am Ende von Tranche F. Die Tranchen sind so geschnitten, dass jede einzeln reviewable und einzeln Docker-gegated landet; ein Stop nach Tranche A oder B ist ein konsistenter Zwischenstand (Versionsvertrag steht, aber kein Release-Artefakt) und kein halboffenes System.

DoD-Checkboxen in diesem Plan tracken den Liefer-/Abnahmestatus: `[x]` markiert eine in einem konkreten Commit ausgelieferte Anforderung, `[ ]` markiert eine offene Anforderung. Liefer-Stand zum Zeitpunkt der Tranche-A-Vorbereitung:

- Tranche A ausgeliefert in commits `8f6b2da` ("feat: deliver M5 AP 1.6 Tranche A tag validator and --version flag") und `ad2c9b1` ("fix: pin Tranche-A --help app name to 'cmake-xray' for CI Linux").
- Tranche B ausgeliefert in vorliegendem Commit-Set (Hash siehe `git log` nach Commit; Reproduzierbares Linux-Archiv, SHA-256-Sidecar, Repro-Smoke und `release-archive`-Docker-Stage stehen).
- Tranche C offen.
- Tranche D offen.
- Tranche E offen.
- Tranche F offen.

### Tranche A - Tag-/Versionsvertrag und `--version`-Flag

Fokus auf Versionsquellen und CLI-Verdrahtung. Tranche A liefert noch kein Release-Artefakt: kein Linux-Archiv, kein OCI-Image, kein Workflow-Trigger. Sie macht aber jeden spaeteren Schritt versionsfaehig, indem sie die kanonische Quelle `XRAY_APP_VERSION`, die Tag-Validierung und das globale `--version`-Flag etabliert.

Voraussichtlich zu aendernde Dateien:

- `CMakeLists.txt` (Cache-Variablen `XRAY_APP_VERSION` und `XRAY_VERSION_SUFFIX`, fail-fast bei doppelter Quelle, `target_compile_definitions` fuer den App-Versions-String).
- `src/hexagon/model/application_info.h` (oder neuer Generated-Header) als Lesepunkt fuer den `#define`-Wert.
- `src/adapters/cli/cli_adapter.{h,cpp}` (CLI11 `app.set_version_flag` mit der App-Version-Quelle).
- `scripts/validate-release-tag.sh` (neu, Bash-Tag-Validator gegen die Plan-Regex).
- `tests/release/test_validate_release_tag.sh` (neu, Smoke-Tests fuer Validator gegen die 4 Positiv- und 12 Negativfaelle aus dem Plan-Smoke-Vertrag).
- `tests/e2e/test_cli.cpp` oder `tests/e2e/test_cli_version.cpp` (CLI-Test fuer `cmake-xray --version`).

Implementierungsschritte:

1. CMake-Plumbing in `CMakeLists.txt` einfuehren:
   - Cache-Variablen `XRAY_APP_VERSION` (Standard `""`) und `XRAY_VERSION_SUFFIX` (Standard `""`).
   - Wenn beide gesetzt sind: `message(FATAL_ERROR ...)` mit klarer Erklaerung der Konfliktquelle.
   - Wenn `XRAY_APP_VERSION` gesetzt ist, ist es die kanonische App-Version. Sonst wird `${PROJECT_VERSION}${XRAY_VERSION_SUFFIX}` als Fallback verwendet (Suffix darf leer sein; ohne Suffix bleibt es die numerische Projektversion).
   - Resultat als Compile-Definition `XRAY_APP_VERSION_STRING="..."` an alle Targets weiterreichen, die `application_info` verwenden.
2. `application_info()` so anpassen, dass es `XRAY_APP_VERSION_STRING` liest. Vor Tranche A war ggf. eine andere Quelle; die alte Variante wird ersetzt, nicht parallel gepflegt.
3. Tag-Validator-Skript `scripts/validate-release-tag.sh` schreiben. Standalone-Bash, exit `0` bei gueltigem Tag (druckt App-Version ohne `v`), exit `1` bei ungueltigem Tag mit Klartext-Begruendung auf stderr. Implementierung nutzt die Plan-Regex aus Sektion `SemVer-Tag-Muster`.
4. `tests/release/test_validate_release_tag.sh` schreiben. Faehrt den Validator gegen die Positiv-/Negativfaelle aus dem Plan-Smoke-Vertrag (mindestens `v0.0.0`, `v1.2.3`, `v1.2.3-rc.1`, `v1.2.3-alpha.1-2` als Positiv; `1.2.3`, `v1.2`, `v1.2.3.4`, `v01.2.3`, `v1.02.3`, `v1.2.03`, `v1.2.3-`, `v1.2.3-01`, `v1.2.3-rc..1`, `v1.2.3+build`, `vfoo`, `v1.2.x` als Negativ). CTest-Eintrag in `tests/CMakeLists.txt`.
5. CLI11-Verdrahtung in `cli_adapter.cpp`: `app.set_version_flag("--version", std::string{xray::hexagon::model::application_info().version})` direkt nach Konstruktion der `app`. Das Flag muss vor Subcommand-Dispatch greifen; CLI11 wirft `CLI::CallForVersion` (Sub-Klasse von `ParseError`), die der bestehende `try`/`catch`-Block in `CliAdapter::run` als Erfolgsfall abhandeln muss (Exit `0`, App-Version ohne `v` auf stdout, kein Subcommand-Lauf).
6. CLI-Test fuer `cmake-xray --version`: erwartet stdout = App-Version + `\n`, leeres stderr, Exit `0`, kein Subcommand-Pipeline-Aufruf (Stub-Ports erhalten keine Calls).
7. Konsistenz-Test (CMake-Konfigurationsausgabe vs. CLI-Output): kann entweder ueber CTest mit `${PROJECT_BINARY_DIR}/cmake-xray --version` und Vergleich mit `XRAY_APP_VERSION` als CMake-Variable, oder ueber einen kleineren C++-Test, der `application_info().version == XRAY_APP_VERSION_STRING` pinnt.
8. Doku-Stub: kurze Notiz in `docs/releasing.md` (oder Tranche-F-Vorbereitung), dass die App-Version aus dem validierten Tag ohne `v` gebildet wird. Vollstaendige Releasing-Doku landet in Tranche F.

Sub-Risiken Tranche A:

- CLI11 `set_version_flag` ruft intern `throw CLI::CallForVersion`. `CliAdapter::run` muss diese Exception als Erfolg (Exit 0) behandeln und das Flag-Standard-Output (CLI11 schreibt `--version`-Output nach `out`) beibehalten. Falls der bestehende `app.exit(e, out, err)`-Pfad das automatisch erledigt, ist nur ein Test als Regression-Pin noetig; falls nicht, muss eine explizite Catch-Klausel ergaenzt werden.
- Versionsquellen-Konflikt: wenn ein Entwickler beide Cache-Variablen setzt, _muss_ CMake fail-fast brechen. Stiller Fallback auf eine der beiden Quellen ist ein Bug-Magnet.
- App-Version vs. CMake-Projektversion: `project(... VERSION ...)` bleibt numerisch (drei Komponenten). Prerelease-Suffixe leben nur in `XRAY_APP_VERSION`. Tests muessen pinnen, dass beides gleichzeitig korrekt funktioniert (CMake Project-Version `1.2.0`, App-Version `1.2.0-rc.1`).
- Kein Release-Tag im Build-Kontext (lokaler Entwickler): `XRAY_APP_VERSION` und `XRAY_VERSION_SUFFIX` sind beide leer. Fallback ist die numerische Projektversion. Die App-Version `1.2.0` ist dann zwar nicht von einem Tag abgeleitet, bleibt aber konsistent ausgegeben.

Definition of Done Tranche A:

- [x] `XRAY_APP_VERSION` und `XRAY_VERSION_SUFFIX` sind als CMake-Cache-Variablen dokumentiert; gleichzeitiges Setzen ist fail-fast.
- [x] `XRAY_APP_VERSION_STRING` wird ueber `add_compile_definitions` global an alle TUs weitergereicht (saemtliche Bibliotheken, Binary und Tests sehen denselben Wert); alte Versionsquellen sind entfernt. Die Plan-Vorlage erwaehnte `target_compile_definitions`; im Single-Repo-Single-Target-Layout ist die globale Variante aequivalent und der saubere Default, weil das Define als `#error`-Guard in `application_info.h` abgesichert ist und jede TU es beim Compile sehen muss.
- [x] `scripts/validate-release-tag.sh` lehnt alle 12 Negativfaelle aus dem Plan-Smoke-Vertrag ab und akzeptiert alle 4 Positivfaelle; die App-Version ohne `v` wird auf stdout gedruckt.
- [x] `cmake-xray --version` druckt die App-Version ohne `v` auf stdout, beendet mit Exit `0`, ohne Subcommand-Pipeline zu starten.
- [x] `--version` ist global und gleichbedeutend an Root-CLI verfuegbar; CLI-Tests pinnen es vor Subcommand-Dispatch.
- [x] Konsistenz-Test pinnt Gleichheit von Tag-ohne-`v`, `--version`-Ausgabe und `XRAY_APP_VERSION_STRING`.
- [x] CMake-Projektversion bleibt numerisch (`MAJOR.MINOR.PATCH`).
- [x] `tests/release/test_validate_release_tag.sh` ist als CTest-Eintrag registriert.
- [x] Docker-Gates aus `README.md` und `docs/quality.md` sind gruen.

### Tranche B - Linux-Archiv und Reproducibility

Erzeugt das offizielle Linux-Release-Artefakt mit deterministischer Pruefsumme und einem Reproducibility-Smoke. Tranche B liefert noch keinen Push und kein OCI-Image; sie endet mit einem im CI-Verify-Job gebauten Archiv plus `*.sha256`-Datei und einem Smoke, der die zwei Builds aus gleichem Commit byte-vergleicht.

Voraussichtlich zu aendernde Dateien:

- `CMakeLists.txt` (CPack TGZ generator oder explizite `install()`-Targets fuer Binary, LICENSE, Nutzungshinweise).
- `scripts/build-release-archive.sh` (neu, ruft den Build und die Tar-Erzeugung mit reproduzierbaren Optionen).
- `.github/workflows/release.yml` (Verify-Job: Archiv bauen, smoke entpacken, Reproducibility-Smoke).
- `tests/release/test_release_archive_repro.sh` (neu, Reproducibility-Smoke).

Implementierungsschritte:

1. Archiv-Inhalt fixieren: `cmake-xray`-Binary, `LICENSE`, eine `usage.txt` mit Kurz-Hinweisen (oder dem `README.md` als Trimmed-Variante). Die exakte Inhaltsliste wird im Skript hartkodiert; CPack-Komponenten reichen alternativ.
2. `scripts/build-release-archive.sh` schreiben: nimmt App-Version als Argument, ruft `cmake --build` mit `Release`-Konfiguration, sammelt die Inhalte in einem Staging-Verzeichnis, ruft `tar` mit den reproduzierbaren Flags `--sort=name --mtime=@${SOURCE_DATE_EPOCH} --owner=0 --group=0 --numeric-owner --pax-option=delete=ctime,delete=atime --format=ustar`, gzipt das Ergebnis (`gzip -n`, kein Filename in Header), und schreibt eine `*.sha256`-Datei.
3. `SOURCE_DATE_EPOCH` aus dem Tag-Commit-Zeitstempel ableiten (`git log -1 --pretty=%ct` auf den Tag). Ein Default-Wert fuer lokale Entwicklerbuilds (zum Beispiel `1`) reicht, um den Repro-Smoke deterministisch zu halten.
4. Verify-Job in `release.yml` (Pre-Release-Stage): Archiv bauen, `*.sha256` verifizieren, Archiv entpacken, `cmake-xray --help` und `cmake-xray --version` aus der entpackten Binary smoke-testen.
5. Reproducibility-Smoke `tests/release/test_release_archive_repro.sh`: zweimal `scripts/build-release-archive.sh` mit identischen Argumenten und sauberem Build-Verzeichnis aufrufen, vergleicht: Archiv-Checksumme, entpackte Dateiliste (inkl. Modus-Bits), Binary-Checksumme aus dem entpackten Inhalt. Im Fail-Pfad printet das Skript einen `diff`-Output beider Listen.
6. CTest-Eintrag fuer den Reproducibility-Smoke; markiert als `RESOURCE_LOCK release-build` falls parallele Builds konkurrierend dasselbe Output-Verzeichnis schreiben wuerden.

Sub-Risiken Tranche B:

- glibc/libstdc++-Versionen sind hostabhaengig. Ein Linux-Archiv, das auf Ubuntu 24.04 gebaut wird, funktioniert nicht zwingend auf aelteren Distributionen. Tranche B liefert kein statisches Linking-Gate; das Risiko bleibt im Plan dokumentiert, verschiebt sich aber nicht in eine spaetere Tranche, weil es nicht zum Release-Artefakt-Vertrag gehoert.
- `gzip -n` ist zwingend, sonst wandert der Quelldateiname in den gzip-Header und macht das Archiv non-reproducible.
- macOS-Tar in moeglichen Cross-Build-Umgebungen kennt einige der GNU-spezifischen Flags nicht. Tranche B baut auf Linux-Runnern; macOS-Build ist Tranche-E-Thema. Konsequenz: der Repro-Smoke aus Schritt 5 ist explizit als Linux-only markiert (`uname -s` Check am Skript-Anfang, `SKIP` auf macOS). Lokale macOS-Maintainer fahren stattdessen den Smoke ueber das `linux/amd64`-Toolchain-Container-Image.

Definition of Done Tranche B:

- [x] `scripts/build-release-archive.sh` baut das Linux-Archiv mit reproduzierbaren Tar-Optionen und schreibt eine `*.sha256`-Datei.
- [x] Archiv enthaelt exakt: `cmake-xray`-Binary, `LICENSE`, `README.md` (kurze Nutzungshinweise).
- [x] Archiv-Inhalt ist sortiert (`--sort=name`), Owner/Group sind `0:0` (`--owner=0 --group=0 --numeric-owner`), `mtime` aus `SOURCE_DATE_EPOCH`, `format=ustar` (kein pax-Header), `gzip -n` ohne Filename-Header.
- [x] Reproducibility-Smoke baut zweimal aus gleichem Commit; Archiv-, Inhaltsliste-, Sha256-Sidecar- und Binary-Checksumme sind byte-identisch. Smoke ist Linux-only; macOS-Hosts erhalten `SKIP`-Statement. Beide Builds teilen sich die FetchContent-Cache via `XRAY_FETCHCONTENT_BASE_DIR`, sodass die zweite Konfigurationsphase keine Drittabhaengigkeiten neu zieht.
- [x] `Dockerfile` exponiert eine `release-archive`-Stage als `FROM toolchain`, die ueber `--build-arg XRAY_APP_VERSION=...` aufgerufen wird; ein `release-archive-entrypoint.sh`-Entrypoint kopiert das Archiv plus Sidecar in `/output` und gibt eine klare Fehlermeldung mit Hint, wenn `/output` nicht gemountet ist.
- [x] CTest-Eintrag `release_archive_reproducibility` faehrt den Smoke; im Test-Gate sichtbar als eigener Test mit `WORKING_DIRECTORY=${PROJECT_SOURCE_DIR}`.
- [x] Verify-Pfad pusht im Tranche-B-Stand kein Artefakt nach GHCR oder GitHub Release; das `release-archive`-Stage produziert nur einen lokalen Container-Layer.
- [x] Docker-Gates aus `README.md` und `docs/quality.md` sind gruen.

### Tranche C - OCI-Image-Vertrag und Idempotenz

Erzeugt und tagt das OCI-Image. Tranche C liefert noch keinen oeffentlichen Push (das passiert nur im finalen Release-Job, der erst Tranche D/E umsetzt), aber sie pinnt den Build-, Tag- und Digest-Vertrag inklusive der `latest`-Idempotenz-Regel und der Re-Run-Sperre bei Digest-Mismatch.

Voraussichtlich zu aendernde Dateien:

- `Dockerfile` (Runtime-Stage so anpassen, dass das `cmake-xray`-Binary aus dem Build-Stage kopiert wird; falls bereits vorhanden, nur Versionierung ergaenzen).
- `.github/workflows/release.yml` (OCI-Build-Job, Push-Job, Digest-Vergleich).
- `scripts/oci-image-publish.sh` (neu, kapselt Buildx-Build, Push, Digest-Read und Idempotenz-Check).
- `tests/release/test_oci_image_smoke.sh` (neu, smoke `--help`/`--version` im Container).

Implementierungsschritte:

1. Buildx-Setup im Workflow: `docker/setup-buildx-action`. Build-Argumente: `XRAY_APP_VERSION` aus dem validierten Tag (Tranche A liefert die Quelle).
2. `scripts/oci-image-publish.sh` schreiben: nimmt Image-Tag-Form (`ghcr.io/<owner>/cmake-xray:<version>`), baut das Image, pusht (oder verzweigt im DRY_RUN auf eine lokale Registry; siehe Tranche D), liest den Digest via `docker buildx imagetools inspect`. Falls der Tag bereits existiert, vergleicht das Skript den existierenden Digest mit dem lokal gebauten und bricht bei Mismatch hart ab.
3. `latest`-Tag-Logik: nur fuer Non-Prerelease setzen (App-Version enthaelt kein `-`), nur nach erfolgreichem Push und Digest-Verifikation des Versions-Tags. `latest` darf fuer denselben Versions-Tag nie automatisch auf einen anderen Digest umgeschrieben werden; das Skript prueft den existierenden `latest`-Digest, vergleicht ihn mit dem Versions-Tag-Digest, und bricht ab, falls beide Digests existieren aber nicht gleich sind.
4. Smoke `tests/release/test_oci_image_smoke.sh`: `docker run --rm <image>:<version> --help` und `--version`. Erwartet Exit `0` und passende Outputs.
5. CTest-Eintrag fuer OCI-Smoke (markiert als optional in Umgebungen ohne Docker; Skript faellt mit `SKIP` falls `docker` nicht verfuegbar).
6. Workflow-Verdrahtung: OCI-Build im Verify-Job (kein Push); OCI-Push im finalen Release-Job (Tranche E haengt den finalen Job zusammen).

Sub-Risiken Tranche C:

- GHCR-Auth: erfordert `permissions: packages: write` und einen Login mit `secrets.GITHUB_TOKEN`. Falsch gesetzte Permissions sind ein Standard-Tripfall.
- `latest`-Race: zwei parallel laufende Workflows fuer unterschiedliche Tags konkurrieren um `latest`. Plan-Regel "Update von `latest` auf einen anderen Digest ist nur dokumentierter manueller Recovery-Pfad" muss im Skript hart durchgesetzt werden, damit die zweite Workflow-Instanz nicht gewinnt.
- Digest-Read fuer existierende Tags ist nicht atomar mit Push. Wenn zwischen Push und Digest-Read ein Drittes Re-Push kommt, ist der Read inkorrekt. In M5-Scope ist das ausgeschlossen, weil Pushes nur ueber den finalen Release-Job laufen, der serialisiert auf Tag laeuft.
- Buildx-Cache-Korruption: ein abgestandener GHA-Build-Cache (`type=gha`) kann zwischen Re-Runs eine alte Layer-Version wiederverwenden und den Digest gegen die Erwartung verschieben. Cache-Schluessel muessen `XRAY_APP_VERSION` enthalten oder der Cache wird beim Versions-Tag-Wechsel invalidiert; Plan-Skript dokumentiert den Cache-Schluessel-Vertrag, sonst kippt die Idempotenz-Pruefung trotz korrektem Build.

Definition of Done Tranche C:

- [ ] `Dockerfile` Runtime-Stage baut `cmake-xray` mit `XRAY_APP_VERSION` als Build-Argument.
- [ ] `scripts/oci-image-publish.sh` baut, pusht (Push-Pfad selbst gegen die lokale Registry aus Tranche D testbar), liest Digest und prueft Idempotenz.
- [ ] Re-Run mit existierendem Versions-Tag und gleichem Digest ist erfolgreich; Re-Run mit Digest-Mismatch bricht vor `latest` ab.
- [ ] `latest`-Tag wird nur fuer Non-Prerelease gesetzt; bei Mismatch zwischen `latest` und Versions-Tag-Digest bricht der Workflow ab.
- [ ] Container-Smoke `cmake-xray --help` und `cmake-xray --version` laeuft im gebauten Image.
- [ ] Im Verify-Job wird das OCI-Image gebaut und Container-Smoke ausgefuehrt, ohne Push. Der Push-Step im finalen Release-Job existiert als separater Workflow-Job-Stub mit `if:`-Gate, das in Tranche E aktiviert wird.
- [ ] Docker-Gates aus `README.md` und `docs/quality.md` sind gruen.

### Tranche D - Release-Dry-Run und Recovery-Pfad

Etabliert den lokalen Dry-Run-Pfad ueber Fake-Publisher und eine lokale OCI-Registry. Tranche D liefert die Asserts `draft_release_created` -> `oci_image_published` -> `release_published` als beobachtbare State-Transitions, deckt die sechs Re-Run-Szenarien aus dem Plan-Smoke-Vertrag ab, und schreibt das Recovery-Runbook fuer partielle Failures.

Voraussichtlich zu aendernde Dateien:

- `scripts/release-dry-run.sh` (neu, Top-Level-Eintrittspunkt).
- `scripts/release-fake-github.sh` (neu, faked GitHub-API-Antworten ueber lokalen HTTP-Server oder Bash-Wrapper).
- `tests/release/test_release_dry_run.sh` (neu, ruft Dry-Run mit Test-Szenarien auf).
- `docs/releasing.md` (Recovery-Runbook-Sektion; kompletter Doku-Sweep in Tranche F).

Implementierungsschritte:

1. Fake-Registry-Setup: `docker run --rm -d -p <port>:5000 --name xray-dry-run-registry registry:2`. Dry-Run pusht dorthin statt nach GHCR. Cleanup im Skript-Trap (`trap "docker stop xray-dry-run-registry || true" EXIT`).
2. Fake-GitHub-API: minimaler Bash-Wrapper fuer `gh api`-Calls. `release-fake-github.sh` haelt einen JSON-State in `${TMPDIR}/fake-github/` und implementiert die Calls, die der Release-Workflow benoetigt: `repos/:owner/:repo/releases` GET/POST/PATCH/DELETE, `repos/:owner/:repo/releases/:id/assets` GET/POST. Der `gh`-Wrapper inspiziert `$DRY_RUN` und routet Calls auf das lokale State-Verzeichnis.
3. `scripts/release-dry-run.sh`: setzt `DRY_RUN=true` und `FAKE_PUBLISHER=true`, refused echte Endpunkte (Abbruch wenn `GH_TOKEN` ungesetzt aber `gh`-Pfad nicht ueberschrieben), startet Fake-Registry und Fake-API, ruft die Tranche-A-D-Bausteine in der Plan-Reihenfolge `draft_release_created` -> `oci_image_published` -> `release_published` auf und assertet jeden State.
4. `tests/release/test_release_dry_run.sh`: faehrt sechs Szenarien aus dem Plan-Smoke-Vertrag:
   - Re-Run mit vorhandenem Draft und unveraenderten Assets -> Wiederverwendung, kein neuer Asset-Upload.
   - Re-Run mit vorhandenem oeffentlichem Release und unveraendertem Manifest -> Bestaetigung, kein neuer Push.
   - Re-Run mit geaendertem Asset -> Abbruch vor `release_published`.
   - Re-Run mit geaenderter Checksumme -> Abbruch vor `release_published`.
   - Re-Run mit Manifest-Mismatch -> Abbruch vor `release_published`.
   - Re-Run mit OCI-Digest-Mismatch -> Abbruch vor `latest`.
5. Recovery-Runbook in `docs/releasing.md`: Schritte fuer partielle Failures (Draft existiert + OCI fehlt; Release publik + OCI fehlt; OCI publik + Release fehlt; OCI publik + `latest` falsch). Jeder Fall mit konkretem `gh api`/`docker buildx imagetools`-Befehl statt abstrakter "Release loeschen"-Anweisung.
6. CTest-Eintrag fuer Dry-Run-Test (markiert als `SKIP` falls `docker` nicht verfuegbar oder Port `<port>` belegt).

Sub-Risiken Tranche D:

- Fake-GitHub-API-Komplexitaet: zu detailliert -> wartungsintensiv; zu grob -> deckt nicht die Idempotenz-Faelle ab. Plan-Vertrag fokussiert sich auf Releases + Assets, nicht auf alle GitHub-API-Endpunkte. Der Fake-Server muss nur die im Workflow tatsaechlich aufgerufenen Endpunkte abdecken.
- Lokaler Port-Conflict: `5000` ist auf macOS-Hosts haeufig durch AirPlay belegt. Dry-Run nutzt einen freien Port via `python3 -c 'import socket; ...'` oder festen Test-Port `15000`.
- `gh`-Wrapper-Pfad: muss vor Default-`gh` auf `PATH` stehen. Standard-Implementation: das Dry-Run-Skript exportiert `PATH="$DRY_RUN_BIN:$PATH"` und legt einen `gh`-Symlink im `$DRY_RUN_BIN` ab.
- Recovery-Runbook-Korrektheit: schnell veraltet, wenn der Workflow umgebaut wird. Tranche-F-Sweep prueft die Konsistenz nochmal beim Doku-Abschluss.

Definition of Done Tranche D:

- [ ] `scripts/release-dry-run.sh` aktiviert `DRY_RUN`/`FAKE_PUBLISHER` und weigert sich gegen echte GitHub- oder Registry-Endpunkte.
- [ ] Lokale OCI-Registry und Fake-GitHub-API werden vom Skript hochgefahren und beim Beenden geraeumt.
- [ ] Die State-Transitions `draft_release_created` -> `oci_image_published` -> `release_published` sind als beobachtbare Asserts gepinnt.
- [ ] Sechs Re-Run-Szenarien aus dem Plan-Smoke-Vertrag laufen als Tests; jedes Szenario hat einen klaren Pass-/Abbruch-Pfad.
- [ ] `docs/releasing.md` enthaelt einen Recovery-Runbook-Abschnitt mit konkreten `gh api`- und `docker buildx imagetools`-Befehlen.
- [ ] Docker-Gates aus `README.md` und `docs/quality.md` sind gruen.

### Tranche E - Plattformartefakt-Allowlist und Preview-Sperre

Klassifiziert die bestehenden macOS-/Windows-Workflow-Jobs und implementiert die technische Allowlist-Sperre, die Plan-Vertrag in `Plattformartefakte macOS/Windows` und `Veroeffentlichungskette` verbindlich macht. Tranche E ist primaer ein Workflow-Refactor und schliesst den finalen Release-Job ab.

Voraussichtlich zu aendernde Dateien:

- `.github/workflows/release.yml` (Job-Klassifikation, Allowlist im finalen Publish-Job, optionaler Preview-Job).
- `scripts/release-allowlist.sh` (neu, validiert die zu publishenden Asset-Namen gegen eine fixe Liste).
- `tests/release/test_release_allowlist.sh` (neu, Negativtests fuer Preview-Artefakte in der Allowlist).

Implementierungsschritte:

1. Bestehende macOS-/Windows-Jobs im Workflow inspizieren und klassifizieren:
   - Wenn aktuell aktiv und nicht verlaesslich: deaktivieren oder als experimentelle Preview-Artefakte mit eigener Tag-/Asset-Konvention markieren.
   - Wenn deaktiviert: bestaetigt deaktiviert lassen.
2. `scripts/release-allowlist.sh` schreiben: nimmt die im finalen Publish-Job zu uploadende Asset-Liste als Argumente, prueft jeden Namen gegen die fixe Allowlist (Linux-Archiv, dessen `*.sha256`, OCI-Manifest-Beitrag), und bricht mit Klartext-Fehler ab, falls ein Asset ausserhalb der Liste auftaucht.
3. Allowlist im finalen Release-Job verdrahten: `scripts/release-allowlist.sh` laeuft als Pre-Publish-Schritt und gated den Upload-Step. Falls ein macOS-/Windows-Preview-Artefakt durchschlaegt, bricht der Job hart ab.
4. Optionaler Preview-Job (separat): laeuft nach dem M5-Release-Job und nutzt eigene Asset-Namen + GitHub-Prerelease-Markierung. Allowlist gilt fuer Preview-Job nicht; statt dessen verwendet er eine eigene, kleinere Allowlist mit Preview-Tags.
5. Negativtests in `tests/release/test_release_allowlist.sh`: Dry-Run mit injiziertem `cmake-xray-1.2.0-darwin-arm64.tar.gz`-Asset muss vor Publish abbrechen; die Allowlist-Fehlermeldung muss den verstossenden Asset-Namen klar nennen.
6. Workflow-Verdrahtung mit Tranche-D-Dry-Run: der Dry-Run muss die Allowlist mit aufrufen, sonst entgleitet eine Inkonsistenz zwischen Dry-Run-Asserts und realer Publish-Sequenz.

Sub-Risiken Tranche E:

- Workflow-Refactor bricht Build-Pipelines unbeabsichtigt: beim Refactor muss `actions/checkout@v...`-Versionen, `permissions:` und `secrets:` exakt erhalten bleiben.
- Preview-Job-Trennung haengt vom GitHub-Release-Modell ab. Wenn ein Preview-Artefakt versehentlich denselben Tag wie M5 erhaelt, kollidiert der Tag-Push. Plan-Vertrag fordert eigene Tag-Klasse fuer Preview; Tranche E bestaetigt das in der Workflow-Konfiguration.
- Allowlist-Skript ist sicherheitskritisch: ein Bug, der unerwuenschte Assets durchlaesst, ist genau die Lehre aus dem Plan-Anforderungs-Bullet `nicht nur als Dokumentations- oder Namenskonvention`. Skript braucht eigene Tests inklusive Negativ-Inputs.

Definition of Done Tranche E:

- [ ] macOS-/Windows-Jobs im Workflow sind explizit klassifiziert (deaktiviert oder Preview-Artefakte mit eigener Allowlist).
- [ ] `scripts/release-allowlist.sh` validiert die zu publishenden Asset-Namen gegen eine fixe Liste und bricht bei Verstoss ab.
- [ ] Finaler Release-Job ruft die Allowlist als Pre-Publish-Schritt; ein injiziertes Preview-Artefakt fuehrt zum dokumentierten Abbruch.
- [ ] Preview-Sperrtests aus dem Plan-Smoke-Vertrag laufen: kein Preview-Artefakt erscheint in Checksums, Release-Manifest oder GHCR/OCI-Verweisen.
- [ ] Dry-Run aus Tranche D faehrt die Allowlist als Teil seiner Sequenz.
- [ ] Drei-Wege-Versionskonsistenz-Check ist gepinnt: Tag-ohne-`v`, `cmake-xray --version`-Ausgabe und Release-Publish-Metadaten (Asset-Namen, OCI-Tag, GitHub-Release-Name) zeigen exakt dieselbe Version. Der Check laeuft im finalen Release-Job nach Allowlist und vor `release_published`.
- [ ] Docker-Gates aus `README.md` und `docs/quality.md` sind gruen.

### Tranche F - Dokumentation und Final-Sweep

Schliesst AP 1.6 ab. Tranche F dokumentiert die Tag-Muster, Versionsregel, `latest`-Regel, macOS-/Windows-Status und den Recovery-Pfad in `docs/releasing.md` und ergaenzt `README.md` um lokale und CI-Nutzung des CLI-Artefakts und des OCI-Images. Anschliessend laeuft ein Coverage-/Lizard-/Clang-Tidy-Sweep.

Voraussichtlich zu aendernde Dateien:

- `docs/releasing.md` (Tag-Muster, Versionsregel, `latest`-Regel, macOS/Windows-Status, Recovery-Pfad-Index).
- `README.md` (CLI-Artefakt-Download und OCI-Image-Pull, lokale und CI-Beispiele).
- `docs/quality.md` (Release-Pipeline-Gates listen).
- `docs/plan-M5-1-6.md` (Liefer-Stand-Eintraege fuer Tranche A bis F flippen, sobald die jeweiligen Commits gelandet sind).

Implementierungsschritte:

1. `docs/releasing.md` erweitern um:
   - Tag-Muster und Beispiele (positive und negative aus dem Plan-Smoke-Vertrag).
   - Versionsregel (CMake-Projektversion vs. veroeffentlichte App-Version, `XRAY_APP_VERSION`, `XRAY_VERSION_SUFFIX`).
   - `latest`-Regel inklusive der Bedingung "nur fuer Non-Prerelease und nur nach Digest-Validation".
   - macOS-/Windows-Status: aktueller Plan-Stand (deaktiviert oder Preview-Artefakt) und der Pfad, wie ein Preview-Artefakt freigegeben wird.
   - Recovery-Runbook-Verweis auf den in Tranche D angelegten Abschnitt.
2. `README.md` erweitern um:
   - Download-Beispiel fuer das Linux-Archiv (`tar -xzf cmake-xray_<version>_linux_x86_64.tar.gz` etc.).
   - Pull-/Run-Beispiel fuer das OCI-Image (`docker run --rm ghcr.io/.../cmake-xray:<version> --help`).
   - Hinweis, dass `latest` nur stabile Releases markiert.
3. `docs/quality.md` um die Release-Pipeline-Gates ergaenzen (Tag-Validator, Reproducibility-Smoke, OCI-Digest-Idempotenz, Allowlist-Sperre, Dry-Run-Asserts).
4. Coverage-Gate abschliessen: `docker build --target coverage-check --build-arg XRAY_COVERAGE_THRESHOLD=100` muss gruen sein. Pruefpunkte: neue CLI-Pfade aus Tranche A (`--version`-Flag), neue Skript-Hilfen aus Tranche B-E (Bash-Skripte sind nicht im C++-Coverage, aber CLI-Tests fuer den `--version`-Pfad sind es).
5. Lizard-Gate abschliessen: `docker build --target quality-check` mit Standardgrenzen muss gruen sein. CLI-Adapter-Aenderungen aus Tranche A duerfen die CCN nicht ueber 10 treiben.
6. Clang-Tidy-Gate abschliessen: `docker build --target quality-check` mit `XRAY_CLANG_TIDY_MAX_FINDINGS=0` muss gruen sein.
7. Plan-`Liefer-Stand`-Eintraege fuer Tranche A bis F auf "ausgeliefert in commit `<hash>`" flippen, sobald jeder Tranche-Commit gelandet ist.

Sub-Risiken Tranche F:

- Doku-Konsistenz: `README.md` referenziert konkrete Versionsnummern, die schnell veralten. Tranche F nutzt Plat zhalter wie `vX.Y.Z` mit klarer Erklaerung, dass `X.Y.Z` durch die aktuelle Release-Version ersetzt werden muss.
- Coverage-Luecken durch Skript-Pfade: Bash-Skripte sind nicht im gcovr-Filter; aber neue C++-Pfade aus Tranche A sind es. Falls eine neue CLI-Codezeile uneberreichbar ist, wird das Coverage-Gate kippen — die Luecke wird dann in Tranche F geschlossen, nicht nach Tranche G verschoben.

Definition of Done Tranche F:

- [ ] `docs/releasing.md` listet Tag-Muster, Versionsregel, `latest`-Regel, macOS-/Windows-Status und Recovery-Pfad.
- [ ] `README.md` enthaelt lokale und CI-Nutzungsbeispiele fuer CLI-Artefakt und OCI-Image.
- [ ] `docs/quality.md` listet Release-Pipeline-Gates als verbindlich.
- [ ] Coverage-, Lizard- und Clang-Tidy-Gates sind gruen.
- [ ] Plan-`Liefer-Stand` reflektiert die Tranche-A-bis-F-Lieferung mit Commit-Hashes.
- [ ] Docker-Gates aus `README.md` und `docs/quality.md` sind gruen.

### Abnahme und Definition of Done AP 1.6

Die Tranchen A bis F werden gemeinsam abgenommen. Die folgenden Kriterien gelten am Ende von Tranche F fuer das gesamte AP-1.6-Set.

Abnahme AP 1.6: alle Docker-Gates aus `README.md` und `docs/quality.md` gruen; `cmake-xray --version` druckt die App-Version ohne `v`; Linux-Archiv ist reproduzierbar gebaut und mit `*.sha256` versehen; OCI-Image ist versioniert publishbar und idempotent re-runnable; `latest`-Tag folgt der dokumentierten Regel; Dry-Run deckt sechs Re-Run-Szenarien ab; macOS-/Windows-Allowlist sperrt Preview-Artefakte aus dem M5-Release; `docs/releasing.md` enthaelt das Recovery-Runbook.

## Entscheidungen

Diese Entscheidungen sind vor Umsetzungsbeginn getroffen und in die Tranchen eingearbeitet:

- CLI11 `app.set_version_flag(...)` ist der Mechanismus fuer das globale `--version`-Flag. Begruendung: native Bibliotheksunterstuetzung; CLI11 wirft `CLI::CallForVersion` aus dem Parser, was der bestehende `try`/`catch`-Block in `CliAdapter::run` als Erfolgsfall behandelt; konsistent mit dem `--help`-Verhalten, das bereits ueber CLI11 gehaendelt wird; keine doppelte Verdrahtung im Subcommand-Dispatcher.
- `XRAY_APP_VERSION` und `XRAY_VERSION_SUFFIX` sind CMake-Cache-Variablen, der App-Versions-String wird per `target_compile_definitions` als `XRAY_APP_VERSION_STRING="..."` an `application_info` weitergereicht. Begruendung: Cache-Var ist von der CI per `-D` aus dem Tag steuerbar; `#define` haelt die Version compile-time stabil und vermeidet Laufzeit-Datei-Lookups; gleichzeitiges Setzen beider Cache-Variablen ist fail-fast, weil zwei konkurrierende Versionsquellen ein Bug-Magnet sind.
- Tag-Validator wird als Bash-Skript `scripts/validate-release-tag.sh` implementiert. Begruendung: konsistent mit den bestehenden `tests/validate_*`-Skripten und `tests/e2e/run_e2e_lib.sh`; portabel auf Ubuntu- und macOS-Runnern; keine zusaetzlichen Build-Time-Dependencies (Python ist im Toolchain-Image, aber Bash ist ueberall).
- Lokale OCI-Registry fuer den Dry-Run laeuft als ad-hoc-Container `docker run --rm -d -p <port>:5000 registry:2`. Begruendung: keine zusaetzlichen Tools (Testcontainers vermieden); deterministisches Lifecycle ueber `trap`-Cleanup im Skript; Standard-`registry:2`-Image ist klein und stabil.
- GitHub-Release-Idempotenz nutzt `gh api`-Aufrufe ueber die `gh`-CLI. Begruendung: `gh` ist im Standard-GitHub-Actions-Runner verfuegbar; identische Calls funktionieren in lokal-laufenden Dry-Runs ueber den Fake-`gh`-Wrapper aus Tranche D; kein JavaScript-`actions/github-script`-Overhead und kein Sub-Workflow-Refactor.
- `SOURCE_DATE_EPOCH` wird im CI-Release-Job aus dem Tag-Commit-Zeitstempel abgeleitet (`git log -1 --pretty=%ct $TAG`); fuer lokale Entwickler- und Repro-Smoke-Builds ohne Tag wird ein fixer Default `1` gesetzt. Begruendung: deterministische Quelle in der CI haelt das Linux-Archiv reproduzierbar; lokaler Default deckt den Repro-Smoke ab, der zweimal aus gleichem Commit ohne Tag baut; das Skript `scripts/build-release-archive.sh` setzt den Wert ueber `export`, sodass `tar --mtime=@${SOURCE_DATE_EPOCH}` und `gzip -n` ihn beide sehen.
- `XRAY_APP_VERSION` wird im finalen Release-Job in der Workflow-YAML gesetzt: das Validator-Skript `scripts/validate-release-tag.sh` druckt die App-Version ohne `v` auf stdout; der Job-Step capturet sie ueber `echo "XRAY_APP_VERSION=$(scripts/validate-release-tag.sh "$GITHUB_REF_NAME")" >> "$GITHUB_ENV"` und reicht sie als `-DXRAY_APP_VERSION=...` in den nachfolgenden `cmake -B build`-Schritt sowie als `--build-arg XRAY_APP_VERSION=...` in den Buildx-Schritt weiter. Begruendung: einzige autoritative Verteilungsquelle aus dem validierten Tag; `$GITHUB_ENV` haelt den Wert ueber Steps hinweg; keine Drift zwischen CMake-Cache-Var, OCI-Build-Argument und Release-Asset-Namen.
- Allowlist-Form fuer den finalen Publish-Job ist eine Bash-Konstante (Array) in `scripts/release-allowlist.sh`. Begruendung: Allowlist ist M5-spezifisch und klein (drei bis fuenf Eintraege); externe Datei wuerde nur eine zusaetzliche Indirektion ohne Audit-Vorteil schaffen; YAML-Liste wuerde Workflow-Logik einbauen, die sich ueber Test-Stellen nicht teilen laesst. Dry-Run aus Tranche D ruft dasselbe Skript, sodass die Allowlist-Definition nirgends dupliziert wird.

## Smoke- und Validierungstests

- Smoke der entpackten Linux-Archive (Vollstaendigkeit/Startfaehigkeit).
- Reproduzierbarkeits-Smoke fuer Linux-Archiv mit zwei Builds aus gleichem Commit und gleicher Version.
- Checksum-Gate vor Publish: SHA-256-Dateien muessen zu den erzeugten Artefakten passen und der entpackte Binary-Smoke muss aus genau diesem geprueften Archiv laufen.
- Smoke der OCI-Ausfuehrung (`cmake-xray --help`, `--version`).
- Tag-Validierung gegen erlaubte Muster und Regressionsfaelle.
- Tag-Validierungstests enthalten mindestens gueltige Faelle `v0.0.0`, `v1.2.3`, `v1.2.3-rc.1` und `v1.2.3-alpha.1-2`.
- Tag-Validierungstests enthalten mindestens ungueltige Faelle `1.2.3`, `v1.2`, `v1.2.3.4`, `v01.2.3`, `v1.02.3`, `v1.2.03`, `v1.2.3-`, `v1.2.3-01`, `v1.2.3-rc..1`, `v1.2.3+build`, `vfoo` und `v1.2.x`.
- Release-Idempotenztests enthalten mindestens: Re-Run mit vorhandenem Draft und unveraenderten Assets, Re-Run mit vorhandenem oeffentlichem Release und unveraendertem Manifest, Re-Run mit geaendertem Asset, Re-Run mit geaenderter Checksumme, Abbruch vor `release_published` bei Manifest-Mismatch und Abbruch vor `latest` bei OCI-Digest-Mismatch.
- Preview-Sperrtests enthalten mindestens: finaler Publish-Job bricht ab, wenn ein macOS-/Windows-Preview-Artefakt in der offiziellen Asset-Allowlist auftaucht, und kein Preview-Artefakt erscheint in Checksums, Release-Manifest oder GHCR/OCI-Verweisen.
- Versionskonsistenz-Check zwischen:
  - Tag,
  - `--version`-Ausgabe,
  - Release-Publish-Metadaten.

## Dokumentation

- `docs/releasing.md` ergänzt um:
  - Tag-Muster,
  - Versionsregel (CMake vs. Release-Version),
  - `latest`-Regel,
  - macOS-/Windows-Status,
  - Recovery-Pfad.
- `README.md` ergänzt um lokale und CI-Nutzung von CLI-Artefakt und OCI.

**Ergebnis**: Nutzer koennen `cmake-xray` als versioniertes Linux-Artefakt oder OCI-Image nutzen, ohne lokales Build-Verzeichnis zu kennen; die Release-Veroeffentlichung folgt einem nachvollziehbaren und abgesicherten Schrittmodell.
