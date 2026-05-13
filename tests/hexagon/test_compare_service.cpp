#include <doctest/doctest.h>

#include <filesystem>
#include <map>
#include <string>
#include <utility>

#include "hexagon/model/analysis_report_snapshot.h"
#include "hexagon/model/compare_result.h"
#include "hexagon/model/report_format_version.h"
#include "hexagon/model/report_inputs.h"
#include "hexagon/ports/driven/analysis_report_reader_port.h"
#include "hexagon/ports/driving/compare_analysis_port.h"
#include "hexagon/services/compare_service.h"

namespace {

using xray::hexagon::model::AnalysisReportSnapshot;
using xray::hexagon::model::CompareDiagnosticSnapshot;
using xray::hexagon::model::CompareDiffKind;
using xray::hexagon::model::CompareHotspotAffectedUnitSnapshot;
using xray::hexagon::model::CompareIncludeHotspotSnapshot;
using xray::hexagon::model::CompareTargetEdgeSnapshot;
using xray::hexagon::model::CompareTargetSnapshot;
using xray::hexagon::model::CompareTranslationUnitSnapshot;
using xray::hexagon::model::CompareResult;
using xray::hexagon::model::ProjectIdentitySource;
using xray::hexagon::model::kReportFormatVersion;
using xray::hexagon::ports::driven::AnalysisReportReadError;
using xray::hexagon::ports::driven::AnalysisReportReadResult;
using xray::hexagon::ports::driving::CompareAnalysisRequest;

class StubAnalysisReportReader final
    : public xray::hexagon::ports::driven::AnalysisReportReaderPort {
public:
    void add(std::string path, AnalysisReportReadResult result) {
        reports_.emplace(std::move(path), std::move(result));
    }

    AnalysisReportReadResult read_analysis_report(
        const std::filesystem::path& path) const override {
        const auto found = reports_.find(path.string());
        if (found != reports_.end()) return found->second;
        return {AnalysisReportReadError::file_not_accessible, "missing fixture", {}};
    }

private:
    std::map<std::string, AnalysisReportReadResult> reports_;
};

CompareTargetSnapshot target(std::string unique_key, std::string display_name,
                             std::string type) {
    return {std::move(display_name), std::move(type), std::move(unique_key)};
}

CompareDiagnosticSnapshot diagnostic(std::string severity, std::string message) {
    return {std::move(severity), std::move(message)};
}

CompareTranslationUnitSnapshot tu(std::string source_path, std::string directory,
                                  int arg_count) {
    CompareTranslationUnitSnapshot unit;
    unit.source_path = std::move(source_path);
    unit.build_context_key = std::move(directory);
    unit.arg_count = arg_count;
    unit.include_path_count = arg_count + 1;
    unit.define_count = arg_count + 2;
    unit.score = arg_count + 3;
    unit.targets = {target("app", "app", "EXECUTABLE")};
    unit.diagnostics = {diagnostic("warning", "old warning")};
    return unit;
}

CompareIncludeHotspotSnapshot hotspot(std::string header, std::string origin,
                                      std::string depth, int affected_count) {
    CompareIncludeHotspotSnapshot item;
    item.header_path = std::move(header);
    item.include_origin = std::move(origin);
    item.include_depth_kind = std::move(depth);
    item.affected_total_count = affected_count;
    item.affected_translation_units = {
        CompareHotspotAffectedUnitSnapshot{"src/a.cpp", "build/app"}};
    item.diagnostics = {diagnostic("note", "old note")};
    return item;
}

CompareTargetEdgeSnapshot edge(std::string from, std::string to, std::string resolution) {
    CompareTargetEdgeSnapshot item;
    item.from_unique_key = std::move(from);
    item.to_unique_key = std::move(to);
    item.kind = "direct";
    item.resolution = std::move(resolution);
    item.from_display_name = "from";
    item.to_display_name = "to";
    return item;
}

AnalysisReportSnapshot report(std::string path) {
    AnalysisReportSnapshot snapshot;
    snapshot.path = std::move(path);
    snapshot.format_version = kReportFormatVersion;
    snapshot.project_identity = "compile-db:same";
    snapshot.project_identity_source =
        ProjectIdentitySource::fallback_compile_database_fingerprint;
    snapshot.configuration.analysis_sections = {"tu-ranking", "include-hotspots"};
    snapshot.configuration.tu_threshold_arg_count = 0;
    snapshot.configuration.tu_threshold_include_path_count = 1;
    snapshot.configuration.tu_threshold_define_count = 2;
    snapshot.configuration.min_hotspot_tus = 2;
    snapshot.configuration.target_hub_in_threshold = 10;
    snapshot.configuration.target_hub_out_threshold = 11;
    snapshot.configuration.include_scope = "all";
    snapshot.configuration.include_depth = "all";
    snapshot.target_graph_state = "loaded";
    snapshot.target_hubs_state = "loaded";
    snapshot.translation_units = {
        tu("src/a.cpp", "build/app", 1),
        tu("src/removed.cpp", "build/app", 2),
    };
    snapshot.include_hotspots = {
        hotspot("include/common.h", "project", "direct", 1),
        hotspot("include/removed.h", "project", "direct", 1),
    };
    snapshot.target_nodes = {
        target("app", "app", "EXECUTABLE"),
        target("old", "old", "STATIC_LIBRARY"),
    };
    snapshot.target_edges = {
        edge("app", "old", "resolved"),
        edge("old", "app", "resolved"),
    };
    snapshot.target_hubs_in = {target("old", "old", "STATIC_LIBRARY")};
    snapshot.target_hubs_out = {target("app", "app", "EXECUTABLE")};
    return snapshot;
}

AnalysisReportReadResult success(AnalysisReportSnapshot snapshot) {
    return {AnalysisReportReadError::none, {}, std::move(snapshot)};
}

CompareAnalysisRequest request(bool allow_drift = false) {
    return {std::filesystem::path{"baseline.json"},
            std::filesystem::path{"current.json"},
            allow_drift};
}

AnalysisReportSnapshot changed_report() {
    auto current = report("current.json");
    current.project_identity = "compile-db:same";
    current.configuration.analysis_sections = {"include-hotspots", "tu-ranking",
                                               "target-graph"};
    current.configuration.tu_threshold_arg_count = 5;
    current.configuration.tu_threshold_include_path_count = 6;
    current.configuration.tu_threshold_define_count = 7;
    current.configuration.min_hotspot_tus = 3;
    current.configuration.target_hub_in_threshold = 12;
    current.configuration.target_hub_out_threshold = 13;
    current.configuration.include_scope = "project";
    current.configuration.include_depth = "direct";

    current.translation_units = {
        tu("src/a.cpp", "build/app", 3),
        tu("src/added.cpp", "build/app", 4),
    };
    current.translation_units.front().targets = {target("lib", "lib", "STATIC_LIBRARY")};
    current.translation_units.front().diagnostics = {diagnostic("warning", "new warning")};

    current.include_hotspots = {
        hotspot("include/common.h", "project", "direct", 2),
        hotspot("include/added.h", "project", "direct", 1),
    };
    current.include_hotspots.front().affected_translation_units = {
        CompareHotspotAffectedUnitSnapshot{"src/a.cpp", "build/app"},
        CompareHotspotAffectedUnitSnapshot{"src/added.cpp", "build/app"}};
    current.include_hotspots.front().diagnostics = {diagnostic("note", "new note")};

    current.target_nodes = {
        target("app", "app-renamed", "STATIC_LIBRARY"),
        target("new", "new", "STATIC_LIBRARY"),
    };
    current.target_edges = {
        edge("app", "old", "external"),
        edge("app", "new", "resolved"),
    };
    current.target_edges.front().from_display_name = "from-renamed";
    current.target_edges.front().to_display_name = "to-renamed";
    current.target_hubs_in = {target("new", "new", "STATIC_LIBRARY")};
    current.target_hubs_out = {target("new", "new", "STATIC_LIBRARY")};
    return current;
}

CompareResult compare(AnalysisReportSnapshot baseline, AnalysisReportSnapshot current,
                      bool allow_drift = false) {
    StubAnalysisReportReader reader;
    reader.add("baseline.json", success(std::move(baseline)));
    reader.add("current.json", success(std::move(current)));
    const xray::hexagon::services::CompareService service{reader};
    return service.compare(request(allow_drift));
}

}  // namespace

TEST_CASE("compare service returns empty diff for identical reports") {
    const auto result = compare(report("baseline.json"), report("current.json"));

    CHECK_FALSE(result.service_error.has_value());
    CHECK(result.inputs.baseline_path == "baseline.json");
    CHECK(result.inputs.current_path == "current.json");
    CHECK(result.inputs.project_identity == "compile-db:same");
    CHECK(result.inputs.project_identity_source ==
          "fallback_compile_database_fingerprint");
    CHECK(result.translation_unit_diffs.empty());
    CHECK(result.include_hotspot_diffs.empty());
    CHECK(result.target_node_diffs.empty());
    CHECK(result.target_edge_diffs.empty());
    CHECK(result.target_hub_diffs.empty());
    CHECK(result.configuration_drifts.empty());
    CHECK(result.data_availability_drifts.empty());
}

TEST_CASE("compare service computes added removed and changed diffs") {
    const auto result = compare(report("baseline.json"), changed_report());

    REQUIRE_FALSE(result.service_error.has_value());
    CHECK(result.summary.translation_units_added == 1);
    CHECK(result.summary.translation_units_removed == 1);
    CHECK(result.summary.translation_units_changed == 1);
    CHECK(result.summary.include_hotspots_added == 1);
    CHECK(result.summary.include_hotspots_removed == 1);
    CHECK(result.summary.include_hotspots_changed == 1);
    CHECK(result.summary.target_nodes_added == 1);
    CHECK(result.summary.target_nodes_removed == 1);
    CHECK(result.summary.target_nodes_changed == 1);
    CHECK(result.summary.target_edges_added == 1);
    CHECK(result.summary.target_edges_removed == 1);
    CHECK(result.summary.target_edges_changed == 1);
    CHECK(result.summary.target_hubs_added == 2);
    CHECK(result.summary.target_hubs_removed == 2);
    CHECK(result.summary.target_hubs_changed == 0);

    CHECK(result.translation_unit_diffs.size() == 3);
    CHECK(result.include_hotspot_diffs.size() == 3);
    CHECK(result.target_node_diffs.size() == 3);
    CHECK(result.target_edge_diffs.size() == 3);
    CHECK(result.target_hub_diffs.size() == 4);
    REQUIRE(result.configuration_drifts.size() == 9);
    CHECK(result.configuration_drifts.front().field ==
          "analysis_configuration.analysis_sections");
}

TEST_CASE("compare service records data availability drift and skips unavailable target diffs") {
    auto current = changed_report();
    current.target_graph_state = "not_loaded";
    current.target_hubs_state = "disabled";

    const auto result = compare(report("baseline.json"), current);

    REQUIRE_FALSE(result.service_error.has_value());
    REQUIRE(result.data_availability_drifts.size() == 2);
    CHECK(result.data_availability_drifts[0].section == "target_graph");
    CHECK(result.data_availability_drifts[1].section == "target_hubs");
    CHECK(result.target_node_diffs.empty());
    CHECK(result.target_edge_diffs.empty());
    CHECK(result.target_hub_diffs.empty());
}

TEST_CASE("compare service maps reader and version errors") {
    StubAnalysisReportReader reader;
    reader.add("baseline.json",
               {AnalysisReportReadError::invalid_json, "not json", {}});
    const xray::hexagon::services::CompareService service{reader};
    auto result = service.compare(request());
    REQUIRE(result.service_error.has_value());
    CHECK(result.service_error->code == "baseline_read_failed");

    StubAnalysisReportReader current_failure;
    current_failure.add("baseline.json", success(report("baseline.json")));
    current_failure.add("current.json",
                        {AnalysisReportReadError::schema_mismatch, "bad schema", {}});
    const xray::hexagon::services::CompareService current_service{current_failure};
    result = current_service.compare(request());
    REQUIRE(result.service_error.has_value());
    CHECK(result.service_error->code == "current_read_failed");

    auto incompatible = report("current.json");
    incompatible.format_version = kReportFormatVersion - 1;
    result = compare(report("baseline.json"), incompatible);
    REQUIRE(result.service_error.has_value());
    CHECK(result.service_error->code == "incompatible_format_version");
    CHECK(result.service_error->message.find("format_version combination (6, 5)") !=
          std::string::npos);

    StubAnalysisReportReader incompatible_reader;
    auto baseline_error = AnalysisReportReadResult{
        AnalysisReportReadError::incompatible_format_version, "unsupported", {}};
    baseline_error.report.format_version = 5;
    auto current_error = AnalysisReportReadResult{
        AnalysisReportReadError::incompatible_format_version, "unsupported", {}};
    current_error.report.format_version = 7;
    incompatible_reader.add("baseline.json", baseline_error);
    incompatible_reader.add("current.json", current_error);
    const xray::hexagon::services::CompareService incompatible_service{
        incompatible_reader};
    result = incompatible_service.compare(request());
    REQUIRE(result.service_error.has_value());
    CHECK(result.service_error->code == "incompatible_format_version");
    CHECK(result.service_error->message.find("format_version combination (5, 7)") !=
          std::string::npos);
}

TEST_CASE("compare service validates project identity rules") {
    auto different_source = report("current.json");
    different_source.project_identity_source =
        ProjectIdentitySource::cmake_file_api_source_root;
    auto result = compare(report("baseline.json"), different_source);
    REQUIRE(result.service_error.has_value());
    CHECK(result.service_error->code == "project_identity_source_mismatch");
    CHECK(result.service_error->message.find(
              "baseline=fallback_compile_database_fingerprint") != std::string::npos);
    CHECK(result.service_error->message.find("current=cmake_file_api_source_root") !=
          std::string::npos);

    auto different_identity = report("current.json");
    different_identity.project_identity = "compile-db:other";
    result = compare(report("baseline.json"), different_identity);
    REQUIRE(result.service_error.has_value());
    CHECK(result.service_error->code == "project_identity_mismatch");
    CHECK(result.service_error->message.find("baseline='compile-db:same'") !=
          std::string::npos);
    CHECK(result.service_error->message.find("current='compile-db:other'") !=
          std::string::npos);
    CHECK(result.service_error->message.find("--allow-project-identity-drift") !=
          std::string::npos);

    auto baseline_file_api = report("baseline.json");
    auto current_file_api = report("current.json");
    baseline_file_api.project_identity_source =
        ProjectIdentitySource::cmake_file_api_source_root;
    current_file_api.project_identity_source =
        ProjectIdentitySource::cmake_file_api_source_root;
    current_file_api.project_identity = "/other";
    result = compare(baseline_file_api, current_file_api, true);
    REQUIRE(result.service_error.has_value());
    CHECK(result.service_error->code == "project_identity_mismatch");
}

TEST_CASE("compare service allows fallback project identity drift with diagnostic") {
    auto baseline = report("baseline.json");
    auto current = report("current.json");
    current.project_identity = "compile-db:other";
    current.translation_units.push_back(tu("src/extra.cpp", "build/app", 1));

    const auto result = compare(baseline, current, true);

    REQUIRE_FALSE(result.service_error.has_value());
    CHECK_FALSE(result.inputs.project_identity.has_value());
    REQUIRE(result.project_identity_drift.has_value());
    CHECK(result.project_identity_drift->baseline_project_identity == "compile-db:same");
    CHECK(result.project_identity_drift->current_project_identity == "compile-db:other");
    CHECK(result.project_identity_drift->baseline_source_path_count == 2);
    CHECK(result.project_identity_drift->current_source_path_count == 3);
    CHECK(result.project_identity_drift->shared_source_path_count == 2);
}
