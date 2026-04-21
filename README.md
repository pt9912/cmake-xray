# cmake-xray

Build visibility for CMake projects.

`cmake-xray` ist ein geplantes Analyse- und Diagnosewerkzeug fuer CMake-basierte C++-Builds. Ziel ist es, Build-Komplexitaet, Include-Hotspots, auffaellige Translation Units und Rebuild-Auswirkungen nachvollziehbar zu machen.

## Status

Das Repository befindet sich aktuell in einer dokumentationsgetriebenen Vorbereitungsphase.

Der aktuell dokumentierte erste Meilenstein ist `v0.1.0` gemass [docs/roadmap.md](./docs/roadmap.md).

Derzeit vorhanden sind:

- Anforderungsdokumente
- Design- und Architekturentwuerfe
- ein Phasenplan fuer das MVP

Noch nicht vorhanden sind:

- produktiver C++-Quellcode
- eine nutzbare CLI-Implementierung
- Releases oder installierbare Artefakte

## Geplanter Umfang

Fuer das erste Release ist vorgesehen:

- Einlesen und Validieren von `compile_commands.json`
- Analyse auffaelliger Translation Units
- Include-Hotspot-Analyse
- dateibasierte Rebuild-Impact-Analyse
- Konsolen- und Markdown-Reports
- Nutzung auf Linux in lokalen Umgebungen und in CI

Nicht Ziel des MVP sind insbesondere:

- Ersatz fuer CMake
- vollstaendige CMake-Interpretation
- IDE-Integration
- HTML-, JSON- oder DOT-Export im ersten Release

## Dokumente

Die fachliche und technische Planung ist in `docs/` abgelegt:

- [docs/lastenheft.md](./docs/lastenheft.md): Anforderungen, Randbedingungen und Abnahmekriterien
- [docs/design.md](./docs/design.md): fachliche und benutzerbezogene Ausgestaltung
- [docs/architecture.md](./docs/architecture.md): geplante Systemstruktur und Datenfluesse
- [docs/roadmap.md](./docs/roadmap.md): inkrementelle Lieferplanung
- [docs/plan-M0.md](./docs/plan-M0.md): Detailplanung fuer den ersten Meilenstein

## Geplante Roadmap

Die aktuelle Roadmap ist in vier inhaltliche Phasen gegliedert:

1. Fundament: Projektgrundgeruest, Build, Tests, Dokumente
2. MVP-Eingaben und CLI: valide Eingabedaten, Help, Exit-Codes
3. Kernanalysen MVP: TU-Ranking, Include-Hotspots, Impact-Analyse
4. Berichte und Qualitaet: Markdown, Referenzdaten, Performance-Baseline

Erweiterungen wie Target-Metadaten, HTML, JSON und weitere Plattformen sind fuer spaetere Schritte vorgesehen.

## Lizenz

Die Projektlizenz ist noch nicht final festgelegt. Laut Lastenheft ist eine der folgenden Lizenzen vorgesehen:

- MIT
- BSD-3-Clause
- Apache-2.0

## Changelog

Aenderungen werden in [CHANGELOG.md](./CHANGELOG.md) dokumentiert.
