# Qualität


## Testabdeckung

Die Testabdeckung wird im Projekt Docker-basiert ermittelt. Dabei gilt:

- `ctest` führt die Tests aus, berechnet aber die Abdeckung nicht selbst.
- Die eigentliche Coverage-Auswertung übernimmt `gcovr`.
- Die Instrumentierung wird über den CMake-Schalter `XRAY_ENABLE_COVERAGE=ON` aktiviert.
- Die Auswertung ist auf den Projektcode unter `src/` gefiltert.

### Docker-Workflow

```bash
docker build --target coverage -t cmake-xray:coverage .
docker run --rm cmake-xray:coverage
```

Der `coverage`-Stage baut einen instrumentierten Build, führt `ctest` aus und gibt per Entrypoint beide Report-Dateien aus:

- `/workspace/build-coverage/coverage/summary.txt`
- `/workspace/build-coverage/coverage/coverage.txt`

Für das eigentliche Build-Gate gibt es zusätzlich einen separaten Stage:

```bash
docker build --target coverage-check -t cmake-xray:coverage-check .
docker build --target coverage-check \
  --build-arg XRAY_COVERAGE_THRESHOLD=96 \
  -t cmake-xray:coverage-check .
```

Die Trennung ist bewusst:

- `coverage` erzeugt den Report und bleibt damit für Auswertung und Diagnose nutzbar.
- `coverage-check` zieht den Schwellwert bereits während `docker build` und lässt den Build bei Unterschreitung scheitern.

### Aktueller Stand

| Metrik                 |        Wert |
| ---------------------- | ----------: |
| Line Coverage (`src/`) |       `95%` |
| Ausgeführte Zeilen     | `173 / 182` |

Größte erkennbare Lücke im aktuellen Report:

- `src/adapters/cli/cli_adapter.cpp`: `85%`

Einordnung:

- Für den aktuellen M1-Umfang ist `95%` solide.
- Die Restlücken liegen vor allem in CLI-Fehler- und Fallbackpfaden, nicht im JSON-Adapter oder in den Kernservices.
- Die Zahl ist als Line-Coverage zu verstehen; Branch-Coverage wird derzeit nicht separat ausgewiesen.

## KI Prompt

Analysiere den Code hinsichtlich seiner Qualität.

Bitte bewerte die folgenden Metriken auf einer Skala von 1-10 und begründe die Bewertung:
- Lesbarkeit & Namensgebung: (Sind Variablen/Funktionen klar benannt?)
- Modularität & Struktur: (Zu komplex? Solid-Prinzipien eingehalten?)
- Wartbarkeit: (Ist der Code leicht erweiterbar?)
- Sicherheit: (Gibt es potenzielle Schwachstellen, z. B. Injection-Risiken?)

Zusatzaufgaben:
- Schlage konkrete Verbesserungen vor, um die Qualität zu erhöhen.

Das Ergebnis im Markdown-Format darstellen und wenn möglich Tabellen verwenden.


## Wichtigste Befunde

| Priorität                                                                                 | Befund                                                                                                             | Warum relevant                                                                               | Referenzen |
| ----------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------- | ---------- |
| Hoch                                                                                      | Die Sicherheitsbasis im Container ist unnötig offen, weil das Runtime-Image als root läuft.                        | Für ein Analysewerkzeug ist das kein akuter Exploit, aber es vergrößert die Auswirkungen     |
| eines späteren Bugs oder kompromittierter Eingaben unnötig.                               | Dockerfile:26, Dockerfile:37                                                                                       |
| Mittel                                                                                    | Die Architektur ist sauber vorbereitet, aber fachlich noch überwiegend Platzhalterlogik.                           | Das Projekt wirkt strukturell gut, die eigentliche Wartbarkeit ist aber noch nicht bewiesen, |
| weil echte Ports, Fehlerpfade und Domänenflüsse noch fehlen.                              | src/main.cpp:6, src/hexagon/services/startup_message_service.cpp:7, src/adapters/cli/placeholder_cli_adapter.cpp:7 |
| Mittel                                                                                    | Tests prüfen aktuell vor allem Verdrahtung und Platzhaltertexte, kaum fachliches Verhalten.                        | Das schützt nur begrenzt vor Regressionen, sobald echte Analyse-Logik hinzukommt.            |
| tests/hexagon/test_startup_message.cpp:5, tests/adapters/test_dependency_boundaries.cpp:6 |
| Mittel                                                                                    | Externe Dependencies werden über Git-Tags per FetchContent bezogen, aber nicht unveränderlich gepinnt.             | Das ist eher ein Supply-Chain- und Reproduzierbarkeitsrisiko als eine direkte                |
| Schwachstelle. Tags sind schwächer als Commit-SHAs.                                       | cmake/Dependencies.cmake:6, cmake/Dependencies.cmake:16, cmake/Dependencies.cmake:25                               |
| Niedrig                                                                                   | Benennungen sind größtenteils klar, aber viele Namen signalisieren Provisorium statt Fachverantwortung.            | placeholder_* ist für M0 ehrlich, skaliert aber schlecht in echte                            |
| Domänenlogik.                                                                             | src/adapters/output/placeholder_report_adapter.cpp:5, src/adapters/cli/placeholder_cli_adapter.cpp:7               |

## Bewertung

| Metrik                    | Score | Begründung                                                                                                                                                                                                                                                                                                                                                                        |
| ------------------------- | ----: | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Lesbarkeit & Namensgebung |  8/10 | Der Code ist klein, konsistent formatiert und leicht lesbar. Namespaces, Dateinamen und Funktionen sind verständlich. Abzug gibt es, weil viele Bezeichner noch technisch-provisorisch statt fachlich sind, etwa placeholder_* oder json_dependency_ready(), das nur die Bibliothek verdrahtet, aber keinen fachlichen Zweck beschreibt.                                          |
| Modularität & Struktur    |  7/10 | Die Trennung in hexagon und adapters ist sauber angelegt, inklusive Boundary-Check im Testlauf. Das ist ein guter struktureller Start. Abzug, weil die eigentlichen Ports und Anwendungsflüsse bisher weitgehend nur dokumentiert, aber noch nicht als belastbare API im Code umgesetzt sind. Die Struktur ist gut, die Modularität ist noch nicht in realer Komplexität erprobt. |
| Wartbarkeit               |  6/10 | Für den aktuellen Umfang ist der Code leicht verständlich und einfach zu ändern. Für künftige Erweiterung fehlt aber noch die tragende Domänenschicht: keine expliziten Port-Interfaces, kaum Fehlerbehandlung, kaum Domänenmodelle, kaum Verhaltenstests. Damit ist die Architektur eher vorbereitet als validiert.                                                              |
| Sicherheit                |  6/10 | Es gibt aktuell keine offensichtlichen Injection-Pfade im C++-Code. Positiv ist auch die klare Trennung externer Bibliotheken vom Hexagon. Abzug gibt es für Container-Ausführung als root, unpinned Dependency-Quellen via FetchContent, und für die Tatsache, dass Eingabevalidierung und Fehlerpfade für spätere JSON-/CLI-Verarbeitung noch nicht vorhanden sind.             |

## Detailbewertung

| Bereich      | Positiv                                                           | Schwächer                                                      |
| ------------ | ----------------------------------------------------------------- | -------------------------------------------------------------- |
| Lesbarkeit   | Kurze Funktionen, klare Includes, wenig unnötige Komplexität      | Platzhalternamen statt fachlicher Begriffe                     |
| Struktur     | Gute Verzeichnisstruktur, xray_hexagon klar von Adaptern getrennt | Hexagon ist aktuell eher Absicht als echte fachliche Schicht   |
| Tests        | Boundary-Check ist sinnvoll, Doctest-Setup ist sauber             | Bisher fast nur Smoke-Tests                                    |
| Build/Docker | Multi-Stage-Dockerfile ist vernünftig, .dockerignore existiert    | Runtime als root, keine Härtung, keine Health-/User-Definition |
| Sicherheit   | Keine direkten Injection-Stellen sichtbar                         | Supply-Chain- und Container-Härtung ausbaufähig                |


## Konkrete Verbesserungen

| Maßnahme                                                                                                                | Nutzen                                                        | Aufwand            |
| ----------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------- | ------------------ |
| Im Runtime-Image einen unprivilegierten Benutzer anlegen und USER setzen                                                | Reduziert Schadensradius im Container deutlich                | Niedrig            |
| FetchContent-Abhängigkeiten auf Commit-SHAs oder verifizierte Archive pinnen                                            | Bessere Reproduzierbarkeit und geringeres Supply-Chain-Risiko | Niedrig bis mittel |
| Platzhalteradapter in fachliche Namen überführen, z. B. console_report_adapter, compile_commands_json_adapter           | Erhöht Lesbarkeit und spätere Orientierung                    | Niedrig            |
| Echte Port-Interfaces im Hexagon einführen statt nur Hilfsfunktionen                                                    | Macht die Architektur testbar und erweiterbar                 | Mittel             |
| Fehlerbehandlung für spätere Input-Adapter früh definieren, z. B. expected<Result, Diagnostic> oder klare Fehlerobjekte | Verhindert Wildwuchs bei Ausnahmen/Exit-Codes                 | Mittel             |
| Tests von Verdrahtung auf Verhalten umstellen                                                                           | Besserer Schutz vor Regressionen                              | Mittel             |
| Den Boundary-Check robuster machen, z. B. über Include-Analyse im Build oder strengere Regeln pro Target                | Weniger fragile Architekturprüfung als reines grep            | Mittel             |
| main.cpp als Composition Root weiter ausbauen und dort nur Verdrahtung lassen                                           | Hält Geschäftslogik aus CLI/IO fern                           | Niedrig            |

## Empfohlene nächste Schritte

1. ~~Runtime-Container härten: non-root user, optional read-only root filesystem als Laufzeitvorgabe dokumentieren.~~
2. ~~Aus den dokumentierten Ports echte Header im Hexagon machen und Adapter daran anbinden.~~
3. Den ersten realen Use Case implementieren: compile_commands.json laden, validieren, Ergebnisobjekt erzeugen.
4. Verhaltenstests für Erfolg, leere Eingabe und Fehlerfälle ergänzen.
5. Dependency-Pinning in cmake/Dependencies.cmake:1 nachschärfen.

## Kurzfazit

Die Codebasis ist für einen M0-Stand ordentlich: klar lesbar, bewusst strukturiert und architektonisch diszipliniert. Die größte Schwäche ist nicht schlechter Code, sondern dass die Qualität der
geplanten Architektur noch nicht an echter Fachlogik bewiesen wurde.
