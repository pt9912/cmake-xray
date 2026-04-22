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

- `CHANGELOG.md` auf den Zielstand bringen
- betroffene Plan-, Status- und Nutzungsdokumente aktualisieren
- sicherstellen, dass nur beabsichtigte Dateien fuer den Release-Commit vorgemerkt sind
- falls Release-Artefakte mit veroefentlicht werden sollen, `./release-assets/` vorbereiten

## Verifikation

Vor Tagging und Release-Erstellung muessen die Docker-Pfade erfolgreich sein:

```bash
docker build --target test -t cmake-xray:test .
docker build --target coverage-check -t cmake-xray:coverage-check .
docker build --target runtime -t cmake-xray .
docker run --rm cmake-xray --help
```

Optional kann der Coverage-Report separat ausgegeben werden:

```bash
docker build --target coverage -t cmake-xray:coverage .
docker run --rm cmake-xray:coverage
```

## Release-Commit und Tag

```bash
git add CHANGELOG.md README.md docs/quality.md docs/releasing.md
git commit -m "docs: finalize release $TAG"
git tag -a "$TAG" -m "Release $TAG"
```

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

Wichtig: Zuerst pruefen, ob der Release bereits existiert. Der `release-homebrew.yml`-Workflow kann den GitHub-Release bereits beim Tag-Push anlegen. In diesem Fall darf nicht erneut `gh release create` aufgerufen werden.

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
