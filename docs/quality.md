# Qualität

## Zweck

Dieses Dokument beschreibt die verbindlichen Qualitaets- und Metrikpfade fuer `cmake-xray`.
Es dokumentiert keine punktuelle Review-Historie und keine einmaligen KI-Befunde, sondern die aktuell gueltigen, reproduzierbaren Docker-Pruefwege.

## Statische Analyse und Metriken

Die statische Analyse und die Metriken werden Docker-basiert fuer den Produktcode unter `src/` erhoben:

- `clang-tidy` prueft regelbasierte statische Analysebefunde
- `lizard` prueft Komplexitaet, Funktionslaenge und Parameteranzahl
- Report und Gate sind bewusst getrennt

### Report-Pfad

```bash
docker build --target quality -t cmake-xray:quality .
docker run --rm cmake-xray:quality
```

Der `quality`-Stage erzeugt und zeigt diese Artefakte:

- `/workspace/build-quality/quality/summary.txt`
- `/workspace/build-quality/quality/clang-tidy.txt`
- `/workspace/build-quality/quality/lizard.txt`
- `/workspace/build-quality/quality/lizard-warnings.txt`

### Gate-Pfad

```bash
docker build --target quality-check -t cmake-xray:quality-check .
```

Optional lassen sich die Grenzwerte per Build-Argument anpassen:

```bash
docker build --target quality-check \
  --build-arg XRAY_CLANG_TIDY_MAX_FINDINGS=0 \
  --build-arg XRAY_LIZARD_MAX_CCN=10 \
  --build-arg XRAY_LIZARD_MAX_LENGTH=50 \
  --build-arg XRAY_LIZARD_MAX_PARAMETERS=5 \
  -t cmake-xray:quality-check .
```

Aktuelle Standardgrenzen:

- `clang-tidy`: maximal `0` Findings
- `lizard` CCN: maximal `10`
- `lizard` Funktionslaenge: maximal `50`
- `lizard` Parameteranzahl: maximal `5`

Konfigurierbare Build-Argumente:

- `XRAY_CLANG_TIDY_MAX_FINDINGS`
- `XRAY_LIZARD_MAX_CCN`
- `XRAY_LIZARD_MAX_LENGTH`
- `XRAY_LIZARD_MAX_PARAMETERS`

## Testabdeckung

Die Coverage wird ebenfalls Docker-basiert fuer `src/` ermittelt:

- `ctest` fuehrt die Tests aus
- `gcovr` erzeugt den Coverage-Report
- die Instrumentierung wird ueber `XRAY_ENABLE_COVERAGE=ON` aktiviert
- das Gate bezieht sich auf Line-Coverage unter `src/`

### Report-Pfad

```bash
docker build --target coverage -t cmake-xray:coverage .
docker run --rm cmake-xray:coverage
```

Der `coverage`-Stage erzeugt und zeigt diese Artefakte:

- `/workspace/build-coverage/coverage/summary.txt`
- `/workspace/build-coverage/coverage/coverage.txt`

### Gate-Pfad

```bash
docker build --target coverage-check \
  --build-arg XRAY_COVERAGE_THRESHOLD=100 \
  -t cmake-xray:coverage-check .
```

Der Standard fuer den aktuellen M3-/MVP-Stand ist:

- minimale Line-Coverage unter `src/`: `100%`

Das Gate ist per `XRAY_COVERAGE_THRESHOLD` konfigurierbar.

## Laufzeit-Image

Das Runtime-Image ist fuer den aktuellen Stand gehaertet:

- das Binary laeuft nicht als `root`
- das Runtime-Stage legt einen dedizierten Systembenutzer `cmake-xray` an
- das Binary wird diesem Benutzer gehoerig nach `/app/cmake-xray` kopiert

Der Runtime-Smoke-Test fuer Release- und Abnahmepfade ist:

```bash
docker build --target runtime -t cmake-xray .
docker run --rm cmake-xray --help
```

## Zusammenhang mit Releasing

Fuer M3 und spaetere Releases sind mindestens diese Docker-Pfade massgeblich:

- `docker build --target test -t cmake-xray:test .`
- `docker build --target coverage-check --build-arg XRAY_COVERAGE_THRESHOLD=100 -t cmake-xray:coverage-check .`
- `docker build --target quality-check -t cmake-xray:quality-check .`
- `docker build --target runtime -t cmake-xray .`
- `docker run --rm cmake-xray --help`

Die Release-Checkliste selbst steht in [docs/releasing.md](./releasing.md).

## Hinweise

- Dieses Dokument soll den reproduzierbaren Ist-Stand beschreiben. Veraltete Review-Notizen, Placeholder-Befunde und einmalige Zwischenstaende gehoeren nicht hier hinein.
- Fuer aktuelle Zahlen und Details sind die Artefakte aus `quality` und `coverage` die primaere Quelle.
