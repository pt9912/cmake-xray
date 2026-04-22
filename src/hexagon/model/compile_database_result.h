#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "compile_entry.h"

namespace xray::hexagon::model {

enum class CompileDatabaseError {
    none,
    file_not_accessible,
    invalid_json,
    not_an_array,
    empty_database,
    invalid_entries,
};

class EntryDiagnostic {
public:
    EntryDiagnostic(std::size_t index, std::string message)
        : index_(index), message_(std::move(message)) {}

    std::size_t index() const { return index_; }
    const std::string& message() const { return message_; }

private:
    std::size_t index_;
    std::string message_;
};

struct CompileDatabaseResult {
    CompileDatabaseError error{CompileDatabaseError::none};
    std::string error_description;
    std::vector<CompileEntry> entries;
    std::vector<EntryDiagnostic> entry_diagnostics;

    bool is_success() const { return error == CompileDatabaseError::none; }
};

}  // namespace xray::hexagon::model
