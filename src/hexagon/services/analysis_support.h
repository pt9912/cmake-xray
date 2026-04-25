#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hexagon/model/build_model_result.h"
#include "hexagon/model/compile_entry.h"
#include "hexagon/model/include_hotspot.h"
#include "hexagon/model/include_resolution.h"
#include "hexagon/model/translation_unit.h"

namespace xray::hexagon::services {

enum class ReportPathDisplayKind {
    input_argument,
    resolved_adapter_path,
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
    ReportPathDisplayInput input, const std::filesystem::path& report_display_base);

std::string normalize_path(const std::filesystem::path& path);
std::string make_display_path(const std::string& normalized_path,
                              const std::filesystem::path& base_directory);

std::vector<model::TranslationUnitObservation> build_translation_unit_observations(
    const std::vector<model::CompileEntry>& entries, const std::filesystem::path& base_directory);

std::vector<model::RankedTranslationUnit> build_ranked_translation_units(
    const std::vector<model::TranslationUnitObservation>& observations,
    const model::IncludeResolutionResult& include_resolution);

std::vector<model::IncludeHotspot> build_include_hotspots(
    const std::vector<model::TranslationUnitObservation>& observations,
    const model::IncludeResolutionResult& include_resolution,
    const std::filesystem::path& base_directory);

std::string resolve_changed_file_key(const std::filesystem::path& base_directory,
                                     const std::filesystem::path& changed_path);

std::string display_changed_file(const std::filesystem::path& base_directory,
                                 const std::string& changed_file_key);

}  // namespace xray::hexagon::services
