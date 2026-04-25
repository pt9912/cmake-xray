#include "adapters/cli/atomic_report_writer.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

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

namespace {

unsigned long current_process_id() {
#ifdef _WIN32
    return static_cast<unsigned long>(::GetCurrentProcessId());
#else
    return static_cast<unsigned long>(::getpid());
#endif
}

#ifdef _WIN32
std::string windows_error_message(unsigned long code) {
    LPSTR raw_buffer = nullptr;
    const auto length = ::FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&raw_buffer), 0, nullptr);
    std::string message;
    if (raw_buffer != nullptr && length > 0) {
        message.assign(raw_buffer, length);
        ::LocalFree(raw_buffer);
    } else {
        message = "Windows error " + std::to_string(code);
    }
    return message;
}
#endif

}  // namespace

std::filesystem::path atomic_report_temp_path(const std::filesystem::path& target_path,
                                              std::size_t attempt) {
    const auto file_name = target_path.filename().generic_string();
    const auto base_name = file_name.empty() ? std::string{"report"} : file_name;
    const auto temp_name = ".cmake-xray-" + base_name + "." +
                           std::to_string(current_process_id()) + "." +
                           std::to_string(attempt) + ".tmp";
    return target_directory(target_path) / temp_name;
}

#ifndef _WIN32
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
#endif

#ifdef _WIN32
std::optional<AtomicFileError> WindowsAtomicFilePlatformOps::create_temp_exclusive(
    const std::filesystem::path& temp_path) {
    const HANDLE handle = ::CreateFileW(
        temp_path.wstring().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return AtomicFileError{windows_error_message(::GetLastError())};
    }
    ::CloseHandle(handle);
    return std::nullopt;
}

std::optional<AtomicFileError> WindowsAtomicFilePlatformOps::replace_existing(
    const std::filesystem::path& temp_path, const std::filesystem::path& target_path) {
    if (!::ReplaceFileW(target_path.wstring().c_str(), temp_path.wstring().c_str(),
                         nullptr, REPLACEFILE_WRITE_THROUGH, nullptr, nullptr)) {
        return AtomicFileError{windows_error_message(::GetLastError())};
    }
    return std::nullopt;
}

std::optional<AtomicFileError> WindowsAtomicFilePlatformOps::move_new(
    const std::filesystem::path& temp_path, const std::filesystem::path& target_path) {
    if (!::MoveFileExW(temp_path.wstring().c_str(), target_path.wstring().c_str(),
                        MOVEFILE_WRITE_THROUGH)) {
        return AtomicFileError{windows_error_message(::GetLastError())};
    }
    return std::nullopt;
}

void WindowsAtomicFilePlatformOps::remove_temp_quiet(
    const std::filesystem::path& temp_path) noexcept {
    std::error_code ec;
    std::filesystem::remove(temp_path, ec);
}
#endif

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
