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
