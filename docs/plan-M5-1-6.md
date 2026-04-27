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
- Artefakte duerfen nicht an lokale Build-Pfade koppeln.

### Veroeffentlichungskette

- Build- und Verify-Jobs duerfen `release`-Artefakte erzeugen und smoke testen.
- Veroeffentlichen in Registry oder GitHub Release darf nur im finalen Release-Job nach allen Gates erfolgen.
- Kein vorzeitiges GHCR-Push aus Verify/Pre-Release-Schritten.

### OCI-Image-Vertrag

- OCI-kompatibles Runtime-Image baut lauffaehig auf Basis der CLI.
- Versioniertes Tagging mit der veroeffentlichten Version.
- Fuer regelmaessige Releases zusaetzlich ein `latest`-Tag.
- `latest` ist fuer Vorabversionen nicht zulässig.
- `latest` wird erst nach erfolgreichem Push und Manifest-Check des versionierten Tags aktualisiert.
- Re-Runs muessen idempotent sein: Wenn der versionierte Tag bereits existiert, muss dessen Digest mit dem neu gebauten Image uebereinstimmen; bei Abweichung bricht der Workflow vor `latest` und vor oeffentlicher Release-Publikation ab.
- Ein bestehendes `latest` darf nur auf den Digest des validierten regulaeren Versions-Tags zeigen. Eine Abweichung muss als auditierbarer Update von altem Digest auf neuen Digest protokolliert werden; fuer denselben Release-Tag darf `latest` nicht auf einen anderen Digest umgeschrieben werden.
- Runtime-Container muss mindestens `cmake-xray --help` ohne externe Toolchain ausfuehren koennen.
- Image-Publish nur nach erfolgreicher Artefakt- und Smoke-Validierung.

## Plattformartefakte macOS/Windows

- Bestehende macOS-/Windows-Jobs im Workflow sind explizit zu klassifizieren:
  - deaktiviert, oder
  - als experimentelle Preview-Artefakte benannt.
- Wenn ausgegeben, muessen sie in Release Notes und Doku klar als nicht vollstaendig freigegeben stehen.

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
  - GitHub-Release-Cleanup ist dokumentiert, inklusive Ruecknahme eines oeffentlichen Releases in einen nicht oeffentlichen Zustand, Loeschung oder Korrektur bereits hochgeladener Assets, Pruefsummen und Release Notes.
  - Wenn der Fehler nach `release_published` auftritt, muss der Runbook-Pfad explizit festlegen, ob der Release geloescht, als fehlerhaft markiert oder durch einen Nachpublish korrigiert wird; die Entscheidung und die betroffenen Asset-/Digest-IDs muessen auditierbar bleiben.
  - Automatische Recovery darf keine neuen Assets oder Tags veroeffentlichen, bevor sie den aktuellen GitHub-Release-Zustand und die GHCR-Digests erneut gelesen und gegen die erwartete Publish-Sequenz validiert hat.

## Release-Dry-Run-Vertrag

- `scripts/release-dry-run.sh` ruft Publish-Operationen auf Fake-Publisher.
- Testziel ist lokal (keine externen Plattformen notwendig).
- Assertions sind explizit:
  - `draft_release_created`
  - danach `oci_image_published`
  - danach `release_published`
  - oeffentlicher Release-Status zuletzt.
- Docker-Pushes im Dry-Run gegen lokale Registry, inklusive Tag-/`latest`-Verhalten und Manifest-Check.

## Smoke- und Validierungstests

- Smoke der entpackten Linux-Archive (Vollstaendigkeit/Startfaehigkeit).
- Smoke der OCI-Ausfuehrung (`cmake-xray --help`, `--version`).
- Tag-Validierung gegen erlaubte Muster und Regressionsfaelle.
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
