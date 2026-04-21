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
- ein leerer Testlauf durchlaeuft
- `cmake-xray --help` oder ein gleichwertiger Aufruf ohne Fehler beendet wird
- die Verzeichnisstruktur der Architektur entspricht
- eine README mit Bauanleitung vorhanden ist

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
- Build muss auf Linux mit GCC und Clang funktionieren (`NF-07`)

**Ergebnis**: `cmake -B build && cmake --build build` laeuft auf Linux ohne Fehler durch.

### 1.3 Minimaler main.cpp

`src/main.cpp` soll als Composition Root vorbereitet sein, aber in M0 nur eine minimale Ausgabe liefern:

- gibt bei Aufruf ohne Argumente oder mit `--help` einen Platzhalter-Hilfetext aus
- gibt Exit-Code 0 zurueck
- keine Analyse-Funktionalitaet

**Ergebnis**: `./build/cmake-xray --help` beendet mit Code 0 und gibt Text aus.

### 1.4 Externe Abhaengigkeiten entscheiden und einbinden

Fuer drei Bereiche muss eine Bibliothek gewaehlt und eingebunden werden (`RB-10`):

| Bereich | Kandidaten | Entscheidungskriterien |
|---|---|---|
| JSON-Parsing | nlohmann/json, simdjson, rapidjson | Header-only bevorzugt, breite Verbreitung, CMake-Integration |
| CLI-Argument-Parsing | CLI11, cxxopts, argparse | Header-only bevorzugt, Unterkommandos moeglich |
| Test-Framework | Catch2, GoogleTest, doctest | CMake-Integration, geringe Build-Zeit |

Einbindung ueber `FetchContent` oder als Git-Submodul. Die Entscheidungen sollen in einer kurzen Notiz in der README oder in einem separaten Abschnitt dokumentiert werden.

**Ergebnis**: Bibliotheken sind eingebunden, der Build laeuft mit ihnen durch, die Wahl ist dokumentiert.

### 1.5 Test-Grundgeruest

- mindestens ein Platzhalter-Test in `tests/` der durchlaeuft
- `ctest` oder das Test-Executable laeuft und meldet Erfolg
- die Teststruktur spiegelt `tests/hexagon/`, `tests/adapters/`, `tests/e2e/`

**Ergebnis**: `cmake --build build --target xray_tests && cd build && ctest` meldet 0 Fehler.

### 1.6 README

Die README soll fuer M0 mindestens enthalten:

- Projektbeschreibung (1-2 Saetze)
- Voraussetzungen (Compiler, CMake-Version)
- Bauanleitung (cmake + build)
- Testanleitung (ctest)
- Hinweis auf gewaehlte externe Abhaengigkeiten und deren Einbindung
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
| 6 | 1.6 README | 1.4 |

Schritte 3 und 4 koennen parallel bearbeitet werden.

## 3. Pruefung

M0 ist abgeschlossen, wenn alle folgenden Befehle auf einem Linux-System mit C++20-Compiler und CMake >= 3.20 erfolgreich durchlaufen:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/cmake-xray --help
cd build && ctest --output-on-failure
```

Danach wird `v0.1.0` getaggt.
