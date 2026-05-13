#include "adapters/input/analysis_json_reader.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "hexagon/model/report_format_version.h"

namespace xray::adapters::input {

namespace {

using xray::hexagon::model::ProjectIdentitySource;
using xray::hexagon::model::kReportFormatVersion;
using xray::hexagon::ports::driven::AnalysisReportReadError;
using xray::hexagon::ports::driven::AnalysisReportReadResult;

constexpr std::array<std::string_view, 13> kRequiredAnalysisKeys{
    "format",
    "format_version",
    "report_type",
    "inputs",
    "summary",
    "analysis_configuration",
    "analysis_section_states",
    "include_filter",
    "translation_unit_ranking",
    "include_hotspots",
    "target_graph_status",
    "target_graph",
    "target_hubs",
};

AnalysisJsonReadResult make_error(AnalysisJsonReadError error, std::string message) {
    AnalysisJsonReadResult result;
    result.error = error;
    result.error_message = std::move(message);
    return result;
}

AnalysisJsonReadResult make_error(AnalysisJsonReadError error, std::string message,
                                  AnalysisJsonReport report) {
    AnalysisJsonReadResult result;
    result.error = error;
    result.error_message = std::move(message);
    result.report = std::move(report);
    return result;
}

AnalysisJsonReadResult schema_mismatch(std::string message) {
    return make_error(AnalysisJsonReadError::schema_mismatch, std::move(message));
}

std::string key_string(std::string_view key) {
    return std::string(key);
}

const nlohmann::json* member(const nlohmann::json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.find(key_string(key));
    if (found == object.end()) return nullptr;
    return &(*found);
}

bool has_string(const nlohmann::json& object, std::string_view key) {
    const auto* value = member(object, key);
    return value != nullptr && value->is_string();
}

bool has_object(const nlohmann::json& object, std::string_view key) {
    const auto* value = member(object, key);
    return value != nullptr && value->is_object();
}

bool has_array(const nlohmann::json& object, std::string_view key) {
    const auto* value = member(object, key);
    return value != nullptr && value->is_array();
}

bool has_exact_keys(const nlohmann::json& object,
                    std::initializer_list<std::string_view> keys) {
    if (!object.is_object() || object.size() != keys.size()) return false;
    for (const auto key : keys) {
        if (!object.contains(key_string(key))) return false;
    }
    return true;
}

bool has_non_negative_integer(const nlohmann::json& object, std::string_view key) {
    const auto* value = member(object, key);
    return value != nullptr && value->is_number_integer() &&
           value->get<std::int64_t>() >= 0 &&
           value->get<std::int64_t>() <= std::numeric_limits<int>::max();
}

bool has_positive_integer(const nlohmann::json& object, std::string_view key) {
    const auto* value = member(object, key);
    return value != nullptr && value->is_number_integer() &&
           value->get<std::int64_t>() >= 1 &&
           value->get<std::int64_t>() <= std::numeric_limits<int>::max();
}

bool has_boolean(const nlohmann::json& object, std::string_view key) {
    const auto* value = member(object, key);
    return value != nullptr && value->is_boolean();
}

bool has_nullable_string(const nlohmann::json& object, std::string_view key) {
    const auto* value = member(object, key);
    return value != nullptr && (value->is_string() || value->is_null());
}

bool string_is_one_of(const nlohmann::json& object, std::string_view key,
                      std::initializer_list<std::string_view> values) {
    const auto* value = member(object, key);
    if (value == nullptr || !value->is_string()) return false;
    const auto text = value->get<std::string>();
    for (const auto allowed : values) {
        if (text == allowed) return true;
    }
    return false;
}

bool nullable_source_shape(const nlohmann::json& object, std::string_view key) {
    const auto* value = member(object, key);
    return value != nullptr &&
           (value->is_null() || (value->is_string() && value->get<std::string>() == "cli"));
}

bool analysis_sections_shape(const nlohmann::json& value) {
    if (!value.is_array()) return false;
    for (const auto& item : value) {
        if (!item.is_string()) return false;
        const auto section = item.get<std::string>();
        if (section != "tu-ranking" && section != "include-hotspots" &&
            section != "target-graph" && section != "target-hubs") {
            return false;
        }
    }
    return true;
}

bool analysis_summary_keys_shape(const nlohmann::json& summary) {
    return has_exact_keys(summary, {"translation_unit_count", "ranking_total_count",
                                    "hotspot_total_count", "top_limit",
                                    "include_analysis_heuristic", "observation_source",
                                    "target_metadata",
                                    "tu_ranking_total_count_after_thresholds",
                                    "tu_ranking_excluded_by_thresholds_count",
                                    "include_hotspot_excluded_by_min_tus_count"});
}

bool analysis_summary_values_shape(const nlohmann::json& summary) {
    return has_non_negative_integer(summary, "translation_unit_count") &&
           has_non_negative_integer(summary, "ranking_total_count") &&
           has_non_negative_integer(summary, "hotspot_total_count") &&
           has_non_negative_integer(summary, "top_limit") &&
           has_boolean(summary, "include_analysis_heuristic") &&
           string_is_one_of(summary, "observation_source", {"exact", "derived"}) &&
           string_is_one_of(summary, "target_metadata",
                            {"not_loaded", "loaded", "partial"}) &&
           has_non_negative_integer(summary,
                                    "tu_ranking_total_count_after_thresholds") &&
           has_non_negative_integer(summary,
                                    "tu_ranking_excluded_by_thresholds_count") &&
           has_non_negative_integer(summary,
                                     "include_hotspot_excluded_by_min_tus_count");
}

bool analysis_summary_shape(const nlohmann::json& summary) {
    return analysis_summary_keys_shape(summary) && analysis_summary_values_shape(summary);
}

bool diagnostics_shape(const nlohmann::json& diagnostics) {
    if (!diagnostics.is_array()) return false;
    for (const auto& diagnostic : diagnostics) {
        if (!has_exact_keys(diagnostic, {"severity", "message"}) ||
            !string_is_one_of(diagnostic, "severity", {"note", "warning"}) ||
            !has_string(diagnostic, "message")) {
            return false;
        }
    }
    return true;
}

bool target_shape(const nlohmann::json& target, bool require_unique_key) {
    if (!has_exact_keys(target, require_unique_key
                                   ? std::initializer_list<std::string_view>{
                                         "display_name", "type", "unique_key"}
                                   : std::initializer_list<std::string_view>{
                                         "display_name", "type"}) ||
        !has_string(target, "display_name") || !has_string(target, "type")) {
        return false;
    }
    return !require_unique_key || has_string(target, "unique_key");
}

bool target_array_shape(const nlohmann::json& targets, bool require_unique_key) {
    if (!targets.is_array()) return false;
    for (const auto& target : targets) {
        if (!target_shape(target, require_unique_key)) return false;
    }
    return true;
}

bool reference_shape(const nlohmann::json& object) {
    return has_exact_keys(object, {"source_path", "directory"}) &&
           has_string(object, "source_path") &&
           has_string(object, "directory");
}

bool translation_unit_item_header_shape(const nlohmann::json& item) {
    return has_exact_keys(item, {"rank", "reference", "metrics", "targets",
                                 "diagnostics"}) &&
           has_positive_integer(item, "rank") && has_object(item, "reference") &&
           has_object(item, "metrics") && has_array(item, "targets") &&
           has_array(item, "diagnostics");
}

bool translation_unit_metrics_shape(const nlohmann::json& metrics) {
    return has_exact_keys(metrics, {"arg_count", "include_path_count", "define_count",
                                    "score"}) &&
           has_non_negative_integer(metrics, "arg_count") &&
           has_non_negative_integer(metrics, "include_path_count") &&
           has_non_negative_integer(metrics, "define_count") &&
           has_non_negative_integer(metrics, "score");
}

bool translation_unit_item_shape(const nlohmann::json& item) {
    return translation_unit_item_header_shape(item) && reference_shape(item["reference"]) &&
           translation_unit_metrics_shape(item["metrics"]) &&
           target_array_shape(item["targets"], false) && diagnostics_shape(item["diagnostics"]);
}

bool translation_unit_ranking_shape(const nlohmann::json& ranking) {
    if (!has_exact_keys(ranking, {"limit", "total_count", "returned_count",
                                  "truncated", "items"}) ||
        !has_non_negative_integer(ranking, "limit") ||
        !has_non_negative_integer(ranking, "total_count") ||
        !has_non_negative_integer(ranking, "returned_count") ||
        !has_boolean(ranking, "truncated") || !has_array(ranking, "items")) {
        return false;
    }
    for (const auto& item : ranking["items"]) {
        if (!translation_unit_item_shape(item)) return false;
    }
    return true;
}

bool affected_unit_shape(const nlohmann::json& item) {
    return has_exact_keys(item, {"reference", "targets"}) &&
           has_object(item, "reference") && has_array(item, "targets") &&
           reference_shape(item["reference"]) && target_array_shape(item["targets"], false);
}

bool affected_units_shape(const nlohmann::json& items) {
    if (!items.is_array()) return false;
    for (const auto& item : items) {
        if (!affected_unit_shape(item)) return false;
    }
    return true;
}

bool include_hotspot_item_header_shape(const nlohmann::json& item) {
    return has_exact_keys(item, {"header_path", "origin", "depth_kind",
                                 "affected_total_count", "affected_returned_count",
                                 "affected_truncated", "affected_translation_units",
                                 "diagnostics"}) &&
           has_string(item, "header_path") &&
           string_is_one_of(item, "origin", {"project", "external", "unknown"}) &&
           string_is_one_of(item, "depth_kind", {"direct", "indirect", "mixed"});
}

bool include_hotspot_item_payload_shape(const nlohmann::json& item) {
    return has_non_negative_integer(item, "affected_total_count") &&
           has_non_negative_integer(item, "affected_returned_count") &&
           has_boolean(item, "affected_truncated") &&
           has_array(item, "affected_translation_units") &&
           has_array(item, "diagnostics") &&
           affected_units_shape(item["affected_translation_units"]) &&
           diagnostics_shape(item["diagnostics"]);
}

bool include_hotspot_item_shape(const nlohmann::json& item) {
    return include_hotspot_item_header_shape(item) && include_hotspot_item_payload_shape(item);
}

bool include_hotspot_collection_shape(const nlohmann::json& hotspots) {
    return has_exact_keys(hotspots, {"limit", "total_count", "returned_count",
                                     "truncated", "excluded_unknown_count",
                                     "excluded_mixed_count", "items"}) &&
           has_non_negative_integer(hotspots, "limit") &&
           has_non_negative_integer(hotspots, "total_count") &&
           has_non_negative_integer(hotspots, "returned_count") &&
           has_boolean(hotspots, "truncated") &&
           has_non_negative_integer(hotspots, "excluded_unknown_count") &&
           has_non_negative_integer(hotspots, "excluded_mixed_count") &&
           has_array(hotspots, "items");
}

bool include_hotspots_shape(const nlohmann::json& hotspots) {
    if (!include_hotspot_collection_shape(hotspots)) return false;
    for (const auto& item : hotspots["items"]) {
        if (!include_hotspot_item_shape(item)) return false;
    }
    return true;
}

bool target_edge_shape(const nlohmann::json& edge) {
    return has_exact_keys(edge, {"from_display_name", "from_unique_key",
                                 "to_display_name", "to_unique_key", "kind",
                                 "resolution"}) &&
           has_string(edge, "from_unique_key") &&
           has_string(edge, "to_unique_key") && has_string(edge, "kind") &&
           string_is_one_of(edge, "kind", {"direct"}) &&
           string_is_one_of(edge, "resolution", {"resolved", "external"}) &&
           has_string(edge, "from_display_name") &&
           has_string(edge, "to_display_name");
}

bool target_graph_shape(const nlohmann::json& graph) {
    if (!has_exact_keys(graph, {"nodes", "edges"}) || !has_array(graph, "nodes") ||
        !has_array(graph, "edges")) {
        return false;
    }
    for (const auto& node : graph["nodes"]) {
        if (!target_shape(node, true)) return false;
    }
    for (const auto& edge : graph["edges"]) {
        if (!target_edge_shape(edge)) return false;
    }
    return true;
}

bool target_hubs_shape(const nlohmann::json& hubs) {
    if (!has_exact_keys(hubs, {"inbound", "outbound", "thresholds"}) ||
        !has_array(hubs, "inbound") || !has_array(hubs, "outbound") ||
        !has_object(hubs, "thresholds")) {
        return false;
    }
    const auto& thresholds = hubs["thresholds"];
    return has_exact_keys(thresholds, {"in_threshold", "out_threshold"}) &&
           has_non_negative_integer(thresholds, "in_threshold") &&
           has_non_negative_integer(thresholds, "out_threshold") &&
           target_array_shape(hubs["inbound"], true) &&
           target_array_shape(hubs["outbound"], true);
}

bool has_required_analysis_keys(const nlohmann::json& root) {
    for (const auto key : kRequiredAnalysisKeys) {
        if (!root.contains(std::string(key))) return false;
    }
    return root.contains("diagnostics");
}

bool analysis_inputs_shape(const nlohmann::json& inputs) {
    return has_exact_keys(inputs, {"compile_database_path", "compile_database_source",
                                   "cmake_file_api_path", "cmake_file_api_resolved_path",
                                   "cmake_file_api_source", "project_identity",
                                   "project_identity_source"}) &&
           has_nullable_string(inputs, "compile_database_path") &&
           nullable_source_shape(inputs, "compile_database_source") &&
           has_nullable_string(inputs, "cmake_file_api_path") &&
           has_nullable_string(inputs, "cmake_file_api_resolved_path") &&
           nullable_source_shape(inputs, "cmake_file_api_source");
}

bool required_sections_are_objects(const nlohmann::json& root) {
    return root["inputs"].is_object() && root["summary"].is_object() &&
           root["analysis_configuration"].is_object() &&
           root["analysis_section_states"].is_object() && root["include_filter"].is_object() &&
           root["translation_unit_ranking"].is_object() &&
           root["include_hotspots"].is_object() && root["target_graph"].is_object() &&
           root["target_hubs"].is_object();
}

bool required_leaf_sections_have_expected_shape(const nlohmann::json& root) {
    return root["target_graph_status"].is_string() &&
           root["diagnostics"].is_array();
}

AnalysisJsonReadResult validate_top_level_shape(const nlohmann::json& root) {
    if (!has_exact_keys(root, {"format", "format_version", "report_type", "inputs",
                               "summary", "analysis_configuration",
                               "analysis_section_states", "include_filter",
                               "translation_unit_ranking", "include_hotspots",
                               "target_graph_status", "target_graph", "target_hubs",
                               "diagnostics"}) ||
        !has_required_analysis_keys(root) ||
        !required_sections_are_objects(root) ||
        !required_leaf_sections_have_expected_shape(root)) {
        return schema_mismatch("analysis JSON does not match the analyze JSON schema");
    }
    return {};
}

bool analysis_configuration_header_shape(const nlohmann::json& configuration) {
    return has_exact_keys(configuration, {"analysis_sections", "tu_thresholds",
                                          "min_hotspot_tus",
                                          "target_hub_in_threshold",
                                          "target_hub_out_threshold"}) &&
           has_array(configuration, "analysis_sections") &&
           analysis_sections_shape(configuration["analysis_sections"]) &&
           has_object(configuration, "tu_thresholds");
}

bool tu_thresholds_shape(const nlohmann::json& thresholds) {
    return has_exact_keys(thresholds, {"arg_count", "include_path_count", "define_count"}) &&
           has_non_negative_integer(thresholds, "arg_count") &&
           has_non_negative_integer(thresholds, "include_path_count") &&
           has_non_negative_integer(thresholds, "define_count");
}

bool analysis_configuration_limits_shape(const nlohmann::json& configuration) {
    return has_non_negative_integer(configuration, "min_hotspot_tus") &&
           has_non_negative_integer(configuration, "target_hub_in_threshold") &&
           has_non_negative_integer(configuration, "target_hub_out_threshold");
}

bool analysis_configuration_shape(const nlohmann::json& configuration) {
    return analysis_configuration_header_shape(configuration) &&
           tu_thresholds_shape(configuration["tu_thresholds"]) &&
           analysis_configuration_limits_shape(configuration);
}

bool section_states_shape(const nlohmann::json& section_states) {
    return has_exact_keys(section_states, {"tu-ranking", "include-hotspots",
                                           "target-graph", "target-hubs"}) &&
           string_is_one_of(section_states, "tu-ranking",
                            {"active", "disabled", "not_loaded"}) &&
           string_is_one_of(section_states, "include-hotspots",
                            {"active", "disabled", "not_loaded"}) &&
           string_is_one_of(section_states, "target-graph",
                            {"active", "disabled", "not_loaded"}) &&
           string_is_one_of(section_states, "target-hubs",
                            {"active", "disabled", "not_loaded"});
}

bool include_filter_shape(const nlohmann::json& include_filter) {
    return has_exact_keys(include_filter, {"include_scope", "include_depth",
                                           "include_depth_limit_requested",
                                           "include_depth_limit_effective",
                                           "include_node_budget_requested",
                                           "include_node_budget_effective",
                                           "include_node_budget_reached"}) &&
           string_is_one_of(include_filter, "include_scope",
                            {"all", "project", "external", "unknown"}) &&
           string_is_one_of(include_filter, "include_depth",
                            {"all", "direct", "indirect"}) &&
           has_non_negative_integer(include_filter, "include_depth_limit_requested") &&
           has_non_negative_integer(include_filter, "include_depth_limit_effective") &&
           has_non_negative_integer(include_filter, "include_node_budget_requested") &&
           has_non_negative_integer(include_filter, "include_node_budget_effective") &&
           has_boolean(include_filter, "include_node_budget_reached");
}

bool analysis_payload_sections_shape(const nlohmann::json& root) {
    return translation_unit_ranking_shape(root["translation_unit_ranking"]) &&
           include_hotspots_shape(root["include_hotspots"]) &&
           string_is_one_of(root, "target_graph_status",
                            {"not_loaded", "loaded", "partial", "disabled"}) &&
           target_graph_shape(root["target_graph"]) &&
           target_hubs_shape(root["target_hubs"]) && diagnostics_shape(root["diagnostics"]);
}

AnalysisJsonReadResult validate_nested_shape(const nlohmann::json& root) {
    if (analysis_inputs_shape(root["inputs"]) && analysis_summary_shape(root["summary"]) &&
        analysis_configuration_shape(root["analysis_configuration"]) &&
        section_states_shape(root["analysis_section_states"]) &&
        include_filter_shape(root["include_filter"]) && analysis_payload_sections_shape(root)) {
        return {};
    }
    return schema_mismatch("analysis JSON does not match the analyze JSON schema");
}

AnalysisJsonReadResult validate_report_type(const nlohmann::json& root) {
    if (!root["report_type"].is_string()) {
        return schema_mismatch("analysis JSON report_type is not a string");
    }
    if (root["report_type"].get<std::string>() != "analyze") {
        return make_error(AnalysisJsonReadError::unsupported_report_type,
                          root["report_type"].get<std::string>());
    }
    return {};
}

AnalysisJsonReadResult validate_format(const nlohmann::json& root) {
    if (!root["format"].is_string() ||
        root["format"].get<std::string>() != "cmake-xray.analysis") {
        return schema_mismatch("analysis JSON format is not cmake-xray.analysis");
    }
    return {};
}

AnalysisJsonReadResult validate_format_version_shape(const nlohmann::json& root) {
    if (!root["format_version"].is_number_integer()) {
        return schema_mismatch("analysis JSON format_version is not an integer");
    }
    return {};
}

AnalysisJsonReadResult validate_format_version_supported(const AnalysisJsonReport& report) {
    if (report.format_version != kReportFormatVersion) {
        return make_error(AnalysisJsonReadError::incompatible_format_version,
                          "analysis JSON format_version is not supported for compare",
                          report);
    }
    return {};
}

AnalysisJsonReadResult validate_top_level_contract(const nlohmann::json& root) {
    if (!root.is_object() || !root.contains("report_type") ||
        !root.contains("format") || !root.contains("format_version")) {
        return schema_mismatch("analysis JSON does not match the analyze JSON schema");
    }
    if (auto rejected = validate_report_type(root); !rejected.is_success()) return rejected;
    if (auto rejected = validate_format(root); !rejected.is_success()) return rejected;
    if (auto rejected = validate_format_version_shape(root); !rejected.is_success()) return rejected;
    return {};
}

AnalysisJsonReadResult validate_v6_analysis_contract(const nlohmann::json& root) {
    if (auto rejected = validate_top_level_shape(root); !rejected.is_success()) return rejected;
    if (auto rejected = validate_nested_shape(root); !rejected.is_success()) return rejected;
    return {};
}

bool parse_project_identity_source(std::string_view value, ProjectIdentitySource& source) {
    if (value == "cmake_file_api_source_root") {
        source = ProjectIdentitySource::cmake_file_api_source_root;
        return true;
    }
    if (value == "fallback_compile_database_fingerprint") {
        source = ProjectIdentitySource::fallback_compile_database_fingerprint;
        return true;
    }
    return false;
}

AnalysisJsonReadResult populate_project_identity(const nlohmann::json& inputs,
                                                 AnalysisJsonReport& report) {
    if (!inputs["project_identity_source"].is_string()) {
        return make_error(AnalysisJsonReadError::invalid_project_identity_source,
                          inputs["project_identity_source"].dump());
    }

    const auto source_text = inputs["project_identity_source"].get<std::string>();
    if (!parse_project_identity_source(source_text, report.project_identity_source)) {
        return make_error(AnalysisJsonReadError::invalid_project_identity_source,
                          source_text);
    }

    if (!inputs["project_identity"].is_string() ||
        inputs["project_identity"].get<std::string>().empty()) {
        return make_error(AnalysisJsonReadError::unrecoverable_project_identity,
                          "analysis JSON project_identity is empty or unrecoverable");
    }

    report.project_identity = inputs["project_identity"].get<std::string>();
    return {};
}

void populate_raw_sections(const nlohmann::json& root, AnalysisJsonReport& report) {
    report.format_version = root["format_version"].get<int>();
    report.inputs = root["inputs"];
    report.summary = root["summary"];
    report.analysis_configuration = root["analysis_configuration"];
    report.analysis_section_states = root["analysis_section_states"];
    report.include_filter = root["include_filter"];
    report.translation_unit_ranking = root["translation_unit_ranking"];
    report.include_hotspots = root["include_hotspots"];
    report.target_graph_status = root["target_graph_status"];
    report.target_graph = root["target_graph"];
    report.target_hubs = root["target_hubs"];
    report.diagnostics = root["diagnostics"];
}

AnalysisJsonReadResult build_report(std::string_view path, const nlohmann::json& root) {
    if (auto rejected = validate_top_level_contract(root); !rejected.is_success()) {
        return rejected;
    }

    AnalysisJsonReport report;
    report.path = std::string(path);
    report.format_version = root["format_version"].get<int>();
    if (auto rejected = validate_format_version_supported(report); !rejected.is_success()) {
        return rejected;
    }
    if (auto rejected = validate_v6_analysis_contract(root); !rejected.is_success()) {
        return rejected;
    }
    populate_raw_sections(root, report);
    if (auto rejected = populate_project_identity(report.inputs, report);
        !rejected.is_success()) {
        return rejected;
    }

    AnalysisJsonReadResult result;
    result.report = std::move(report);
    return result;
}

std::string string_value(const nlohmann::json& object, std::string_view key) {
    const auto found = object.find(std::string(key));
    if (found == object.end() || !found->is_string()) return {};
    return found->get<std::string>();
}

int int_value(const nlohmann::json& object, std::string_view key) {
    const auto found = object.find(std::string(key));
    if (found == object.end() || !found->is_number_integer()) return 0;
    return found->get<int>();
}

xray::hexagon::model::CompareDiagnosticSnapshot diagnostic_snapshot(
    const nlohmann::json& diagnostic) {
    return {string_value(diagnostic, "severity"), string_value(diagnostic, "message")};
}

std::vector<xray::hexagon::model::CompareDiagnosticSnapshot> diagnostic_snapshots(
    const nlohmann::json& diagnostics) {
    std::vector<xray::hexagon::model::CompareDiagnosticSnapshot> out;
    if (!diagnostics.is_array()) return out;
    for (const auto& diagnostic : diagnostics) out.push_back(diagnostic_snapshot(diagnostic));
    return std::vector<xray::hexagon::model::CompareDiagnosticSnapshot>(std::move(out));
}

xray::hexagon::model::CompareTargetSnapshot target_snapshot(const nlohmann::json& target) {
    return {string_value(target, "display_name"), string_value(target, "type"),
            string_value(target, "unique_key")};
}

std::vector<xray::hexagon::model::CompareTargetSnapshot> target_snapshots(
    const nlohmann::json& targets) {
    std::vector<xray::hexagon::model::CompareTargetSnapshot> out;
    if (!targets.is_array()) return out;
    for (const auto& target : targets) out.push_back(target_snapshot(target));
    return std::vector<xray::hexagon::model::CompareTargetSnapshot>(std::move(out));
}

xray::hexagon::model::CompareTranslationUnitSnapshot translation_unit_snapshot(
    const nlohmann::json& item) {
    xray::hexagon::model::CompareTranslationUnitSnapshot out;
    const auto reference = item.value("reference", nlohmann::json::object());
    const auto metrics = item.value("metrics", nlohmann::json::object());
    out.source_path = string_value(reference, "source_path");
    out.build_context_key = string_value(reference, "directory");
    out.arg_count = int_value(metrics, "arg_count");
    out.include_path_count = int_value(metrics, "include_path_count");
    out.define_count = int_value(metrics, "define_count");
    out.score = int_value(metrics, "score");
    out.targets = target_snapshots(item.value("targets", nlohmann::json::array()));
    out.diagnostics = diagnostic_snapshots(item.value("diagnostics", nlohmann::json::array()));
    return out;
}

xray::hexagon::model::CompareHotspotAffectedUnitSnapshot affected_unit_snapshot(
    const nlohmann::json& item) {
    const auto reference = item.value("reference", nlohmann::json::object());
    return {string_value(reference, "source_path"), string_value(reference, "directory")};
}

std::vector<xray::hexagon::model::CompareHotspotAffectedUnitSnapshot> affected_unit_snapshots(
    const nlohmann::json& items) {
    std::vector<xray::hexagon::model::CompareHotspotAffectedUnitSnapshot> out;
    if (!items.is_array()) return out;
    for (const auto& item : items) out.push_back(affected_unit_snapshot(item));
    return std::vector<xray::hexagon::model::CompareHotspotAffectedUnitSnapshot>(
        std::move(out));
}

xray::hexagon::model::CompareIncludeHotspotSnapshot hotspot_snapshot(
    const nlohmann::json& item) {
    return {string_value(item, "header_path"),
            string_value(item, "origin"),
            string_value(item, "depth_kind"),
            int_value(item, "affected_total_count"),
            affected_unit_snapshots(
                item.value("affected_translation_units", nlohmann::json::array())),
            diagnostic_snapshots(item.value("diagnostics", nlohmann::json::array()))};
}

xray::hexagon::model::CompareTargetEdgeSnapshot edge_snapshot(const nlohmann::json& edge) {
    return {string_value(edge, "from_unique_key"), string_value(edge, "to_unique_key"),
            string_value(edge, "kind"), string_value(edge, "resolution"),
            string_value(edge, "from_display_name"), string_value(edge, "to_display_name")};
}

template <typename T, typename Mapper>
std::vector<T> map_array(const nlohmann::json& items, Mapper mapper) {
    std::vector<T> out;
    if (!items.is_array()) return out;
    for (const auto& item : items) out.push_back(mapper(item));
    return std::vector<T>(std::move(out));
}

xray::hexagon::model::AnalysisReportSnapshot to_snapshot(const AnalysisJsonReport& report) {
    xray::hexagon::model::AnalysisReportSnapshot out;
    out.path = report.path;
    out.format_version = report.format_version;
    out.project_identity = report.project_identity;
    out.project_identity_source = report.project_identity_source;

    const auto thresholds =
        report.analysis_configuration.value("tu_thresholds", nlohmann::json::object());
    out.configuration.analysis_sections =
        report.analysis_configuration.value("analysis_sections", std::vector<std::string>{});
    out.configuration.tu_threshold_arg_count = int_value(thresholds, "arg_count");
    out.configuration.tu_threshold_include_path_count =
        int_value(thresholds, "include_path_count");
    out.configuration.tu_threshold_define_count = int_value(thresholds, "define_count");
    out.configuration.min_hotspot_tus = int_value(report.analysis_configuration, "min_hotspot_tus");
    out.configuration.target_hub_in_threshold =
        int_value(report.analysis_configuration, "target_hub_in_threshold");
    out.configuration.target_hub_out_threshold =
        int_value(report.analysis_configuration, "target_hub_out_threshold");
    out.configuration.include_scope = string_value(report.include_filter, "include_scope");
    out.configuration.include_depth = string_value(report.include_filter, "include_depth");
    out.target_graph_state =
        report.target_graph_status.is_string() ? report.target_graph_status.get<std::string>() : "";
    const auto target_hubs_section_state =
        string_value(report.analysis_section_states, "target-hubs");
    out.target_hubs_state =
        target_hubs_section_state == "disabled" ? "disabled" : out.target_graph_state;

    out.translation_units =
        map_array<xray::hexagon::model::CompareTranslationUnitSnapshot>(
            report.translation_unit_ranking.value("items", nlohmann::json::array()),
            translation_unit_snapshot);
    out.include_hotspots =
        map_array<xray::hexagon::model::CompareIncludeHotspotSnapshot>(
            report.include_hotspots.value("items", nlohmann::json::array()), hotspot_snapshot);
    out.target_nodes = map_array<xray::hexagon::model::CompareTargetSnapshot>(
        report.target_graph.value("nodes", nlohmann::json::array()), target_snapshot);
    out.target_edges = map_array<xray::hexagon::model::CompareTargetEdgeSnapshot>(
        report.target_graph.value("edges", nlohmann::json::array()), edge_snapshot);
    out.target_hubs_in = map_array<xray::hexagon::model::CompareTargetSnapshot>(
        report.target_hubs.value("inbound", nlohmann::json::array()), target_snapshot);
    out.target_hubs_out = map_array<xray::hexagon::model::CompareTargetSnapshot>(
        report.target_hubs.value("outbound", nlohmann::json::array()), target_snapshot);
    return out;
}

}  // namespace

AnalysisReportReadError analysis_json_read_error_to_port_error(AnalysisJsonReadError error) {
    switch (error) {
        case AnalysisJsonReadError::none:
            return AnalysisReportReadError::none;
        case AnalysisJsonReadError::file_not_accessible:
            return AnalysisReportReadError::file_not_accessible;
        case AnalysisJsonReadError::invalid_json:
            return AnalysisReportReadError::invalid_json;
        case AnalysisJsonReadError::schema_mismatch:
            return AnalysisReportReadError::schema_mismatch;
        case AnalysisJsonReadError::unsupported_report_type:
            return AnalysisReportReadError::unsupported_report_type;
        case AnalysisJsonReadError::incompatible_format_version:
            return AnalysisReportReadError::incompatible_format_version;
        case AnalysisJsonReadError::invalid_project_identity_source:
            return AnalysisReportReadError::invalid_project_identity_source;
        case AnalysisJsonReadError::unrecoverable_project_identity:
            return AnalysisReportReadError::unrecoverable_project_identity;
    }
    return AnalysisReportReadError::schema_mismatch;
}

std::string project_identity_source_text(ProjectIdentitySource source) {
    switch (source) {
        case ProjectIdentitySource::cmake_file_api_source_root:
            return "cmake_file_api_source_root";
        case ProjectIdentitySource::fallback_compile_database_fingerprint:
            return "fallback_compile_database_fingerprint";
    }
    return "";
}

AnalysisJsonReadResult AnalysisJsonReader::read(std::string_view path) const {
    const auto path_str = std::string(path);
    std::ifstream file(path_str);
    if (!file.is_open()) {
        return make_error(AnalysisJsonReadError::file_not_accessible,
                          "cannot read analysis JSON file: " + path_str);
    }

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(file);
    } catch (const nlohmann::json::parse_error&) {
        return make_error(AnalysisJsonReadError::invalid_json,
                          "analysis JSON file is not valid JSON: " + path_str);
    }

    return build_report(path, root);
}

AnalysisReportReadResult AnalysisJsonReader::read_analysis_report(
    const std::filesystem::path& path) const {
    const auto read_result = read(path.string());
    AnalysisReportReadResult result;
    result.error = analysis_json_read_error_to_port_error(read_result.error);
    result.message = read_result.error_message;
    if (read_result.is_success()) {
        result.report = to_snapshot(read_result.report);
    } else if (read_result.error == AnalysisJsonReadError::incompatible_format_version) {
        result.report.path = read_result.report.path;
        result.report.format_version = read_result.report.format_version;
    }
    return result;
}

}  // namespace xray::adapters::input
