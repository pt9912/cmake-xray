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

class CompileDatabaseResult {
public:
    CompileDatabaseResult(CompileDatabaseError error, std::string error_description,
                          std::vector<CompileEntry> entries,
                          std::vector<EntryDiagnostic> entry_diagnostics,
                          std::size_t total_invalid_entries = 0)
        : error_(error),
          error_description_(std::move(error_description)),
          entries_(std::move(entries)),
          entry_diagnostics_(std::move(entry_diagnostics)),
          total_invalid_entries_(total_invalid_entries) {}

    CompileDatabaseResult() = default;

    bool is_success() const { return error_ == CompileDatabaseError::none; }

    CompileDatabaseError error() const { return error_; }
    const std::string& error_description() const { return error_description_; }
    const std::vector<CompileEntry>& entries() const { return entries_; }
    const std::vector<EntryDiagnostic>& entry_diagnostics() const { return entry_diagnostics_; }
    std::size_t total_invalid_entries() const { return total_invalid_entries_; }

private:
    CompileDatabaseError error_{CompileDatabaseError::none};
    std::string error_description_;
    std::vector<CompileEntry> entries_;
    std::vector<EntryDiagnostic> entry_diagnostics_;
    std::size_t total_invalid_entries_{0};
};

}  // namespace xray::hexagon::model
