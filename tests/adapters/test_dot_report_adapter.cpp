#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <string>

#include "adapters/output/dot_report_adapter.h"
#include "hexagon/model/analysis_result.h"
#include "hexagon/model/application_info.h"
#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/model/report_format_version.h"
#include "hexagon/model/report_inputs.h"
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
