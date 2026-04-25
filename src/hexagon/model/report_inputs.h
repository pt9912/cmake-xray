#pragma once

#include <optional>
#include <string>

namespace xray::hexagon::model {

enum class ReportInputSource {
    cli,
    not_provided,
};

enum class ChangedFileSource {
    compile_database_directory,
    file_api_source_root,
    cli_absolute,
    unresolved_file_api_source_root,
};

struct ReportInputs {
    std::optional<std::string> compile_database_path;
    ReportInputSource compile_database_source{ReportInputSource::not_provided};

    std::optional<std::string> cmake_file_api_path;
    std::optional<std::string> cmake_file_api_resolved_path;
    ReportInputSource cmake_file_api_source{ReportInputSource::not_provided};

    std::optional<std::string> changed_file;
    std::optional<ChangedFileSource> changed_file_source;
};

}  // namespace xray::hexagon::model
