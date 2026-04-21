# Plan M0 - Fundament (`v0.1.0`)

## 0. Dokumenteninformationen

| Feld | Wert |
|---|---|
| Dokument | Plan M0 `cmake-xray` |
| Version | `0.1` |
| Stand | `2026-04-21` |
| Status | Entwurf |
| Referenzen | [Lastenheft](./lastenheft.md), [Architektur](./architecture.md), [Phasenplan](./roadmap.md) |

### 0.1 Zweck
Dieser Plan beschreibt die konkreten Schritte fuer Milestone M0 (`v0.1.0`). Ziel ist ein baubares, testbares Projekt mit hexagonaler Grundstruktur und den noetigsten Dokumentationsartefakten.

### 0.2 Abschlusskriterium
M0 gilt als erreicht, wenn:

- das Projekt auf Linux mit CMake und einem C++20-faehigen Compiler baut
- mindestens ein Platzhalter-Test durchlaeuft
- ein minimaler Programmaufruf von `cmake-xray` ohne Fehler beendet wird
- die Verzeichnisstruktur der Architektur entspricht
- eine README mit Installations- und Nutzungsbeispielen fuer den M0-Stand vorhanden ist

Relevante Kennungen: `RB-01`, `RB-02`, `RB-03`, `NF-07`, `NF-16`, `NF-17`, `RB-10`, `AK-07`, `AK-08`

## 1. Arbeitspakete

### 1.1 Verzeichnisstruktur anlegen

Die Struktur folgt der Architektur (architecture.md, Kap. 4):

```
src/
├── CMakeLists.txt
├── main.cpp
├── hexagon/
│   ├── CMakeLists.txt
│   ├── model/
│   ├── ports/
│   │   ├── driving/
│   │   └── driven/
│   └── services/
└── adapters/
    ├── CMakeLists.txt
    ├── cli/
    ├── input/
    └── output/

tests/
├── CMakeLists.txt
├── hexagon/
├── adapters/
└── e2e/
    └── testdata/
```

In M0 bleiben die meisten Verzeichnisse leer oder enthalten nur Platzhalter. Ziel ist die Struktur, nicht der Inhalt.

**Ergebnis**: Verzeichnisbaum existiert und ist im Repository eingecheckt.

### 1.2 CMake-Grundgeruest

| Datei | Verantwortung |
|---|---|
| `CMakeLists.txt` (Root) | Projekt-Name, C++20-Standard, Unterverzeichnisse einbinden |
| `src/CMakeLists.txt` | `xray_hexagon` und `xray_adapters` Libraries, `cmake-xray` Executable |
| `src/hexagon/CMakeLists.txt` | `xray_hexagon` Library (keine externen Abhaengigkeiten) |
| `src/adapters/CMakeLists.txt` | `xray_adapters` Library (abhaengig von `xray_hexagon`) |
| `tests/CMakeLists.txt` | Test-Executable, abhaengig von Test-Framework und beiden Libraries |

Anforderungen:

- C++20 als Mindeststandard (`RB-03`)
- `cmake_minimum_required` auf eine aktuelle, stabile CMake-Version (mindestens 3.20)
- `xray_hexagon` darf keine externen Abhaengigkeiten haben
- externe Bibliotheken duerfen nur in `xray_adapters`, `xray_tests` oder in der Composition Root verwendet werden, nicht in `src/hexagon/`
- Ports und Modelle des Hexagons duerfen keine Typen externer Bibliotheken in ihren oeffentlichen Schnittstellen fuehren
- Build muss auf Linux mit GCC und Clang funktionieren (`NF-07`)

**Ergebnis**: `cmake -B build && cmake --build build` laeuft auf Linux ohne Fehler durch.

### 1.3 Minimaler main.cpp

`src/main.cpp` soll als Composition Root vorbereitet sein, aber in M0 nur ein minimales Platzhalter-Programm liefern:

- gibt bei Aufruf ohne Argumente eine kurze Platzhalter-Ausgabe aus
- gibt Exit-Code 0 zurueck
- enthaelt noch keinen CLI-Parser und keine Unterkommandos
- keine Analyse-Funktionalitaet

**Ergebnis**: `./build/cmake-xray` beendet mit Code 0 und gibt Text aus.

### 1.4 Externe Abhaengigkeiten entscheiden und einbinden

Fuer drei Bereiche muss eine Bibliothek gewaehlt und eingebunden werden (`RB-10`):

| Bereich | Kandidaten | Entscheidungskriterien |
|---|---|---|
| JSON-Parsing | nlohmann/json, simdjson, rapidjson | Header-only bevorzugt, breite Verbreitung, CMake-Integration |
| CLI-Argument-Parsing | CLI11, cxxopts, argparse | Header-only bevorzugt, Unterkommandos moeglich |
| Test-Framework | Catch2, GoogleTest, doctest | CMake-Integration, geringe Build-Zeit |

Einbindung ueber `FetchContent` oder als Git-Submodul. Die Entscheidungen sollen in einer kurzen Notiz in der README oder in einem separaten Abschnitt dokumentiert werden.
Die Bibliotheken muessen in M0 noch nicht fachlich genutzt werden; entscheidend ist, dass Auswahl, Einbindung und Build-Reproduzierbarkeit vorbereitet und dokumentiert sind.

Abhaengigkeitsgrenzen fuer M0:

| Bibliothekstyp | Zulaessige Verwendung in M0 |
|---|---|
| JSON-Parsing | nur in Adaptern, insbesondere fuer spaetere Eingabeadapter unter `src/adapters/input/` |
| CLI-Argument-Parsing | nur im CLI-Adapter unter `src/adapters/cli/`; `src/main.cpp` bleibt in M0 noch ohne eigentlichen CLI-Parser |
| Test-Framework | nur in `tests/` und im Target `xray_tests` |

Fuer M0 reicht es aus, die spaetere Nutzung ueber CMake-Targets und Verzeichnisgrenzen sauber vorzubereiten. Das Hexagon bleibt auch dann frei von externen Bibliotheken, wenn JSON- oder CLI-Bibliotheken bereits eingebunden, aber fachlich erst ab M1 genutzt werden.

**Ergebnis**: Bibliotheken sind eingebunden, der Build laeuft mit ihnen durch, die Wahl ist dokumentiert.

### 1.5 Test-Grundgeruest

- mindestens ein Platzhalter-Test in `tests/` der durchlaeuft
- `ctest` oder das Test-Executable laeuft und meldet Erfolg
- die Teststruktur spiegelt `tests/hexagon/`, `tests/adapters/`, `tests/e2e/`

**Ergebnis**: `cmake --build build --target xray_tests && cd build && ctest` meldet 0 Fehler.

### 1.6 Dockerfile

Ein `Dockerfile` definiert die Referenz-Build-Umgebung und stellt sicher, dass Build und Tests unabhaengig vom Host-System reproduzierbar sind (`NF-07`, `RB-04`).

Das Dockerfile soll:

- als Multi-Stage-Build aufgebaut sein
- auf einem etablierten Base-Image aufbauen (z.B. `ubuntu:24.04` oder `debian:bookworm`)
- einen C++20-faehigen Compiler installieren (GCC >= 12 oder Clang >= 15)
- CMake >= 3.20 bereitstellen
- mindestens die Stages `build`, `test` und `runtime` enthalten

Empfohlene Stage-Aufteilung:

| Stage | Zweck |
|---|---|
| `build` | konfiguriert das Projekt und baut `cmake-xray` sowie die Tests |
| `test` | fuehrt die in der `build`-Stage erzeugten Artefakte mit `ctest` oder einem gleichwertigen Aufruf aus und bricht bei Fehlern ab |
| `runtime` | enthaelt nur die fuer M0 noetigen Laufzeitartefakte, insbesondere das Platzhalter-Binary und ggf. eine minimale Startanweisung |

Optional kann zusaetzlich eine gemeinsame Basis-Stage wie `base` oder `toolchain` verwendet werden, um Paketinstallation und Build-Werkzeuge fuer `build` und `test` wiederzuverwenden.

Angestrebte Nutzung:

```
docker build --target test -t cmake-xray:test .
docker build --target runtime -t cmake-xray .
docker run --rm cmake-xray
```

Der Test-Build muss fehlschlagen, wenn Konfiguration, Build oder Testlauf fehlschlagen. Das Runtime-Image muss erfolgreich gebaut werden und `docker run --rm cmake-xray` soll das Platzhalter-Binary fuer M0 mit Exit-Code 0 starten.

**Ergebnis**: `docker build --target test`, `docker build --target runtime` und `docker run --rm cmake-xray` laufen fehlerfrei durch.

### 1.7 README

Die README soll fuer M0 mindestens enthalten:

- Projektbeschreibung (1-2 Saetze)
- Voraussetzungen mit Verweis auf das Dockerfile als Referenzumgebung
- Installations- bzw. Inbetriebnahmebeispiel fuer den Quellbuild
- Bauanleitung (cmake + build sowie Docker-Variante)
- Testanleitung (ctest sowie Docker-Multi-Stage-Test-Target)
- mindestens ein Nutzungsbeispiel fuer den M0-Stand, z.B. der Aufruf des Platzhalter-Binaries oder des Docker-Containers
- Hinweis auf gewaehlte externe Abhaengigkeiten und deren Einbindung
- kurze Erklaerung der Docker-Stages und ihres Zwecks
- Verweis auf `docs/` fuer weitere Dokumentation

Relevante Kennungen: `NF-16`, `NF-17`, `AK-08`

**Ergebnis**: README.md existiert mit den genannten Inhalten.

## 2. Reihenfolge

| Schritt | Arbeitspaket | Abhaengig von |
|---|---|---|
| 1 | 1.1 Verzeichnisstruktur anlegen | - |
| 2 | 1.2 CMake-Grundgeruest | 1.1 |
| 3 | 1.4 Externe Abhaengigkeiten | 1.2 |
| 4 | 1.3 Minimaler main.cpp | 1.2 |
| 5 | 1.5 Test-Grundgeruest | 1.2, 1.4 |
| 6 | 1.6 Dockerfile | 1.2, 1.4, 1.5 |
| 7 | 1.7 README | 1.4, 1.6 |

Schritte 3 und 4 koennen parallel bearbeitet werden.

## 3. Pruefung

M0 ist abgeschlossen, wenn beide Pruefwege erfolgreich durchlaufen:

**Lokal** (Linux-System mit C++20-Compiler und CMake >= 3.20):

``` 
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/cmake-xray
cd build && ctest --output-on-failure
```

**Docker** (reproduzierbare Referenzumgebung):

``` 
docker build --target test -t cmake-xray:test .
docker build --target runtime -t cmake-xray .
docker run --rm cmake-xray
```

Danach wird `v0.1.0` getaggt.
