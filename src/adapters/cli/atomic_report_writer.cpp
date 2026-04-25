#include "adapters/cli/atomic_report_writer.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <string>
#include <system_error>
#include <unistd.h>

namespace xray::adapters::cli {

namespace {

constexpr std::size_t kMaxTempReservationAttempts = 64;

std::string system_message(int error_number) {
    return std::error_code(error_number == 0 ? EIO : error_number, std::generic_category())
        .message();
}

std::filesystem::path target_directory(const std::filesystem::path& target_path) {
    const auto parent = target_path.parent_path();
    return parent.empty() ? std::filesystem::path{"."} : parent;
}

std::optional<std::filesystem::path> reserve_temp_path(
    const std::filesystem::path& target_path, AtomicFilePlatformOps& ops) {
    for (std::size_t attempt = 0; attempt < kMaxTempReservationAttempts; ++attempt) {
        const auto candidate = atomic_report_temp_path(target_path, attempt);
        if (!ops.create_temp_exclusive(candidate).has_value()) return candidate;
    }
    return std::nullopt;
}

std::optional<std::string> stream_bytes(const std::filesystem::path& temp_path,
                                        std::string_view content) {
    std::ofstream stream(temp_path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) return system_message(errno);
    if (!content.empty()) {
        stream.write(content.data(), static_cast<std::streamsize>(content.size()));
    }
    stream.close();
    if (!stream.good()) return system_message(errno);
    return std::nullopt;
}

std::optional<AtomicFileError> install_temp(AtomicFilePlatformOps& ops,
                                            const std::filesystem::path& temp_path,
                                            const std::filesystem::path& target_path) {
    std::error_code ec;
    const bool target_exists = std::filesystem::exists(target_path, ec);
    if (ec) return AtomicFileError{ec.message()};
    if (target_exists) return ops.replace_existing(temp_path, target_path);
    return ops.move_new(temp_path, target_path);
}

}  // namespace

std::filesystem::path atomic_report_temp_path(const std::filesystem::path& target_path,
                                              std::size_t attempt) {
    const auto file_name = target_path.filename().generic_string();
    const auto base_name = file_name.empty() ? std::string{"report"} : file_name;
    const auto temp_name = ".cmake-xray-" + base_name + "." +
                           std::to_string(::getpid()) + "." +
                           std::to_string(attempt) + ".tmp";
    return target_directory(target_path) / temp_name;
}

std::optional<AtomicFileError> PosixAtomicFilePlatformOps::create_temp_exclusive(
    const std::filesystem::path& temp_path) {
    const int fd = ::open(temp_path.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd < 0) return AtomicFileError{std::strerror(errno)};
    ::close(fd);
    return std::nullopt;
}

std::optional<AtomicFileError> PosixAtomicFilePlatformOps::replace_existing(
    const std::filesystem::path& temp_path, const std::filesystem::path& target_path) {
    if (::rename(temp_path.c_str(), target_path.c_str()) != 0) {
        return AtomicFileError{std::strerror(errno)};
    }
    return std::nullopt;
}

std::optional<AtomicFileError> PosixAtomicFilePlatformOps::move_new(
    const std::filesystem::path& temp_path, const std::filesystem::path& target_path) {
    if (::rename(temp_path.c_str(), target_path.c_str()) != 0) {
        return AtomicFileError{std::strerror(errno)};
    }
    return std::nullopt;
}

void PosixAtomicFilePlatformOps::remove_temp_quiet(
    const std::filesystem::path& temp_path) noexcept {
    std::error_code ec;
    std::filesystem::remove(temp_path, ec);
}

AtomicReportWriter::AtomicReportWriter(AtomicFilePlatformOps& ops) : ops_(&ops) {}

std::optional<AtomicWriteFailure> AtomicReportWriter::write_atomic(
    const std::filesystem::path& target_path, std::string_view content) {
    const auto temp_path = reserve_temp_path(target_path, *ops_);
    if (!temp_path.has_value()) {
        return AtomicWriteFailure{target_path.string(),
                                  "cannot reserve a temporary report path"};
    }

    if (const auto write_error = stream_bytes(*temp_path, content); write_error.has_value()) {
        ops_->remove_temp_quiet(*temp_path);
        return AtomicWriteFailure{target_path.string(), *write_error};
    }

    if (const auto install_error = install_temp(*ops_, *temp_path, target_path);
        install_error.has_value()) {
        ops_->remove_temp_quiet(*temp_path);
        return AtomicWriteFailure{target_path.string(), install_error->message};
    }

    return std::nullopt;
}

}  // namespace xray::adapters::cli
