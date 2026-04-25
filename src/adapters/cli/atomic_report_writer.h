#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace xray::adapters::cli {

struct AtomicFileError {
    std::string message;
};

class AtomicFilePlatformOps {
public:
    virtual ~AtomicFilePlatformOps() = default;

    virtual std::optional<AtomicFileError> create_temp_exclusive(
        const std::filesystem::path& temp_path) = 0;

    virtual std::optional<AtomicFileError> replace_existing(
        const std::filesystem::path& temp_path,
        const std::filesystem::path& target_path) = 0;

    virtual std::optional<AtomicFileError> move_new(
        const std::filesystem::path& temp_path,
        const std::filesystem::path& target_path) = 0;

    virtual void remove_temp_quiet(
        const std::filesystem::path& temp_path) noexcept = 0;
};

class PosixAtomicFilePlatformOps final : public AtomicFilePlatformOps {
public:
    std::optional<AtomicFileError> create_temp_exclusive(
        const std::filesystem::path& temp_path) override;

    std::optional<AtomicFileError> replace_existing(
        const std::filesystem::path& temp_path,
        const std::filesystem::path& target_path) override;

    std::optional<AtomicFileError> move_new(
        const std::filesystem::path& temp_path,
        const std::filesystem::path& target_path) override;

    void remove_temp_quiet(const std::filesystem::path& temp_path) noexcept override;
};

#ifdef _WIN32
class WindowsAtomicFilePlatformOps final : public AtomicFilePlatformOps {
public:
    std::optional<AtomicFileError> create_temp_exclusive(
        const std::filesystem::path& temp_path) override;

    std::optional<AtomicFileError> replace_existing(
        const std::filesystem::path& temp_path,
        const std::filesystem::path& target_path) override;

    std::optional<AtomicFileError> move_new(
        const std::filesystem::path& temp_path,
        const std::filesystem::path& target_path) override;

    void remove_temp_quiet(const std::filesystem::path& temp_path) noexcept override;
};

using DefaultAtomicFilePlatformOps = WindowsAtomicFilePlatformOps;
#else
using DefaultAtomicFilePlatformOps = PosixAtomicFilePlatformOps;
#endif

struct AtomicWriteFailure {
    std::string path;
    std::string reason;
};

class AtomicReportWriter {
public:
    explicit AtomicReportWriter(AtomicFilePlatformOps& ops);

    std::optional<AtomicWriteFailure> write_atomic(
        const std::filesystem::path& target_path, std::string_view content);

private:
    AtomicFilePlatformOps* ops_;
};

// Exposed for tests so the temp-path naming stays consistent across writer
// implementations and write-attempt collision tests.
std::filesystem::path atomic_report_temp_path(
    const std::filesystem::path& target_path, std::size_t attempt);

}  // namespace xray::adapters::cli
