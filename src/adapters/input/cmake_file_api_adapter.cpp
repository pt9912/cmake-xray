#include "adapters/input/cmake_file_api_adapter.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "hexagon/model/build_model_result.h"
#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/compile_entry.h"
#include "hexagon/model/diagnostic.h"
#include "hexagon/model/observation_source.h"
#include "hexagon/model/target_info.h"

namespace xray::adapters::input {

namespace {

using xray::hexagon::model::BuildModelResult;
using xray::hexagon::model::CompileDatabaseError;
using xray::hexagon::model::CompileDatabaseResult;
using xray::hexagon::model::CompileEntry;
using xray::hexagon::model::Diagnostic;
using xray::hexagon::model::DiagnosticSeverity;
using xray::hexagon::model::ObservationSource;
using xray::hexagon::model::TargetAssignment;
using xray::hexagon::model::TargetInfo;
using xray::hexagon::model::TargetMetadataStatus;

BuildModelResult make_file_api_error(CompileDatabaseError error, std::string description) {
    BuildModelResult result;
    result.source = ObservationSource::derived;
    result.compile_database = CompileDatabaseResult{error, std::move(description), {}, {}};
    return result;
}

std::filesystem::path resolve_reply_directory(const std::filesystem::path& path) {
    if (path.filename() == "reply") return path;
    return path / ".cmake" / "api" / "v1" / "reply";
}

std::string extract_suffix(const std::string& filename, std::string_view prefix) {
    // index-<suffix>.json or error-<suffix>.json -> suffix
    const auto prefix_end = prefix.size();
    const auto json_ext = filename.rfind(".json");
    if (json_ext == std::string::npos || json_ext <= prefix_end) return {};
    return filename.substr(prefix_end, json_ext - prefix_end);
}

struct ReplyCandidate {
    std::filesystem::path path;
    std::string suffix;
    bool is_error{false};
};

std::vector<ReplyCandidate> scan_reply_candidates(const std::filesystem::path& reply_dir) {
    std::vector<ReplyCandidate> candidates;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(reply_dir, ec)) {
        if (!entry.is_regular_file()) continue;
        const auto filename = entry.path().filename().string();
        if (filename.starts_with("index-") && filename.ends_with(".json")) {
            auto suffix = extract_suffix(filename, "index-");
            if (!suffix.empty())
                candidates.push_back({entry.path(), std::move(suffix), false});
        } else if (filename.starts_with("error-") && filename.ends_with(".json")) {
            auto suffix = extract_suffix(filename, "error-");
            if (!suffix.empty())
                candidates.push_back({entry.path(), std::move(suffix), true});
        }
    }
    return candidates;
}

const ReplyCandidate* find_latest_candidate(const std::vector<ReplyCandidate>& candidates) {
    if (candidates.empty()) return nullptr;
    const ReplyCandidate* latest = &candidates[0];
    for (std::size_t i = 1; i < candidates.size(); ++i) {
        if (candidates[i].suffix > latest->suffix) latest = &candidates[i];
    }
    return latest;
}

nlohmann::json parse_json_file(const std::filesystem::path& path, bool& parse_ok) {
    std::ifstream file(path);
    if (!file.is_open()) {
        parse_ok = false;
        return {};
    }
    try {
        auto result = nlohmann::json::parse(file);
        parse_ok = true;
        return result;
    } catch (const nlohmann::json::parse_error&) {
        parse_ok = false;
        return {};
    }
}

std::string find_codemodel_json_file(const nlohmann::json& index) {
    if (!index.contains("objects") || !index["objects"].is_array()) return {};
    for (const auto& obj : index["objects"]) {
        if (obj.value("kind", "") == "codemodel" && obj.contains("jsonFile")) {
            return obj["jsonFile"].get<std::string>();
        }
    }
    return {};
}

std::string normalize_path(const std::filesystem::path& path) {
    return std::filesystem::absolute(path).lexically_normal().generic_string();
}

std::string resolve_source_path(const std::string& source_path_raw,
                                const std::filesystem::path& source_root) {
    const std::filesystem::path source_path{source_path_raw};
    if (source_path.is_absolute()) return normalize_path(source_path);
    return normalize_path(source_root / source_path);
}

struct ParsedTarget {
    TargetInfo info;
    std::vector<CompileEntry> entries;
    // Maps each entry's index to the target info for assignment building
};

struct ParsedCodemodel {
    std::string source_root;
    std::string build_root;
    std::vector<std::string> target_json_files;
    std::vector<std::string> target_names;
    // directories[].build for each target's directoryIndex
    std::vector<std::string> target_build_dirs;
};

BuildModelResult parse_and_load(const std::filesystem::path& reply_dir, bool is_retry);

BuildModelResult do_parse_and_load(const std::filesystem::path& reply_dir, bool is_retry) {
    // Step 2: find reply entry
    const auto candidates = scan_reply_candidates(reply_dir);
    const auto* latest = find_latest_candidate(candidates);
    if (latest == nullptr) {
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                  "no index or error reply found in " + reply_dir.string());
    }
    if (latest->is_error) {
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                  "cmake file api error reply is the most recent entry: " +
                                      latest->path.string());
    }

    // Parse index
    bool parse_ok = false;
    const auto index = parse_json_file(latest->path, parse_ok);
    if (!parse_ok) {
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                  "cmake file api index is not valid JSON: " +
                                      latest->path.string());
    }

    // Step 3: find codemodel reference
    const auto codemodel_json_file = find_codemodel_json_file(index);
    if (codemodel_json_file.empty()) {
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                  "cmake file api index does not contain a codemodel object");
    }

    // Step 4: parse codemodel
    const auto codemodel_path = reply_dir / codemodel_json_file;
    const auto codemodel = parse_json_file(codemodel_path, parse_ok);
    if (!parse_ok) {
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                  "cmake file api codemodel is not valid JSON: " +
                                      codemodel_path.string());
    }

    if (!codemodel.contains("paths") || !codemodel["paths"].contains("source") ||
        !codemodel["paths"].contains("build")) {
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                  "cmake file api codemodel is missing paths.source or paths.build");
    }

    const auto source_root = codemodel["paths"]["source"].get<std::string>();
    const auto build_root = codemodel["paths"]["build"].get<std::string>();

    if (!codemodel.contains("configurations") || !codemodel["configurations"].is_array() ||
        codemodel["configurations"].empty()) {
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                  "cmake file api codemodel contains no configurations");
    }

    if (codemodel["configurations"].size() > 1) {
        return make_file_api_error(
            CompileDatabaseError::file_api_invalid,
            "cmake file api codemodel contains " +
                std::to_string(codemodel["configurations"].size()) +
                " configurations; multi-config generators are not supported in this version");
    }

    const auto& config = codemodel["configurations"][0];

    if (!config.contains("targets") || !config["targets"].is_array() ||
        config["targets"].empty()) {
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                  "cmake file api codemodel configuration contains no targets");
    }

    // Extract directories for build path resolution
    std::vector<std::string> directory_build_paths;
    if (config.contains("directories") && config["directories"].is_array()) {
        for (const auto& dir : config["directories"]) {
            directory_build_paths.push_back(dir.value("build", "."));
        }
    }

    // Step 5: parse targets
    const std::filesystem::path source_root_path{source_root};
    const std::filesystem::path build_root_path{build_root};
    std::vector<CompileEntry> all_entries;
    // observation_key -> list of targets
    std::map<std::string, std::vector<TargetInfo>> assignments;

    for (const auto& target_ref : config["targets"]) {
        if (!target_ref.contains("jsonFile")) continue;

        const auto target_json_file = target_ref["jsonFile"].get<std::string>();
        const auto target_path = reply_dir / target_json_file;
        const auto target = parse_json_file(target_path, parse_ok);

        if (!parse_ok) {
            if (is_retry) {
                return make_file_api_error(
                    CompileDatabaseError::file_api_invalid,
                    "cmake file api target reply is not accessible after retry: " +
                        target_path.string());
            }
            // Retry: re-scan for newer index
            return parse_and_load(reply_dir, true);
        }

        const auto target_name = target.value("name", "");
        const auto target_type = target.value("type", "");
        if (target_name.empty()) continue;

        const auto target_unique_key = target_name + "::" + target_type;
        const TargetInfo target_info{target_name, target_type, target_unique_key};

        // Resolve build directory for this target
        const auto dir_index = target.value("directoryIndex", 0);
        const auto dir_build = (dir_index < directory_build_paths.size())
                                   ? directory_build_paths[dir_index]
                                   : ".";
        const auto resolved_directory =
            normalize_path(build_root_path / dir_build);

        if (!target.contains("sources") || !target["sources"].is_array()) continue;
        if (!target.contains("compileGroups") || !target["compileGroups"].is_array()) continue;

        const auto& sources = target["sources"];
        const auto& compile_groups = target["compileGroups"];

        for (const auto& source : sources) {
            if (!source.contains("compileGroupIndex")) continue;

            const auto group_index = source["compileGroupIndex"].get<std::size_t>();
            if (group_index >= compile_groups.size()) continue;

            const auto source_path_raw = source.value("path", "");
            if (source_path_raw.empty()) continue;

            const auto resolved_source =
                resolve_source_path(source_path_raw, source_root_path);

            // Build arguments from structured fields only
            const auto& group = compile_groups[group_index];
            const auto language = group.value("language", "CXX");
            std::vector<std::string> arguments;
            arguments.push_back(language == "CXX" ? "c++" : "cc");

            if (group.contains("includes") && group["includes"].is_array()) {
                for (const auto& inc : group["includes"]) {
                    const auto inc_path = inc.value("path", "");
                    if (inc_path.empty()) continue;
                    const auto is_system = inc.value("isSystem", false);
                    arguments.push_back(is_system ? "-isystem" : "-I");
                    arguments.push_back(inc_path);
                }
            }

            if (group.contains("defines") && group["defines"].is_array()) {
                for (const auto& def : group["defines"]) {
                    const auto define = def.value("define", "");
                    if (!define.empty()) {
                        arguments.push_back("-D" + define);
                    }
                }
            }

            arguments.push_back("-c");
            arguments.push_back(resolved_source);

            all_entries.push_back(
                CompileEntry::from_arguments(source_path_raw, resolved_directory,
                                             std::move(arguments)));

            // Track target assignment by observation key
            const auto observation_key = resolved_source + "|" + resolved_directory;
            assignments[observation_key].push_back(target_info);
        }
    }

    if (all_entries.empty()) {
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                  "cmake file api contains no compilable sources");
    }

    // Assemble result
    // Deduplicate targets per assignment and sort deterministically
    std::vector<TargetAssignment> target_assignments;
    for (auto& [key, targets] : assignments) {
        // Sort targets deterministically by (display_name, type)
        std::sort(targets.begin(), targets.end(), [](const auto& a, const auto& b) {
            return a.unique_key < b.unique_key;
        });
        // Remove duplicate targets (same name+type from same observation)
        targets.erase(std::unique(targets.begin(), targets.end(),
                                  [](const auto& a, const auto& b) {
                                      return a.unique_key == b.unique_key;
                                  }),
                      targets.end());
        target_assignments.push_back({key, std::move(targets)});
    }

    // Sort assignments deterministically
    std::sort(target_assignments.begin(), target_assignments.end(),
              [](const auto& a, const auto& b) {
                  return a.observation_key < b.observation_key;
              });

    BuildModelResult result;
    result.source = ObservationSource::derived;
    result.target_metadata = TargetMetadataStatus::loaded;
    result.compile_database =
        CompileDatabaseResult{CompileDatabaseError::none, {}, std::move(all_entries), {}};
    result.target_assignments = std::move(target_assignments);
    result.source_root = source_root;
    return result;
}

BuildModelResult parse_and_load(const std::filesystem::path& reply_dir, bool is_retry) {
    return do_parse_and_load(reply_dir, is_retry);
}

}  // namespace

xray::hexagon::model::BuildModelResult CmakeFileApiAdapter::load_build_model(
    std::string_view path) const {
    const auto reply_dir = resolve_reply_directory(std::filesystem::path{std::string(path)});

    std::error_code ec;
    if (!std::filesystem::is_directory(reply_dir, ec) || ec) {
        return make_file_api_error(
            CompileDatabaseError::file_api_not_accessible,
            "cannot access cmake file api reply directory: " + reply_dir.string());
    }

    return parse_and_load(reply_dir, false);
}

}  // namespace xray::adapters::input
