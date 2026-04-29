# Plattform-Smoke-Checkliste (AP M5-1.7 Tranche D.4)

Diese Checkliste begleitet manuelle Plattformnachweise. Sie spiegelt die
verbindlichen Required-Check-Schritte aus
[`.github/workflows/build.yml`](../.github/workflows/build.yml) wider, sodass
Reviewer und Releaser die Plattformsicht ohne CI lokal reproduzieren koennen.

Der Plattformstatus-Vertrag aus [plan-M5-1-7.md](./plan-M5-1-7.md) bleibt
unveraendert: Linux ist `supported`, macOS und Windows sind `known_limited`,
solange Atomic-Replace- und CLI-Pflicht-Smokes nicht als Required Checks im
nativen Job liegen. Eine erfolgreiche manuelle Checkliste begruendet keine
Statushaerung — `validated_smoke` braucht den automatisierten, in
Branch-Protection verankerten Nachweis aus
[`docs/quality.md`](./quality.md) "Plattformstatus (AP M5-1.7)".

## Voraussetzungen

| Werkzeug | Mindestversion | Quelle |
|---|---|---|
| CMake | 3.20 (CI pin: 3.30.5) | [`tests/platform/toolchain-minimums.json`](../tests/platform/toolchain-minimums.json) |
| GCC | 10 (Linux) | toolchain-minimums.json |
| Clang | 12 (Linux/macOS/Windows ClangCL) | toolchain-minimums.json |
| AppleClang | 13 (macOS) | toolchain-minimums.json |
| MSVC | 19.29 (Windows) | toolchain-minimums.json |
| Python | 3.x mit `jsonschema` (`tests/requirements-json-schema.txt`) | tests/validate_json_schema.py |

## Linux x86_64 (`supported`)

```bash
# 1. Toolchain-Versionen sichtbar machen.
cmake --version
python3 --version

# 2. Configure plus Toolchain-Minimums-Gate (FATAL_ERROR bei Drift).
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 3. Build und ctest.
cmake --build build --parallel
ctest --test-dir build --output-on-failure --parallel
```

Linux ist die Referenzplattform; zusaetzlich gelten die Docker-Gates aus
[`docs/quality.md`](./quality.md) ("Statische Analyse", "Coverage",
"Release-Pipeline-Gates"). Ein roter Docker-Gate-Pfad bricht den Linux-
`supported`-Status.

## macOS (`known_limited`)

```bash
cmake --version
python3 --version
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure --parallel
```

Hosts:
- `macos-latest` (arm64, AppleClang) ist der primaere CI-Smoke. Die
  optionale Tranche-D.3-Intel-Variante wurde plan-konform ausgelassen,
  weil arm64 als Repraesentativplattform reicht; ein lokaler Intel-Lauf
  ist freiwillig moeglich.

`known_limited` bleibt bestehen, bis Atomic-Replace- und CLI-Pflicht-Smokes
in den jeweiligen Native-Job eingehaengt sind. Die Adapter-Tests (DOT, HTML,
JSON, atomic_report_writer) sind bereits Bestandteil von `xray_tests` und
laufen in `ctest`.

## Windows x86_64 (`known_limited`)

### MSYS-Bash-Pfad (Visual-Studio-Generator, Default-MSVC)

```bash
cmake --version
python --version
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build --output-on-failure -C Release --parallel
```

Pfad-Konvertierung ist im Test-Treiber via
[`tests/e2e/run_e2e_lib.sh`](../tests/e2e/run_e2e_lib.sh) `MSYS_NO_PATHCONV=1`
und `MSYS2_ARG_CONV_EXCL=*` ausgeschaltet (`msys_path_mode=disabled`).

### PowerShell-Pfad (Tranche D.1)

```powershell
pwsh -File scripts/platform-smoke.ps1 -Binary build/Release/cmake-xray.exe
```

Der Smoke faehrt alle Pflichtkommandos aus
[plan-M5-1-7.md](./plan-M5-1-7.md) "Normative CLI-Smokes" inklusive der
`--output`-Smokes aus Tranche C.1 mit nativen Windows-Pfaden ohne
MSYS-Magie ab (`msys_path_mode=native_powershell`).

### Optionaler lokaler Ninja-/clang-cl-Smoke

Ein zweiter Generator-Pfad neben dem Visual-Studio-Default war als
Tranche D.2 vorgesehen, wurde aber zurueckgezogen, weil GHA-windows-
latest ohne vcvars `mt.exe` nicht findet (`CMAKE_MT-NOTFOUND`) und der
Workaround eine Drittanbieter-Action oder brittle vcvars-Pfade
erfordert. Lokal mit Visual-Studio-Developer-Prompt funktioniert er
trotzdem:

```bash
cmake -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
cmake --build build-ninja --parallel
./build-ninja/cmake-xray.exe --help > /dev/null
```

## docs/examples Host-Portabilitaet (vor "lokal gruen")

`docs/examples/` enthaelt DOT-Beispiele, die absolute Source-/Build-
Pfade in `unique_key`-Attributen einbetten. Lokale Test-Laeufe via
`docker build --target test` arbeiten unter `/workspace/...` und
maskieren damit Host-Pfad-Drift, die auf einem CI-Runner mit
`/home/runner/...` (oder einem anderen Host-Layout) auftaucht.

Vor jeder "lokal gruen"-Aussage zur `docs/examples`-Drift einmal
explizit unter einem alternativen Mount-Pfad pruefen:

```bash
bash scripts/verify-doc-examples-portability.sh
```

Das Skript baut `cmake-xray:test`, mountet das Repo unter
`/alt-mount` (statt `/workspace`) und faehrt
`tests/validate_doc_examples.py --binary ...` mit dieser Pfad-
Geometrie. Wenn die DOT-`unique_key`-Normalisierung im Validator
korrekt ist, geht der Lauf gruen trotz divergentem Praefix; wenn
nicht, bricht er mit derselben Fehlermeldung ab, die ein nativer
GHA-Linux-Lauf produzieren wuerde. Der CTest-Eintrag
`doc_examples_host_portability` ruft dasselbe Skript auf und
skippt sauber, wenn `ctest` ohne Host-Docker laeuft.

## Manueller Smoke-Report (optional)

Ein automatisierter Smoke-Report-Verifier ist bewusst nicht Teil von AP 1.7
(plan-M5-1-7.md "Smoke-Report-Vertrag" beschreibt das Schema fuer eine
spaetere Tranche). Wer den Inhalt heute manuell festhalten moechte:

- Host und Architektur (`uname -a`, PowerShell `[System.Environment]::OSVersion`)
- CMake-Version (`cmake --version`)
- Compiler-ID und -Version (Configure-Output enthaelt
  `AP M5-1.7 toolchain check: ...`)
- Pflichtkommando-Liste mit Exit-Codes
- bei `--output`-Smokes: SHA-256 der erzeugten Reports

Diese Notizen reichen *nicht* fuer `validated_smoke`; der Statusvertrag
verlangt automatisiertes, in Branch-Protection verankertes Verfahren.

## Querverweise

- [docs/quality.md "Plattformstatus (AP M5-1.7)"](./quality.md)
- [docs/releasing.md "Plattformartefakte macOS und Windows"](./releasing.md)
- [docs/plan-M5-1-7.md "Plattformstatus-Vertrag"](./plan-M5-1-7.md)
- [tests/platform/toolchain-minimums.json](../tests/platform/toolchain-minimums.json)
