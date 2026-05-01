#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>

#include "adapters/output/dot_report_adapter.h"
#include "hexagon/model/analysis_result.h"
#include "hexagon/model/application_info.h"
#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/model/report_format_version.h"
#include "hexagon/model/report_inputs.h"
#include "hexagon/model/target_graph.h"
#include "hexagon/model/translation_unit.h"

namespace {

using xray::adapters::output::compute_analyze_budget;
using xray::adapters::output::compute_impact_budget;
using xray::adapters::output::DotReportAdapter;
using xray::hexagon::model::AnalysisResult;
using xray::hexagon::model::ChangedFileSource;
using xray::hexagon::model::CompileDatabaseError;
using xray::hexagon::model::CompileDatabaseResult;
using xray::hexagon::model::ImpactedTarget;
using xray::hexagon::model::ImpactedTranslationUnit;
using xray::hexagon::model::ImpactKind;
using xray::hexagon::model::ImpactResult;
using xray::hexagon::model::IncludeHotspot;
using xray::hexagon::model::RankedTranslationUnit;
using xray::hexagon::model::ReportInputs;
using xray::hexagon::model::ReportInputSource;
using xray::hexagon::model::TargetAssignment;
using xray::hexagon::model::TargetDependency;
using xray::hexagon::model::TargetDependencyKind;
using xray::hexagon::model::TargetDependencyResolution;
using xray::hexagon::model::TargetGraph;
using xray::hexagon::model::TargetGraphStatus;
using xray::hexagon::model::TargetImpactClassification;
using xray::hexagon::model::TargetInfo;
using xray::hexagon::model::TranslationUnitReference;
using xray::hexagon::model::kReportFormatVersion;

AnalysisResult make_minimal_analysis_result() {
    AnalysisResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    return result;
}

ImpactResult make_minimal_impact_result() {
    ImpactResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    return result;
}

TranslationUnitReference reference(std::string source_path, std::string directory,
                                   std::string unique_key) {
    auto source_path_key = unique_key;
    return {std::move(source_path), std::move(directory),
            std::move(source_path_key), std::move(unique_key)};
}

RankedTranslationUnit ranked_tu(std::string source_path, std::string directory,
                                std::string unique_key) {
    return RankedTranslationUnit{
        reference(std::move(source_path), std::move(directory), std::move(unique_key)),
        1, 0, 0, 0, {}, {},
    };
}

}  // namespace

TEST_CASE("compute_analyze_budget pins context_limit to min(top_limit, 5)") {
    CHECK(compute_analyze_budget(1).context_limit == 1);
    CHECK(compute_analyze_budget(3).context_limit == 3);
    CHECK(compute_analyze_budget(5).context_limit == 5);
    CHECK(compute_analyze_budget(10).context_limit == 5);
    CHECK(compute_analyze_budget(100).context_limit == 5);
}

TEST_CASE("compute_analyze_budget pins node_limit to max(25, 4*top + 10)") {
    CHECK(compute_analyze_budget(1).node_limit == 25);
    CHECK(compute_analyze_budget(3).node_limit == 25);
    CHECK(compute_analyze_budget(4).node_limit == 26);
    CHECK(compute_analyze_budget(10).node_limit == 50);
    CHECK(compute_analyze_budget(100).node_limit == 410);
}

TEST_CASE("compute_analyze_budget pins edge_limit to max(40, 6*top + 20)") {
    CHECK(compute_analyze_budget(1).edge_limit == 40);
    CHECK(compute_analyze_budget(3).edge_limit == 40);
    CHECK(compute_analyze_budget(4).edge_limit == 44);
    CHECK(compute_analyze_budget(10).edge_limit == 80);
    CHECK(compute_analyze_budget(100).edge_limit == 620);
}

TEST_CASE("compute_analyze_budget clamps adversarially large top_limit to a sane bound") {
    // The clamp prevents 4*top_limit / 6*top_limit from wrapping a 64-bit
    // std::size_t. Anything above the documented clamp produces the same
    // budget as the clamp value itself.
    const auto at_clamp = compute_analyze_budget(100000);
    const auto over_clamp = compute_analyze_budget(200000);
    CHECK(at_clamp.context_limit == over_clamp.context_limit);
    CHECK(at_clamp.node_limit == over_clamp.node_limit);
    CHECK(at_clamp.edge_limit == over_clamp.edge_limit);
}

TEST_CASE("compute_impact_budget pins fixed M5 budgets and zero context_limit") {
    const auto budget = compute_impact_budget();
    CHECK(budget.node_limit == 100);
    CHECK(budget.edge_limit == 200);
    CHECK(budget.context_limit == 0);
}

TEST_CASE("DOT analyze report emits valid digraph header with stable graph attributes") {
    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(make_minimal_analysis_result(), 3);
    REQUIRE(!report.empty());

    CHECK(report.find("digraph cmake_xray_analysis {") != std::string::npos);
    CHECK(report.find("xray_report_type=\"analyze\";") != std::string::npos);
    CHECK(report.find("format_version=" + std::to_string(kReportFormatVersion) + ";") !=
          std::string::npos);
    CHECK(report.find("graph_node_limit=25;") != std::string::npos);
    CHECK(report.find("graph_edge_limit=40;") != std::string::npos);
    CHECK(report.find("graph_truncated=false;") != std::string::npos);
    CHECK(report.back() == '\n');
    CHECK(report.find_last_of('}') != std::string::npos);
}

TEST_CASE("DOT impact report emits valid digraph header with stable graph attributes") {
    const DotReportAdapter adapter;
    const auto report = adapter.write_impact_report(make_minimal_impact_result());
    REQUIRE(!report.empty());

    CHECK(report.find("digraph cmake_xray_impact {") != std::string::npos);
    CHECK(report.find("xray_report_type=\"impact\";") != std::string::npos);
    CHECK(report.find("format_version=" + std::to_string(kReportFormatVersion) + ";") !=
          std::string::npos);
    CHECK(report.find("graph_node_limit=100;") != std::string::npos);
    CHECK(report.find("graph_edge_limit=200;") != std::string::npos);
    CHECK(report.find("graph_truncated=false;") != std::string::npos);
}

TEST_CASE("DOT analyze emits a translation_unit node with the documented attribute order") {
    AnalysisResult result = make_minimal_analysis_result();
    result.translation_units = {ranked_tu("src/app/main.cpp", "build/debug",
                                          "src/app/main.cpp|build/debug")};

    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);

    CHECK(report.find("tu_1 [") != std::string::npos);
    CHECK(report.find("kind=\"translation_unit\"") != std::string::npos);
    CHECK(report.find("label=\"main.cpp\"") != std::string::npos);
    CHECK(report.find("path=\"src/app/main.cpp\"") != std::string::npos);
    CHECK(report.find("directory=\"build/debug\"") != std::string::npos);
    CHECK(report.find("unique_key=\"src/app/main.cpp|build/debug\"") != std::string::npos);

    const auto pos_kind = report.find("kind=\"translation_unit\"");
    const auto pos_label = report.find("label=", pos_kind);
    const auto pos_path = report.find("path=\"src/app", pos_label);
    const auto pos_directory = report.find("directory=", pos_path);
    const auto pos_unique_key = report.find("unique_key=", pos_directory);
    REQUIRE(pos_kind != std::string::npos);
    REQUIRE(pos_label != std::string::npos);
    REQUIRE(pos_path != std::string::npos);
    REQUIRE(pos_directory != std::string::npos);
    REQUIRE(pos_unique_key != std::string::npos);
    CHECK(pos_kind < pos_label);
    CHECK(pos_label < pos_path);
    CHECK(pos_path < pos_directory);
    CHECK(pos_directory < pos_unique_key);
}

TEST_CASE("DOT escape preserves backslash, quote, newline, tab and control bytes") {
    AnalysisResult result = make_minimal_analysis_result();
    // Concatenated string segments terminate each \x escape at the closing
    // quote so adjacent hex-looking characters do not extend the escape.
    const std::string source_path =
        std::string{"src\\with\"quote\nand\ttab\x01"} + "end.cpp";
    result.translation_units = {ranked_tu(source_path, "build", "key|build")};

    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 1);

    // Each special char ends up as a documented escape sequence in the
    // rendered DOT path attribute.
    CHECK(report.find("path=\"src\\\\with\\\"quote\\nand\\ttab\\x01end.cpp\"") !=
          std::string::npos);
    // No raw control bytes survive into the output.
    CHECK(report.find('\x01') == std::string::npos);
    CHECK(report.find('\t') == std::string::npos);
}

TEST_CASE("DOT label uses the basename and middle-truncates strings longer than 48 chars") {
    AnalysisResult result = make_minimal_analysis_result();
    const std::string long_name(60, 'a');
    result.translation_units = {ranked_tu("src/" + long_name + ".cpp", "build", "key|build")};

    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 1);

    // 22 head chars + "..." + 23 tail chars = 48. Basename is "aaa...aaa.cpp".
    const std::string basename = long_name + ".cpp";
    const std::string expected_label = basename.substr(0, 22) + "..." +
                                       basename.substr(basename.size() - 23);
    REQUIRE(expected_label.size() == 48);
    CHECK(report.find("label=\"" + expected_label + "\"") != std::string::npos);
    // The full path attribute keeps the unmodified string.
    CHECK(report.find("path=\"src/" + long_name + ".cpp\"") != std::string::npos);
}

TEST_CASE("DOT label leaves short basenames untruncated and intact") {
    AnalysisResult result = make_minimal_analysis_result();
    result.translation_units = {ranked_tu("util.cpp", "build", "util|build")};

    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 1);

    CHECK(report.find("label=\"util.cpp\"") != std::string::npos);
    CHECK(report.find("path=\"util.cpp\"") != std::string::npos);
}

TEST_CASE("DOT escape covers carriage return alongside other control chars") {
    AnalysisResult result = make_minimal_analysis_result();
    result.translation_units = {ranked_tu("src\rcr.cpp", "build", "cr|build")};

    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 1);
    CHECK(report.find("path=\"src\\rcr.cpp\"") != std::string::npos);
    CHECK(report.find('\r') == std::string::npos);
}

TEST_CASE("DOT label falls back to the full path when it ends in a separator") {
    AnalysisResult result = make_minimal_analysis_result();
    result.translation_units = {ranked_tu("src/dir/", "build", "dir|build")};

    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 1);
    // Trailing separator: label_from_path falls back to truncate_label on the
    // whole path because there is no basename component after the slash.
    CHECK(report.find("label=\"src/dir/\"") != std::string::npos);
    CHECK(report.find("path=\"src/dir/\"") != std::string::npos);
}

TEST_CASE("DOT label honors backslash path separators") {
    AnalysisResult result = make_minimal_analysis_result();
    result.translation_units = {ranked_tu("src\\windows\\main.cpp", "build",
                                          "win|build")};

    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 1);

    CHECK(report.find("label=\"main.cpp\"") != std::string::npos);
}

TEST_CASE("DOT impact report emits a changed_file node when ReportInputs.changed_file is set") {
    ImpactResult result = make_minimal_impact_result();
    result.inputs = ReportInputs{
        std::nullopt, ReportInputSource::not_provided,
        std::nullopt, std::nullopt, ReportInputSource::not_provided,
        std::optional<std::string>{"src/lib/core.cpp"},
        std::optional<ChangedFileSource>{ChangedFileSource::compile_database_directory},
    };

    const DotReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);

    CHECK(report.find("changed_file [") != std::string::npos);
    CHECK(report.find("kind=\"changed_file\"") != std::string::npos);
    CHECK(report.find("label=\"core.cpp\"") != std::string::npos);
    CHECK(report.find("path=\"src/lib/core.cpp\"") != std::string::npos);
}

TEST_CASE("DOT impact report omits the changed_file node when ReportInputs.changed_file is unset") {
    const DotReportAdapter adapter;
    const auto report = adapter.write_impact_report(make_minimal_impact_result());
    CHECK(report.find("changed_file [") == std::string::npos);
    CHECK(report.find("kind=\"changed_file\"") == std::string::npos);
}

TEST_CASE("DOT analyze emits primary translation unit with rank attribute and edges to targets") {
    AnalysisResult result = make_minimal_analysis_result();
    auto tu = ranked_tu("src/app.cpp", "build", "src/app.cpp|build");
    tu.rank = 7;
    tu.targets = {TargetInfo{"app", "EXECUTABLE", "app::EXE"}};
    result.translation_units = {tu};

    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 5);

    CHECK(report.find("rank=7") != std::string::npos);
    CHECK(report.find("kind=\"target\"") != std::string::npos);
    CHECK(report.find("name=\"app\"") != std::string::npos);
    CHECK(report.find("type=\"EXECUTABLE\"") != std::string::npos);
    CHECK(report.find("unique_key=\"app::EXE\"") != std::string::npos);
    // Edge from primary TU to its target.
    CHECK(report.find("tu_1 -> target_1 [kind=\"tu_target\"") != std::string::npos);
    // Analyze edges carry no style attribute.
    CHECK(report.find("style=") == std::string::npos);
}

TEST_CASE("DOT analyze emits include_hotspot with context counts and tu_include_hotspot edge") {
    AnalysisResult result = make_minimal_analysis_result();
    auto tu = ranked_tu("src/app.cpp", "build", "src/app.cpp|build");
    tu.rank = 1;
    result.translation_units = {tu};
    result.include_hotspots = {
        IncludeHotspot{"include/header.h",
                       {reference("src/app.cpp", "build", "src/app.cpp|build")},
                       {}},
    };

    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 5);

    CHECK(report.find("kind=\"include_hotspot\"") != std::string::npos);
    CHECK(report.find("path=\"include/header.h\"") != std::string::npos);
    CHECK(report.find("context_total_count=1") != std::string::npos);
    CHECK(report.find("context_returned_count=1") != std::string::npos);
    CHECK(report.find("context_truncated=false") != std::string::npos);
    CHECK(report.find("tu_1 -> hotspot_1 [kind=\"tu_include_hotspot\"") != std::string::npos);
}

TEST_CASE("DOT analyze marks pure context translation units as context_only") {
    AnalysisResult result = make_minimal_analysis_result();
    auto primary = ranked_tu("src/app.cpp", "build", "src/app.cpp|build");
    primary.rank = 1;
    result.translation_units = {primary};
    // Hotspot affects an additional TU that is NOT a primary ranking node.
    result.include_hotspots = {
        IncludeHotspot{"include/header.h",
                       {reference("src/extra.cpp", "build/extra",
                                  "src/extra.cpp|build/extra")},
                       {}},
    };

    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 5);
    // The extra TU is added as a context_only node and connected to the hotspot.
    CHECK(report.find("context_tu_1") != std::string::npos);
    CHECK(report.find("context_only=true") != std::string::npos);
    CHECK(report.find("path=\"src/extra.cpp\"") != std::string::npos);
}

TEST_CASE("DOT analyze sets graph_truncated when context_limit drops candidate context TUs") {
    AnalysisResult result = make_minimal_analysis_result();
    auto primary = ranked_tu("src/app.cpp", "build", "src/app.cpp|build");
    primary.rank = 1;
    result.translation_units = {primary};
    // 7 affected TUs > context_limit of min(top_limit=3, 5) = 3.
    std::vector<TranslationUnitReference> affected;
    for (int i = 0; i < 7; ++i) {
        const auto label = "src/affected" + std::to_string(i) + ".cpp";
        affected.push_back(reference(label, "build", label + "|build"));
    }
    result.include_hotspots = {
        IncludeHotspot{"include/header.h", affected, {}},
    };

    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    CHECK(report.find("graph_truncated=true") != std::string::npos);
    CHECK(report.find("context_total_count=7") != std::string::npos);
    CHECK(report.find("context_returned_count=3") != std::string::npos);
    CHECK(report.find("context_truncated=true") != std::string::npos);
}

TEST_CASE("DOT analyze enforces edge_limit and removes orphan context translation units") {
    AnalysisResult result = make_minimal_analysis_result();
    auto primary = ranked_tu("src/app.cpp", "build", "src/app.cpp|build");
    primary.rank = 1;
    result.translation_units = {primary};
    // Many affected TUs across many hotspots so that edge_limit can hit
    // before all context candidates produce edges.
    std::vector<TranslationUnitReference> affected_a;
    for (int i = 0; i < 5; ++i) {
        const auto label = "src/orphan_a" + std::to_string(i) + ".cpp";
        affected_a.push_back(reference(label, "build", label + "|build"));
    }
    result.include_hotspots = {
        IncludeHotspot{"include/a.h", affected_a, {}},
    };

    const DotReportAdapter adapter;
    // top_limit=1 → context_limit=1, edge_limit=40 (more than needed). We
    // simply check that the orphan-removal pipeline runs and that connected
    // context TUs are still present while non-listed ones do not appear.
    const auto report = adapter.write_analysis_report(result, 1);
    CHECK(report.find("context_tu_1") != std::string::npos);
    CHECK(report.find("path=\"src/orphan_a0.cpp\"") != std::string::npos);
    // Only one hotspot context (context_limit=1) was emitted; the others
    // never became candidate context nodes.
    CHECK(report.find("context_tu_2") == std::string::npos);
}

TEST_CASE("DOT impact emits direct and heuristic translation unit nodes with style edges") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = "include/common.h";
    result.affected_translation_units = {
        ImpactedTranslationUnit{
            reference("src/app.cpp", "build/app", "src/app.cpp|build/app"),
            ImpactKind::direct,
            {},
        },
        ImpactedTranslationUnit{
            reference("src/lib.cpp", "build/lib", "src/lib.cpp|build/lib"),
            ImpactKind::heuristic,
            {},
        },
    };

    const DotReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);

    CHECK(report.find("changed_file [") != std::string::npos);
    CHECK(report.find("direct_tu_1") != std::string::npos);
    CHECK(report.find("impact=\"direct\"") != std::string::npos);
    CHECK(report.find("heuristic_tu_1") != std::string::npos);
    CHECK(report.find("impact=\"heuristic\"") != std::string::npos);
    // Edge styles match the contract.
    CHECK(report.find("changed_file -> direct_tu_1 [kind=\"changed_file_direct_tu\", "
                       "style=\"solid\"") != std::string::npos);
    CHECK(report.find("changed_file -> heuristic_tu_1 [kind=\"changed_file_heuristic_tu\", "
                       "style=\"dashed\"") != std::string::npos);
}

TEST_CASE("DOT impact target picks impact=direct when target is both direct and heuristic") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = "include/common.h";
    // Two ImpactedTarget entries for the same target unique_key, one direct.
    result.affected_targets = {
        ImpactedTarget{TargetInfo{"app", "EXECUTABLE", "app::EXE"},
                       TargetImpactClassification::heuristic},
        ImpactedTarget{TargetInfo{"app", "EXECUTABLE", "app::EXE"},
                       TargetImpactClassification::direct},
    };

    const DotReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);

    CHECK(report.find("kind=\"target\"") != std::string::npos);
    // Direct wins on dual membership.
    CHECK(report.find("impact=\"direct\"") != std::string::npos);
    CHECK(report.find("impact=\"heuristic\"") == std::string::npos);
}

TEST_CASE("DOT impact emits direct_tu_target and heuristic_tu_target edges with correct styles") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = "include/common.h";
    result.affected_translation_units = {
        ImpactedTranslationUnit{
            reference("src/app.cpp", "build/app", "src/app.cpp|build/app"),
            ImpactKind::direct,
            {TargetInfo{"app", "EXECUTABLE", "app::EXE"}},
        },
        ImpactedTranslationUnit{
            reference("src/lib.cpp", "build/lib", "src/lib.cpp|build/lib"),
            ImpactKind::heuristic,
            {TargetInfo{"core", "STATIC_LIBRARY", "core::STATIC"}},
        },
    };
    result.affected_targets = {
        ImpactedTarget{TargetInfo{"app", "EXECUTABLE", "app::EXE"},
                       TargetImpactClassification::direct},
        ImpactedTarget{TargetInfo{"core", "STATIC_LIBRARY", "core::STATIC"},
                       TargetImpactClassification::heuristic},
    };

    const DotReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);

    CHECK(report.find("direct_tu_1 -> target_1 [kind=\"direct_tu_target\", style=\"solid\"") !=
          std::string::npos);
    CHECK(report.find(
              "heuristic_tu_1 -> target_2 [kind=\"heuristic_tu_target\", style=\"dashed\"") !=
          std::string::npos);
}

TEST_CASE("DOT analyze ranking tie-breaks identical rank by reference fields") {
    AnalysisResult result = make_minimal_analysis_result();
    auto a = ranked_tu("src/zsame.cpp", "build/alpha", "src/zsame.cpp|build/alpha");
    a.rank = 1;
    auto b = ranked_tu("src/zsame.cpp", "build/zeta", "src/zsame.cpp|build/zeta");
    b.rank = 1;
    auto c = ranked_tu("src/zsame.cpp", "build/alpha", "src/zsame.cpp|build/alpha-other");
    c.rank = 1;
    result.translation_units = {b, c, a};

    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 5);
    // After tie-break: a (build/alpha, |alpha) < c (build/alpha, |alpha-other) < b (build/zeta).
    const auto pos_a = report.find("unique_key=\"src/zsame.cpp|build/alpha\"");
    const auto pos_c = report.find("unique_key=\"src/zsame.cpp|build/alpha-other\"");
    const auto pos_b = report.find("unique_key=\"src/zsame.cpp|build/zeta\"");
    REQUIRE(pos_a != std::string::npos);
    REQUIRE(pos_c != std::string::npos);
    REQUIRE(pos_b != std::string::npos);
    CHECK(pos_a < pos_c);
    CHECK(pos_c < pos_b);
}

TEST_CASE("DOT analyze target list tie-breaks display_name by type then unique_key") {
    AnalysisResult result = make_minimal_analysis_result();
    auto tu = ranked_tu("src/app.cpp", "build", "src/app.cpp|build");
    tu.rank = 1;
    tu.targets = {
        TargetInfo{"shared", "STATIC_LIBRARY", "shared::STATIC_b"},
        TargetInfo{"shared", "EXECUTABLE", "shared::EXEC"},
        TargetInfo{"shared", "STATIC_LIBRARY", "shared::STATIC_a"},
    };
    result.translation_units = {tu};

    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 5);
    // EXECUTABLE < STATIC_LIBRARY by type; within STATIC_LIBRARY, STATIC_a < STATIC_b.
    const auto pos_exec = report.find("unique_key=\"shared::EXEC\"");
    const auto pos_static_a = report.find("unique_key=\"shared::STATIC_a\"");
    const auto pos_static_b = report.find("unique_key=\"shared::STATIC_b\"");
    REQUIRE(pos_exec != std::string::npos);
    REQUIRE(pos_static_a != std::string::npos);
    REQUIRE(pos_static_b != std::string::npos);
    CHECK(pos_exec < pos_static_a);
    CHECK(pos_static_a < pos_static_b);
}

TEST_CASE("DOT analyze sets graph_truncated when node_limit is exhausted") {
    AnalysisResult result = make_minimal_analysis_result();
    // top_limit=1 -> node_limit=25. Generate 30 TUs to exceed.
    for (int i = 0; i < 30; ++i) {
        const auto path = "src/file" + std::to_string(i) + ".cpp";
        auto tu = ranked_tu(path, "build", path + "|build");
        tu.rank = static_cast<std::size_t>(i + 1);
        result.translation_units.push_back(tu);
    }
    const DotReportAdapter adapter;
    // Pass top_limit=30 but the formula yields node_limit = max(25, 4*30+10) = 130.
    // Force the limit by using a smaller top_limit; but the limit is at least 25.
    // To actually hit the budget, we need TUs > 25 with top_limit small.
    // Instead, simulate by piling into a single hotspot's affected list so
    // context allocation walks past node_limit.
    auto base = ranked_tu("src/seed.cpp", "build", "src/seed.cpp|build");
    base.rank = 1;
    AnalysisResult overflow_result = make_minimal_analysis_result();
    overflow_result.translation_units = {base};
    std::vector<TranslationUnitReference> affected;
    for (int i = 0; i < 60; ++i) {
        const auto path = "src/affected" + std::to_string(i) + ".cpp";
        affected.push_back(reference(path, "build", path + "|build"));
    }
    overflow_result.include_hotspots = {
        IncludeHotspot{"include/header.h", affected, {}},
    };
    const auto report = adapter.write_analysis_report(overflow_result, 1);
    CHECK(report.find("graph_truncated=true") != std::string::npos);
}

TEST_CASE("DOT analyze sets graph_truncated when context node_limit is hit during expansion") {
    // top_limit=5 -> context_limit=5, node_limit=30, edge_limit=50.
    // 5 primary TUs + 5 hotspots = 10 nodes used. Each hotspot contributes
    // 5 unique context TUs (5*5 = 25 candidates), exceeding the remaining
    // node_limit of 20. At least 5 context candidates are dropped at the
    // node_count >= budget.node_limit guard in build_analyze_context_nodes.
    AnalysisResult result = make_minimal_analysis_result();
    for (int i = 0; i < 5; ++i) {
        const auto path = "src/seed" + std::to_string(i) + ".cpp";
        auto tu = ranked_tu(path, "build", path + "|build");
        tu.rank = static_cast<std::size_t>(i + 1);
        result.translation_units.push_back(tu);
    }
    for (int h = 0; h < 5; ++h) {
        const auto header = "include/h" + std::to_string(h) + ".h";
        std::vector<TranslationUnitReference> affected;
        for (int c = 0; c < 5; ++c) {
            const auto tu_label = "src/ctx_h" + std::to_string(h) +
                                   "_t" + std::to_string(c) + ".cpp";
            affected.push_back(reference(tu_label, "build", tu_label + "|build"));
        }
        result.include_hotspots.push_back(IncludeHotspot{header, affected, {}});
    }
    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 5);
    CHECK(report.find("graph_truncated=true") != std::string::npos);
}

TEST_CASE("DOT analyze sets graph_truncated when target candidates exhaust node_limit") {
    AnalysisResult result = make_minimal_analysis_result();
    auto seed = ranked_tu("src/seed.cpp", "build", "src/seed.cpp|build");
    seed.rank = 1;
    // 30 unique targets attached to the single primary TU. With top_limit=1,
    // node_limit=25; seed + 0 hotspots + 24 targets fit before the next
    // try_add_node sees node_count == node_limit and sets truncated=true.
    for (int i = 0; i < 30; ++i) {
        const auto name = "t" + std::to_string(i);
        seed.targets.push_back(TargetInfo{name, "EXEC", name + "::EXEC"});
    }
    result.translation_units = {seed};
    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 1);
    CHECK(report.find("graph_truncated=true") != std::string::npos);
}

TEST_CASE("DOT analyze edge_limit truncates the combined context+tu_target edge candidate stream") {
    // top_limit=20 -> node_limit=90, edge_limit=140, context_limit=5.
    // 20 primary TUs with shared targets fit in node_limit (20 primary + 20
    // hotspots + 10 unique targets = 50 nodes). 20 hotspots each list all 20
    // primary refs, so context_pairs has 20 hotspots * 5 (context_limit) =
    // 100 candidates -> 100 tu_include_hotspot edges. Plus 20 * 5 = 100
    // tu_target edges. The combined 200 candidates exceed edge_limit=140;
    // try_add_analyze_edge returns false at edge 141 and aborts the rest.
    AnalysisResult result = make_minimal_analysis_result();
    std::vector<TranslationUnitReference> all_primary_refs;
    for (int p = 0; p < 20; ++p) {
        const auto path = "src/p" + std::to_string(p) + ".cpp";
        const auto unique_key = path + "|build";
        all_primary_refs.push_back(reference(path, "build", unique_key));
        auto tu = ranked_tu(path, "build", unique_key);
        tu.rank = static_cast<std::size_t>(p + 1);
        const int target_group = p < 10 ? 0 : 1;
        for (int t = 0; t < 5; ++t) {
            const auto name = "tg" + std::to_string(target_group) + "_t" + std::to_string(t);
            tu.targets.push_back(TargetInfo{name, "EXEC", name + "::EXEC"});
        }
        result.translation_units.push_back(tu);
    }
    for (int h = 0; h < 20; ++h) {
        const auto header = "include/h" + std::to_string(h) + ".h";
        result.include_hotspots.push_back(IncludeHotspot{header, all_primary_refs, {}});
    }
    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 20);
    CHECK(report.find("graph_truncated=true") != std::string::npos);
}

TEST_CASE("DOT analyze edge_limit drops surplus tu_target edges and removes orphan context") {
    AnalysisResult result = make_minimal_analysis_result();
    // 5 primary TUs each with 12 unique targets. Ranking edge candidates =
    // 5 * 12 = 60. top_limit=5 -> edge_limit=50. After 50 edges the helper
    // returns false and truncated=true.
    for (int p = 0; p < 5; ++p) {
        auto tu = ranked_tu("src/p" + std::to_string(p) + ".cpp", "build",
                             "src/p" + std::to_string(p) + ".cpp|build");
        tu.rank = static_cast<std::size_t>(p + 1);
        for (int t = 0; t < 12; ++t) {
            const auto name = "p" + std::to_string(p) + "_t" + std::to_string(t);
            tu.targets.push_back(TargetInfo{name, "EXEC", name + "::EXEC"});
        }
        result.translation_units.push_back(tu);
    }
    // One hotspot whose only context TU is NOT in the primary set, so it is
    // added as a context_only node. If its context edge is dropped by
    // edge_limit, the orphan-removal pass must erase the node (line 424).
    result.include_hotspots = {
        IncludeHotspot{"include/header.h",
                       {reference("src/orphan.cpp", "build", "src/orphan.cpp|build")},
                       {}},
    };
    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 5);
    CHECK(report.find("graph_truncated=true") != std::string::npos);
    // The orphan context TU was added as a node candidate but its edge was
    // dropped by edge_limit; orphan removal should erase the node.
    CHECK(report.find("path=\"src/orphan.cpp\"") == std::string::npos);
}

TEST_CASE("DOT impact node_limit truncates surplus translation unit candidates") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = "include/header.h";
    // node_limit=100. 1 changed_file + 100 direct TUs would exceed it. The
    // try_add_impact_node helper aborts the loop and sets truncated=true.
    for (int i = 0; i < 110; ++i) {
        const auto path = "src/d" + std::to_string(i) + ".cpp";
        result.affected_translation_units.push_back(ImpactedTranslationUnit{
            reference(path, "build", path + "|build"),
            ImpactKind::direct,
            {},
        });
    }
    const DotReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    CHECK(report.find("graph_truncated=true") != std::string::npos);
}

TEST_CASE("DOT impact edge_limit truncates surplus tu_target edges") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = "include/header.h";
    // Direct TUs each with many targets so direct_tu_target edges exhaust
    // edge_limit=200 during phase 3.
    for (int t = 0; t < 30; ++t) {
        const auto name = "tg" + std::to_string(t);
        result.affected_targets.push_back(ImpactedTarget{
            TargetInfo{name, "EXEC", name + "::EXEC"},
            TargetImpactClassification::direct,
        });
    }
    for (int u = 0; u < 10; ++u) {
        const auto path = "src/d" + std::to_string(u) + ".cpp";
        ImpactedTranslationUnit tu{
            reference(path, "build", path + "|build"),
            ImpactKind::direct,
            {},
        };
        for (int t = 0; t < 30; ++t) {
            const auto name = "tg" + std::to_string(t);
            tu.targets.push_back(TargetInfo{name, "EXEC", name + "::EXEC"});
        }
        result.affected_translation_units.push_back(tu);
    }
    // 10 changed_file_direct_tu + 10*30 direct_tu_target = 310 candidate edges,
    // edge_limit=200, so the helper sets truncated=true.
    const DotReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    CHECK(report.find("graph_truncated=true") != std::string::npos);
}

TEST_CASE("DOT impact omits the changed_file node when changed_file is unset") {
    const DotReportAdapter adapter;
    const auto report = adapter.write_impact_report(make_minimal_impact_result());
    CHECK(report.find("changed_file [") == std::string::npos);
    CHECK(report.find("changed_file -> ") == std::string::npos);
}

TEST_CASE("DOT impact emits no edges when changed_file is missing even with affected TUs") {
    // The contract makes changed_file the source of every impact edge kind.
    // When it is absent, build_impact_edges must emit no edges at all -
    // even if the result still carries affected_translation_units. Nodes for
    // those TUs may appear (they are still meaningful data) but no edge of
    // any kind is in the rendered graph.
    auto result = make_minimal_impact_result();
    result.affected_translation_units = {
        ImpactedTranslationUnit{
            reference("src/d.cpp", "build", "src/d.cpp|build"),
            ImpactKind::direct,
            {TargetInfo{"app", "EXECUTABLE", "app::EXE"}},
        },
        ImpactedTranslationUnit{
            reference("src/h.cpp", "build", "src/h.cpp|build"),
            ImpactKind::heuristic,
            {},
        },
    };
    result.affected_targets = {
        ImpactedTarget{TargetInfo{"app", "EXECUTABLE", "app::EXE"},
                       TargetImpactClassification::direct},
    };

    const DotReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    CHECK(report.find("changed_file [") == std::string::npos);
    CHECK(report.find(" -> ") == std::string::npos);
    // Without changed_file there are no edges of any kind, regardless of
    // edge style or source/target label.
    CHECK(report.find("kind=\"changed_file_direct_tu\"") == std::string::npos);
    CHECK(report.find("kind=\"changed_file_heuristic_tu\"") == std::string::npos);
    CHECK(report.find("kind=\"direct_tu_target\"") == std::string::npos);
    CHECK(report.find("kind=\"heuristic_tu_target\"") == std::string::npos);
}

TEST_CASE("DOT analyze graph attributes follow the documented order") {
    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(make_minimal_analysis_result(), 5);

    const auto pos_type = report.find("xray_report_type");
    const auto pos_version = report.find("format_version");
    const auto pos_node = report.find("graph_node_limit");
    const auto pos_edge = report.find("graph_edge_limit");
    const auto pos_truncated = report.find("graph_truncated");

    REQUIRE(pos_type != std::string::npos);
    REQUIRE(pos_version != std::string::npos);
    REQUIRE(pos_node != std::string::npos);
    REQUIRE(pos_edge != std::string::npos);
    REQUIRE(pos_truncated != std::string::npos);
    CHECK(pos_type < pos_version);
    CHECK(pos_version < pos_node);
    CHECK(pos_node < pos_edge);
    CHECK(pos_edge < pos_truncated);
}

// ---- AP M5-1.3 Tranche D: hardening edge cases ---------------------------

TEST_CASE("DOT label peels Windows drive paths down to the basename") {
    AnalysisResult result = make_minimal_analysis_result();
    result.translation_units = {
        ranked_tu("C:\\Windows\\System32\\foo.cpp", "build", "C:\\Windows|build")};
    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 1);
    CHECK(report.find("label=\"foo.cpp\"") != std::string::npos);
    // Backslashes in path/directory/unique_key remain DOT-escaped.
    CHECK(report.find("path=\"C:\\\\Windows\\\\System32\\\\foo.cpp\"") !=
          std::string::npos);
}

TEST_CASE("DOT label peels UNC paths down to the basename") {
    AnalysisResult result = make_minimal_analysis_result();
    result.translation_units = {
        ranked_tu("\\\\server\\share\\dir\\foo.cpp", "build",
                  "\\\\server\\share|build")};
    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 1);
    CHECK(report.find("label=\"foo.cpp\"") != std::string::npos);
    CHECK(report.find(
              "path=\"\\\\\\\\server\\\\share\\\\dir\\\\foo.cpp\"") !=
          std::string::npos);
}

TEST_CASE("DOT label peels Windows extended-length paths down to the basename") {
    AnalysisResult result = make_minimal_analysis_result();
    result.translation_units = {
        ranked_tu("\\\\?\\C:\\very\\long\\path\\foo.cpp", "build",
                  "\\\\?\\C:\\very\\long|build")};
    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 1);
    CHECK(report.find("label=\"foo.cpp\"") != std::string::npos);
    CHECK(report.find("\\\\?\\\\C:") != std::string::npos);
}

TEST_CASE("DOT label leaves a bare drive letter untouched") {
    AnalysisResult result = make_minimal_analysis_result();
    // No path separator — label_from_path falls back to truncate_label on the
    // whole string.
    result.translation_units = {ranked_tu("C:", "build", "C:|build")};
    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 1);
    CHECK(report.find("label=\"C:\"") != std::string::npos);
}

TEST_CASE("DOT label drops a trailing Windows separator") {
    AnalysisResult result = make_minimal_analysis_result();
    // Trailing separator: the basename component is empty, so label_from_path
    // falls back to truncate_label on the whole path (mirrors the trailing-
    // forward-slash test above).
    result.translation_units = {
        ranked_tu("C:\\dir\\", "build", "C:\\dir\\|build")};
    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 1);
    CHECK(report.find("label=\"C:\\\\dir\\\\\"") != std::string::npos);
}

TEST_CASE("DOT escape covers every ASCII control byte in 0x00..0x1F") {
    // Build a path containing every byte in 0x00..0x1F that is not already
    // expressed as a named escape (\t, \n, \r). Each byte must surface as
    // \xNN in the rendered DOT and never as a raw control byte.
    // std::string is size-tracked, so an embedded NUL flows through the
    // adapter and through std::string::find without truncation; the loop
    // therefore includes 0x00.
    std::string control_path;
    for (int code = 0x00; code < 0x20; ++code) {
        const auto ch = static_cast<unsigned char>(code);
        if (ch == '\t' || ch == '\n' || ch == '\r') continue;
        control_path.push_back(static_cast<char>(ch));
    }
    AnalysisResult result = make_minimal_analysis_result();
    result.translation_units = {ranked_tu(control_path + ".cpp", "build",
                                          "control|build")};
    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 1);
    for (int code = 0x00; code < 0x20; ++code) {
        const auto ch = static_cast<unsigned char>(code);
        if (ch == '\t' || ch == '\n' || ch == '\r') continue;
        std::array<char, 5> needle{};
        std::snprintf(needle.data(), needle.size(), "\\x%02X",
                      static_cast<unsigned>(ch));
        CHECK(report.find(needle.data()) != std::string::npos);
        CHECK(report.find(static_cast<char>(ch)) == std::string::npos);
    }
}

TEST_CASE("DOT escape uses named escape for tab, newline and carriage return") {
    AnalysisResult result = make_minimal_analysis_result();
    // tab/newline/CR get the named escape, not \xNN. Concatenation prevents
    // adjacent characters from extending hypothetical escape sequences.
    result.translation_units = {
        ranked_tu(std::string{"a\tb\nc\rd"} + ".cpp", "build", "named|build")};
    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 1);
    CHECK(report.find("a\\tb\\nc\\rd.cpp") != std::string::npos);
    // The \xNN form must NOT be used for these named-escape bytes.
    CHECK(report.find("\\x09") == std::string::npos);
    CHECK(report.find("\\x0A") == std::string::npos);
    CHECK(report.find("\\x0D") == std::string::npos);
}

TEST_CASE("DOT escape passes printable ASCII specials through unchanged") {
    // The DOT contract only escapes \\ and \" inside quoted strings; HTML-style
    // specials like <, >, & and ' carry no special meaning to Graphviz and must
    // pass through verbatim. Pinning this avoids accidental over-escaping.
    AnalysisResult result = make_minimal_analysis_result();
    result.translation_units = {
        ranked_tu("a<b>&'c.cpp", "build", "specials|build")};
    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 1);
    CHECK(report.find("path=\"a<b>&'c.cpp\"") != std::string::npos);
}

TEST_CASE("DOT escape passes DEL (0x7F) through as a raw byte") {
    // Graphviz accepts raw 0x7F inside quoted attribute strings; the adapter
    // therefore lets it through and only escapes ASCII < 0x20. This test
    // pins the observed behaviour so any future broadening of the escape
    // range is intentional.
    AnalysisResult result = make_minimal_analysis_result();
    const std::string path = std::string{"a\x7F"} + "b.cpp";
    result.translation_units = {ranked_tu(path, "build", "del|build")};
    const DotReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 1);
    // Raw 0x7F survives, no \x7F escape is emitted.
    CHECK(report.find('\x7F') != std::string::npos);
    CHECK(report.find("\\x7F") == std::string::npos);
}

TEST_CASE("compute_analyze_budget pins budgets at every documented threshold") {
    // top_limit=0 is unreachable from the CLI (CLI::PositiveNumber rejects 0
    // before the binding hits compute_analyze_budget). This row is an
    // internal contract pin: it covers the floor branch of the max(25,…)
    // and max(40,…) formulas where the linear term is below the floor.
    CHECK(compute_analyze_budget(0).context_limit == 0);
    CHECK(compute_analyze_budget(0).node_limit == 25);
    CHECK(compute_analyze_budget(0).edge_limit == 40);
    // Crossover into the linear regime: node_limit = 4*top+10 strictly above
    // 25 first at top=4 -> 26; edge_limit = 6*top+20 strictly above 40 first
    // at top=4 -> 44.
    CHECK(compute_analyze_budget(3).node_limit == 25);
    CHECK(compute_analyze_budget(4).node_limit == 26);
    CHECK(compute_analyze_budget(3).edge_limit == 40);
    CHECK(compute_analyze_budget(4).edge_limit == 44);
    // context_limit caps at 5; top=5 is the last value that also equals
    // top_limit, top=6 already returns the cap.
    CHECK(compute_analyze_budget(4).context_limit == 4);
    CHECK(compute_analyze_budget(5).context_limit == 5);
    CHECK(compute_analyze_budget(6).context_limit == 5);
}

// --- M6 AP 1.2 A.2: target-graph fields in DOT v2 ---

namespace m6_dot {

TargetDependency direct_edge(std::string from_uk, std::string to_uk,
                             TargetDependencyResolution res =
                                 TargetDependencyResolution::resolved,
                             std::string raw_id = "") {
    const std::string to_display = raw_id.empty() ? to_uk : raw_id;
    return TargetDependency{from_uk, from_uk, to_uk,        to_display,
                            TargetDependencyKind::direct, res};
}

}  // namespace m6_dot

TEST_CASE("dot analyze v2: graph_target_graph_status reflects AnalysisResult.target_graph_status") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_graph_status = TargetGraphStatus::loaded;
    const DotReportAdapter adapter;
    const auto rendered = adapter.write_analysis_report(result, 5);
    CHECK(rendered.find("graph_target_graph_status=\"loaded\"") != std::string::npos);
}

TEST_CASE("dot analyze v2: target_graph nodes are emitted with kind=target after the M4 target row") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_graph_status = TargetGraphStatus::loaded;
    result.target_graph.nodes.push_back({"app", "EXECUTABLE", "app::EXECUTABLE"});
    result.target_graph.nodes.push_back({"core", "STATIC_LIBRARY", "core::STATIC_LIBRARY"});

    const DotReportAdapter adapter;
    const auto rendered = adapter.write_analysis_report(result, 5);

    CHECK(rendered.find("kind=\"target\", label=\"app\"") != std::string::npos);
    CHECK(rendered.find("kind=\"target\", label=\"core\"") != std::string::npos);
}

TEST_CASE("dot analyze v2: resolved target_dependency edge has style=solid and resolution=resolved") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_graph_status = TargetGraphStatus::loaded;
    result.target_graph.nodes.push_back({"app", "EXECUTABLE", "app::EXECUTABLE"});
    result.target_graph.nodes.push_back({"core", "STATIC_LIBRARY", "core::STATIC_LIBRARY"});
    result.target_graph.edges.push_back(
        m6_dot::direct_edge("app::EXECUTABLE", "core::STATIC_LIBRARY"));

    const DotReportAdapter adapter;
    const auto rendered = adapter.write_analysis_report(result, 5);

    CHECK(rendered.find(
              "[kind=\"target_dependency\", style=\"solid\", resolution=\"resolved\"]") !=
          std::string::npos);
}

TEST_CASE("dot analyze v2: external target_dependency edge serializes external_target_id and dashed style") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_graph_status = TargetGraphStatus::partial;
    result.target_graph.nodes.push_back({"app", "EXECUTABLE", "app::EXECUTABLE"});
    result.target_graph.edges.push_back(m6_dot::direct_edge(
        "app::EXECUTABLE", "<external>::libfoo",
        TargetDependencyResolution::external, "libfoo"));

    const DotReportAdapter adapter;
    const auto rendered = adapter.write_analysis_report(result, 5);

    // Synthetic external_target node with kind/label/external_target_id.
    CHECK(rendered.find(
              "external_1 [kind=\"external_target\", label=\"libfoo\", external_target_id=\"libfoo\"]") !=
          std::string::npos);
    // Dashed external edge points at external_1.
    CHECK(rendered.find(
              "-> external_1 [kind=\"target_dependency\", style=\"dashed\", resolution=\"external\", external_target_id=\"libfoo\"]") !=
          std::string::npos);
    // graph_target_graph_status reflects partial.
    CHECK(rendered.find("graph_target_graph_status=\"partial\"") != std::string::npos);
}

TEST_CASE("dot analyze v2: external_target nodes deduplicate by to_unique_key with deterministic indexing") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_graph_status = TargetGraphStatus::partial;
    result.target_graph.nodes.push_back({"a", "STATIC_LIBRARY", "a::STATIC_LIBRARY"});
    result.target_graph.nodes.push_back({"b", "STATIC_LIBRARY", "b::STATIC_LIBRARY"});
    // Two real targets reference the same external library; expect ONE
    // external_target node and stable external_<index> ordering by sorted
    // to_unique_key (libbar < libfoo so libbar gets external_1, libfoo external_2).
    result.target_graph.edges.push_back(m6_dot::direct_edge(
        "a::STATIC_LIBRARY", "<external>::libfoo",
        TargetDependencyResolution::external, "libfoo"));
    result.target_graph.edges.push_back(m6_dot::direct_edge(
        "b::STATIC_LIBRARY", "<external>::libfoo",
        TargetDependencyResolution::external, "libfoo"));
    result.target_graph.edges.push_back(m6_dot::direct_edge(
        "a::STATIC_LIBRARY", "<external>::libbar",
        TargetDependencyResolution::external, "libbar"));

    const DotReportAdapter adapter;
    const auto rendered = adapter.write_analysis_report(result, 5);

    // Sorted-allocation contract: libbar wins external_1, libfoo external_2.
    CHECK(rendered.find(
              "external_1 [kind=\"external_target\", label=\"libbar\"") !=
          std::string::npos);
    CHECK(rendered.find(
              "external_2 [kind=\"external_target\", label=\"libfoo\"") !=
          std::string::npos);
    // libfoo node only appears once (no external_3 or duplicate external_2).
    CHECK(rendered.find("external_3") == std::string::npos);
}

TEST_CASE("dot analyze v2: raw_id with quote/backslash/newline escapes per the M5 string contract") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_graph_status = TargetGraphStatus::partial;
    result.target_graph.nodes.push_back({"app", "EXECUTABLE", "app::EXECUTABLE"});
    const std::string weird = "wei\"rd\\target\nnewline";
    result.target_graph.edges.push_back(m6_dot::direct_edge(
        "app::EXECUTABLE", "<external>::" + weird,
        TargetDependencyResolution::external, weird));

    const DotReportAdapter adapter;
    const auto rendered = adapter.write_analysis_report(result, 5);

    // Both label and external_target_id carry the escaped form (no truncation
    // since the raw id is below 48 chars).
    CHECK(rendered.find("label=\"wei\\\"rd\\\\target\\nnewline\"") !=
          std::string::npos);
    CHECK(rendered.find("external_target_id=\"wei\\\"rd\\\\target\\nnewline\"") !=
          std::string::npos);
}

TEST_CASE("dot analyze v2: label is middle-truncated past 48 chars, external_target_id stays full") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_graph_status = TargetGraphStatus::partial;
    result.target_graph.nodes.push_back({"app", "EXECUTABLE", "app::EXECUTABLE"});
    // 50-char raw id triggers middle truncation in label only.
    const std::string long_id(50, 'x');
    result.target_graph.edges.push_back(m6_dot::direct_edge(
        "app::EXECUTABLE", "<external>::" + long_id,
        TargetDependencyResolution::external, long_id));

    const DotReportAdapter adapter;
    const auto rendered = adapter.write_analysis_report(result, 5);

    // Label truncation pattern: 22 head chars + "..." + 23 tail chars (all 'x' here).
    const std::string truncated_label = std::string(22, 'x') + "..." + std::string(23, 'x');
    CHECK(rendered.find("label=\"" + truncated_label + "\"") != std::string::npos);
    // external_target_id keeps the full 50-char value.
    CHECK(rendered.find("external_target_id=\"" + long_id + "\"") !=
          std::string::npos);
}

TEST_CASE("dot impact v2: target_graph propagation produces target_dependency edges without an impact attribute") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = "src/main.cpp";
    result.target_graph_status = TargetGraphStatus::loaded;
    result.target_graph.nodes.push_back({"a", "STATIC_LIBRARY", "a::STATIC_LIBRARY"});
    result.target_graph.nodes.push_back({"b", "STATIC_LIBRARY", "b::STATIC_LIBRARY"});
    result.target_graph.edges.push_back(
        m6_dot::direct_edge("a::STATIC_LIBRARY", "b::STATIC_LIBRARY"));

    const DotReportAdapter adapter;
    const auto rendered = adapter.write_impact_report(result);

    CHECK(rendered.find("graph_target_graph_status=\"loaded\"") != std::string::npos);
    // Edge attribute set must NOT include an "impact" key per AP 1.2 (AP 1.3 decides).
    CHECK(rendered.find(
              "[kind=\"target_dependency\", style=\"solid\", resolution=\"resolved\"]") !=
          std::string::npos);
    CHECK(rendered.find("kind=\"target_dependency\", impact=") == std::string::npos);
}

TEST_CASE("dot impact v2: external_target node and dashed target_dependency mirror the analyze contract") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = "src/main.cpp";
    result.target_graph_status = TargetGraphStatus::partial;
    result.target_graph.nodes.push_back({"app", "EXECUTABLE", "app::EXECUTABLE"});
    result.target_graph.edges.push_back(m6_dot::direct_edge(
        "app::EXECUTABLE", "<external>::ghost",
        TargetDependencyResolution::external, "ghost"));

    const DotReportAdapter adapter;
    const auto rendered = adapter.write_impact_report(result);

    CHECK(rendered.find(
              "external_1 [kind=\"external_target\", label=\"ghost\", external_target_id=\"ghost\"]") !=
          std::string::npos);
    CHECK(rendered.find(
              "-> external_1 [kind=\"target_dependency\", style=\"dashed\", resolution=\"external\", external_target_id=\"ghost\"]") !=
          std::string::npos);
}

// --- M6 AP 1.2 A.2: defensive truncation branches (filter + budget overflow) ---

TEST_CASE("dot analyze v2: target_dependency edge is dropped and graph_truncated set when from_unique_key is not in target_graph.nodes") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_graph_status = TargetGraphStatus::loaded;
    result.target_graph.nodes.push_back({"a", "STATIC_LIBRARY", "a::STATIC_LIBRARY"});
    // Source target is not declared as a node; the filter step must drop the
    // edge and flip graph_truncated=true.
    result.target_graph.edges.push_back(
        m6_dot::direct_edge("missing::STATIC_LIBRARY", "a::STATIC_LIBRARY"));

    const DotReportAdapter adapter;
    const auto rendered = adapter.write_analysis_report(result, 0);

    CHECK(rendered.find("graph_truncated=true") != std::string::npos);
    CHECK(rendered.find("kind=\"target_dependency\"") == std::string::npos);
}

TEST_CASE("dot analyze v2: resolved target_dependency edge is dropped when to_unique_key is not in target_graph.nodes") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_graph_status = TargetGraphStatus::loaded;
    result.target_graph.nodes.push_back({"a", "STATIC_LIBRARY", "a::STATIC_LIBRARY"});
    result.target_graph.edges.push_back(m6_dot::direct_edge(
        "a::STATIC_LIBRARY", "missing::STATIC_LIBRARY",
        TargetDependencyResolution::resolved));

    const DotReportAdapter adapter;
    const auto rendered = adapter.write_analysis_report(result, 0);

    CHECK(rendered.find("graph_truncated=true") != std::string::npos);
    CHECK(rendered.find("kind=\"target_dependency\"") == std::string::npos);
}

TEST_CASE("dot analyze v2: extra target_graph.nodes beyond the node budget set graph_truncated") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_graph_status = TargetGraphStatus::loaded;
    // top=0 -> node_limit=25; with 26 target_graph.nodes the last one trips
    // build_extra_target_nodes' budget guard.
    for (int i = 0; i < 26; ++i) {
        const auto uk = "n" + std::to_string(i) + "::STATIC_LIBRARY";
        result.target_graph.nodes.push_back({uk, "STATIC_LIBRARY", uk});
    }

    const DotReportAdapter adapter;
    const auto rendered = adapter.write_analysis_report(result, 0);

    CHECK(rendered.find("graph_truncated=true") != std::string::npos);
    // Exactly node_limit (25) target nodes appear; the 26th is dropped.
    std::size_t target_count = 0;
    std::size_t pos = 0;
    while ((pos = rendered.find("kind=\"target\"", pos)) != std::string::npos) {
        ++target_count;
        ++pos;
    }
    CHECK(target_count == 25);
}

TEST_CASE("dot analyze v2: external_target node allocation beyond the node budget yields graph_truncated and the lookup miss drops the edge") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_graph_status = TargetGraphStatus::partial;
    // Fill the entire 25-node budget with target_graph.nodes so the external
    // allocator can't add any synthetic external_<index> nodes; the inner
    // edge loop then misses external_id_by_to_uk for the candidate.
    for (int i = 0; i < 25; ++i) {
        const auto uk = "n" + std::to_string(i) + "::STATIC_LIBRARY";
        result.target_graph.nodes.push_back({uk, "STATIC_LIBRARY", uk});
    }
    result.target_graph.edges.push_back(m6_dot::direct_edge(
        "n0::STATIC_LIBRARY", "<external>::ghost",
        TargetDependencyResolution::external, "ghost"));

    const DotReportAdapter adapter;
    const auto rendered = adapter.write_analysis_report(result, 0);

    CHECK(rendered.find("graph_truncated=true") != std::string::npos);
    CHECK(rendered.find("kind=\"external_target\"") == std::string::npos);
    CHECK(rendered.find("kind=\"target_dependency\"") == std::string::npos);
}

TEST_CASE("dot analyze v2: target_dependency edges beyond the edge budget set graph_truncated") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_graph_status = TargetGraphStatus::loaded;
    // top=0 -> node_limit=25, edge_limit=40. Fit 25 nodes and 41 edges so the
    // 41st target_dependency hits the edge_limit guard.
    for (int i = 0; i < 25; ++i) {
        const auto uk = "n" + std::to_string(i) + "::STATIC_LIBRARY";
        result.target_graph.nodes.push_back({uk, "STATIC_LIBRARY", uk});
    }
    int edge_count = 0;
    for (int i = 0; i < 25 && edge_count < 41; ++i) {
        for (int j = 0; j < 25 && edge_count < 41; ++j) {
            if (i == j) continue;
            result.target_graph.edges.push_back(m6_dot::direct_edge(
                "n" + std::to_string(i) + "::STATIC_LIBRARY",
                "n" + std::to_string(j) + "::STATIC_LIBRARY"));
            ++edge_count;
        }
    }
    REQUIRE(edge_count == 41);

    const DotReportAdapter adapter;
    const auto rendered = adapter.write_analysis_report(result, 0);

    CHECK(rendered.find("graph_truncated=true") != std::string::npos);
}

TEST_CASE("dot impact v2: target_dependency edge is dropped when from_unique_key is not in target_graph.nodes") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = "src/main.cpp";
    result.target_graph_status = TargetGraphStatus::loaded;
    result.target_graph.nodes.push_back({"a", "STATIC_LIBRARY", "a::STATIC_LIBRARY"});
    result.target_graph.edges.push_back(
        m6_dot::direct_edge("missing::STATIC_LIBRARY", "a::STATIC_LIBRARY"));

    const DotReportAdapter adapter;
    const auto rendered = adapter.write_impact_report(result);

    CHECK(rendered.find("graph_truncated=true") != std::string::npos);
    CHECK(rendered.find("kind=\"target_dependency\"") == std::string::npos);
}

TEST_CASE("dot impact v2: resolved target_dependency edge is dropped when to_unique_key is not in target_graph.nodes") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = "src/main.cpp";
    result.target_graph_status = TargetGraphStatus::loaded;
    result.target_graph.nodes.push_back({"a", "STATIC_LIBRARY", "a::STATIC_LIBRARY"});
    result.target_graph.edges.push_back(m6_dot::direct_edge(
        "a::STATIC_LIBRARY", "missing::STATIC_LIBRARY",
        TargetDependencyResolution::resolved));

    const DotReportAdapter adapter;
    const auto rendered = adapter.write_impact_report(result);

    CHECK(rendered.find("graph_truncated=true") != std::string::npos);
    CHECK(rendered.find("kind=\"target_dependency\"") == std::string::npos);
}

TEST_CASE("dot impact v2: extra target_graph.nodes beyond the impact node budget set graph_truncated") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = "src/main.cpp";
    result.target_graph_status = TargetGraphStatus::loaded;
    // Impact node budget is 100. With 1 changed_file already in the graph,
    // 100 target_graph.nodes leave room for 99; the 100th trips the guard.
    for (int i = 0; i < 100; ++i) {
        const auto uk = "n" + std::to_string(i) + "::STATIC_LIBRARY";
        result.target_graph.nodes.push_back({uk, "STATIC_LIBRARY", uk});
    }

    const DotReportAdapter adapter;
    const auto rendered = adapter.write_impact_report(result);

    CHECK(rendered.find("graph_truncated=true") != std::string::npos);
}

TEST_CASE("dot impact v2: external_target node allocation beyond the impact node budget drops external edges") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = "src/main.cpp";
    result.target_graph_status = TargetGraphStatus::partial;
    // Pin 99 graph nodes plus 1 changed_file = 100, fully consuming the impact
    // budget; the external candidate then misses the external allocation and
    // the inner edge loop drops it via the external lookup miss.
    for (int i = 0; i < 99; ++i) {
        const auto uk = "n" + std::to_string(i) + "::STATIC_LIBRARY";
        result.target_graph.nodes.push_back({uk, "STATIC_LIBRARY", uk});
    }
    result.target_graph.edges.push_back(m6_dot::direct_edge(
        "n0::STATIC_LIBRARY", "<external>::ghost",
        TargetDependencyResolution::external, "ghost"));

    const DotReportAdapter adapter;
    const auto rendered = adapter.write_impact_report(result);

    CHECK(rendered.find("graph_truncated=true") != std::string::npos);
    CHECK(rendered.find("kind=\"external_target\"") == std::string::npos);
    CHECK(rendered.find("kind=\"target_dependency\"") == std::string::npos);
}

TEST_CASE("dot impact v2: target_dependency edges beyond the impact edge budget set graph_truncated") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = "src/main.cpp";
    result.target_graph_status = TargetGraphStatus::loaded;
    // Impact edge budget is 200. Use 25 nodes with all-pairs minus self
    // direction = 25*24 = 600 candidate edges; cap at 201 so the 201st trips
    // the guard.
    for (int i = 0; i < 25; ++i) {
        const auto uk = "n" + std::to_string(i) + "::STATIC_LIBRARY";
        result.target_graph.nodes.push_back({uk, "STATIC_LIBRARY", uk});
    }
    int edge_count = 0;
    for (int i = 0; i < 25 && edge_count < 201; ++i) {
        for (int j = 0; j < 25 && edge_count < 201; ++j) {
            if (i == j) continue;
            result.target_graph.edges.push_back(m6_dot::direct_edge(
                "n" + std::to_string(i) + "::STATIC_LIBRARY",
                "n" + std::to_string(j) + "::STATIC_LIBRARY"));
            ++edge_count;
        }
    }
    REQUIRE(edge_count == 201);

    const DotReportAdapter adapter;
    const auto rendered = adapter.write_impact_report(result);

    CHECK(rendered.find("graph_truncated=true") != std::string::npos);
}

// ---- AP M6-1.3 A.4: impact-DOT v3 priority attributes -----------------

TEST_CASE("AP1.3 A.4: impact-DOT carries graph_impact_target_depth_{requested,effective}") {
    using xray::hexagon::model::ImpactResult;
    ImpactResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = xray::hexagon::model::CompileDatabaseResult{
        xray::hexagon::model::CompileDatabaseError::none, {}, {}, {}};
    result.changed_file = "include/x.h";
    result.inputs.changed_file = std::string{"include/x.h"};
    result.impact_target_depth_requested = 2;
    result.impact_target_depth_effective = 1;
    const DotReportAdapter adapter;
    const auto rendered = adapter.write_impact_report(result);
    CHECK(rendered.find("graph_impact_target_depth_requested=2") !=
          std::string::npos);
    CHECK(rendered.find("graph_impact_target_depth_effective=1") !=
          std::string::npos);
}

TEST_CASE("AP1.3 A.4: analyze-DOT does NOT carry graph_impact_target_depth_*") {
    using xray::hexagon::model::AnalysisResult;
    AnalysisResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = xray::hexagon::model::CompileDatabaseResult{
        xray::hexagon::model::CompileDatabaseError::none, {}, {}, {}};
    const DotReportAdapter adapter;
    const auto rendered = adapter.write_analysis_report(result, 5);
    CHECK(rendered.find("graph_impact_target_depth_requested") == std::string::npos);
    CHECK(rendered.find("graph_impact_target_depth_effective") == std::string::npos);
}

TEST_CASE("AP1.3 A.4: impact-DOT renders priority_class/graph_distance/evidence_strength on prioritised target nodes") {
    using xray::hexagon::model::ImpactResult;
    using xray::hexagon::model::PrioritizedImpactedTarget;
    using xray::hexagon::model::TargetEvidenceStrength;
    using xray::hexagon::model::TargetGraphStatus;
    using xray::hexagon::model::TargetImpactClassification;
    using xray::hexagon::model::TargetInfo;
    using xray::hexagon::model::TargetPriorityClass;
    ImpactResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = xray::hexagon::model::CompileDatabaseResult{
        xray::hexagon::model::CompileDatabaseError::none, {}, {}, {}};
    result.changed_file = "include/x.h";
    result.inputs.changed_file = std::string{"include/x.h"};
    result.target_graph_status = TargetGraphStatus::loaded;
    result.impact_target_depth_requested = 2;
    result.impact_target_depth_effective = 1;
    const TargetInfo lib{"lib", "STATIC_LIBRARY", "lib::STATIC_LIBRARY"};
    result.affected_targets.push_back({lib, TargetImpactClassification::direct});
    result.prioritized_affected_targets.push_back(
        PrioritizedImpactedTarget{lib, TargetPriorityClass::direct, 0,
                                  TargetEvidenceStrength::direct});
    const DotReportAdapter adapter;
    const auto rendered = adapter.write_impact_report(result);
    CHECK(rendered.find("priority_class=\"direct\"") != std::string::npos);
    CHECK(rendered.find("graph_distance=0") != std::string::npos);
    CHECK(rendered.find("evidence_strength=\"direct\"") != std::string::npos);
}

TEST_CASE("AP1.3 A.4: impact-DOT skips priority attributes on graph-only target nodes that have no prioritised entry") {
    using xray::hexagon::model::ImpactResult;
    using xray::hexagon::model::TargetGraphStatus;
    using xray::hexagon::model::TargetInfo;
    ImpactResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = xray::hexagon::model::CompileDatabaseResult{
        xray::hexagon::model::CompileDatabaseError::none, {}, {}, {}};
    result.changed_file = "include/x.h";
    result.inputs.changed_file = std::string{"include/x.h"};
    result.target_graph_status = TargetGraphStatus::loaded;
    // Graph-only floating target without an affected_targets or
    // prioritised entry: the v3 attribute join must NOT attach.
    result.target_graph.nodes.push_back(
        TargetInfo{"floating", "STATIC_LIBRARY", "floating::STATIC_LIBRARY"});
    const DotReportAdapter adapter;
    const auto rendered = adapter.write_impact_report(result);
    const auto floating_pos = rendered.find("name=\"floating\"");
    REQUIRE(floating_pos != std::string::npos);
    const auto close_pos = rendered.find("];", floating_pos);
    REQUIRE(close_pos != std::string::npos);
    const auto attrs = rendered.substr(floating_pos, close_pos - floating_pos);
    CHECK(attrs.find("priority_class") == std::string::npos);
    CHECK(attrs.find("graph_distance") == std::string::npos);
    CHECK(attrs.find("evidence_strength") == std::string::npos);
}
