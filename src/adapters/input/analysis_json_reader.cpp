#include "adapters/input/analysis_json_reader.h"

#include <array>
#include <fstream>
#include <string>
#include <utility>

#include "hexagon/model/report_format_version.h"

namespace xray::adapters::input {

namespace {

using xray::hexagon::model::ProjectIdentitySource;
using xray::hexagon::model::kReportFormatVersion;

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

AnalysisJsonReadResult schema_mismatch(std::string message) {
    return make_error(AnalysisJsonReadError::schema_mismatch, std::move(message));
}

bool has_required_analysis_keys(const nlohmann::json& root) {
    for (const auto key : kRequiredAnalysisKeys) {
        if (!root.contains(std::string(key))) return false;
    }
    return root.contains("diagnostics");
}

bool has_required_input_keys(const nlohmann::json& inputs) {
    return inputs.is_object() && inputs.contains("compile_database_path") &&
           inputs.contains("compile_database_source") && inputs.contains("cmake_file_api_path") &&
           inputs.contains("cmake_file_api_resolved_path") &&
           inputs.contains("cmake_file_api_source") && inputs.contains("project_identity") &&
           inputs.contains("project_identity_source");
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
    if (!root.is_object() || !has_required_analysis_keys(root) ||
        !required_sections_are_objects(root) ||
        !required_leaf_sections_have_expected_shape(root)) {
        return schema_mismatch("analysis JSON does not match the analyze JSON schema");
    }
    return {};
}

AnalysisJsonReadResult validate_report_type(const nlohmann::json& root) {
    if (!root["report_type"].is_string()) {
        return schema_mismatch("analysis JSON report_type is not a string");
    }
    if (root["report_type"].get<std::string>() != "analyze") {
        return make_error(AnalysisJsonReadError::unsupported_report_type,
                          "analysis JSON report_type is not analyze");
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

AnalysisJsonReadResult validate_format_version(const nlohmann::json& root) {
    if (!root["format_version"].is_number_integer()) {
        return schema_mismatch("analysis JSON format_version is not an integer");
    }
    if (root["format_version"].get<int>() != kReportFormatVersion) {
        return make_error(AnalysisJsonReadError::incompatible_format_version,
                          "analysis JSON format_version is not supported for compare");
    }
    return {};
}

AnalysisJsonReadResult validate_top_level_contract(const nlohmann::json& root) {
    if (auto rejected = validate_top_level_shape(root); !rejected.is_success()) return rejected;
    if (auto rejected = validate_report_type(root); !rejected.is_success()) return rejected;
    if (auto rejected = validate_format(root); !rejected.is_success()) return rejected;
    if (auto rejected = validate_format_version(root); !rejected.is_success()) return rejected;
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
    if (!has_required_input_keys(inputs)) {
        return make_error(AnalysisJsonReadError::schema_mismatch,
                          "analysis JSON inputs do not match the analyze JSON schema");
    }
    if (!inputs["project_identity_source"].is_string()) {
        return make_error(AnalysisJsonReadError::invalid_project_identity_source,
                          "analysis JSON project_identity_source is invalid");
    }

    const auto source_text = inputs["project_identity_source"].get<std::string>();
    if (!parse_project_identity_source(source_text, report.project_identity_source)) {
        return make_error(AnalysisJsonReadError::invalid_project_identity_source,
                          "analysis JSON project_identity_source is invalid");
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
    populate_raw_sections(root, report);
    if (auto rejected = populate_project_identity(report.inputs, report);
        !rejected.is_success()) {
        return rejected;
    }

    AnalysisJsonReadResult result;
    result.report = std::move(report);
    return result;
}

}  // namespace

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

}  // namespace xray::adapters::input
