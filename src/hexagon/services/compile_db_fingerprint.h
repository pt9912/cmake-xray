#pragma once

#include <string>
#include <vector>

namespace xray::hexagon::model {
struct RankedTranslationUnit;
}

namespace xray::hexagon::services {

std::string normalize_project_identity_path(std::string path);

std::string compile_db_project_identity_from_source_paths(
    const std::vector<std::string>& source_paths);

std::string compile_db_project_identity_from_ranked_units(
    const std::vector<xray::hexagon::model::RankedTranslationUnit>& translation_units);

}  // namespace xray::hexagon::services
