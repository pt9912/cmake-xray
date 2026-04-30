#include "adapters/input/cmake_file_api_adapter.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "hexagon/model/build_model_result.h"
#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/compile_entry.h"
#include "hexagon/model/diagnostic.h"
#include "hexagon/model/observation_source.h"
#include "hexagon/model/target_graph.h"
#include "hexagon/model/target_info.h"
#include "hexagon/services/target_graph_support.h"

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
using xray::hexagon::model::TargetDependency;
using xray::hexagon::model::TargetDependencyKind;
using xray::hexagon::model::TargetDependencyResolution;
using xray::hexagon::model::TargetGraph;
using xray::hexagon::model::TargetGraphStatus;
using xray::hexagon::model::TargetInfo;
using xray::hexagon::model::TargetMetadataStatus;

BuildModelResult make_file_api_error(CompileDatabaseError error, std::string description) {
    BuildModelResult result;
    result.source = ObservationSource::derived;
    result.compile_database = CompileDatabaseResult{error, std::move(description), {}, {}};
    return result;
}

std::filesystem::path resolve_reply_directory(const std::filesystem::path& path) {
    auto path_text = path.generic_string();
    if (path_text.ends_with("/reply/")) path_text.pop_back();
    if (path_text == "reply" || path_text.ends_with("/reply")) {
        return std::filesystem::path{path_text}.lexically_normal();
    }
    return (path / ".cmake" / "api" / "v1" / "reply").lexically_normal();
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

std::vector<ReplyCandidate> scan_reply_candidates(const std::filesystem::path& reply_dir,
                                                  bool& scan_ok) {
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
    scan_ok = !ec;
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
    auto normalized = path.has_root_directory()
                          ? path.lexically_normal().generic_string()
                          : std::filesystem::absolute(path).lexically_normal().generic_string();
    if (normalized.size() > 1 && normalized.ends_with('/')) normalized.pop_back();
    return normalized;
}

std::string resolve_source_path(const std::string& source_path_raw,
                                const std::filesystem::path& source_root) {
    const std::filesystem::path source_path{source_path_raw};
    if (source_path.is_absolute()) return normalize_path(source_path);
    return normalize_path(source_root / source_path);
}

struct ParsedReplyIndex {
    ReplyCandidate latest;
    nlohmann::json index;
};

struct ParsedCodemodel {
    std::string latest_suffix;
    std::string source_root;
    std::filesystem::path source_root_path;
    std::filesystem::path build_root_path;
    nlohmann::json target_refs;
    std::size_t total_targets{0};
};

struct CodemodelPaths {
    std::string source_root;
    std::string build_root;
};

struct CachedTargetForGraph {
    TargetInfo target_info;
    std::string raw_id;
    nlohmann::json dependencies;  // null if absent; preserved if array.
    bool dependencies_malformed{false};
};

struct ParsedTargets {
    std::vector<CompileEntry> entries;
    std::map<std::string, std::vector<TargetInfo>> assignments;
    std::size_t targets_without_sources{0};
    // Phase 1 outputs feeding Phase 2 dependency resolution.
    std::vector<TargetInfo> nodes;
    std::vector<CachedTargetForGraph> cached_targets;
    std::unordered_map<std::string, TargetInfo> id_to_info;  // first-wins.
    // Per colliding raw_id, the unique_keys of subsequently displaced targets.
    std::map<std::string, std::vector<std::string>> id_collision_displaced;
};

struct SourceEntryContext {
    const nlohmann::json& compile_groups;
    const std::filesystem::path& source_root_path;
    const std::string& resolved_directory;
    const TargetInfo& target_info;
};

BuildModelResult load_from_reply_directory(const std::filesystem::path& reply_dir);

std::optional<BuildModelResult> load_latest_index_reply(const std::filesystem::path& reply_dir,
                                                        const std::string& previous_suffix,
                                                        ParsedReplyIndex& parsed_reply) {
    bool scan_ok = false;
    const auto candidates = scan_reply_candidates(reply_dir, scan_ok);
    if (!scan_ok) {
        return make_file_api_error(CompileDatabaseError::file_api_not_accessible,
                                   "cannot read cmake file api reply directory: " +
                                       reply_dir.string());
    }

    const auto* latest = find_latest_candidate(candidates);
    if (latest == nullptr) {
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                   "no index or error reply found in " + reply_dir.string());
    }

    if (!previous_suffix.empty() && latest->suffix <= previous_suffix) {
        return make_file_api_error(
            CompileDatabaseError::file_api_invalid,
            "cmake file api target reply is missing and no newer index is available");
    }
    if (latest->is_error) {
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                   "cmake file api error reply is the most recent entry: " +
                                       latest->path.string());
    }

    bool parse_ok = false;
    const auto index = parse_json_file(latest->path, parse_ok);
    if (!parse_ok) {
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                   "cmake file api index is not valid JSON: " +
                                       latest->path.string());
    }

    parsed_reply = ParsedReplyIndex{*latest, index};
    return std::nullopt;
}

std::optional<BuildModelResult> parse_codemodel_json(const std::filesystem::path& codemodel_path,
                                                     nlohmann::json& codemodel) {
    bool parse_ok = false;
    codemodel = parse_json_file(codemodel_path, parse_ok);
    if (parse_ok) return std::nullopt;

    return make_file_api_error(CompileDatabaseError::file_api_invalid,
                               "cmake file api codemodel is not valid JSON: " +
                                   codemodel_path.string());
}

std::optional<BuildModelResult> extract_codemodel_paths(const nlohmann::json& codemodel,
                                                        CodemodelPaths& paths_out) {
    if (!codemodel.contains("paths")) {
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                   "cmake file api codemodel is missing paths.source or paths.build");
    }

    const auto& paths = codemodel["paths"];
    if (!paths.contains("source") || !paths.contains("build")) {
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                   "cmake file api codemodel is missing paths.source or paths.build");
    }

    paths_out.source_root = paths["source"].get<std::string>();
    paths_out.build_root = paths["build"].get<std::string>();
    return std::nullopt;
}

std::optional<BuildModelResult> extract_target_refs_from_codemodel(const nlohmann::json& codemodel,
                                                                   nlohmann::json& target_refs) {
    if (!codemodel.contains("configurations") || !codemodel["configurations"].is_array()) {
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                   "cmake file api codemodel contains no configurations");
    }

    const auto& configurations = codemodel["configurations"];
    if (configurations.empty()) {
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                   "cmake file api codemodel contains no configurations");
    }
    if (configurations.size() > 1) {
        return make_file_api_error(
            CompileDatabaseError::file_api_invalid,
            "cmake file api codemodel contains " +
                std::to_string(configurations.size()) +
                " configurations; multi-config generators are not supported in this version");
    }

    const auto& config = configurations[0];
    if (!config.contains("targets") || !config["targets"].is_array() ||
        config["targets"].empty()) {
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                   "cmake file api codemodel configuration contains no targets");
    }

    target_refs = config["targets"];
    return std::nullopt;
}

std::optional<BuildModelResult> load_codemodel_reply(const std::filesystem::path& reply_dir,
                                                     const ParsedReplyIndex& parsed_reply,
                                                     ParsedCodemodel& parsed_codemodel) {
    const auto codemodel_json_file = find_codemodel_json_file(parsed_reply.index);
    if (codemodel_json_file.empty()) {
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                   "cmake file api index does not contain a codemodel object");
    }

    const auto codemodel_path = reply_dir / codemodel_json_file;
    nlohmann::json codemodel;
    if (const auto error = parse_codemodel_json(codemodel_path, codemodel); error.has_value())
        return error;

    CodemodelPaths paths;
    if (const auto error = extract_codemodel_paths(codemodel, paths); error.has_value())
        return error;

    nlohmann::json target_refs;
    if (const auto error = extract_target_refs_from_codemodel(codemodel, target_refs);
        error.has_value())
        return error;

    parsed_codemodel = ParsedCodemodel{
        .latest_suffix = parsed_reply.latest.suffix,
        .source_root = paths.source_root,
        .source_root_path = std::filesystem::path{paths.source_root},
        .build_root_path = std::filesystem::path{paths.build_root},
        .target_refs = target_refs,
        .total_targets = target_refs.size(),
    };
    return std::nullopt;
}

std::string resolve_target_directory(const nlohmann::json& target,
                                     const std::filesystem::path& build_root_path) {
    const auto target_build_rel =
        (target.contains("paths") && target["paths"].contains("build"))
            ? target["paths"]["build"].get<std::string>()
            : ".";
    return normalize_path(build_root_path / target_build_rel);
}

void append_group_include_arguments(const nlohmann::json& group,
                                    std::vector<std::string>& arguments) {
    if (!group.contains("includes") || !group["includes"].is_array()) return;

    for (const auto& inc : group["includes"]) {
        const auto inc_path = inc.value("path", "");
        if (inc_path.empty()) continue;
        const auto is_system = inc.value("isSystem", false);
        arguments.push_back(is_system ? "-isystem" : "-I");
        arguments.push_back(inc_path);
    }
}

void append_group_define_arguments(const nlohmann::json& group,
                                   std::vector<std::string>& arguments) {
    if (!group.contains("defines") || !group["defines"].is_array()) return;

    for (const auto& def : group["defines"]) {
        const auto define = def.value("define", "");
        if (!define.empty()) arguments.push_back("-D" + define);
    }
}

std::string compile_driver(const nlohmann::json& group) {
    return group.value("language", "CXX") == "CXX" ? "c++" : "cc";
}

void append_compile_source_argument(std::vector<std::string>& arguments,
                                    const std::string& resolved_source) {
    arguments.push_back("-c");
    arguments.push_back(resolved_source);
}

std::vector<std::string> build_compile_arguments(const nlohmann::json& group,
                                                 const std::string& resolved_source) {
    std::vector<std::string> arguments{compile_driver(group)};
    append_group_include_arguments(group, arguments);
    append_group_define_arguments(group, arguments);
    append_compile_source_argument(arguments, resolved_source);
    return arguments;
}

const nlohmann::json* find_compile_group(const SourceEntryContext& context,
                                         const nlohmann::json& source) {
    if (!source.contains("compileGroupIndex")) return nullptr;

    const auto group_index = source["compileGroupIndex"].get<std::size_t>();
    if (group_index >= context.compile_groups.size()) return nullptr;
    return &context.compile_groups[group_index];
}

std::optional<std::string> resolve_entry_source_path(const nlohmann::json& source,
                                                     const std::filesystem::path& source_root_path) {
    const auto source_path_raw = source.value("path", "");
    if (source_path_raw.empty()) return std::nullopt;
    return resolve_source_path(source_path_raw, source_root_path);
}

void append_target_assignment(ParsedTargets& parsed_targets, const std::string& resolved_source,
                              const SourceEntryContext& context) {
    parsed_targets.assignments[resolved_source + "|" + context.resolved_directory].push_back(
        context.target_info);
}

bool append_source_entry(const nlohmann::json& source, const SourceEntryContext& context,
                         ParsedTargets& parsed_targets) {
    const auto* group = find_compile_group(context, source);
    if (group == nullptr) return false;

    const auto resolved_source = resolve_entry_source_path(source, context.source_root_path);
    if (!resolved_source.has_value()) return false;

    auto arguments = build_compile_arguments(*group, *resolved_source);
    parsed_targets.entries.push_back(
        CompileEntry::from_arguments(*resolved_source, context.resolved_directory,
                                     std::move(arguments)));
    append_target_assignment(parsed_targets, *resolved_source, context);
    return true;
}

void append_target_entries(const nlohmann::json& target, const ParsedCodemodel& parsed_codemodel,
                           const TargetInfo& target_info, ParsedTargets& parsed_targets) {
    if (!target.contains("sources") || !target["sources"].is_array() ||
        !target.contains("compileGroups") || !target["compileGroups"].is_array()) {
        ++parsed_targets.targets_without_sources;
        return;
    }

    const auto resolved_directory =
        resolve_target_directory(target, parsed_codemodel.build_root_path);
    const auto& sources = target["sources"];
    const SourceEntryContext context{target["compileGroups"], parsed_codemodel.source_root_path,
                                     resolved_directory, target_info};
    const auto entries_before = parsed_targets.entries.size();

    for (const auto& source : sources) {
        append_source_entry(source, context, parsed_targets);
    }

    if (parsed_targets.entries.size() == entries_before) {
        ++parsed_targets.targets_without_sources;
    }
}

struct TargetReplyResult {
    bool retry{false};
};

std::optional<BuildModelResult> append_target_reply(const std::filesystem::path& reply_dir,
                                                    const ParsedCodemodel& parsed_codemodel,
                                                    const nlohmann::json& target_ref,
                                                    ParsedTargets& parsed_targets,
                                                    TargetReplyResult& reply_result) {
    if (!target_ref.contains("jsonFile") || !target_ref["jsonFile"].is_string()) {
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                   "cmake file api target reference is missing jsonFile");
    }

    const auto target_json_file = target_ref["jsonFile"].get<std::string>();
    if (target_json_file.empty()) {
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                   "cmake file api target reference is missing jsonFile");
    }
    const auto target_path = reply_dir / target_json_file;
    bool parse_ok = false;
    const auto target = parse_json_file(target_path, parse_ok);

    if (!parse_ok) {
        reply_result.retry = true;
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                   "cmake file api target reply is not accessible: " +
                                       target_path.string());
    }

    const auto target_name = target.value("name", "");
    if (target_name.empty()) {
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                   "cmake file api target reply is missing name: " +
                                       target_path.string());
    }

    const auto target_type = target.value("type", "");
    const auto target_info =
        TargetInfo{target_name, target_type, target_name + "::" + target_type};

    // Phase 1: every target reply contributes a node to the graph and, when
    // its raw file-API id is present, an entry to the id->TargetInfo map.
    // First-wins resolution on collision; later targets stay as nodes but are
    // not reachable via the colliding id.
    parsed_targets.nodes.push_back(target_info);
    const auto raw_id = target.value("id", std::string{});
    if (!raw_id.empty()) {
        const auto [it, inserted] =
            parsed_targets.id_to_info.try_emplace(raw_id, target_info);
        if (!inserted) {
            parsed_targets.id_collision_displaced[raw_id].push_back(
                target_info.unique_key);
        }
    }

    // Cache the dependencies subobject for Phase 2; a present-but-malformed
    // (non-array) dependencies field surfaces as a graph-status diagnostic
    // in Phase 2 without aborting the whole reply.
    CachedTargetForGraph cached;
    cached.target_info = target_info;
    cached.raw_id = raw_id;
    if (target.contains("dependencies")) {
        if (target["dependencies"].is_array()) {
            cached.dependencies = target["dependencies"];
        } else {
            cached.dependencies_malformed = true;
        }
    }
    parsed_targets.cached_targets.push_back(std::move(cached));

    append_target_entries(target, parsed_codemodel, target_info, parsed_targets);
    return std::nullopt;
}

void sort_compile_entries(std::vector<CompileEntry>& entries) {
    std::sort(entries.begin(), entries.end(),
              [](const CompileEntry& a, const CompileEntry& b) {
                  if (a.file() != b.file()) return a.file() < b.file();
                  if (a.directory() != b.directory()) return a.directory() < b.directory();
                  // Tiebreaker for same source in same directory from different targets:
                  // higher argument count first so the most complex entry survives
                  // downstream emplace() on unique_key.
                  return a.arguments().size() > b.arguments().size();
              });
}

std::vector<TargetAssignment> build_target_assignments(
    std::map<std::string, std::vector<TargetInfo>> assignments) {
    std::vector<TargetAssignment> target_assignments;
    for (auto& [key, targets] : assignments) {
        std::sort(targets.begin(), targets.end(), [](const auto& a, const auto& b) {
            return a.unique_key < b.unique_key;
        });
        targets.erase(std::unique(targets.begin(), targets.end(),
                                  [](const auto& a, const auto& b) {
                                      return a.unique_key == b.unique_key;
                                  }),
                      targets.end());
        target_assignments.push_back({key, std::move(targets)});
    }

    std::sort(target_assignments.begin(), target_assignments.end(),
              [](const auto& a, const auto& b) {
                  return a.observation_key < b.observation_key;
              });
    return target_assignments;
}

std::vector<Diagnostic> build_partial_target_diagnostics(const ParsedCodemodel& parsed_codemodel,
                                                         const ParsedTargets& parsed_targets) {
    if (parsed_targets.targets_without_sources == 0) return {};

    return {{DiagnosticSeverity::note,
             std::to_string(parsed_targets.targets_without_sources) + " of " +
                 std::to_string(parsed_codemodel.total_targets) +
                 " targets have no compilable sources and are not included in the analysis"}};
}

struct EdgeIdentity {
    std::string from_unique_key;
    std::string to_unique_key;
    TargetDependencyKind kind;
    bool operator==(const EdgeIdentity& o) const noexcept {
        return kind == o.kind && from_unique_key == o.from_unique_key
               && to_unique_key == o.to_unique_key;
    }
};

struct PairKey {
    std::string a;
    std::string b;
    bool operator==(const PairKey& o) const noexcept {
        return a == o.a && b == o.b;
    }
};

struct EdgeIdentityHash {
    std::size_t operator()(const EdgeIdentity& e) const noexcept {
        std::size_t h = std::hash<std::string>{}(e.from_unique_key);
        const std::size_t to_h = std::hash<std::string>{}(e.to_unique_key);
        h ^= to_h + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        const std::size_t kind_h = std::hash<int>{}(static_cast<int>(e.kind));
        h ^= kind_h + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return h;
    }
};

struct PairKeyHash {
    std::size_t operator()(const PairKey& p) const noexcept {
        std::size_t h = std::hash<std::string>{}(p.a);
        h ^= std::hash<std::string>{}(p.b) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return h;
    }
};

void emit_id_collision_diagnostics(const ParsedTargets& parsed_targets,
                                   std::vector<Diagnostic>& diagnostics) {
    for (const auto& [raw_id, displaced_raw] : parsed_targets.id_collision_displaced) {
        const auto winner_it = parsed_targets.id_to_info.find(raw_id);
        if (winner_it == parsed_targets.id_to_info.end()) continue;
        std::vector<std::string> displaced = displaced_raw;
        std::sort(displaced.begin(), displaced.end());
        displaced.erase(std::unique(displaced.begin(), displaced.end()), displaced.end());
        std::string list;
        for (std::size_t i = 0; i < displaced.size(); ++i) {
            if (i > 0) list += ", ";
            list += displaced[i];
        }
        diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "file api reply contains duplicate target id '" + raw_id +
                 "'; first occurrence '" + winner_it->second.unique_key +
                 "' wins, conflicting targets: " + list});
    }
}

void resolve_target_dependencies(const ParsedTargets& parsed_targets,
                                 std::vector<TargetDependency>& edges,
                                 std::vector<Diagnostic>& diagnostics,
                                 bool& graph_partial) {
    std::unordered_set<EdgeIdentity, EdgeIdentityHash> seen_edges;
    std::unordered_set<PairKey, PairKeyHash> seen_external_diags;
    std::unordered_set<PairKey, PairKeyHash> seen_self_edge_diags;

    for (const auto& cached : parsed_targets.cached_targets) {
        if (cached.dependencies_malformed) {
            diagnostics.push_back(
                {DiagnosticSeverity::warning,
                 "target '" + cached.target_info.display_name +
                     "' has a malformed dependencies field; edges skipped"});
            graph_partial = true;
            continue;
        }
        if (!cached.dependencies.is_array()) continue;

        for (const auto& dep : cached.dependencies) {
            const bool has_id = dep.is_object() && dep.contains("id") &&
                                dep["id"].is_string();
            const auto raw_id = has_id ? dep["id"].get<std::string>() : std::string{};
            if (raw_id.empty()) {
                diagnostics.push_back(
                    {DiagnosticSeverity::warning,
                     "target '" + cached.target_info.display_name +
                         "' has a dependency entry without a target id; entry skipped"});
                graph_partial = true;
                continue;
            }

            const auto resolved_it = parsed_targets.id_to_info.find(raw_id);
            const bool is_resolved = (resolved_it != parsed_targets.id_to_info.end());
            const std::string to_unique_key =
                is_resolved ? resolved_it->second.unique_key : "<external>::" + raw_id;
            const std::string to_display_name =
                is_resolved ? resolved_it->second.display_name : raw_id;

            if (cached.target_info.unique_key == to_unique_key) {
                const PairKey k{cached.target_info.unique_key, to_unique_key};
                if (seen_self_edge_diags.insert(k).second) {
                    diagnostics.push_back(
                        {DiagnosticSeverity::note,
                         "target '" + cached.target_info.display_name +
                             "' has a self-dependency; edge skipped"});
                }
                graph_partial = true;
                continue;
            }

            if (!is_resolved) {
                const PairKey k{cached.target_info.unique_key, raw_id};
                if (seen_external_diags.insert(k).second) {
                    diagnostics.push_back(
                        {DiagnosticSeverity::note,
                         "target '" + cached.target_info.display_name +
                             "' references unknown target id '" + raw_id + "'"});
                }
                graph_partial = true;
            }

            const EdgeIdentity edge_id{cached.target_info.unique_key, to_unique_key,
                                       TargetDependencyKind::direct};
            if (!seen_edges.insert(edge_id).second) continue;
            edges.push_back(TargetDependency{cached.target_info.unique_key,
                                             cached.target_info.display_name,
                                             to_unique_key,
                                             to_display_name,
                                             TargetDependencyKind::direct,
                                             is_resolved
                                                 ? TargetDependencyResolution::resolved
                                                 : TargetDependencyResolution::external});
        }
    }
}

BuildModelResult build_success_result(const ParsedCodemodel& parsed_codemodel,
                                      ParsedTargets parsed_targets) {
    sort_compile_entries(parsed_targets.entries);

    const auto target_metadata = parsed_targets.targets_without_sources > 0
                                     ? TargetMetadataStatus::partial
                                     : TargetMetadataStatus::loaded;
    auto diagnostics = build_partial_target_diagnostics(parsed_codemodel, parsed_targets);

    bool graph_partial = false;
    emit_id_collision_diagnostics(parsed_targets, diagnostics);
    if (!parsed_targets.id_collision_displaced.empty()) graph_partial = true;

    TargetGraph target_graph;
    target_graph.nodes = std::move(parsed_targets.nodes);
    resolve_target_dependencies(parsed_targets, target_graph.edges, diagnostics,
                                graph_partial);
    xray::hexagon::services::sort_target_graph(target_graph);

    const auto target_graph_status =
        graph_partial ? TargetGraphStatus::partial : TargetGraphStatus::loaded;

    // Designated init keeps NRVO happy and stays explicit about the new
    // target-graph fields without depending on declaration order.
    return BuildModelResult{
        .source = ObservationSource::derived,
        .target_metadata = target_metadata,
        .compile_database = CompileDatabaseResult{CompileDatabaseError::none, {},
                                                  std::move(parsed_targets.entries), {}},
        .target_assignments =
            build_target_assignments(std::move(parsed_targets.assignments)),
        .source_root = parsed_codemodel.source_root,
        .diagnostics = std::move(diagnostics),
        .cmake_file_api_resolved_path = std::nullopt,
        .target_graph = std::move(target_graph),
        .target_graph_status = target_graph_status,
    };
}

struct LoadAttemptResult {
    BuildModelResult model;
    bool retryable{false};
    std::string retry_suffix;
};

LoadAttemptResult try_load_from_index(const std::filesystem::path& reply_dir,
                                      const std::string& previous_suffix) {
    ParsedReplyIndex parsed_reply;
    if (const auto error = load_latest_index_reply(reply_dir, previous_suffix, parsed_reply);
        error.has_value()) {
        return {*error};
    }

    ParsedCodemodel parsed_codemodel;
    if (const auto error = load_codemodel_reply(reply_dir, parsed_reply, parsed_codemodel);
        error.has_value()) {
        return {*error};
    }

    ParsedTargets parsed_targets;
    for (const auto& target_ref : parsed_codemodel.target_refs) {
        TargetReplyResult reply_result;
        if (const auto error = append_target_reply(reply_dir, parsed_codemodel,
                                                   target_ref, parsed_targets, reply_result);
            error.has_value()) {
            if (reply_result.retry) {
                return {*error, true, parsed_codemodel.latest_suffix};
            }
            return {*error};
        }
    }

    if (parsed_targets.entries.empty()) {
        return {make_file_api_error(CompileDatabaseError::file_api_invalid,
                                    "cmake file api contains no compilable sources")};
    }

    return {build_success_result(parsed_codemodel, std::move(parsed_targets))};
}

BuildModelResult load_from_reply_directory(const std::filesystem::path& reply_dir) {
    try {
        auto first = try_load_from_index(reply_dir, {});
        if (first.model.is_success() || !first.retryable) return first.model;
        auto retry = try_load_from_index(reply_dir, first.retry_suffix);
        return retry.model;
    } catch (const nlohmann::json::exception& e) {
        return make_file_api_error(CompileDatabaseError::file_api_invalid,
                                  std::string("cmake file api reply data has unexpected structure: ") +
                                      e.what());
    }
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

    auto result = load_from_reply_directory(reply_dir);
    result.cmake_file_api_resolved_path = reply_dir;
    return result;
}

}  // namespace xray::adapters::input
