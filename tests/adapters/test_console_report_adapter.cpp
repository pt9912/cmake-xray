#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <utility>

#include "adapters/output/console_report_adapter.h"
#include "hexagon/model/analysis_result.h"
#include "hexagon/model/application_info.h"
#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/model/include_hotspot.h"
#include "hexagon/model/translation_unit.h"

namespace {

using xray::adapters::output::ConsoleReportAdapter;
using xray::hexagon::model::AnalysisResult;
using xray::hexagon::model::CompileDatabaseError;
using xray::hexagon::model::CompileDatabaseResult;
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
    result.include_analysis_heuristic = true;
    result.translation_units = {
        RankedTranslationUnit{
            .reference = make_reference("src/app/main.cpp", "build/debug",
                                        "src/app/main.cpp|build/debug"),
            .rank = 1,
            .arg_count = 8,
            .include_path_count = 2,
            .define_count = 1,
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
                    make_reference("src/app/main.cpp", "build/release",
                                   "src/app/main.cpp|build/release"),
                    make_reference("src/lib/core.cpp", "build/lib",
                                   "src/lib/core.cpp|build/lib"),
                },
            .diagnostics = {},
        },
    };
    return result;
}

std::size_t count_occurrences(std::string_view text, std::string_view needle) {
    std::size_t count = 0;
    std::size_t position = text.find(needle);
    while (position != std::string_view::npos) {
        ++count;
        position = text.find(needle, position + needle.size());
    }
    return count;
}

}  // namespace

TEST_CASE("console report adapter keeps full hotspot mapping for emitted hotspots") {
    const ConsoleReportAdapter adapter;

    const auto report = adapter.write_analysis_report(make_analysis_result(), 1);

    CHECK(report.find("include hotspots [heuristic]") != std::string::npos);
    CHECK(report.find("top 1 of 1 include hotspots") != std::string::npos);
    CHECK(report.find("src/app/main.cpp [directory: build/debug]") != std::string::npos);
    CHECK(report.find("src/app/main.cpp [directory: build/release]") != std::string::npos);
    CHECK(report.find("src/lib/core.cpp [directory: build/lib]") != std::string::npos);
}

TEST_CASE("console report adapter disambiguates duplicate impact observations") {
    const ConsoleReportAdapter adapter;
    ImpactResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.changed_file = "src/app/main.cpp";
    result.affected_translation_units = {
        ImpactedTranslationUnit{
            make_reference("src/app/main.cpp", "build/debug", "src/app/main.cpp|build/debug"),
            ImpactKind::direct,
        },
        ImpactedTranslationUnit{
            make_reference("src/app/main.cpp", "build/release",
                           "src/app/main.cpp|build/release"),
            ImpactKind::direct,
        },
    };

    const auto report = adapter.write_impact_report(result);

    CHECK(report.find("affected translation units: 2") != std::string::npos);
    CHECK(report.find("src/app/main.cpp [directory: build/debug] [direct]") !=
          std::string::npos);
    CHECK(report.find("src/app/main.cpp [directory: build/release] [direct]") !=
          std::string::npos);
}

TEST_CASE("console report adapter renders report-wide diagnostics after visible sections") {
    const ConsoleReportAdapter adapter;
    auto result = make_analysis_result();
    result.diagnostics = {
        {xray::hexagon::model::DiagnosticSeverity::note,
         "include-based results are heuristic; conditional or generated includes may be missing"},
    };

    const auto report = adapter.write_analysis_report(result, 1);

    CHECK(report.find("top 1 of 1 translation units") != std::string::npos);
    CHECK(report.find("note: include-based results are heuristic; conditional or generated includes may be missing") !=
          std::string::npos);
}

TEST_CASE("console report adapter renders file api target metadata for analyze") {
    const ConsoleReportAdapter adapter;
    auto result = make_analysis_result();
    const TargetInfo app{"app", "EXECUTABLE", "app::EXECUTABLE"};
    const TargetInfo app_release{"app-release", "EXECUTABLE", "app-release::EXECUTABLE"};
    const TargetInfo core{"core", "STATIC_LIBRARY", "core::STATIC_LIBRARY"};
    result.observation_source = ObservationSource::derived;
    result.target_metadata = TargetMetadataStatus::partial;
    result.translation_units[0].targets = {app};
    result.target_assignments = {
        {"src/app/main.cpp|build/debug", {app}},
        {"src/app/main.cpp|build/release", {app_release}},
        {"src/lib/core.cpp|build/lib", {core}},
    };

    const auto report = adapter.write_analysis_report(result, 1);

    CHECK(report.find("observation source: derived\n") != std::string::npos);
    CHECK(report.find("target metadata: partial\n") != std::string::npos);
    CHECK(report.find("translation units with target mapping: 1 of 1\n") !=
          std::string::npos);
    CHECK(report.find("src/app/main.cpp [directory: build/debug] [targets: app]\n") !=
          std::string::npos);
    CHECK(report.find("src/app/main.cpp [directory: build/release] [targets: app-release]\n") !=
          std::string::npos);
    CHECK(report.find("src/lib/core.cpp [directory: build/lib] [targets: core]\n") !=
          std::string::npos);
}

TEST_CASE("console report adapter renders target impact sections") {
    const ConsoleReportAdapter adapter;
    const TargetInfo app{"app", "EXECUTABLE", "app::EXECUTABLE"};
    const TargetInfo core{"core", "STATIC_LIBRARY", "core::STATIC_LIBRARY"};
    ImpactResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.changed_file = "include/common/config.h";
    result.heuristic = true;
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

    CHECK(report.find("observation source: exact\n") != std::string::npos);
    CHECK(report.find("target metadata: loaded\n") != std::string::npos);
    CHECK(report.find("affected targets: 2\n") != std::string::npos);
    CHECK(report.find("src/app/main.cpp [directory: build/app] [targets: app] [direct]\n") !=
          std::string::npos);
    CHECK(report.find("directly affected targets\n- app [type: EXECUTABLE]\n") !=
          std::string::npos);
    CHECK(report.find("heuristically affected targets\n- core [type: STATIC_LIBRARY]\n") !=
          std::string::npos);
}

TEST_CASE("console report adapter falls back for unnamed targets") {
    const ConsoleReportAdapter adapter;
    const TargetInfo keyed_target{"", "EXECUTABLE", "generated::EXECUTABLE"};
    const TargetInfo typed_target{"", "UTILITY", ""};
    auto result = make_analysis_result();
    result.target_metadata = TargetMetadataStatus::loaded;
    result.translation_units[0].targets = {keyed_target, typed_target};

    const auto report = adapter.write_analysis_report(result, 1);

    CHECK(report.find("[targets: generated::EXECUTABLE, UTILITY]") != std::string::npos);
}

TEST_CASE("console report adapter preserves inline diagnostics and handles empty hotspots") {
    const ConsoleReportAdapter adapter;
    auto result = make_analysis_result();
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
                    {xray::hexagon::model::DiagnosticSeverity::warning,
                     "could not resolve include \"generated/version.h\" from src/app/main.cpp"},
                },
        },
        RankedTranslationUnit{
            .reference = make_reference("src/app/alt.cpp", "build/release",
                                        "src/app/alt.cpp|build/release"),
            .rank = 2,
            .arg_count = 7,
            .include_path_count = 2,
            .define_count = 1,
            .diagnostics =
                {
                    {xray::hexagon::model::DiagnosticSeverity::warning,
                     "could not resolve include \"generated/version.h\" from src/app/main.cpp"},
                },
        },
    };
    result.translation_units[0].diagnostics = {
        {xray::hexagon::model::DiagnosticSeverity::warning,
         "could not resolve include \"generated/version.h\" from src/app/main.cpp"},
    };
    result.include_hotspots.clear();
    result.diagnostics = {
        {xray::hexagon::model::DiagnosticSeverity::note,
         "include-based results are heuristic; conditional or generated includes may be missing"},
    };

    const auto report = adapter.write_analysis_report(result, 2);

    CHECK(report.find("no include hotspots found") != std::string::npos);
    CHECK(count_occurrences(
              report,
              "could not resolve include \"generated/version.h\" from src/app/main.cpp") == 2);
    CHECK(count_occurrences(
              report,
              "include-based results are heuristic; conditional or generated includes may be missing") ==
          1);
}

// ---- M6 AP 1.2 Tranche A.3: Direct Target Dependencies / Target Hubs /
// Target Graph Reference console sections. Status matrix per
// docs/plan-M6-1-2.md "Console-Adapter (Status-Matrix Reporttyp x Status)"
// plus disambiguation and external-suffix order regressions.

namespace m6_console {

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

ImpactResult make_impact_result_for_graph() {
    ImpactResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.changed_file = "include/x.h";
    return result;
}

}  // namespace m6_console

TEST_CASE(
    "console analyze v2: target_graph_status=not_loaded omits Direct Target "
    "Dependencies and Target Hubs sections (Compile-DB-only byte-stable)") {
    AnalysisResult result = make_analysis_result();
    const ConsoleReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 5);
    CHECK(report.find("Direct Target Dependencies") == std::string::npos);
    CHECK(report.find("Target Hubs") == std::string::npos);
}

TEST_CASE(
    "console analyze v2: loaded status renders Direct Target Dependencies "
    "section with edge lines and Target Hubs section with thresholds") {
    AnalysisResult result = make_analysis_result();
    result.target_graph_status = m6_console::TargetGraphStatus::loaded;
    result.target_graph.nodes = {
        TargetInfo{"app", "EXECUTABLE", "app::EXECUTABLE"},
        TargetInfo{"core", "STATIC_LIBRARY", "core::STATIC_LIBRARY"},
    };
    result.target_graph.edges = {
        m6_console::resolved_edge("app::EXECUTABLE", "app",
                                   "core::STATIC_LIBRARY", "core"),
    };
    result.target_hubs_in.push_back(
        {"core", "STATIC_LIBRARY", "core::STATIC_LIBRARY"});
    result.target_hubs_out.push_back({"app", "EXECUTABLE", "app::EXECUTABLE"});
    const ConsoleReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 5);
    CHECK(report.find("Direct Target Dependencies (target_graph_status: loaded):\n"
                      "  app -> core\n") != std::string::npos);
    CHECK(report.find(
              "Target Hubs (in_threshold: 10, out_threshold: 10):\n"
              "  Inbound (>= 10 incoming): core\n"
              "  Outbound (>= 10 outgoing): app\n") != std::string::npos);
}

TEST_CASE(
    "console analyze v2: partial status with external edge appends "
    "[external] suffix to the to-element") {
    AnalysisResult result = make_analysis_result();
    result.target_graph_status = m6_console::TargetGraphStatus::partial;
    result.target_graph.edges = {
        m6_console::external_edge("app::EXECUTABLE", "app",
                                   "<external>::ghost", "ghost"),
    };
    const ConsoleReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 5);
    CHECK(report.find(
              "Direct Target Dependencies (target_graph_status: partial):\n"
              "  app -> ghost [external]\n") != std::string::npos);
}

TEST_CASE(
    "console analyze v2: loaded status with empty edges emits the leersatz "
    "instead of edge lines") {
    AnalysisResult result = make_analysis_result();
    result.target_graph_status = m6_console::TargetGraphStatus::loaded;
    const ConsoleReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 5);
    CHECK(report.find("Direct Target Dependencies (target_graph_status: loaded):\n"
                      "  No direct target dependencies.\n") != std::string::npos);
}

TEST_CASE(
    "console analyze v2: empty inbound and outbound hub vectors render "
    "leersaetze instead of comma-separated lists") {
    AnalysisResult result = make_analysis_result();
    result.target_graph_status = m6_console::TargetGraphStatus::loaded;
    const ConsoleReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 5);
    CHECK(report.find("  Inbound (>= 10 incoming): No incoming hubs.\n") !=
          std::string::npos);
    CHECK(report.find("  Outbound (>= 10 outgoing): No outgoing hubs.\n") !=
          std::string::npos);
}

TEST_CASE(
    "console analyze v2: hub-list disambiguation suffix uses identity_key "
    "for collisions") {
    AnalysisResult result = make_analysis_result();
    result.target_graph_status = m6_console::TargetGraphStatus::loaded;
    // Pre-sorted: foo::EXECUTABLE precedes foo::STATIC_LIBRARY.
    result.target_hubs_in = {
        TargetInfo{"foo", "EXECUTABLE", "foo::EXECUTABLE"},
        TargetInfo{"foo", "STATIC_LIBRARY", "foo::STATIC_LIBRARY"},
    };
    const ConsoleReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 5);
    CHECK(report.find(
              "  Inbound (>= 10 incoming): "
              "foo [key: foo::EXECUTABLE], foo [key: foo::STATIC_LIBRARY]\n") !=
          std::string::npos);
}

TEST_CASE(
    "console analyze v2: mixed disambiguation across to-column places "
    "[external] before [key:...] suffix") {
    // Plan-Mischfall: an edge to internal `foo` and an edge to external
    // `foo` in the same section both get [key: ...] suffix; the plan
    // pins the suffix order [external] [key: ...] for the external row,
    // and the [key: ...] suffix alone for the internal row.
    AnalysisResult result = make_analysis_result();
    result.target_graph_status = m6_console::TargetGraphStatus::loaded;
    result.target_graph.edges = {
        m6_console::resolved_edge("app::EXECUTABLE", "app",
                                   "foo::STATIC_LIBRARY", "foo"),
        m6_console::external_edge("app::EXECUTABLE", "app",
                                   "<external>::foo", "foo"),
    };
    const ConsoleReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 5);
    CHECK(report.find("  app -> foo [key: foo::STATIC_LIBRARY]\n") !=
          std::string::npos);
    CHECK(report.find("  app -> foo [external] [key: <external>::foo]\n") !=
          std::string::npos);
}

TEST_CASE(
    "console impact v2: target_graph_status=not_loaded omits Target Graph "
    "Reference and stays Hub-free") {
    auto result = m6_console::make_impact_result_for_graph();
    const ConsoleReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    CHECK(report.find("Target Graph Reference") == std::string::npos);
    // Impact never carries hubs even when graph is loaded.
    CHECK(report.find("Target Hubs") == std::string::npos);
}

TEST_CASE(
    "console impact v2: loaded status renders Target Graph Reference "
    "section with edge lines") {
    auto result = m6_console::make_impact_result_for_graph();
    result.target_graph_status = m6_console::TargetGraphStatus::loaded;
    result.target_graph.nodes = {
        TargetInfo{"app", "EXECUTABLE", "app::EXECUTABLE"},
        TargetInfo{"core", "STATIC_LIBRARY", "core::STATIC_LIBRARY"},
    };
    result.target_graph.edges = {
        m6_console::resolved_edge("app::EXECUTABLE", "app",
                                   "core::STATIC_LIBRARY", "core"),
    };
    const ConsoleReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    CHECK(report.find("Target Graph Reference (target_graph_status: loaded):\n"
                      "  app -> core\n") != std::string::npos);
    // No hub section in impact.
    CHECK(report.find("Target Hubs") == std::string::npos);
}

TEST_CASE(
    "console impact v2: partial status with external edge appends "
    "[external] suffix to the to-element") {
    auto result = m6_console::make_impact_result_for_graph();
    result.target_graph_status = m6_console::TargetGraphStatus::partial;
    result.target_graph.edges = {
        m6_console::external_edge("app::EXECUTABLE", "app",
                                   "<external>::boost", "boost"),
    };
    const ConsoleReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    CHECK(report.find("Target Graph Reference (target_graph_status: partial):\n"
                      "  app -> boost [external]\n") != std::string::npos);
}
