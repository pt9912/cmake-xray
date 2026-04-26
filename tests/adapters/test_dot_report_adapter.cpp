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
using xray::hexagon::model::ImpactResult;
using xray::hexagon::model::RankedTranslationUnit;
using xray::hexagon::model::ReportInputs;
using xray::hexagon::model::ReportInputSource;
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
