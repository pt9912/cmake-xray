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
- `src/adapters/cli/application_info.{h,cpp}`
- `src/adapters/cli/cli_adapter.{h,cpp}`
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
- Unerlaubt und hart fehlerhaft:
  - `vfoo`
  - unvollstaendige Versionen
  - Build-Metadaten ohne freigegebene Semantik

Regel: Die Tag-Pruefung muss vor Build/Publish laufen und den Workflow mit nonzero Ergebnis beenden.

### Projekt- und App-Version

- Die `project(... VERSION ...)` in der Root-`CMakeLists.txt` bleibt numerisch (`MAJOR.MINOR.PATCH`).
- Veroeffentliche Version fuer App/CLI/Release darf Prerelease-Suffixe enthalten.
- `XRAY_APP_VERSION` oder `XRAY_VERSION_SUFFIX` ist die Quelle fuer die ausgegebene CLI-/Release-Version.
- `v1.2.0-rc.1` auf Tag-Ebene entspricht ausgegebener App-Version `1.2.0-rc.1`; interne Projektversion bleibt `1.2.0`.

## `cmake-xray --version` Vertrag

Top-Level-Verhalten:

- `cmake-xray --version` ist direkt auf der Root-CLI ausfuehrbar.
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
