#include <doctest/doctest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

#include "adapters/input/analysis_json_reader.h"
#include "hexagon/model/compare_result.h"
#include "hexagon/model/report_inputs.h"

namespace {

using xray::adapters::input::AnalysisJsonReadError;
using xray::adapters::input::AnalysisJsonReader;
using xray::adapters::input::project_identity_source_text;
using xray::hexagon::model::ProjectIdentitySource;

nlohmann::json valid_analysis_report() {
    return nlohmann::json::parse(R"json({
  "format": "cmake-xray.analysis",
  "format_version": 6,
  "report_type": "analyze",
  "inputs": {
    "compile_database_path": "compile_commands.json",
    "compile_database_source": "cli",
    "cmake_file_api_path": null,
    "cmake_file_api_resolved_path": null,
    "cmake_file_api_source": null,
    "project_identity": "compile-db:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
    "project_identity_source": "fallback_compile_database_fingerprint"
  },
  "summary": {
    "translation_unit_count": 0,
    "ranking_total_count": 0,
    "hotspot_total_count": 0,
    "top_limit": 10,
    "include_analysis_heuristic": false,
    "observation_source": "exact",
    "target_metadata": "not_loaded",
    "tu_ranking_total_count_after_thresholds": 0,
    "tu_ranking_excluded_by_thresholds_count": 0,
    "include_hotspot_excluded_by_min_tus_count": 0
  },
  "analysis_configuration": {
    "analysis_sections": ["tu-ranking", "include-hotspots"],
    "tu_thresholds": {
      "arg_count": 0,
      "include_path_count": 0,
      "define_count": 0
    },
    "min_hotspot_tus": 0,
    "target_hub_in_threshold": 0,
    "target_hub_out_threshold": 0
  },
  "analysis_section_states": {
    "tu-ranking": "active",
    "include-hotspots": "active",
    "target-graph": "not_loaded",
    "target-hubs": "not_loaded"
  },
  "include_filter": {
    "include_scope": "all",
    "include_depth": "all",
    "include_depth_limit_requested": 32,
    "include_depth_limit_effective": 0,
    "include_node_budget_requested": 10000,
    "include_node_budget_effective": 0,
    "include_node_budget_reached": false
  },
  "translation_unit_ranking": {
    "limit": 10,
    "total_count": 0,
    "returned_count": 0,
    "truncated": false,
    "items": []
  },
  "include_hotspots": {
    "limit": 10,
    "total_count": 0,
    "returned_count": 0,
    "truncated": false,
    "excluded_unknown_count": 0,
    "excluded_mixed_count": 0,
    "items": []
  },
  "target_graph_status": "not_loaded",
  "target_graph": {
    "nodes": [],
    "edges": []
  },
  "target_hubs": {
    "inbound": [],
    "outbound": [],
    "thresholds": {
      "in_threshold": 0,
      "out_threshold": 0
    }
  },
  "diagnostics": []
})json");
}

std::filesystem::path write_temp_json(const std::string& name, const nlohmann::json& document) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path);
    out << document.dump(2);
    return path;
}

std::filesystem::path write_temp_text(const std::string& name, const std::string& text) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path);
    out << text;
    return path;
}

void remove_temp_file(const std::filesystem::path& path) {
    std::remove(path.string().c_str());
}

AnalysisJsonReadError read_error_for(nlohmann::json document) {
    const auto path = write_temp_json("cmake-xray-analysis-reader-negative.json", document);
    const AnalysisJsonReader reader;
    const auto result = reader.read(path.string());
    remove_temp_file(path);
    return result.error;
}

}  // namespace

TEST_CASE("analysis JSON reader loads a valid analyze v6 report") {
    const auto path =
        write_temp_json("cmake-xray-analysis-reader-valid.json", valid_analysis_report());
    const AnalysisJsonReader reader;

    const auto result = reader.read(path.string());
    remove_temp_file(path);

    REQUIRE(result.is_success());
    CHECK(result.report.path == path.string());
    CHECK(result.report.format_version == 6);
    CHECK(result.report.project_identity ==
          "compile-db:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    CHECK(result.report.project_identity_source ==
          ProjectIdentitySource::fallback_compile_database_fingerprint);
    CHECK(result.report.translation_unit_ranking.at("items").empty());
    CHECK(result.report.target_graph.at("nodes").empty());
}

TEST_CASE("analysis JSON reader accepts file-api project identity source") {
    auto document = valid_analysis_report();
    document["inputs"]["project_identity"] = "/repo/cmake-xray";
    document["inputs"]["project_identity_source"] = "cmake_file_api_source_root";

    const auto path = write_temp_json("cmake-xray-analysis-reader-file-api.json", document);
    const AnalysisJsonReader reader;
    const auto result = reader.read(path.string());
    remove_temp_file(path);

    REQUIRE(result.is_success());
    CHECK(result.report.project_identity == "/repo/cmake-xray");
    CHECK(result.report.project_identity_source ==
          ProjectIdentitySource::cmake_file_api_source_root);
}

TEST_CASE("analysis JSON reader reports file and parse errors") {
    const AnalysisJsonReader reader;
    CHECK(reader.read("/tmp/cmake-xray-analysis-reader-missing.json").error ==
          AnalysisJsonReadError::file_not_accessible);

    const auto path = write_temp_text("cmake-xray-analysis-reader-invalid.json", "{");
    CHECK(reader.read(path.string()).error == AnalysisJsonReadError::invalid_json);
    remove_temp_file(path);
}

TEST_CASE("analysis JSON reader rejects non-analyze or incompatible reports") {
    CHECK(read_error_for(nlohmann::json::array()) == AnalysisJsonReadError::schema_mismatch);

    auto missing_key = valid_analysis_report();
    missing_key.erase("target_hubs");
    CHECK(read_error_for(missing_key) == AnalysisJsonReadError::schema_mismatch);

    auto wrong_shape = valid_analysis_report();
    wrong_shape["summary"] = nlohmann::json::array();
    CHECK(read_error_for(wrong_shape) == AnalysisJsonReadError::schema_mismatch);

    auto report_type_not_string = valid_analysis_report();
    report_type_not_string["report_type"] = 7;
    CHECK(read_error_for(report_type_not_string) == AnalysisJsonReadError::schema_mismatch);

    auto impact = valid_analysis_report();
    impact["report_type"] = "impact";
    CHECK(read_error_for(impact) == AnalysisJsonReadError::unsupported_report_type);

    auto wrong_format = valid_analysis_report();
    wrong_format["format"] = "cmake-xray.impact";
    CHECK(read_error_for(wrong_format) == AnalysisJsonReadError::schema_mismatch);

    auto version_not_integer = valid_analysis_report();
    version_not_integer["format_version"] = "6";
    CHECK(read_error_for(version_not_integer) == AnalysisJsonReadError::schema_mismatch);

    auto version_five = valid_analysis_report();
    version_five["format_version"] = 5;
    CHECK(read_error_for(version_five) == AnalysisJsonReadError::incompatible_format_version);

    auto legacy_shape = nlohmann::json::object({
        {"format", "cmake-xray.analysis"},
        {"format_version", 5},
        {"report_type", "analyze"},
    });
    CHECK(read_error_for(legacy_shape) == AnalysisJsonReadError::incompatible_format_version);
}

TEST_CASE("analysis JSON reader rejects malformed nested compare inputs") {
    auto missing_sections = valid_analysis_report();
    missing_sections["analysis_configuration"].erase("analysis_sections");
    CHECK(read_error_for(missing_sections) == AnalysisJsonReadError::schema_mismatch);

    auto wrong_items_shape = valid_analysis_report();
    wrong_items_shape["translation_unit_ranking"]["items"] = "not an array";
    CHECK(read_error_for(wrong_items_shape) == AnalysisJsonReadError::schema_mismatch);

    auto missing_target_nodes = valid_analysis_report();
    missing_target_nodes["target_graph"].erase("nodes");
    CHECK(read_error_for(missing_target_nodes) == AnalysisJsonReadError::schema_mismatch);

    auto extra_property = valid_analysis_report();
    extra_property["summary"]["unexpected"] = 1;
    CHECK(read_error_for(extra_property) == AnalysisJsonReadError::schema_mismatch);

    auto negative_metric = valid_analysis_report();
    negative_metric["summary"]["translation_unit_count"] = -1;
    CHECK(read_error_for(negative_metric) == AnalysisJsonReadError::schema_mismatch);

    auto invalid_enum = valid_analysis_report();
    invalid_enum["include_filter"]["include_scope"] = "headers";
    CHECK(read_error_for(invalid_enum) == AnalysisJsonReadError::schema_mismatch);

    auto missing_hub_thresholds = valid_analysis_report();
    missing_hub_thresholds["target_hubs"].erase("thresholds");
    CHECK(read_error_for(missing_hub_thresholds) ==
          AnalysisJsonReadError::schema_mismatch);
}

TEST_CASE("analysis JSON reader validates project identity fields") {
    auto missing_inputs_key = valid_analysis_report();
    missing_inputs_key["inputs"].erase("project_identity");
    CHECK(read_error_for(missing_inputs_key) == AnalysisJsonReadError::schema_mismatch);

    auto non_string_source = valid_analysis_report();
    non_string_source["inputs"]["project_identity_source"] = nullptr;
    CHECK(read_error_for(non_string_source) ==
          AnalysisJsonReadError::invalid_project_identity_source);

    auto unknown_source = valid_analysis_report();
    unknown_source["inputs"]["project_identity_source"] = "unknown";
    CHECK(read_error_for(unknown_source) ==
          AnalysisJsonReadError::invalid_project_identity_source);

    auto empty_identity = valid_analysis_report();
    empty_identity["inputs"]["project_identity"] = "";
    CHECK(read_error_for(empty_identity) ==
          AnalysisJsonReadError::unrecoverable_project_identity);
}

TEST_CASE("analysis JSON reader renders project identity source text") {
    CHECK(project_identity_source_text(ProjectIdentitySource::cmake_file_api_source_root) ==
          "cmake_file_api_source_root");
    CHECK(project_identity_source_text(
              ProjectIdentitySource::fallback_compile_database_fingerprint) ==
          "fallback_compile_database_fingerprint");
    CHECK(project_identity_source_text(static_cast<ProjectIdentitySource>(99)).empty());
}

TEST_CASE("compare result model exposes format version and default counters") {
    xray::hexagon::model::CompareResult result;

    CHECK(xray::hexagon::model::kCompareFormatVersion == 1);
    CHECK(result.summary.translation_units_added == 0);
    CHECK(result.summary.target_hubs_changed == 0);
    CHECK_FALSE(result.project_identity_drift.has_value());
}
