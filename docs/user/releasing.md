# Releasing

## Zweck

Dieses Dokument beschreibt den minimalen, reproduzierbaren Release-Ablauf fuer `cmake-xray`.
Der Ablauf ist versionsunabhaengig formuliert und verwendet deshalb Platzhalter der Form `X.Y.Z`.

## Vorbereitung

Release-Version und Tag festlegen:

```bash
VER="X.Y.Z"
TAG="v$VER"
```

Vor einem Release:

- noch nicht veroeffentlichte Aenderungen bleiben unter `## [Unreleased]` in `CHANGELOG.md`; ein datierter Versionsabschnitt wird erst fuer den finalen Release-Commit angelegt
- `CHANGELOG.md` auf den Zielstand bringen
- betroffene Plan-, Status- und Nutzungsdokumente aktualisieren
- kuratierte Beispielausgaben unter `docs/examples/` aus den kanonischen M3- und M4-Fixtures regenerieren, falls sich Report-Inhalt oder -Layout geaendert hat
- bei Aenderungen am Analyse- oder Report-Pfad die Performance-Artefakte unter `build/reports/performance/` und `docs/user/performance.md` aktualisieren
- sicherstellen, dass nur beabsichtigte Dateien fuer den Release-Commit vorgemerkt sind
- falls Release-Artefakte mit veroefentlicht werden sollen, `./release-assets/` vorbereiten
- fuer tag-basierte Veroeffentlichung sicherstellen, dass `.github/workflows/release.yml` zum geplanten Artefaktumfang passt

## Verifikation

Vor Tagging und Release-Erstellung muessen die Docker-Pfade ueber das
Makefile erfolgreich sein:

```bash
make docker-gates
make runtime
docker run --rm \
  -v "$PWD/tests/e2e/testdata/m3:/data:ro" \
  cmake-xray analyze --compile-commands /data/report_project/compile_commands.json --format markdown
docker run --rm \
  -v "$PWD/tests/e2e/testdata/m3:/data:ro" \
  cmake-xray impact --compile-commands /data/report_impact_header/compile_commands.json --changed-file include/common/config.h --format markdown
```

Ab `v1.1.0` zusaetzlich die File-API-Pfade pruefen:

```bash
docker run --rm \
  -v "$PWD/tests/e2e/testdata/m4:/data:ro" \
  cmake-xray analyze --cmake-file-api /data/file_api_only/build
docker run --rm \
  -v "$PWD/tests/e2e/testdata/m4:/data:ro" \
  cmake-xray analyze --cmake-file-api /data/multi_target/build --format markdown
docker run --rm \
  -v "$PWD/tests/e2e/testdata/m4:/data:ro" \
  cmake-xray impact --cmake-file-api /data/file_api_only/build --changed-file include/common/config.h
docker run --rm \
  -v "$PWD/tests/e2e/testdata/m4:/data:ro" \
  cmake-xray analyze --compile-commands /data/partial_targets/compile_commands.json --cmake-file-api /data/partial_targets/build
```

Optional koennen die Reports separat ausgegeben werden:

```bash
make coverage-report
make quality-report
```

Vor einem MVP-Release zusaetzlich pruefen:

- `docs/user/performance.md` verweist auf aktuelle Messwerte und die Artefakte unter `build/reports/performance/`
- `docs/examples/` und die M3-Golden-Files unter `tests/e2e/testdata/m3/` stammen noch aus denselben kanonischen Fixtures

## Release-Commit und Tag

```bash
git add CHANGELOG.md README.md docs/examples docs/user/performance.md docs/user/quality.md docs/user/releasing.md docs/planning/plan-M3.md
git commit -m "docs: finalize release $TAG"
git tag -a "$TAG" -m "Release $TAG"
```

Nach `git push origin main "$TAG"` startet der Release-Workflow unter `.github/workflows/release.yml`. Er:

- verifiziert Test-, Coverage-, Quality- und Runtime-Pfade erneut
- paketiert ein versioniertes Linux-CLI-Release-Artefakt als `.tar.gz`
- erstellt eine SHA-256-Pruefsumme fuer das Archiv
- veroeffentlicht ein OCI-kompatibles Container-Image nach `ghcr.io/<owner>/<repo>:<tag>`
- aktualisiert oder erstellt den GitHub-Release und laedt die erzeugten Assets hoch

## Release-Notes aus dem Changelog

Den Abschnitt fuer die Zielversion aus `CHANGELOG.md` extrahieren:

```bash
VER_ESC=$(printf '%s' "$VER" | sed 's/\./\\./g')
sed -n "/^## \\[$VER_ESC\\]/,/^## \\[/{/^## \\[$VER_ESC\\]/!{/^## \\[/!p}}" \
  CHANGELOG.md > /tmp/release-notes.md
```

Pruefen, dass die erzeugten Release-Notes nicht nur existieren, sondern auch plausibel lang sind:

```bash
LINES=$(wc -l < /tmp/release-notes.md)
echo "Release notes: ${LINES} lines"
cat /tmp/release-notes.md
if [ "$LINES" -lt 5 ]; then
  echo "ERROR: release notes too short (${LINES} lines) — check CHANGELOG.md extraction"
  exit 1
fi
```

## GitHub-Release

Der Normalpfad ist der automatisierte Release-Workflow beim Tag-Push.
Er erstellt das Release als Draft, laeuft Tag-Validator, Allowlist-Guard,
Drei-Wege-Versionscheck und OCI-Push durch und veroeffentlicht das
Release nur, wenn alle Gates gruen sind. Manuelle `gh`-Aufrufe gehoeren
ausschliesslich in den Recovery-Pfad und nicht in den Normalbetrieb;
plan-M5-1-8.md "docs/user/releasing.md ... im Normalpfad kein
`gh release upload --clobber` und keinen Wildcard-Asset-Upload" ist
hier verbindlich.

Recovery-Voraussetzung: Vor jedem manuellen `gh`-Aufruf muss ein
Ist-/Soll-Abgleich von Release-Status, Asset-Liste, Checksums und
OCI-Digest erfolgen. Die konkreten Recovery-Faelle und ihre
Abgleich-Schritte stehen im Abschnitt
"[Recovery-Runbook (AP M5-1.6 Tranche D.2)](#recovery-runbook-ap-m5-16-tranche-d2)"
unten; das untenstehende Snippet ist nur die rohe Befehlsvorlage fuer
den Fall "Release existiert bereits, Asset-Liste muss nach
erfolgreichem Ist-/Soll-Abgleich abgeglichen werden":

```bash
# RECOVERY ONLY -- nicht im Normalpfad ausfuehren. Ist-/Soll-Abgleich
# muss vorher gegen Allowlist (scripts/release-allowlist.sh),
# bekannten Asset-Checksumme und OCI-Digest gelaufen sein.
if gh release view "$TAG" >/dev/null 2>&1; then
  echo "Release $TAG existiert bereits — Notes aktualisieren und Assets hochladen"
  gh release edit "$TAG" --notes-file /tmp/release-notes.md
  gh release upload "$TAG" ./release-assets/* --clobber
else
  echo "Release $TAG existiert noch nicht — neu erstellen"
  gh release create "$TAG" \
    --target main \
    --title "$TAG" \
    --notes-file /tmp/release-notes.md \
    ./release-assets/*
fi
```

Falls kein Asset-Upload erforderlich ist, koennen die `./release-assets/*`-Argumente weggelassen werden.

## Tag-, Versions- und `latest`-Vertrag (AP M5-1.6)

Der Release-Workflow akzeptiert nur Tag-Pushes mit dem in
[`docs/planning/plan-M5-1-6.md`](../planning/plan-M5-1-6.md) Sektion "SemVer-Tag-Muster"
fixierten Muster:

- `vMAJOR.MINOR.PATCH` (`v0.0.0`, `v1.2.3`).
- `vMAJOR.MINOR.PATCH-PRERELEASE` (`v1.2.3-rc.1`, `v1.2.3-alpha.1-2`).
- Build-Metadaten (`+...`) sind nicht erlaubt.

Die Tag-Validierung laeuft als erste Stufe ueber
`scripts/validate-release-tag.sh`; das Skript druckt die App-Version ohne
fuehrendes `v` auf stdout. Der Release-Workflow reicht dieses
Versions-String an alle nachgelagerten Stufen weiter (CMake-Cache-Var
`XRAY_APP_VERSION`, OCI-Tag, Asset-Naming, `--version`-Output). Der
Verify-Job extrahiert die Binary aus dem reproduzierbaren Linux-Archiv
und vergleicht ihren `--version`-Output gegen die erwartete Version,
und faehrt denselben Vergleich am lokal gebauten OCI-Image (vor Push).
Der finale Release-Job verifiziert zusaetzlich Tag-Gleichheit, Asset-
Namen-Suffix, OCI-Tag-Erreichbarkeit ueber `imagetools inspect` und
`--version`-Output am tatsaechlich publishten OCI-Image. Eine Drift in
einer dieser Quellen bricht den Workflow vor `release_published` ab.

CMake-Projektversion (`project(... VERSION ...)`) bleibt numerisch:
`MAJOR.MINOR.PATCH`. Die ausgelieferte App-Version (mit moeglichem
Prerelease-Suffix) lebt ausschliesslich in `XRAY_APP_VERSION` und in
`XRAY_APP_VERSION_STRING` (Compile-Time-Define). Lokale Entwicklerbuilds
ohne Tag fallen auf `${PROJECT_VERSION}${XRAY_VERSION_SUFFIX}` zurueck;
gleichzeitiges Setzen beider Cache-Variablen ist `FATAL_ERROR`.

`latest`-Tag des OCI-Images:

- nur fuer regulaere Releases (Versions-String enthaelt kein `-`).
- nur nach erfolgreichem Push und Digest-Validierung des Versions-Tags.
- ein bestehender `latest`-Digest (typischerweise der vorherige stable
  Release) wird durch den Push transparent auf den neuen Versions-Tag-
  Digest umgesetzt; der Workflow protokolliert die Transition als
  `info:`-Zeile zur Audit-Spur.
- nach dem Push wird der `latest`-Digest gegen den neu publishten
  Versions-Tag-Digest verifiziert; ein Mismatch (z. B. durch ein
  konkurrierendes Retag waehrend des Workflows) bricht hart ab, bevor
  der GitHub-Release public promotet wird.
- Fall 4 (Recovery-Runbook unten) bleibt der Pfad fuer den Fall, dass
  `latest` durch externe Inspektion auf einem Digest stehengeblieben
  ist, der weder dem aktuellen Release noch einem dokumentierten
  Vorgaenger-Release entspricht.

## Plattformartefakte macOS und Windows

### Plattformstatus (AP M5-1.7)

M5 unterscheidet drei Statusklassen:

- `supported` — offiziell freigegeben, dokumentiert und releasefaehig.
- `validated_smoke` — Build-, Atomic-Replace- und CLI-Pflicht-Smokes laufen
  gruen, aber ohne offizielle Releasefreigabe.
- `known_limited` — ein Pflichtgate ist nicht vollstaendig gruen, fehlt
  oder der Status wird nur ueber dokumentierte Einschraenkungen erreicht.

Aktueller M5-Stand:

| Plattform | Status | Release-Artefakt |
|---|---|---|
| Linux x86_64 | `supported` | Linux-CLI-Archiv (`.tar.gz`) plus OCI-Image |
| macOS arm64 | `validated_smoke` | kein offizielles Release-Artefakt |
| Windows x86_64 | `validated_smoke` | kein offizielles Release-Artefakt |

`validated_smoke` darf erst dokumentiert werden, wenn pro Plattform (a) der
zugehoerige Required Check in der Branch-Protection verankert ist und (b)
ein gruener CI-Lauf auf den jeweiligen Plattform-Runnern auditiert ist.
AP 1.7 hat die workflow-internen Voraussetzungen voll geliefert: `ctest`
fuehrt auf der Native-Matrix die Atomic-Replace-Tests (B) und die
CLI-Pflicht-Smokes inklusive `--output` (C.1) aus, plus die
Adapter-Tests fuer UNC/Extended-Length (C.2) und auf Windows zusaetzlich
`scripts/platform-smoke.ps1` (D.1). Beide externen Bedingungen sind seit
2026-04-30 erfuellt: Branch-Protection auf `main` verankert die fuenf
Required Checks (`Native (linux-x86_64)`, `Native (macos-arm64)`,
`Native (windows-x86_64)`, `Docker Runtime Build`, `docs/examples
Host-Portability`), und build.yml run 25153798452 (commit `7be4829`,
post-protection) bildet den auditierbaren gruenen Run-Audit. Das
Plattformstatus-Vokabular und die Required-Check-Namen sind in
[docs/planning/plan-M5-1-7.md](../planning/plan-M5-1-7.md) verbindlich festgelegt;
per-Adapter-Coverage und Atomic-Replace-Matrix in
[docs/user/quality.md](./quality.md) "Plattformstatus (AP M5-1.7)".

### Build-Matrix vs. Release-Matrix

- Die native CI-Build-Matrix in [`.github/workflows/build.yml`](../../.github/workflows/build.yml)
  laeuft auf Linux, macOS und Windows; alle drei Hosts bauen, fuehren
  `ctest` aus und pruefen Toolchain-Mindestversionen ueber
  [`cmake/ToolchainMinimums.cmake`](../../cmake/ToolchainMinimums.cmake)
  fail-fast.
- Die Release-Pipeline in [`.github/workflows/release.yml`](../../.github/workflows/release.yml)
  ist Linux-only: weder das Linux-CLI-Archiv noch das OCI-Image kennt eine
  macOS-/Windows-Variante; macOS- und Windows-Build-Steps sind in
  `release.yml` bewusst nicht vorhanden.
- [`scripts/release-allowlist.sh`](../../scripts/release-allowlist.sh) setzt
  die Linux-Sperre technisch durch: jeder Asset-Name ausserhalb der
  fixen Linux-Allowlist (`cmake-xray_<version>_linux_x86_64.tar.gz` plus
  `*.sha256`-Sidecar) bricht den finalen Publish-Job vor
  `release_published` ab. Der Guard ist aus AP M5-1.6 unveraendert
  uebernommen und in
  [docs/user/quality.md](./quality.md) "Release-Pipeline-Gates" verlinkt.

### Bekannte Einschraenkungen

- macOS-/Windows-Native-Jobs fahren `ctest` ueber alle Pflicht-Suites
  (`xray_tests` inklusive Atomic-Replace- und UNC/Extended-Length-
  Adaptertests aus den Tranchen B und C.2, sowie `e2e_binary_smoke`,
  `e2e_binary_artifacts` (mit den `--output`-Smokes aus C.1) und
  `e2e_binary_verbosity`). Windows fuegt `scripts/platform-smoke.ps1`
  (D.1) als konvertierungsfreien `native_powershell`-Pflichtmodus
  hinzu. Beide Plattformen sind seit 2026-04-30 als `validated_smoke`
  freigegeben; Branch-Protection auf `main` verankert die `Native
  (...)`-Required-Checks und build.yml run 25153798452 ist der
  auditierbare Post-Protection-Lauf. Eine spaetere Tranche kann
  zusaetzlich einen `Platform Smoke Report (...)`-Verifier-Pfad bauen,
  ohne den Release-Pfad selbst zu beruehren.
- Der Release-Asset-Allowlist-Guard hat keine macOS-/Windows-Whitelist;
  ein etwaiger Preview-Pfad mueesste eine eigene, separat versionierte
  Allowlist und Tag-Konvention mitbringen (heute nicht eingerichtet).
- Recovery-Pfade aus AP 1.6 (siehe Recovery-Runbook unten) gelten
  unveraendert nur fuer das Linux-CLI-Archiv plus OCI-Image; macOS-/
  Windows-Recovery existiert nicht, weil keine offiziellen
  macOS-/Windows-Artefakte erzeugt werden.

## Recovery-Runbook (AP M5-1.6 Tranche D.2)

Der automatisierte Release-Workflow folgt der Plan-Reihenfolge

```
draft_release_created -> oci_image_published -> release_published
```

und verlangt Idempotenz auf jeder Stufe (siehe `docs/planning/plan-M5-1-6.md` Sektionen "Veroeffentlichungskette" und "Recovery- und Rollback-Vertrag"). Wenn ein Run an einer Stufe abbricht, ist der Tag-Stand nicht halb-publik: einer der vier folgenden Faelle trifft zu. Jeder Fall hat einen festen Recovery-Pfad. Wenn ein erneuter Workflow-Run die Symptome nicht aufloesen kann (z. B. weil Plan-Praezedenz die Mutation verbietet), ist der manuelle Pfad der einzige zulaessige Fortgang.

Vor jedem Recovery-Schritt: aktuellen Zustand erfassen. Ohne `Ist-Stand` keine `Soll-Stand`-Korrektur.

```bash
TAG=v1.2.3
VERSION="${TAG#v}"
IMAGE_REPO=ghcr.io/<owner>/cmake-xray

# GitHub-Release-Zustand
gh release view "$TAG" --json tag,name,isDraft,assets

# OCI-Tag-Digests (Versions-Tag plus latest)
docker buildx imagetools inspect "${IMAGE_REPO}:${VERSION}" --format '{{.Manifest.Digest}}'
docker buildx imagetools inspect "${IMAGE_REPO}:latest"     --format '{{.Manifest.Digest}}'
```

### Fall 1: Draft existiert, OCI-Push fehlgeschlagen

Erkennen: `gh release view` zeigt einen Release fuer `$TAG` mit `isDraft=true`. `docker buildx imagetools inspect ${IMAGE_REPO}:${VERSION}` schlaegt mit `manifest unknown` fehl.

Recovery: Draft loeschen, OCI-Push erneut anstossen, dann den vollen Workflow neu auf den Tag laufen lassen.

```bash
gh release delete "$TAG" --yes
# Danach den Release-Workflow erneut starten oder lokal die Push-Sequenz nachholen:
docker buildx imagetools inspect "${IMAGE_REPO}:${VERSION}" >/dev/null 2>&1 \
    || bash scripts/oci-image-publish.sh "$IMAGE_REPO" "$VERSION" build \
    && bash scripts/oci-image-publish.sh "$IMAGE_REPO" "$VERSION" push
```

Begruendung: ohne OCI ist der Release nicht freigegeben; den Draft zu behalten waere irrefuehrend, weil ein nachfolgender Workflow-Run den Draft beim erneuten Asset-Vergleich akzeptieren und zum Public-Release befoerdern wuerde, ohne dass der OCI-Tag jemals existierte.

### Fall 2: Release publik, OCI-Push fehlgeschlagen

Erkennen: `gh release view` zeigt `isDraft=false`. OCI-Versions-Tag fehlt remote. Das ist die kritischste Variante, weil Endnutzer den Release sehen.

Recovery: Release sichtbar als fehlerhaft markieren, OCI-Push nachholen, Release-Notes nachfuehren. Der Release wird *nicht* zurueckgezogen; das verletzt das Publish-Versprechen.

```bash
gh release edit "$TAG" --notes-file - <<EOF
Release $TAG (RECOVERY IN PROGRESS)

Original-Release-Notes folgen unten. Der versionierte OCI-Tag
${IMAGE_REPO}:${VERSION} wurde nach dem GitHub-Release-Publish nicht
fertig gestellt; die OCI-Veroeffentlichung wird nachgereicht.
EOF
bash scripts/oci-image-publish.sh "$IMAGE_REPO" "$VERSION" build
bash scripts/oci-image-publish.sh "$IMAGE_REPO" "$VERSION" push
case "$VERSION" in *-*) ;; *) bash scripts/oci-image-publish.sh "$IMAGE_REPO" "$VERSION" latest ;; esac
gh release edit "$TAG" --notes-file release-assets/release-notes.md
```

Die "RECOVERY IN PROGRESS"-Notiz ist Pflicht: ohne sie sehen Endnutzer einen Release, der OCI-Pull-Befehle verspricht, die nicht funktionieren. Nach erfolgreichem Push und Original-Notes-Nachfuehrung ist der Recovery-Pfad abgeschlossen.

### Fall 3: OCI publik, GitHub-Release fehlt

Erkennen: `docker buildx imagetools inspect ${IMAGE_REPO}:${VERSION}` zeigt einen gueltigen Digest. `gh release view "$TAG"` schlaegt mit `release not found` fehl.

Recovery: Release lokal aus den validierten Artefakten neu erstellen.

```bash
mkdir -p release-assets
bash scripts/build-release-archive.sh "$VERSION" release-assets
gh release create "$TAG" \
    --verify-tag \
    --title "$TAG" \
    --notes-file release-assets/release-notes.md \
    "release-assets/cmake-xray_${VERSION}_linux_x86_64.tar.gz" \
    "release-assets/cmake-xray_${VERSION}_linux_x86_64.tar.gz.sha256"
```

Begruendung: die OCI-Veroeffentlichung ist bereits sichtbar; ein Public-Release fehlt nur. Die Asset-Liste muss byte-stabil zur OCI-Version passen — `scripts/build-release-archive.sh` mit dem gleichen Tag-Commit-Zeitstempel (`SOURCE_DATE_EPOCH=$(git log -1 --pretty=%ct $TAG)`) garantiert das.

### Fall 4: OCI publik, `latest` zeigt auf falschen Digest

Erkennen: `docker buildx imagetools inspect ${IMAGE_REPO}:${VERSION} --format '{{.Manifest.Digest}}'` und `... ${IMAGE_REPO}:latest --format '{{.Manifest.Digest}}'` liefern unterschiedliche Werte, obwohl `$VERSION` der zuletzt veroeffentlichte stable Release ist.

Recovery: `latest` manuell auf den korrekten Digest umsetzen. Plan-Vertrag verlangt, dass automatisierte Re-Runs einen `latest`-Digest-Mismatch hart abbrechen; der manuelle Pfad ist die einzige zulaessige Korrektur.

```bash
# Pull the versioned manifest into the local cache, retag and push as
# :latest. This produces a :latest entry whose manifest digest is
# byte-identical to the versioned tag and mirrors what release.yml's
# cmd_latest does in the normal flow. `docker buildx imagetools create`
# would also re-tag remotely without a rebuild, but it wraps a
# single-platform source in an OCI Image Index, which leaves :latest
# pointing at an *index* digest that differs from :${VERSION}'s
# manifest digest -- functionally correct (clients pulling :latest get
# the right image) but the digest verification below would fail.
docker pull "${IMAGE_REPO}:${VERSION}"
docker tag "${IMAGE_REPO}:${VERSION}" "${IMAGE_REPO}:latest"
docker push "${IMAGE_REPO}:latest"

# Verifikation: beide Tags muessen jetzt denselben Manifest-Digest
# tragen. buildx 0.30.x (Ubuntu 24.04 docker package, GHA ubuntu-latest
# runners) ignoriert `--format '{{.Manifest.Digest}}'` und liefert
# stattdessen die mehrzeilige Default-Ausgabe; das gleiche awk-Filter
# wie in scripts/oci-image-publish.sh `read_remote_digest` macht den
# Vergleich version-unabhaengig robust.
inspect_digest() {
    docker buildx imagetools inspect "$1" --format '{{.Manifest.Digest}}' 2>/dev/null \
        | awk '
            /^sha256:/ { print; exit }
            /^Digest:[[:space:]]+sha256:/ { print $2; exit }
        '
}
diff <(inspect_digest "${IMAGE_REPO}:${VERSION}") \
     <(inspect_digest "${IMAGE_REPO}:latest")
```

Begruendung: `latest`-Mismatch ist der einzige Fall, in dem Plan-Vertrag explizit "manuelle Recovery-Sequenz" verlangt. Der `docker pull/tag/push`-Pfad re-tagged ohne lokalen Rebuild und ohne Index-Wrapper, sodass `:latest` und `:${VERSION}` byte-stabil denselben Manifest-Digest tragen. Die `diff`-Verifikation am Schluss schliesst die Recovery ab.

### Audit-Trail

Jeder Recovery-Schritt ist im finalen Release-Commit oder einem Folge-Commit zu dokumentieren. Mindestbestandteile:

- Auslesen-Befehle aus dem Vor-Recovery-Zustand (mit Output).
- Die ausgefuehrten Recovery-Befehle (mit Output).
- Verifikations-Befehle nach dem Recovery (Tag, Digest, Asset-Liste).
- Die betroffenen Release-, Asset- und Digest-IDs, sodass spaetere Re-Runs den Zustand auditieren koennen.

## Hinweise

- Tags sollen auf den Commit zeigen, der die finalen Release-Artefakte enthaelt.
- Unverwandte lokale Aenderungen bleiben ausserhalb des Release-Commits.
- Falls ein Tag bereits vor dem finalen Commit erzeugt wurde, muss es auf den finalen Release-Commit umgesetzt werden.
- Der `coverage`-Stage dient dem Report; das eigentliche Build-Gate fuer Mindestabdeckung liegt in `coverage-check`.
- Der `quality`-Stage dient dem Report; das eigentliche Build-Gate fuer statische Analyse und Metriken liegt in `quality-check`.
- Fuer `v1.0.0` gehoeren Markdown-Smoke-Tests, Golden-Output-Konsistenz, `docs/examples/` und `docs/user/performance.md` zum Release-Check.
- Ab `v1.1.0` gehoeren zusaetzlich die File-API-Smoke-Tests, die M4-Golden-Output-Konsistenz unter `tests/e2e/testdata/m4/` und die Target-Beispiele unter `docs/examples/` zum Release-Check.
- Fuer regulare Releases soll das Container-Image zusaetzlich als `latest` veroeffentlicht werden; Vorabversionen mit Suffix wie `-rc1` bleiben auf ihren Versions-Tag beschraenkt.
