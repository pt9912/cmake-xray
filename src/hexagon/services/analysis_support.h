#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "hexagon/model/compile_entry.h"
#include "hexagon/model/include_hotspot.h"
#include "hexagon/model/include_resolution.h"
#include "hexagon/model/translation_unit.h"

namespace xray::hexagon::services {

std::filesystem::path compile_commands_base_directory(std::string_view compile_commands_path);

std::string display_compile_commands_path(std::string_view compile_commands_path);
std::string normalize_path(const std::filesystem::path& path);
std::string make_display_path(const std::string& normalized_path,
                              const std::filesystem::path& base_directory);

std::vector<model::TranslationUnitObservation> build_translation_unit_observations(
    const std::vector<model::CompileEntry>& entries, std::string_view compile_commands_path);

std::vector<model::RankedTranslationUnit> build_ranked_translation_units(
    const std::vector<model::TranslationUnitObservation>& observations,
    const model::IncludeResolutionResult& include_resolution);

std::vector<model::IncludeHotspot> build_include_hotspots(
    const std::vector<model::TranslationUnitObservation>& observations,
    const model::IncludeResolutionResult& include_resolution,
    std::string_view compile_commands_path);

std::string resolve_changed_file_key(const std::filesystem::path& base_directory,
                                     const std::filesystem::path& changed_path);

std::string display_changed_file(const std::filesystem::path& base_directory,
                                 const std::string& changed_file_key);

}  // namespace xray::hexagon::services
