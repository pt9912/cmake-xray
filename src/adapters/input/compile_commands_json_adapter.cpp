#include "adapters/input/compile_commands_json_adapter.h"

#include <cstddef>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

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
    std::string error_message;
    bool valid{true};
};

EntryValidation validate_entry(const nlohmann::json& obj) {
    EntryValidation result;
    std::string problems;

    bool has_file = obj.contains("file") && obj["file"].is_string() &&
                    !obj["file"].get<std::string>().empty();
    bool has_directory = obj.contains("directory") && obj["directory"].is_string() &&
                         !obj["directory"].get<std::string>().empty();
    bool has_command = obj.contains("command") && obj["command"].is_string() &&
                       !obj["command"].get<std::string>().empty();

    if (!has_file) {
        problems += "missing \"file\" field";
    } else {
        result.file = obj["file"].get<std::string>();
    }

    if (!has_directory) {
        if (!problems.empty()) problems += ", ";
        problems += "missing \"directory\" field";
    } else {
        result.directory = obj["directory"].get<std::string>();
    }

    bool arguments_usable = false;
    if (obj.contains("arguments") && obj["arguments"].is_array() &&
        !obj["arguments"].empty()) {
        for (const auto& arg : obj["arguments"]) {
            if (arg.is_string()) {
                result.arguments.push_back(arg.get<std::string>());
            }
        }
        arguments_usable = !result.arguments.empty();
    }

    if (!arguments_usable && has_command) {
        result.arguments = {obj["command"].get<std::string>()};
    } else if (!arguments_usable) {
        if (!problems.empty()) problems += ", ";
        problems += "missing \"command\" and \"arguments\"";
    }

    if (!problems.empty()) {
        result.valid = false;
        result.error_message = std::move(problems);
    }

    return result;
}

}  // namespace

CompileDatabaseResult CompileCommandsJsonAdapter::load_compile_database(
    std::string_view path) const {
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
        } else {
            entries.emplace_back(std::move(validation.file),
                                 std::move(validation.directory),
                                 std::move(validation.arguments));
        }
    }

    if (total_invalid > 0) {
        std::string description = "compile_commands.json contains " +
                                  std::to_string(total_invalid) +
                                  " invalid entries: " + std::string(path);
        return CompileDatabaseResult{CompileDatabaseError::invalid_entries,
                                     std::move(description), {},
                                     std::move(diagnostics), total_invalid};
    }

    return CompileDatabaseResult{CompileDatabaseError::none, {},
                                 std::move(entries), {}};
}

}  // namespace xray::adapters::input
