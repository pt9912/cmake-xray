# Releasing

## Zweck

Dieses Dokument beschreibt den minimalen Release-Ablauf fuer `cmake-xray`.
Fuer `v0.1.0` reicht ein dokumentierter, reproduzierbarer Ablauf fuer Build, Test, Changelog und Tagging.

## Checkliste

Vor einem Release:

- `CHANGELOG.md` auf den Zielstand bringen
- betroffene Plan- und Statusdokumente aktualisieren
- sicherstellen, dass nur beabsichtigte Dateien fuer den Release-Commit vorgemerkt sind

Verifikation:

```bash
docker build --target test -t cmake-xray:test .
docker build --target runtime -t cmake-xray .
docker run --rm cmake-xray
```

Abschluss:

```bash
git add CHANGELOG.md README.md docs/plan-M0.md docs/releasing.md
git commit -m "docs: finalize release v0.1.0"
git tag -a v0.1.0 -m "Release v0.1.0"
```

## Hinweise

- Tags sollen auf den Commit zeigen, der die finalen Release-Artefakte enthaelt.
- Unverwandte lokale Aenderungen bleiben ausserhalb des Release-Commits.
- Falls ein Tag bereits vor dem finalen Commit erzeugt wurde, muss es auf den finalen Release-Commit umgesetzt werden.
