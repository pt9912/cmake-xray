#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "diagnostic.h"
#include "target_info.h"

namespace xray::hexagon::model {

struct TranslationUnitReference {
    std::string source_path;
    std::string directory;
    std::string source_path_key;
    std::string unique_key;
};

struct TranslationUnitObservation {
    TranslationUnitReference reference;
    std::string resolved_source_path;
    std::string resolved_directory;
    std::vector<std::string> quote_include_paths;
    std::vector<std::string> include_paths;
    std::vector<std::string> system_include_paths;
    std::size_t arg_count{0};
    std::size_t include_path_count{0};
    std::size_t define_count{0};
    std::vector<Diagnostic> diagnostics;

    std::size_t score() const { return arg_count + include_path_count + define_count; }
};

struct RankedTranslationUnit {
    TranslationUnitReference reference;
    std::size_t rank{0};
    std::size_t arg_count{0};
    std::size_t include_path_count{0};
    std::size_t define_count{0};
    std::vector<Diagnostic> diagnostics;
    std::vector<TargetInfo> targets;

    std::size_t score() const { return arg_count + include_path_count + define_count; }
};

}  // namespace xray::hexagon::model
