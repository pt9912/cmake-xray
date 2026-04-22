#pragma once

#include <string>
#include <utility>
#include <vector>

namespace xray::hexagon::model {

enum class CompileCommandSource {
    arguments,
    command,
};

class CompileEntry {
public:
    static CompileEntry from_arguments(std::string file, std::string directory,
                                       std::vector<std::string> arguments) {
        return CompileEntry{std::move(file), std::move(directory), std::move(arguments), {},
                            CompileCommandSource::arguments};
    }

    static CompileEntry from_command(std::string file, std::string directory, std::string command) {
        return CompileEntry{std::move(file), std::move(directory), {}, std::move(command),
                            CompileCommandSource::command};
    }

    const std::string& file() const { return file_; }
    const std::string& directory() const { return directory_; }
    CompileCommandSource command_source() const { return command_source_; }
    const std::vector<std::string>& arguments() const { return arguments_; }
    const std::string& command() const { return command_; }

private:
    CompileEntry(std::string file, std::string directory, std::vector<std::string> arguments,
                 std::string command, CompileCommandSource command_source)
        : file_(std::move(file)),
          directory_(std::move(directory)),
          arguments_(std::move(arguments)),
          command_(std::move(command)),
          command_source_(command_source) {}

    std::string file_;
    std::string directory_;
    std::vector<std::string> arguments_;
    std::string command_;
    CompileCommandSource command_source_{CompileCommandSource::arguments};
};

}  // namespace xray::hexagon::model
