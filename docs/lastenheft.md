# Lastenheft - cmake-xray

## 0. Dokumenteninformationen

| Feld | Wert |
|---|---|
| Dokument | Lastenheft `cmake-xray` |
| Version | `0.6` |
| Stand | `2026-04-21` |
| Status | Arbeitsstand |

### 0.1 Aenderungshistorie

| Version | Datum | Aenderung |
|---|---|---|
| `0.1` | `2026-04-21` | Erstfassung des Lastenhefts |
| `0.2` | `2026-04-21` | Anforderungen konsolidiert, Kennungen eingefuehrt und Tabellenstruktur ergaenzt |
| `0.3` | `2026-04-21` | Rueckverfolgbarkeit, Dokumentenstand, Messgroessen und Review-Anmerkungen ergaenzt |
| `0.4` | `2026-04-21` | Kennungsregeln, AK-Abdeckung, Performance-Einordnung und Erfolgsmasstaebe weiter geschaerft; fruehere Lizenzkennungen `RB-09` bis `RB-11` zu `RB-09` zusammengefasst |
| `0.5` | `2026-04-21` | Datengrundlage der Include-Analyse geklaert, NF-Prioritaeten ergaenzt und Eingabeverhalten weiter praezisiert |
| `0.6` | `2026-04-21` | Verweis auf Folgedokumente ergaenzt |

## 1. Einleitung

### 1.1 Ziel des Dokuments
Dieses Lastenheft beschreibt die Anforderungen an das Open-Source-Projekt **cmake-xray**.
Das Produkt soll als Analyse- und Diagnosewerkzeug fuer CMake-basierte C++-Builds eingesetzt werden und typische Schwachstellen in Build-Strukturen sichtbar machen.

### 1.2 Projektbezeichnung
**cmake-xray**
Untertitel: **Build visibility for CMake projects**

### 1.3 Ausgangssituation
C++-Projekte mit CMake wachsen oft ueber laengere Zeitraeume. Dabei entstehen schleichend Probleme wie:

- lange Build-Zeiten
- unnoetige Rebuilds
- unklare Target-Abhaengigkeiten
- aufgeblaehte Include-Strukturen
- schlechte Wartbarkeit der Build-Konfiguration

Diese Probleme sind in vielen Projekten zwar spuerbar, aber nur schwer systematisch analysierbar. Bestehende Werkzeuge liefern oft nur Rohdaten oder setzen tiefes Spezialwissen voraus. Es fehlt ein fokussiertes Werkzeug, das Build-Probleme verstaendlich aufbereitet und konkrete Ansatzpunkte zur Verbesserung liefert.

### 1.4 Zielsetzung
Ziel ist die Entwicklung eines Open-Source-Werkzeugs, das vorhandene Build-Daten aus CMake-basierten Projekten analysiert und die Ergebnisse ueber eine CLI sowie exportierbare Reports bereitstellt.
Das Werkzeug soll insbesondere dabei helfen:

- Build-Komplexitaet sichtbar zu machen
- Include-Hotspots zu erkennen
- auffaellige Translation Units zu identifizieren
- Zusammenhaenge zwischen Dateien und Targets nachvollziehbar darzustellen
- die Auswirkungen von Dateiaenderungen auf Rebuilds abzuschaetzen

### 1.5 Begriffsabgrenzung
Fuer dieses Lastenheft gelten folgende Begriffe:

- **Translation Unit**: eine in den Build-Eingaben referenzierte Quelldatei samt zugehoerigem Compile-Aufruf
- **auffaellige Translation Unit**: eine Translation Unit, die im Analyseergebnis anhand dokumentierter Kennzahlen hoeher eingestuft wird als andere Translation Units; moegliche Kennzahlen sind zum Beispiel Anzahl aufgeloester Includes, Tiefe von Include-Ketten, Groesse des Compile-Aufrufs oder Anzahl gesetzter Defines
- **Include-Hotspot**: ein Header, der in einer fuer den Nutzer sichtbaren Anzahl von Translation Units vorkommt und deshalb im Bericht gesondert ausgewiesen wird
- **Rebuild-Impact**: eine aus vorhandenen Build-Daten abgeleitete Abschaetzung, welche Translation Units oder Targets von einer Dateiaenderung betroffen sein koennen

### 1.6 Kennzeichnungssystem
Alle Anforderungen in diesem Dokument erhalten eindeutige, stabile Kennungen. Diese Kennungen dienen als Referenz in spaeteren Design-, Planungs- und Architekturdokumenten.

Verwendete Praefixe:

- `F-xx` fuer funktionale Anforderungen
- `NF-xx` fuer nichtfunktionale Anforderungen
- `RB-xx` fuer Randbedingungen
- `S-xx` fuer Schnittstellenanforderungen
- `AK-xx` fuer Abnahmekriterien

Nachtraeglich ergaenzte Anforderungen erhalten zur Wahrung stabiler Referenzen die jeweils naechste freie Kennung, auch wenn sie thematisch in einem frueheren Abschnitt einsortiert werden.

### 1.7 Verwandte Dokumente

| Dokument | Beschreibung |
|---|---|
| [Design](./design.md) | Fachliche und benutzerbezogene Ausgestaltung auf Basis dieses Lastenhefts |
| [Architecture](./architecture.md) | Technische Systemstruktur und Datenfluesse |
| [Phasenplan](./roadmap.md) | Inkrementelle Lieferplanung in Phasen |

### 1.8 Schreibkonvention
Dieses Dokument verwendet bewusst ASCII-Umschreibungen wie `ae`, `oe` und `ue`, damit Inhalt und Kennungen in einfachen Textumgebungen, Terminals und Build-Logs konsistent darstellbar bleiben.

---

## 2. Produktuebersicht

### 2.1 Produktidee
**cmake-xray** ist ein Analysewerkzeug fuer CMake-Projekte. Es liest vorhandene Build-Artefakte und Projektinformationen ein und erzeugt daraus Diagnoseergebnisse und Berichte.

Das Produkt ist **kein eigenes Build-System** und **kein Ersatz fuer CMake**.
Es ergaenzt bestehende Toolchains, indem es Transparenz in bestehende Build-Prozesse bringt.

### 2.2 Produktnutzen
Der Nutzen des Produkts liegt in folgenden Punkten:

- schnellere Identifikation von Build-Problemen
- bessere Entscheidungsgrundlage fuer Refactorings im Build-System
- Transparenz ueber kritische Includes und Kopplungen
- verbesserte Wartbarkeit grosser C++-Codebasen
- nutzbare Reports fuer lokale Analyse und CI-Pipelines

### 2.3 Zielgruppen
Das Produkt richtet sich an:

- C++-Entwicklerinnen und -Entwickler
- Build- und Tooling-Verantwortliche
- Maintainer groesserer CMake-Projekte
- Open-Source-Projekte mit komplexen C++-Builds
- Teams, die Build-Zeiten und Build-Stabilitaet verbessern wollen

---

## 3. Ziele und Nicht-Ziele

### 3.1 Projektziele
Das Produkt soll:

1. Build-Daten aus CMake-basierten Projekten verarbeiten koennen
2. nachvollziehbare Analyseergebnisse fuer Translation Units, Includes und Rebuild-Auswirkungen liefern
3. als CLI-Werkzeug nutzbar sein
4. Reports in menschenlesbaren Formaten erzeugen
5. lokal sowie in CI-Umgebungen einsetzbar sein
6. in einem klar abgegrenzten ersten Release nutzbaren Mehrwert liefern

### 3.2 Nicht-Ziele
Das Produkt soll zunaechst **nicht**:

- CMake ersetzen
- einen vollstaendigen CMake-Interpreter implementieren
- eine vollwertige IDE-Erweiterung sein
- alle Build-Systeme unterstuetzen
- komplexe GUI-Anwendungen als Primaerziel verfolgen
- Compileroptimierungen selbst durchfuehren
- automatische Refactorings im grossen Stil vornehmen

---

## 4. Einsatzbereich

### 4.1 Fachlicher Einsatzbereich
Das Werkzeug wird in Entwicklungsumgebungen verwendet, in denen C++-Projekte mit CMake gebaut werden. Typische Einsatzfaelle sind:

- Analyse von Build-Problemen in bestehenden Projekten
- Untersuchung von Include-Strukturen
- Bewertung der Auswirkungen geplanter Aenderungen
- Erstellung von Diagnoseberichten fuer Pull Requests oder CI-Laeufe

### 4.2 Technischer Einsatzbereich
Das Produkt soll auf Entwicklerrechnern und in CI-Umgebungen lauffaehig sein.

Fuer das erste Release gilt:

- primaere Zielplattform ist Linux
- Nutzung in lokalen Entwicklerumgebungen und in CI muss moeglich sein

Fuer spaetere Releases gilt:

- macOS und Windows sollen unterstuetzt werden

---

## 5. Stakeholder

| Stakeholder | Interesse am Produkt | Erwartung |
|---|---|---|
| Open-Source Maintainer | bessere Transparenz ueber Build-Probleme | schnelle, reproduzierbare Analysen |
| C++-Entwickler | Diagnose von Include- und Rebuild-Problemen | einfache CLI und verstaendliche Ausgaben |
| CI-/DevOps-Verantwortliche | automatisierte Build-Berichte | stabile Reports und aussagekraeftige Exit-Codes |
| Beitragende aus der Community | Weiterentwicklung des Projekts | klare Nutzungs- und Beitragsdokumentation |

---

## 6. Funktionale Anforderungen

### 6.1 Verarbeitung von Eingabedaten
Das System muss `compile_commands.json` einlesen und verarbeiten koennen.

| Kennung | Prioritaet | Anforderung |
|---|---|---|
| `F-01` | Muss | Einlesen und Validieren einer `compile_commands.json` |
| `F-02` | Muss | Erkennung fehlender, fehlerhafter oder unvollstaendiger Eintraege |
| `F-03` | Muss | Ausgabe einer konkreten Fehlermeldung bei ungueltigen Eingabedaten |
| `F-04` | Muss | Extraktion mindestens von Quelldateipfad, Arbeitsverzeichnis und Compile-Aufruf je Eintrag |
| `F-05` | Soll | Verarbeitung zusaetzlicher Build-Metadaten, wenn diese fuer targetbezogene Analysen bereitgestellt werden |
| `F-41` | Muss | Behandlung einer syntaktisch gueltigen, aber leeren `compile_commands.json` mit einer klaren Rueckmeldung und einem definierten Exit-Code |

### 6.2 Analyse von Translation Units
Das System soll Translation Units untersuchen und in einer nachvollziehbaren Reihenfolge ausgeben.

| Kennung | Prioritaet | Anforderung |
|---|---|---|
| `F-06` | Muss | Identifikation der analysierten Translation Units |
| `F-07` | Muss | Ausgabe einer Rangfolge auffaelliger Translation Units |
| `F-08` | Muss | Ausgabe der fuer die Bewertung verwendeten Kennzahlen je ausgewiesener Translation Unit |
| `F-09` | Muss | Kennzeichnung der Grundlage der Bewertung im Ergebnis oder in der Hilfe zum Befehl |
| `F-10` | Kann | Bewertung anhand konfigurierbarer Schwellenwerte |
| `F-11` | Kann | Vergleich zwischen zwei Analysezeitpunkten |

### 6.3 Include-Analyse
Das System soll Include-Beziehungen analysieren und Include-Hotspots sichtbar machen.

Die Ermittlung von Include-Beziehungen erfolgt auf Basis der in den Eingabedaten beschriebenen Compile-Aufrufe und weiterer verfuegbarer Build-Informationen. Das konkrete Verfahren, etwa durch Quelltextauswertung, compilergestuetzte Abhaengigkeitsinformationen oder vorhandene Dependency-Artefakte, wird im Pflichtenheft festgelegt.

| Kennung | Prioritaet | Anforderung |
|---|---|---|
| `F-12` | Muss | Ermittlung von Headern, die in mehreren Translation Units vorkommen |
| `F-13` | Muss | Ausgabe einer sortierten Liste von Include-Hotspots |
| `F-14` | Muss | Ausgabe mindestens von Header-Bezeichnung und Anzahl betroffener Translation Units je Include-Hotspot |
| `F-15` | Muss | Zuordnung von Include-Hotspots zu betroffenen Translation Units |
| `F-16` | Soll | Unterscheidung zwischen Projekt-Headern und externen Headern |
| `F-17` | Soll | Eingrenzung auf direkte Includes oder indirekte Includes, sofern die zugrunde liegenden Daten dies erlauben |

### 6.4 Analyse von Target-Abhaengigkeiten
Das System soll targetbezogene Zusammenhaenge ausgeben koennen, sofern geeignete Build-Metadaten vorliegen.

| Kennung | Prioritaet | Anforderung |
|---|---|---|
| `F-18` | Soll | Ausgabe direkter Abhaengigkeiten zwischen Targets in textueller Form |
| `F-19` | Soll | Zuordnung von Translation Units zu Targets, sofern diese Information in den Eingabedaten vorliegt |
| `F-20` | Soll | Hervorhebung von Targets mit vielen ein- oder ausgehenden Abhaengigkeiten |

### 6.5 Rebuild-Impact-Analyse
Das System soll die Auswirkungen einer Dateiaenderung abschaetzen koennen.

| Kennung | Prioritaet | Anforderung |
|---|---|---|
| `F-21` | Muss | Angabe eines Dateipfads ueber die CLI, fuer den die Rebuild-Auswirkungen abgeschaetzt werden sollen |
| `F-22` | Muss | Ausgabe der voraussichtlich betroffenen Translation Units |
| `F-23` | Muss | Hinweis, wenn die Abschaetzung aufgrund fehlender Daten nur teilweise moeglich ist |
| `F-24` | Soll | Ausgabe der voraussichtlich betroffenen Targets, sofern targetbezogene Metadaten vorliegen |
| `F-25` | Soll | Priorisierung der Auswirkungen, zum Beispiel nach direkter und indirekter Betroffenheit |

### 6.6 Berichtserzeugung
Das System soll Analyseergebnisse exportieren koennen.

| Kennung | Prioritaet | Anforderung |
|---|---|---|
| `F-26` | Muss | Konsolenausgabe |
| `F-27` | Muss | Markdown-Export |
| `F-28` | Soll | HTML-Export |
| `F-29` | Kann | JSON-Export |
| `F-30` | Kann | Graphviz/DOT-Export |

### 6.7 CLI-Bedienung
Das Produkt muss als Kommandozeilenwerkzeug bedienbar sein.

| Kennung | Prioritaet | Anforderung |
|---|---|---|
| `F-31` | Muss | nachvollziehbare Befehlsstruktur |
| `F-32` | Muss | `--help` fuer Hauptkommando und Unterkommandos |
| `F-33` | Muss | aussagekraeftige Exit-Codes fuer Erfolg und Fehlerfaelle |
| `F-34` | Muss | verstaendliche Fehlermeldungen |
| `F-39` | Soll | Unterstuetzung mindestens eines detailreicheren Ausgabemodus fuer Diagnosezwecke, zum Beispiel `--verbose` |
| `F-40` | Kann | Unterstuetzung eines reduzierten Ausgabemodus, zum Beispiel `--quiet` |

### 6.8 Konfiguration
Das System soll konfigurierbar sein.

| Kennung | Prioritaet | Anforderung |
|---|---|---|
| `F-35` | Soll | Angabe von Eingabedateien und Ausgabepfaden |
| `F-36` | Soll | Steuerung des Ausgabeformats |
| `F-37` | Soll | Aktivierung und Deaktivierung einzelner Analysen |
| `F-38` | Soll | Konfiguration von Schwellenwerten oder Grenzwerten fuer Auswertungen |

---

## 7. Beispielhafte Anwendungsfaelle

### 7.1 Analyse eines bestehenden Projekts
Ein Entwickler moechte verstehen, welche Teile des Builds unnoetig komplex sind.
Er fuehrt eine Projektanalyse aus und erhaelt einen Bericht ueber auffaellige Translation Units und Include-Hotspots.
Betrifft: `F-06`, `F-07`, `F-08`, `F-12`, `F-13`, `F-14`, `F-26`

### 7.2 Abschaetzung der Auswirkung einer Header-Aenderung
Ein Maintainer plant eine Aenderung an einer zentralen Header-Datei.
Er fuehrt eine Impact-Analyse fuer diese Datei aus und erhaelt die betroffenen Translation Units sowie, falls verfuegbar, die betroffenen Targets.
Betrifft: `F-21`, `F-22`, `F-23`, `F-24`

### 7.3 CI-gestuetzter Build-Report
Ein Projekt erzeugt im CI-Lauf einen Konsolen- oder Markdown-Bericht, um Build-Probleme in Pull Requests sichtbar zu machen.
Betrifft: `F-26`, `F-27`, `S-09`, `S-10`, `S-11`

---

## 8. Nichtfunktionale Anforderungen

### 8.1 Benutzbarkeit

| Kennung | Prioritaet | Anforderung |
|---|---|---|
| `NF-01` | Soll | Die CLI soll konsistent und nachvollziehbar aufgebaut sein. |
| `NF-02` | Muss | Fehlermeldungen sollen konkret und handlungsorientiert sein. |
| `NF-03` | Soll | Standardausgaben sollen ohne zusaetzliche Konfiguration sinnvoll nutzbar sein. |

### 8.2 Performance

| Kennung | Prioritaet | Anforderung |
|---|---|---|
| `NF-04` | Soll | Auf einer dokumentierten Referenzumgebung sollen Projekte mit bis zu 1.000 Translation Units im ersten Meilenstein in hoechstens 60 Sekunden analysierbar sein. |
| `NF-05` | Soll | Auf derselben Referenzumgebung soll der Arbeitsspeicherverbrauch bei diesen Projekten 2 GB nicht ueberschreiten. |
| `NF-06` | Soll | Die fuer Performance-Angaben verwendete Referenzumgebung und das Referenzprojekt muessen dokumentiert werden. |

Diese Werte gelten als Mindestanforderung fuer den ersten Meilenstein. Fuer spaetere Releases sollen die Grenzwerte auf Basis realer Nutzungserfahrungen und groesserer Referenzprojekte fortgeschrieben werden.

### 8.3 Portabilitaet

| Kennung | Prioritaet | Anforderung |
|---|---|---|
| `NF-07` | Muss | Das Produkt muss auf Linux baubar und nutzbar sein. |
| `NF-08` | Soll | Das Produkt soll perspektivisch auch auf macOS und Windows nutzbar sein. |
| `NF-09` | Soll | Es soll mit gaengigen Compilern und gaengigen CMake-Versionen einsetzbar sein. |

### 8.4 Wartbarkeit

| Kennung | Prioritaet | Anforderung |
|---|---|---|
| `NF-10` | Muss | Der Code soll automatisiert testbar sein. |
| `NF-11` | Soll | Das Projekt soll fuer externe Beitragende nachvollziehbar dokumentiert sein. |

### 8.5 Erweiterbarkeit

| Kennung | Prioritaet | Anforderung |
|---|---|---|
| `NF-12` | Soll | Neue Analysearten sollen mit vertretbarem Aufwand ergaenzbar sein. |
| `NF-13` | Soll | Weitere Ausgabeformate sollen spaeter ergaenzbar sein. |

### 8.6 Zuverlaessigkeit

| Kennung | Prioritaet | Anforderung |
|---|---|---|
| `NF-14` | Muss | Fehlerhafte oder unvollstaendige Eingaben duerfen nicht zu schwer verstaendlichem Verhalten fuehren. |
| `NF-15` | Muss | Die Analyse soll bei gleichen Eingabedaten reproduzierbare Ergebnisse liefern. |

### 8.7 Dokumentation

| Kennung | Prioritaet | Anforderung |
|---|---|---|
| `NF-16` | Muss | Das Projekt soll eine verstaendliche README enthalten. |
| `NF-17` | Muss | Installations- und Nutzungsbeispiele sollen dokumentiert werden. |
| `NF-18` | Soll | Beispielausgaben fuer die wichtigsten Analysearten sollen vorhanden sein. |
| `NF-19` | Soll | Fuer zentrale Analysearten sollen Referenzprojekte oder Referenzdaten mit erwarteten Ergebnissen fuer automatisierte Tests vorhanden sein. |
| `NF-20` | Soll | Fuer maschinenlesbare Ausgabeformate muessen Formatversion oder Schema-Version dokumentiert werden. |

---

## 9. Randbedingungen

### 9.1 Technische Randbedingungen

| Kennung | Kategorie | Randbedingung |
|---|---|---|
| `RB-01` | Technisch | Implementierung in **C++** |
| `RB-02` | Technisch | Build mit **CMake** |
| `RB-03` | Technisch | Nutzung eines modernen Sprachstandards, mindestens **C++20** |
| `RB-04` | Technisch | Das Produkt darf keine interaktive Laufzeitumgebung voraussetzen und muss in lokalen Entwicklerumgebungen sowie in CI-Automatisierungen nutzbar sein. |
| `RB-05` | Technisch | quelloffene Entwicklung auf GitHub |

### 9.2 Organisatorische Randbedingungen

| Kennung | Kategorie | Randbedingung |
|---|---|---|
| `RB-06` | Organisatorisch | Das Projekt soll community-freundlich aufgebaut sein. |
| `RB-07` | Organisatorisch | Beitraege von externen Entwicklern sollen erleichtert werden. |
| `RB-08` | Organisatorisch | Das Projekt soll eine klare Lizenz erhalten. |

### 9.3 Lizenz
Fuer das Projekt soll eine etablierte Open-Source-Lizenz verwendet werden, bevorzugt eine der folgenden:

| Kennung | Kategorie | Randbedingung |
|---|---|---|
| `RB-09` | Lizenz | Eine der folgenden Lizenzen soll verwendet werden: MIT, BSD-3-Clause oder Apache-2.0. |

---

## 10. Schnittstellen

### 10.1 Eingabeschnittstellen
Das Produkt soll folgende Eingaben unterstuetzen:

| Kennung | Typ | Schnittstelle |
|---|---|---|
| `S-01` | Eingabe | `compile_commands.json` |
| `S-02` | Eingabe | optional weitere Build-Metadaten fuer targetbezogene Analysen, zum Beispiel aus der CMake File API oder anderen Build-Artefakten; nicht Bestandteil des MVP |
| `S-03` | Eingabe | Dateipfade und Optionen ueber CLI-Parameter |

### 10.2 Ausgabeschnittstellen
Das Produkt soll Ergebnisse ausgeben koennen als:

| Kennung | Typ | Schnittstelle |
|---|---|---|
| `S-04` | Ausgabe | Konsole |
| `S-05` | Ausgabe | Markdown-Datei |
| `S-06` | Ausgabe | optional HTML-Datei |
| `S-07` | Ausgabe | optional JSON |
| `S-08` | Ausgabe | optional DOT/Graphviz |

### 10.3 Integrationsschnittstellen
Das Produkt soll in folgende Umgebungen integrierbar sein:

| Kennung | Typ | Schnittstelle |
|---|---|---|
| `S-09` | Integration | lokale Shell-Skripte |
| `S-10` | Integration | GitHub Actions |
| `S-11` | Integration | andere CI-Pipelines |

---

## 11. Qualitaetsziele

Die Qualitaetsziele werden wie folgt priorisiert:

| Qualitaetsmerkmal | Prioritaet | Begruendung |
|---|---|---|
| Verstaendlichkeit der Ergebnisse | sehr hoch | Ohne klare Aussage verliert das Werkzeug seinen Nutzen |
| Zuverlaessigkeit | sehr hoch | Fehlerhafte Analysen fuehren zu Fehlentscheidungen |
| Performance | hoch | Das Werkzeug muss in realen Projekten praktikabel sein |
| Benutzbarkeit | hoch | Die Hemmschwelle fuer den Einsatz in Alltag und CI muss niedrig sein |
| Portabilitaet | mittel | Linux hat fuer das erste Release Vorrang |
| Funktionsbreite | mittel | Fokus ist wichtiger als fruehe Vollstaendigkeit |

---

## 12. Abgrenzung des ersten Releases (MVP)

### 12.1 Inhalt des MVP
Das erste Release soll mindestens folgende Funktionen enthalten:

- Einlesen von `compile_commands.json`
- CLI-Grundgeruest
- Analyse auffaelliger Translation Units mit Rangfolge
- einfache Include-Hotspot-Analyse mit Anzahl betroffener Translation Units
- Konsolen- und Markdown-Report
- Impact-Analyse auf Dateiebene fuer Translation Units

### 12.2 Nicht Bestandteil des MVP
Folgende Funktionen sind fuer spaetere Versionen vorgesehen:

- HTML-Export
- targetbezogene Abhaengigkeitsanalysen, soweit dafuer zusaetzliche Metadaten benoetigt werden
- IDE-Integrationen
- tiefgehende Build-Zeit-Messungen
- automatische Optimierungsvorschlaege mit Refactoring
- Unterstuetzung weiterer Build-Systeme

---

## 13. Abnahmekriterien

Das Produkt gilt fuer den ersten Meilenstein als abnahmefaehig, wenn:

| Kennung | Abnahmekriterium | Prueft |
|---|---|---|
| `AK-01` | eine gueltige `compile_commands.json` erfolgreich eingelesen und verarbeitet werden kann | `F-01`, `F-04` |
| `AK-02` | eine ungueltige, leere oder unvollstaendige `compile_commands.json` mit einer klaren Fehlermeldung oder definierten Rueckmeldung und einem passenden Exit-Code behandelt wird | `F-02`, `F-03`, `F-33`, `F-34`, `F-41` |
| `AK-03` | mindestens ein CLI-Befehl in der Konsolenausgabe eine Rangfolge auffaelliger Translation Units inklusive zugehoeriger Kennzahlen ausgibt | `F-06`, `F-07`, `F-08`, `F-09`, `F-26` |
| `AK-04` | mindestens ein Bericht Include-Hotspots mit Header-Bezeichnung und Anzahl betroffener Translation Units ausweist | `F-12`, `F-13`, `F-14`, `F-15` |
| `AK-05` | eine Impact-Analyse fuer eine Datei ausfuehrbar ist und betroffene Translation Units oder einen nachvollziehbaren Hinweis auf fehlende Daten ausgibt | `F-21`, `F-22`, `F-23` |
| `AK-06` | ein Markdown-Report erzeugt werden kann | `F-27` |
| `AK-07` | das Projekt auf Linux baubar und testbar ist | `NF-07`, `RB-01`, `RB-02`, `RB-03` |
| `AK-08` | eine README mit Installations- und Nutzungsbeispielen vorhanden ist | `NF-16`, `NF-17` |
| `AK-09` | `--help` fuer Hauptkommando und mindestens ein Unterkommando gibt eine verstaendliche Hilfetextausgabe aus | `F-31`, `F-32` |

---

## 14. Risiken

Konkrete Minderungsmassnahmen fuer diese Risiken werden im Projektplan und in spaeteren Umsetzungsdokumenten abgeleitet.

### 14.1 Fachliche Risiken
- Die Aussagekraft der Analysen koennte anfangs geringer sein als von Nutzern erwartet.
- Nutzer koennten exakte Build-Zeitdaten erwarten, obwohl das Produkt primaer analytische Abschaetzungen liefert.

### 14.2 Technische Risiken
- Unterschiede zwischen Compilern, Generatoren und Plattformen koennen die Analyse erschweren.
- `compile_commands.json` enthaelt nicht in jedem Projekt alle Informationen fuer targetbezogene oder tiefergehende Build-Erklaerungen.
- Grosse Projekte koennen hohe Anforderungen an Speicher und Laufzeit stellen.

### 14.3 Projektrisiken
- Zu breiter Scope kann das Projekt unklar und schwer wartbar machen.
- Zu viele optionale Anforderungen im ersten Release koennen die Lieferfaehigkeit des MVP gefaehrden.

---

## 15. Erfolgsmassstaebe

Das Projekt ist erfolgreich, wenn:

- mindestens drei reale CMake-Projekte mit jeweils mindestens 50 Translation Units damit analysiert werden koennen
- die Berichte in diesen Projekten konkrete Probleme sichtbar machen
- mindestens ein Projekt das Tool lokal oder in CI einsetzt
- erste Community-Beitraege oder Feature-Anfragen entstehen
- das Projekt als nuetzliches Diagnosewerkzeug wahrgenommen wird

---

## 16. Zusammenfassung
**cmake-xray** soll ein fokussiertes Open-Source-Werkzeug zur Analyse von CMake-basierten C++-Builds werden.
Der Schwerpunkt liegt nicht auf dem Ersetzen des Build-Systems, sondern auf **Transparenz, Diagnose und Berichtserzeugung**.

Das Produkt soll insbesondere helfen, Build-Komplexitaet zu verstehen, Include-Hotspots aufzudecken und Rebuild-Auswirkungen nachvollziehbar zu machen.
Fuer den Start ist ein klar begrenztes MVP entscheidend, damit das Projekt schnell nutzbar wird und nicht in zu grosser Breite stecken bleibt.
