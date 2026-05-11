#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hexagon/model/build_model_result.h"
#include "hexagon/model/compile_entry.h"
#include "hexagon/model/include_classification.h"
#include "hexagon/model/include_filter_options.h"
#include "hexagon/model/include_resolution.h"
#include "hexagon/model/translation_unit.h"

namespace xray::hexagon::services {

enum class ReportPathDisplayKind {
    input_argument,
    resolved_adapter_path,
};

enum class ReportPathCasePolicy {
    platform_default,
    case_sensitive,    // POSIX semantics
    case_insensitive,  // Windows drive/segment semantics
};

struct ReportPathDisplayInput {
    std::optional<std::filesystem::path> display_path;
    ReportPathDisplayKind kind{ReportPathDisplayKind::input_argument};
    bool was_relative{false};
};

std::filesystem::path compile_commands_base_directory(
    std::string_view compile_commands_path, const std::filesystem::path& fallback_base);

std::optional<std::filesystem::path> source_root_from_build_model(
    const model::BuildModelResult& build_model);

std::optional<std::string> to_report_display_path(
    ReportPathDisplayInput input, const std::filesystem::path& report_display_base,
    ReportPathCasePolicy case_policy = ReportPathCasePolicy::platform_default);

std::string normalize_path(const std::filesystem::path& path);
std::string make_display_path(const std::string& normalized_path,
                              const std::filesystem::path& base_directory);

std::vector<model::TranslationUnitObservation> build_translation_unit_observations(
    const std::vector<model::CompileEntry>& entries, const std::filesystem::path& base_directory);

std::vector<model::RankedTranslationUnit> build_ranked_translation_units(
    const std::vector<model::TranslationUnitObservation>& observations,
    const model::IncludeResolutionResult& include_resolution);

struct IncludeHotspotsBuildResult {
    std::vector<model::IncludeHotspot> hotspots;
    std::size_t total_count{0};
    std::size_t excluded_unknown_count{0};
    std::size_t excluded_mixed_count{0};
};

struct IncludeHotspotFilters {
    model::IncludeScope scope{model::IncludeScope::all};
    model::IncludeDepthFilter depth_filter{model::IncludeDepthFilter::all};
};

model::IncludeOrigin classify_include_origin(
    const std::string& header_path,
    const std::vector<model::TranslationUnitObservation>& observations,
    const std::filesystem::path& source_root);

// Lexicographic Sortier-Tupel per docs/planning/done/plan-M6-1-4.md §697-698
// (affected_tu_count desc, normalized_header_display_path asc,
//  include_origin asc, include_depth_kind asc). Exposed so the tie-breaker
// branches stay covered by a focused unit test even though they are
// unreachable through the public build_include_hotspots entry (the by_header
// map dedups on header_path before the sort runs).
bool compare_hotspots_for_sort(const model::IncludeHotspot& lhs,
                                const model::IncludeHotspot& rhs);

IncludeHotspotsBuildResult build_include_hotspots(
    const std::vector<model::TranslationUnitObservation>& observations,
    const model::IncludeResolutionResult& include_resolution,
    const std::filesystem::path& base_directory,
    const std::filesystem::path& source_root,
    IncludeHotspotFilters filters);

std::string resolve_changed_file_key(const std::filesystem::path& base_directory,
                                     const std::filesystem::path& changed_path);

std::string display_changed_file(const std::filesystem::path& base_directory,
                                 const std::string& changed_file_key);

}  // namespace xray::hexagon::services
