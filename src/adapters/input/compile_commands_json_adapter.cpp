#include "adapters/input/compile_commands_json_adapter.h"

#include <cstddef>
#include <fstream>
#include <string_view>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "hexagon/model/build_model_result.h"
#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/compile_entry.h"

namespace xray::adapters::input {

namespace {

using xray::hexagon::model::CompileDatabaseError;
using xray::hexagon::model::CompileDatabaseResult;
using xray::hexagon::model::CompileEntry;
using xray::hexagon::model::EntryDiagnostic;

constexpr std::size_t max_reported_entry_errors = 20;

CompileDatabaseResult make_error(CompileDatabaseError error, std::string description) {
    return CompileDatabaseResult{error, std::move(description), {}, {}};
}

struct EntryValidation {
    std::string file;
    std::string directory;
    std::vector<std::string> arguments;
    std::string command;
    bool uses_arguments{false};
    std::string error_message;
    bool valid{true};
};

struct RequiredStringField {
    std::string_view key;
    std::string_view missing_field_message;
    std::string* target;
};

void append_problem(std::string& problems, std::string_view problem) {
    if (!problems.empty()) problems += ", ";
    problems += problem;
}

bool try_read_required_string(const nlohmann::json& obj, std::string_view key,
                              std::string& target) {
    const auto key_str = std::string(key);
    if (!obj.contains(key_str) || !obj[key_str].is_string()) return false;

    target = obj[key_str].get<std::string>();
    return !target.empty();
}

void validate_required_string(const nlohmann::json& obj, const RequiredStringField& field,
                              std::string& problems) {
    if (!try_read_required_string(obj, field.key, *field.target)) {
        append_problem(problems, field.missing_field_message);
    }
}

std::vector<std::string> read_arguments(const nlohmann::json& obj) {
    if (!obj.contains("arguments") || !obj["arguments"].is_array() || obj["arguments"].empty()) {
        return {};
    }

    std::vector<std::string> arguments;
    for (const auto& arg : obj["arguments"]) {
        if (arg.is_string()) arguments.push_back(arg.get<std::string>());
    }
    return arguments;
}

bool try_read_command(const nlohmann::json& obj, std::string& command) {
    return try_read_required_string(obj, "command", command);
}

void validate_arguments(const nlohmann::json& obj, EntryValidation& result,
                        std::string& problems) {
    result.arguments = read_arguments(obj);
    if (!result.arguments.empty()) {
        result.uses_arguments = true;
        return;
    }

    if (try_read_command(obj, result.command)) return;

    append_problem(problems, "missing \"command\" and \"arguments\"");
}

EntryValidation validate_entry(const nlohmann::json& obj) {
    EntryValidation result;
    std::string problems;

    validate_required_string(
        obj, RequiredStringField{"file", "missing \"file\" field", &result.file}, problems);
    validate_required_string(obj,
                             RequiredStringField{"directory", "missing \"directory\" field",
                                                 &result.directory},
                             problems);
    validate_arguments(obj, result, problems);

    if (problems.empty()) return result;

    result.valid = false;
    result.error_message = std::move(problems);
    return result;
}

CompileDatabaseResult build_entry_result(const nlohmann::json& root, std::string_view path) {
    std::vector<CompileEntry> entries;
    std::vector<EntryDiagnostic> diagnostics;
    std::size_t total_invalid = 0;
    entries.reserve(root.size());

    for (std::size_t i = 0; i < root.size(); ++i) {
        auto validation = validate_entry(root[i]);
        if (!validation.valid) {
            ++total_invalid;
            if (diagnostics.size() < max_reported_entry_errors) {
                diagnostics.emplace_back(i, std::move(validation.error_message));
            }
            continue;
        }

        if (validation.uses_arguments) {
            entries.push_back(CompileEntry::from_arguments(
                std::move(validation.file), std::move(validation.directory),
                std::move(validation.arguments)));
        } else {
            entries.push_back(CompileEntry::from_command(
                std::move(validation.file), std::move(validation.directory),
                std::move(validation.command)));
        }
    }

    if (total_invalid == 0) {
        return CompileDatabaseResult{CompileDatabaseError::none, {}, std::move(entries), {}};
    }

    std::string description = "compile_commands.json contains " + std::to_string(total_invalid) +
                              " invalid entries: " + std::string(path);
    return CompileDatabaseResult{CompileDatabaseError::invalid_entries, std::move(description), {},
                                 std::move(diagnostics), total_invalid};
}

CompileDatabaseResult load_compile_database(std::string_view path) {
    const auto path_str = std::string(path);
    std::ifstream file(path_str);
    if (!file.is_open()) {
        return make_error(CompileDatabaseError::file_not_accessible,
                          "cannot open compile_commands.json: " + std::string(path));
    }

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(file);
    } catch (const nlohmann::json::parse_error&) {
        return make_error(CompileDatabaseError::invalid_json,
                          "compile_commands.json is not valid JSON: " + std::string(path));
    }

    if (!root.is_array()) {
        return make_error(CompileDatabaseError::not_an_array,
                          "compile_commands.json root is not an array: " + std::string(path));
    }

    if (root.empty()) {
        return make_error(CompileDatabaseError::empty_database,
                          "compile_commands.json is empty: " + std::string(path));
    }

    return build_entry_result(root, path);
}

}  // namespace

xray::hexagon::model::BuildModelResult CompileCommandsJsonAdapter::load_build_model(
    std::string_view path) const {
    // source=exact, no target metadata; remaining fields (target_assignments,
    // source_root, diagnostics) are populated by CmakeFileApiAdapter in M4.
    // Aggregate return avoids a gcov NRVO artifact on the closing brace.
    return {xray::hexagon::model::ObservationSource::exact,
            xray::hexagon::model::TargetMetadataStatus::not_loaded,
            load_compile_database(path), {}, {}, {}};
}

}  // namespace xray::adapters::input
