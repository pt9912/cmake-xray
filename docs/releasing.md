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
- kuratierte Beispielausgaben unter `docs/examples/` aus den kanonischen M3-Fixtures regenerieren, falls sich Report-Inhalt oder -Layout geaendert hat
- bei Aenderungen am Analyse- oder Report-Pfad die Performance-Artefakte unter `build/reports/performance/` und `docs/performance.md` aktualisieren
- sicherstellen, dass nur beabsichtigte Dateien fuer den Release-Commit vorgemerkt sind
- falls Release-Artefakte mit veroefentlicht werden sollen, `./release-assets/` vorbereiten
- fuer tag-basierte Veroeffentlichung sicherstellen, dass `.github/workflows/release.yml` zum geplanten Artefaktumfang passt

## Verifikation

Vor Tagging und Release-Erstellung muessen die Docker-Pfade erfolgreich sein:

```bash
docker build --target test -t cmake-xray:test .
docker build --target coverage-check --build-arg XRAY_COVERAGE_THRESHOLD=100 -t cmake-xray:coverage-check .
docker build --target quality-check -t cmake-xray:quality-check .
docker build --target runtime -t cmake-xray .
docker run --rm cmake-xray --help
docker run --rm \
  -v "$PWD/tests/e2e/testdata/m3:/data:ro" \
  cmake-xray analyze --compile-commands /data/report_project/compile_commands.json --format markdown
docker run --rm \
  -v "$PWD/tests/e2e/testdata/m3:/data:ro" \
  cmake-xray impact --compile-commands /data/report_impact_header/compile_commands.json --changed-file include/common/config.h --format markdown
```

Optional koennen die Reports separat ausgegeben werden:

```bash
docker build --target coverage -t cmake-xray:coverage .
docker run --rm cmake-xray:coverage
docker build --target quality -t cmake-xray:quality .
docker run --rm cmake-xray:quality
```

Vor einem MVP-Release zusaetzlich pruefen:

- `docs/performance.md` verweist auf aktuelle Messwerte und die Artefakte unter `build/reports/performance/`
- `docs/examples/` und die M3-Golden-Files unter `tests/e2e/testdata/m3/` stammen noch aus denselben kanonischen Fixtures

## Release-Commit und Tag

```bash
git add CHANGELOG.md README.md docs/examples docs/performance.md docs/quality.md docs/releasing.md docs/plan-M3.md
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

Der Standardpfad ist der automatisierte Release-Workflow beim Tag-Push. Die folgenden Befehle sind als manueller Fallback gedacht, falls die Workflow-Ausfuehrung absichtlich ueberschrieben oder nach einer fehlgeschlagenen Veroeffentlichung gezielt repariert werden soll.

```bash
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

## Hinweise

- Tags sollen auf den Commit zeigen, der die finalen Release-Artefakte enthaelt.
- Unverwandte lokale Aenderungen bleiben ausserhalb des Release-Commits.
- Falls ein Tag bereits vor dem finalen Commit erzeugt wurde, muss es auf den finalen Release-Commit umgesetzt werden.
- Der `coverage`-Stage dient dem Report; das eigentliche Build-Gate fuer Mindestabdeckung liegt in `coverage-check`.
- Der `quality`-Stage dient dem Report; das eigentliche Build-Gate fuer statische Analyse und Metriken liegt in `quality-check`.
- Fuer `v1.0.0` gehoeren Markdown-Smoke-Tests, Golden-Output-Konsistenz, `docs/examples/` und `docs/performance.md` zum Release-Check.
- Fuer regulare Releases soll das Container-Image zusaetzlich als `latest` veroeffentlicht werden; Vorabversionen mit Suffix wie `-rc1` bleiben auf ihren Versions-Tag beschraenkt.
