#pragma once

#include <string>
#include <utility>
#include <vector>

namespace xray::hexagon::model {

class CompileEntry {
public:
    CompileEntry(std::string file, std::string directory, std::vector<std::string> arguments)
        : file_(std::move(file)),
          directory_(std::move(directory)),
          arguments_(std::move(arguments)) {}

    const std::string& file() const { return file_; }
    const std::string& directory() const { return directory_; }
    const std::vector<std::string>& arguments() const { return arguments_; }

private:
    std::string file_;
    std::string directory_;
    std::vector<std::string> arguments_;
};

}  // namespace xray::hexagon::model
