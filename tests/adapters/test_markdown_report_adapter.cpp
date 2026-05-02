#include <doctest/doctest.h>

#include <string>
#include <utility>
#include <vector>

#include "adapters/output/markdown_report_adapter.h"
#include "hexagon/model/analysis_result.h"
#include "hexagon/model/application_info.h"
#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/model/include_classification.h"
#include "hexagon/model/translation_unit.h"

namespace {

using xray::adapters::output::MarkdownReportAdapter;
using xray::hexagon::model::AnalysisResult;
using xray::hexagon::model::CompileDatabaseError;
using xray::hexagon::model::CompileDatabaseResult;
using xray::hexagon::model::Diagnostic;
using xray::hexagon::model::DiagnosticSeverity;
using xray::hexagon::model::ImpactKind;
using xray::hexagon::model::ImpactResult;
using xray::hexagon::model::ImpactedTarget;
using xray::hexagon::model::ImpactedTranslationUnit;
using xray::hexagon::model::IncludeHotspot;
using xray::hexagon::model::ObservationSource;
using xray::hexagon::model::RankedTranslationUnit;
using xray::hexagon::model::TargetImpactClassification;
using xray::hexagon::model::TargetInfo;
using xray::hexagon::model::TargetMetadataStatus;
using xray::hexagon::model::TranslationUnitReference;

TranslationUnitReference make_reference(std::string source_path, std::string directory,
                                        std::string unique_key) {
    return {
        .source_path = std::move(source_path),
        .directory = std::move(directory),
        .source_path_key = unique_key,
        .unique_key = std::move(unique_key),
    };
}

AnalysisResult make_analysis_result() {
    AnalysisResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.compile_database_path = "tests/e2e/testdata/m3/report_project/compile_commands.json";
    result.include_analysis_heuristic = true;
    result.translation_units = {
        RankedTranslationUnit{
            .reference = make_reference("src/app/main.cpp", "build/debug",
                                        "src/app/main.cpp|build/debug"),
            .rank = 1,
            .arg_count = 8,
            .include_path_count = 2,
            .define_count = 1,
            .diagnostics =
                {
                    {DiagnosticSeverity::warning,
                     "could not resolve include \"generated/version.h\" from src/app/main.cpp"},
                },
        },
        RankedTranslationUnit{
            .reference = make_reference("src/lib/core.cpp", "build/lib",
                                        "src/lib/core.cpp|build/lib"),
            .rank = 2,
            .arg_count = 4,
            .include_path_count = 1,
            .define_count = 0,
            .diagnostics = {},
        },
    };
    result.include_hotspots = {
        IncludeHotspot{
            .header_path = "include/common/config.h",
            .affected_translation_units =
                {
                    make_reference("src/app/main.cpp", "build/debug",
                                   "src/app/main.cpp|build/debug"),
                    make_reference("src/lib/core.cpp", "build/lib",
                                   "src/lib/core.cpp|build/lib"),
                },
            .diagnostics =
                {
                    {DiagnosticSeverity::note, "hotspot detail"},
                },
        },
    };
    result.diagnostics = {
        {DiagnosticSeverity::warning, "compile database was normalized"},
        {DiagnosticSeverity::note,
         "include-based results are heuristic; conditional or generated includes may be missing"},
    };
    return result;
}

RankedTranslationUnit make_ranked_translation_unit(std::string source_path, std::string directory,
                                                   std::string unique_key, std::size_t rank,
                                                   std::size_t arg_count,
                                                   std::size_t include_path_count,
                                                   std::size_t define_count,
                                                   std::vector<Diagnostic> diagnostics = {}) {
    return RankedTranslationUnit{
        .reference = make_reference(std::move(source_path), std::move(directory),
                                    std::move(unique_key)),
        .rank = rank,
        .arg_count = arg_count,
        .include_path_count = include_path_count,
        .define_count = define_count,
        .diagnostics = std::move(diagnostics),
    };
}

ImpactResult make_impact_result() {
    ImpactResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.compile_database_path =
        "tests/e2e/testdata/m3/report_impact_header/compile_commands.json";
    result.changed_file = "include/common/config.h";
    result.heuristic = true;
    result.affected_translation_units = {
        ImpactedTranslationUnit{
            make_reference("src/app/main.cpp", "build/debug", "src/app/main.cpp|build/debug"),
            ImpactKind::heuristic,
        },
    };
    result.diagnostics = {
        {DiagnosticSeverity::warning,
         "could not resolve include \"generated/version.h\" from src/app/main.cpp"},
    };
    return result;
}

}  // namespace

TEST_CASE("markdown report adapter renders project reports with exact section structure") {
    const MarkdownReportAdapter adapter;

    const auto report = adapter.write_analysis_report(make_analysis_result(), 1);

    CHECK(report ==
          "# Project Analysis Report\n"
          "\n"
          "- Report type: analyze\n"
          "- Compile database: tests/e2e/testdata/m3/report\\_project/compile\\_commands.json\n"
          "- Translation units: 2\n"
          "- Translation unit ranking entries: 1 of 2\n"
          "- Include hotspot entries: 1 of 1\n"
          "- Top limit: 1\n"
          "- Include analysis heuristic: yes\n"
          "\n"
          "## Translation Unit Ranking\n"
          "1. src/app/main.cpp [directory: build/debug]\n"
          "    Metrics: arg_count=8, include_path_count=2, define_count=1\n"
          "    Diagnostics:\n"
          "    - warning: could not resolve include \"generated/version.h\" from src/app/main.cpp\n"
          "\n"
          "## Include Hotspots\n"
          "1. Header: include/common/config.h\n"
          "    Affected translation units: 2\n"
          "    Translation units:\n"
          "    - src/app/main.cpp [directory: build/debug]\n"
          "    - src/lib/core.cpp [directory: build/lib]\n"
          "    Diagnostics:\n"
          "    - note: hotspot detail\n"
          "\n"
          "## Diagnostics\n"
          "- warning: compile database was normalized\n"
          "- note: include-based results are heuristic; conditional or generated includes may be missing\n");
}

TEST_CASE("markdown report adapter renders heuristic and uncertain impact classifications") {
    const MarkdownReportAdapter adapter;

    const auto heuristic_report = adapter.write_impact_report(make_impact_result());
    CHECK(heuristic_report.find("- Impact classification: heuristic\n") != std::string::npos);
    CHECK(heuristic_report.find("## Heuristically Affected Translation Units\n- src/app/main.cpp [directory: build/debug]\n") !=
          std::string::npos);

    auto uncertain_result = make_impact_result();
    uncertain_result.affected_translation_units.clear();
    uncertain_result.diagnostics = {
        {DiagnosticSeverity::note, "conditional or generated includes may be missing from this result"},
    };

    const auto uncertain_report = adapter.write_impact_report(uncertain_result);
    CHECK(uncertain_report ==
          "# Impact Analysis Report\n"
          "\n"
          "- Report type: impact\n"
          "- Compile database: tests/e2e/testdata/m3/report\\_impact\\_header/compile\\_commands.json\n"
          "- Changed file: include/common/config.h\n"
          "- Affected translation units: 0\n"
          "- Impact classification: uncertain\n"
          "\n"
          "No affected translation units found.\n"
          "\n"
          "## Directly Affected Translation Units\n"
          "No directly affected translation units.\n"
          "\n"
          "## Heuristically Affected Translation Units\n"
          "No heuristically affected translation units.\n"
          "\n"
          "## Prioritised Affected Targets\n"
          "\n"
          "Requested depth: `2`. Effective depth: `0` (no graph).\n"
          "\n"
          "Target graph not loaded; prioritisation skipped.\n"
          "\n"
          "## Diagnostics\n"
          "- note: conditional or generated includes may be missing from this result\n");
}

TEST_CASE("markdown report adapter renders file api target metadata for analyze") {
    const MarkdownReportAdapter adapter;
    auto result = make_analysis_result();
    const TargetInfo app{"app", "EXECUTABLE", "app::EXECUTABLE"};
    const TargetInfo core{"core_lib", "STATIC_LIBRARY", "core::STATIC_LIBRARY"};
    result.observation_source = ObservationSource::derived;
    result.target_metadata = TargetMetadataStatus::partial;
    result.translation_units[0].targets = {app};
    result.translation_units[1].targets = {core};
    result.target_assignments = {
        {"src/app/main.cpp|build/debug", {app}},
        {"src/lib/core.cpp|build/lib", {core}},
    };

    const auto report = adapter.write_analysis_report(result, 2);

    CHECK(report.find("- Observation source: derived\n") != std::string::npos);
    CHECK(report.find("- Target metadata: partial\n") != std::string::npos);
    CHECK(report.find("- Translation units with target mapping: 2 of 2\n") !=
          std::string::npos);
    CHECK(report.find("1. src/app/main.cpp [directory: build/debug] [targets: app]\n") !=
          std::string::npos);
    CHECK(report.find("    - src/lib/core.cpp [directory: build/lib] [targets: core\\_lib]\n") !=
          std::string::npos);
}

TEST_CASE("markdown report adapter renders target impact sections") {
    const MarkdownReportAdapter adapter;
    const TargetInfo app{"app", "EXECUTABLE", "app::EXECUTABLE"};
    const TargetInfo core{"core_lib", "STATIC_LIBRARY", "core::STATIC_LIBRARY"};
    auto result = make_impact_result();
    result.observation_source = ObservationSource::exact;
    result.target_metadata = TargetMetadataStatus::loaded;
    result.affected_translation_units = {
        ImpactedTranslationUnit{
            make_reference("src/app/main.cpp", "build/app", "src/app/main.cpp|build/app"),
            ImpactKind::direct,
            {app},
        },
        ImpactedTranslationUnit{
            make_reference("src/lib/core.cpp", "build/lib", "src/lib/core.cpp|build/lib"),
            ImpactKind::heuristic,
            {core},
        },
    };
    result.affected_targets = {
        ImpactedTarget{app, TargetImpactClassification::direct},
        ImpactedTarget{core, TargetImpactClassification::heuristic},
    };

    const auto report = adapter.write_impact_report(result);

    CHECK(report.find("- Observation source: exact\n") != std::string::npos);
    CHECK(report.find("- Target metadata: loaded\n") != std::string::npos);
    CHECK(report.find("- Affected targets: 2\n") != std::string::npos);
    CHECK(report.find("## Directly Affected Translation Units\n"
                      "- src/app/main.cpp [directory: build/app] [targets: app]\n") !=
          std::string::npos);
    CHECK(report.find("## Directly Affected Targets\n"
                      "- app [type: EXECUTABLE]\n") != std::string::npos);
    CHECK(report.find("## Heuristically Affected Targets\n"
                      "- core\\_lib [type: STATIC\\_LIBRARY]\n") != std::string::npos);
    CHECK(report.find("## Heuristically Affected Targets") <
          report.find("## Diagnostics"));
}

TEST_CASE("markdown report adapter falls back for unnamed targets") {
    const MarkdownReportAdapter adapter;
    const TargetInfo keyed_target{"", "EXECUTABLE", "generated::EXECUTABLE"};
    const TargetInfo typed_target{"", "UTILITY_TARGET", ""};
    auto result = make_analysis_result();
    result.target_metadata = TargetMetadataStatus::loaded;
    result.translation_units[0].targets = {keyed_target, typed_target};

    const auto report = adapter.write_analysis_report(result, 1);

    CHECK(report.find("[targets: generated::EXECUTABLE, UTILITY\\_TARGET]") !=
          std::string::npos);
}

TEST_CASE("markdown report adapter escapes markdown-sensitive paths and messages") {
    const MarkdownReportAdapter adapter;
    auto result = make_analysis_result();
    result.compile_database_path = "#tmp/report_[1].json";
    result.translation_units[0].reference.source_path = "- [main]_file.cpp";
    result.translation_units[0].diagnostics = {
        {DiagnosticSeverity::warning, "1. [list] *item*"},
    };
    result.include_hotspots.clear();
    result.diagnostics = {
        {DiagnosticSeverity::note, "#heading style"},
    };

    const auto report = adapter.write_analysis_report(result, 1);

    CHECK(report.find("- Compile database: \\#tmp/report\\_\\[1\\].json\n") !=
          std::string::npos);
    CHECK(report.find("1. \\- \\[main\\]\\_file.cpp [directory: build/debug]\n") !=
          std::string::npos);
    CHECK(report.find("- warning: 1\\. \\[list\\] \\*item\\*\n") != std::string::npos);
    CHECK(report.find("- note: \\#heading style\n") != std::string::npos);
}

TEST_CASE("markdown report adapter renders direct impact classification without hits") {
    const MarkdownReportAdapter adapter;
    ImpactResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.compile_database_path = "compile_commands.json";
    result.changed_file = "src/app/main.cpp";
    result.heuristic = false;

    const auto report = adapter.write_impact_report(result);

    CHECK(report.find("- Impact classification: direct\n") != std::string::npos);
    CHECK(report.find("No affected translation units found.\n") != std::string::npos);
}

TEST_CASE("markdown report adapter preserves marker-dependent indentation for two-digit list entries") {
    const MarkdownReportAdapter adapter;
    AnalysisResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.compile_database_path = "tests/e2e/testdata/m3/report_top_limit/compile_commands.json";
    result.include_analysis_heuristic = true;
    result.translation_units = {
        make_ranked_translation_unit("src/nine.cpp", "build/nine", "src/nine.cpp|build/nine", 9,
                                     4, 1, 0),
        make_ranked_translation_unit("src/ten.cpp", "build/ten", "src/ten.cpp|build/ten", 10, 5,
                                     1, 1,
                                     {{DiagnosticSeverity::note, "rank 10 diagnostic"}}),
    };

    for (std::size_t index = 1; index <= 10; ++index) {
        result.include_hotspots.push_back(IncludeHotspot{
            .header_path = "include/hotspot_" + std::to_string(index) + ".h",
            .affected_translation_units =
                {
                    make_reference("src/hotspot.cpp", "build/hotspot",
                                   "src/hotspot.cpp|build/hotspot"),
                },
            .diagnostics =
                index == 10 ? std::vector<Diagnostic>{{DiagnosticSeverity::note,
                                                       "hotspot 10 diagnostic"}}
                            : std::vector<Diagnostic>{},
        });
    }

    const auto report = adapter.write_analysis_report(result, 10);

    CHECK(report.find("9. src/nine.cpp [directory: build/nine]\n"
                      "    Metrics: arg_count=4, include_path_count=1, define_count=0\n") !=
          std::string::npos);
    CHECK(report.find("10. src/ten.cpp [directory: build/ten]\n"
                      "     Metrics: arg_count=5, include_path_count=1, define_count=1\n"
                      "     Diagnostics:\n"
                      "     - note: rank 10 diagnostic\n") != std::string::npos);
    CHECK(report.find("9. Header: include/hotspot\\_9.h\n"
                      "    Affected translation units: 1\n"
                      "    Translation units:\n"
                      "    - src/hotspot.cpp [directory: build/hotspot]\n") !=
          std::string::npos);
    CHECK(report.find("10. Header: include/hotspot\\_10.h\n"
                      "     Affected translation units: 1\n"
                      "     Translation units:\n"
                      "     - src/hotspot.cpp [directory: build/hotspot]\n"
                      "     Diagnostics:\n"
                      "     - note: hotspot 10 diagnostic\n") != std::string::npos);
}

// ---- M6 AP 1.2 Tranche A.3: Target Graph / Target Hubs / Target Graph
// Reference Markdown sections. Status matrix per docs/planning/plan-M6-1-2.md
// "Markdown-Adapter (Status-Matrix Reporttyp x Status, identisch zu Console)"
// plus disambiguation, table-cell escaping and Compile-DB-only byte-stability
// regressions.

namespace m6_md {

using xray::hexagon::model::TargetDependency;
using xray::hexagon::model::TargetDependencyKind;
using xray::hexagon::model::TargetDependencyResolution;
using xray::hexagon::model::TargetGraphStatus;

TargetDependency resolved_edge(std::string from_uk, std::string from_dn,
                               std::string to_uk, std::string to_dn) {
    return TargetDependency{std::move(from_uk),       std::move(from_dn),
                            std::move(to_uk),         std::move(to_dn),
                            TargetDependencyKind::direct,
                            TargetDependencyResolution::resolved};
}

TargetDependency external_edge(std::string from_uk, std::string from_dn,
                               std::string to_uk, std::string to_dn) {
    return TargetDependency{std::move(from_uk),       std::move(from_dn),
                            std::move(to_uk),         std::move(to_dn),
                            TargetDependencyKind::direct,
                            TargetDependencyResolution::external};
}

}  // namespace m6_md

TEST_CASE(
    "markdown analyze v2: target_graph_status=not_loaded omits Direct Target "
    "Dependencies and Target Hubs sections") {
    // Compile-DB-only run: status defaults to not_loaded; the M5 byte-stable
    // contract requires neither section to surface in the report.
    AnalysisResult result = make_analysis_result();
    const MarkdownReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 10);
    CHECK(report.find("## Direct Target Dependencies") == std::string::npos);
    CHECK(report.find("## Target Hubs") == std::string::npos);
    // The hotspot section is followed directly by the diagnostics section.
    CHECK(report.find("\n## Diagnostics\n") != std::string::npos);
}

TEST_CASE(
    "markdown analyze v2: loaded status renders Direct Target Dependencies "
    "section with status line and edge table") {
    AnalysisResult result = make_analysis_result();
    result.target_graph_status = m6_md::TargetGraphStatus::loaded;
    result.target_graph.nodes = {
        TargetInfo{"app", "EXECUTABLE", "app::EXECUTABLE"},
        TargetInfo{"core", "STATIC_LIBRARY", "core::STATIC_LIBRARY"},
    };
    result.target_graph.edges = {
        m6_md::resolved_edge("app::EXECUTABLE", "app",
                              "core::STATIC_LIBRARY", "core"),
    };
    const MarkdownReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 10);
    CHECK(report.find("## Direct Target Dependencies\n\n"
                      "Status: `loaded`.\n\n"
                      "| From | To | Resolution | External Target |\n"
                      "|---|---|---|---|\n"
                      "| `app` | `core` | resolved |  |\n") != std::string::npos);
    CHECK(report.find("No direct target dependencies.") == std::string::npos);
}

TEST_CASE(
    "markdown analyze v2: partial status with external edge fills External "
    "Target column") {
    AnalysisResult result = make_analysis_result();
    result.target_graph_status = m6_md::TargetGraphStatus::partial;
    result.target_graph.edges = {
        m6_md::external_edge("app::EXECUTABLE", "app",
                              "<external>::ghost", "ghost"),
    };
    const MarkdownReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 10);
    CHECK(report.find("Status: `partial`.") != std::string::npos);
    // Resolved column carries plain "external"; External Target cell carries
    // the raw_id wrapped in backticks.
    CHECK(report.find("| `app` | `ghost` | external | `ghost` |") !=
          std::string::npos);
}

TEST_CASE(
    "markdown analyze v2: loaded status with empty edges renders leersatz "
    "paragraph instead of an empty table") {
    AnalysisResult result = make_analysis_result();
    result.target_graph_status = m6_md::TargetGraphStatus::loaded;
    // Nodes loaded but zero edges: leersatz paragraph, no table header.
    result.target_graph.nodes = {
        TargetInfo{"only_one", "EXECUTABLE", "only_one::EXECUTABLE"},
    };
    const MarkdownReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 10);
    CHECK(report.find("## Direct Target Dependencies\n\n"
                      "Status: `loaded`.\n\n"
                      "No direct target dependencies.\n") != std::string::npos);
    CHECK(report.find("| From | To | Resolution | External Target |") ==
          std::string::npos);
}

TEST_CASE(
    "markdown analyze v2: Target Hubs section emits 2-row table with "
    "thresholds in the Threshold column") {
    AnalysisResult result = make_analysis_result();
    result.target_graph_status = m6_md::TargetGraphStatus::loaded;
    result.target_hubs_in.push_back(
        {"core", "STATIC_LIBRARY", "core::STATIC_LIBRARY"});
    result.target_hubs_out.push_back({"app", "EXECUTABLE", "app::EXECUTABLE"});
    const MarkdownReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 10);
    CHECK(report.find("## Target Hubs\n\n"
                      "| Direction | Threshold | Targets |\n"
                      "|---|---|---|\n"
                      "| Inbound | 10 | `core` |\n"
                      "| Outbound | 10 | `app` |\n") != std::string::npos);
}

TEST_CASE(
    "markdown analyze v2: empty inbound and outbound hub vectors render "
    "leersaetze in the Targets cell") {
    AnalysisResult result = make_analysis_result();
    result.target_graph_status = m6_md::TargetGraphStatus::loaded;
    const MarkdownReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 10);
    CHECK(report.find("| Inbound | 10 | No incoming hubs. |") != std::string::npos);
    CHECK(report.find("| Outbound | 10 | No outgoing hubs. |") != std::string::npos);
}

TEST_CASE(
    "markdown analyze v2: hub list disambiguation suffix uses identity_key "
    "for collisions") {
    AnalysisResult result = make_analysis_result();
    result.target_graph_status = m6_md::TargetGraphStatus::loaded;
    result.target_hubs_in = {
        // Pre-sorted by (unique_key, display_name, type): foo::EXECUTABLE
        // precedes foo::STATIC_LIBRARY.
        TargetInfo{"foo", "EXECUTABLE", "foo::EXECUTABLE"},
        TargetInfo{"foo", "STATIC_LIBRARY", "foo::STATIC_LIBRARY"},
    };
    const MarkdownReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 10);
    CHECK(report.find(
              "| Inbound | 10 | "
              "`foo [key: foo::EXECUTABLE]`, `foo [key: foo::STATIC_LIBRARY]` |") !=
          std::string::npos);
}

TEST_CASE(
    "markdown analyze v2: mixed disambiguation across to-column keeps "
    "<external>::* keys verbatim in suffix") {
    // Plan mischfall: edge to internal `foo` and edge to external `foo`.
    // Both rows get [key: ...] suffix; markdown does not HTML-escape the
    // angle brackets, the backtick wrapper preserves them verbatim.
    AnalysisResult result = make_analysis_result();
    result.target_graph_status = m6_md::TargetGraphStatus::loaded;
    result.target_graph.edges = {
        m6_md::resolved_edge("app::EXECUTABLE", "app",
                              "foo::STATIC_LIBRARY", "foo"),
        m6_md::external_edge("app::EXECUTABLE", "app",
                              "<external>::foo", "foo"),
    };
    const MarkdownReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 10);
    CHECK(report.find("`foo [key: foo::STATIC_LIBRARY]`") != std::string::npos);
    CHECK(report.find("`foo [key: <external>::foo]`") != std::string::npos);
}

TEST_CASE(
    "markdown analyze v2: pipe in display name escapes to backslash-pipe in "
    "table cells") {
    AnalysisResult result = make_analysis_result();
    result.target_graph_status = m6_md::TargetGraphStatus::loaded;
    result.target_graph.edges = {
        m6_md::resolved_edge("lib|name::STATIC_LIBRARY", "lib|name",
                              "core::STATIC_LIBRARY", "core"),
    };
    const MarkdownReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 10);
    CHECK(report.find("`lib\\|name`") != std::string::npos);
    // Make sure no unescaped `|` lands in the cell content where the column
    // separator lives.
    CHECK(report.find("| `lib|name` |") == std::string::npos);
}

TEST_CASE(
    "markdown analyze v2: backtick in display name switches to double "
    "backtick wrapper with surrounding spaces") {
    AnalysisResult result = make_analysis_result();
    result.target_graph_status = m6_md::TargetGraphStatus::loaded;
    result.target_graph.edges = {
        m6_md::resolved_edge("a`b::STATIC_LIBRARY", "a`b",
                              "core::STATIC_LIBRARY", "core"),
    };
    const MarkdownReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 10);
    CHECK(report.find("`` a`b ``") != std::string::npos);
}

TEST_CASE(
    "markdown analyze v2: newline in display name normalizes to slash "
    "separator inside the table cell") {
    AnalysisResult result = make_analysis_result();
    result.target_graph_status = m6_md::TargetGraphStatus::loaded;
    result.target_graph.edges = {
        m6_md::resolved_edge("before\nafter::STATIC_LIBRARY", "before\nafter",
                              "core::STATIC_LIBRARY", "core"),
    };
    const MarkdownReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 10);
    CHECK(report.find("`before / after`") != std::string::npos);
}

TEST_CASE(
    "markdown analyze v2: CRLF, lone CR, tab and ASCII control bytes in "
    "display name normalize to slash separator and single space") {
    AnalysisResult result = make_analysis_result();
    result.target_graph_status = m6_md::TargetGraphStatus::loaded;
    result.target_graph.edges = {
        // \r\n collapses to a single " / "; lone \r turns into " / "; \t
        // and other control bytes (\x01) collapse to a single space. The
        // display name combines all four so a single edge row exercises
        // every branch of normalize_table_cell_whitespace.
        m6_md::resolved_edge(
            "weird::STATIC_LIBRARY",
            // String concat terminates the \x01 hex escape so the trailing
            // `e` is a literal character (and not a third hex digit).
            std::string("a\r\nb\rc\td\x01" "e"),
            "core::STATIC_LIBRARY", "core"),
    };
    const MarkdownReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 10);
    // Whitespace contract: \r\n → " / ", lone \r → " / ", \t → " ", \x01
    // → " ". Sequence "a\r\nb\rc\td\x01e" therefore becomes
    // "a / b / c d e". Single backtick wrapper applies (no backtick in
    // value).
    CHECK(report.find("`a / b / c d e`") != std::string::npos);
}

TEST_CASE(
    "markdown analyze v2: combined pipe, backtick and newline in display "
    "name compose escape, normalize, double-backtick wrapper") {
    AnalysisResult result = make_analysis_result();
    result.target_graph_status = m6_md::TargetGraphStatus::loaded;
    result.target_graph.edges = {
        m6_md::resolved_edge("weird::STATIC_LIBRARY", "a|b`c\nd",
                              "core::STATIC_LIBRARY", "core"),
    };
    const MarkdownReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 10);
    // Whitespace normalization first → "a|b`c / d", then double-backtick
    // wrapper with inner spaces, plus pipe escape → "`` a\|b`c / d ``".
    CHECK(report.find("`` a\\|b`c / d ``") != std::string::npos);
}

TEST_CASE(
    "markdown impact v2: target_graph_status=not_loaded omits Target Graph "
    "Reference section") {
    auto result = make_impact_result();
    const MarkdownReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    CHECK(report.find("## Target Graph Reference") == std::string::npos);
    // Impact never carries hub data even when status is loaded.
    CHECK(report.find("## Target Hubs") == std::string::npos);
}

TEST_CASE(
    "markdown impact v2: loaded status renders Target Graph Reference "
    "section between affected targets and diagnostics") {
    auto result = make_impact_result();
    result.target_graph_status = m6_md::TargetGraphStatus::loaded;
    result.target_graph.nodes = {
        TargetInfo{"app", "EXECUTABLE", "app::EXECUTABLE"},
        TargetInfo{"core", "STATIC_LIBRARY", "core::STATIC_LIBRARY"},
    };
    result.target_graph.edges = {
        m6_md::resolved_edge("app::EXECUTABLE", "app",
                              "core::STATIC_LIBRARY", "core"),
    };
    const MarkdownReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    CHECK(report.find("## Target Graph Reference\n\n"
                      "Status: `loaded`.\n\n"
                      "| From | To | Resolution | External Target |\n"
                      "|---|---|---|---|\n"
                      "| `app` | `core` | resolved |  |\n") != std::string::npos);
    // Section ordering: Target Graph Reference appears AFTER the heuristic
    // targets section and BEFORE the diagnostics section.
    const auto pos_h_targets = report.find("## Heuristically Affected Targets");
    const auto pos_graph_ref = report.find("## Target Graph Reference");
    const auto pos_diags = report.find("## Diagnostics");
    REQUIRE(pos_graph_ref != std::string::npos);
    REQUIRE(pos_diags != std::string::npos);
    if (pos_h_targets != std::string::npos) {
        CHECK(pos_h_targets < pos_graph_ref);
    }
    CHECK(pos_graph_ref < pos_diags);
}

TEST_CASE(
    "markdown impact v2: partial status with external edge fills External "
    "Target column") {
    auto result = make_impact_result();
    result.target_graph_status = m6_md::TargetGraphStatus::partial;
    result.target_graph.edges = {
        m6_md::external_edge("app::EXECUTABLE", "app",
                              "<external>::boost", "boost"),
    };
    const MarkdownReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    CHECK(report.find("Status: `partial`.") != std::string::npos);
    CHECK(report.find("| `app` | `boost` | external | `boost` |") !=
          std::string::npos);
}

// ---- AP M6-1.3 A.4: Prioritised Affected Targets section --------------

TEST_CASE("markdown impact v3: not_loaded keeps Prioritised Affected Targets section with depth header and skipped paragraph") {
    auto result = make_impact_result();
    result.impact_target_depth_requested = 2;
    result.impact_target_depth_effective = 0;
    const MarkdownReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    CHECK(report.find("## Prioritised Affected Targets\n\n"
                      "Requested depth: `2`. Effective depth: `0` (no graph).\n\n"
                      "Target graph not loaded; prioritisation skipped.\n") !=
          std::string::npos);
    // No table rendered when not_loaded.
    CHECK(report.find("| Priority class |") == std::string::npos);
}

TEST_CASE("markdown impact v3: loaded with empty prioritised list emits the leersatz, no table") {
    auto result = make_impact_result();
    result.target_graph_status = m6_md::TargetGraphStatus::loaded;
    result.impact_target_depth_requested = 2;
    result.impact_target_depth_effective = 1;
    const MarkdownReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    CHECK(report.find("## Prioritised Affected Targets\n\n"
                      "Requested depth: `2`. Effective depth: `1`.\n\n"
                      "No prioritised targets.\n") != std::string::npos);
    CHECK(report.find("| Priority class |") == std::string::npos);
}

TEST_CASE("markdown impact v3: populated prioritised list renders the six-column table with backticked target columns") {
    using xray::hexagon::model::PrioritizedImpactedTarget;
    using xray::hexagon::model::TargetEvidenceStrength;
    using xray::hexagon::model::TargetPriorityClass;
    auto result = make_impact_result();
    result.target_graph_status = m6_md::TargetGraphStatus::loaded;
    result.impact_target_depth_requested = 2;
    result.impact_target_depth_effective = 1;
    result.prioritized_affected_targets = {
        PrioritizedImpactedTarget{
            TargetInfo{"hub", "STATIC_LIBRARY", "hub::STATIC_LIBRARY"},
            TargetPriorityClass::direct, 0, TargetEvidenceStrength::direct},
        PrioritizedImpactedTarget{
            TargetInfo{"app", "EXECUTABLE", "app::EXECUTABLE"},
            TargetPriorityClass::direct_dependent, 1,
            TargetEvidenceStrength::heuristic},
    };
    const MarkdownReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    CHECK(report.find(
              "| Display name | Type | Priority class | Graph distance | Evidence strength | Unique key |\n"
              "|---|---|---|---|---|---|\n"
              "| `hub` | STATIC_LIBRARY | direct | 0 | direct | `hub::STATIC_LIBRARY` |\n"
              "| `app` | EXECUTABLE | direct_dependent | 1 | heuristic | `app::EXECUTABLE` |\n") !=
          std::string::npos);
}
