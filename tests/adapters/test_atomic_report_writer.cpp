#include <doctest/doctest.h>

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "adapters/cli/atomic_report_writer.h"
#include "adapters/cli/cli_report_renderer.h"

namespace {

using xray::adapters::cli::atomic_report_temp_path;
using xray::adapters::cli::AtomicFileError;
using xray::adapters::cli::AtomicFilePlatformOps;
using xray::adapters::cli::AtomicReportWriter;
using xray::adapters::cli::DefaultAtomicFilePlatformOps;
using xray::adapters::cli::RenderError;
using xray::adapters::cli::RenderResult;
using xray::adapters::cli::StringFunctionCliReportRenderer;

class TempDir {
public:
    TempDir() : path_(make_unique_path()) { std::filesystem::create_directories(path_); }

    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::filesystem::path& path() const { return path_; }

private:
    static std::filesystem::path make_unique_path() {
        static std::size_t counter = 0;
        return std::filesystem::temp_directory_path() /
               ("cmake-xray-atomic-writer-" + std::to_string(counter++));
    }

    std::filesystem::path path_;
};

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream(path);
    return std::string(std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{});
}

class RecordingPlatformOps : public AtomicFilePlatformOps {
public:
    std::vector<std::string> calls;
    std::optional<AtomicFileError> create_error;
    std::optional<AtomicFileError> replace_error;
    std::optional<AtomicFileError> move_error;
    bool collide_first_create{false};
    bool actually_create{true};

    std::optional<AtomicFileError> create_temp_exclusive(
        const std::filesystem::path& temp_path) override {
        calls.push_back("create:" + temp_path.string());
        if (collide_first_create) {
            collide_first_create = false;
            return AtomicFileError{"already exists"};
        }
        if (create_error.has_value()) return create_error;
        if (actually_create) {
            std::ofstream stream(temp_path);
        }
        return std::nullopt;
    }

    std::optional<AtomicFileError> replace_existing(
        const std::filesystem::path& temp_path,
        const std::filesystem::path& target_path) override {
        calls.push_back("replace:" + temp_path.string() + "->" + target_path.string());
        if (replace_error.has_value()) return replace_error;
        std::error_code ec;
        std::filesystem::rename(temp_path, target_path, ec);
        if (ec) return AtomicFileError{ec.message()};
        return std::nullopt;
    }

    std::optional<AtomicFileError> move_new(
        const std::filesystem::path& temp_path,
        const std::filesystem::path& target_path) override {
        calls.push_back("move:" + temp_path.string() + "->" + target_path.string());
        if (move_error.has_value()) return move_error;
        std::error_code ec;
        std::filesystem::rename(temp_path, target_path, ec);
        if (ec) return AtomicFileError{ec.message()};
        return std::nullopt;
    }

    void remove_temp_quiet(const std::filesystem::path& temp_path) noexcept override {
        calls.push_back("remove:" + temp_path.string());
        std::error_code ec;
        std::filesystem::remove(temp_path, ec);
    }
};

bool calls_contain_prefix(const std::vector<std::string>& calls, const std::string& prefix) {
    for (const auto& call : calls) {
        if (call.rfind(prefix, 0) == 0) return true;
    }
    return false;
}

}  // namespace

TEST_CASE("atomic temp path uses cmake-xray prefix in target directory") {
    const std::filesystem::path target{"/tmp/example/report.md"};
    const auto temp = atomic_report_temp_path(target, 0);
    CHECK(temp.parent_path() == target.parent_path());
    CHECK(temp.filename().string().rfind(".cmake-xray-", 0) == 0);
    CHECK(temp.extension() == ".tmp");
}

TEST_CASE("atomic temp path falls back to current directory when target has no parent") {
    const auto temp = atomic_report_temp_path(std::filesystem::path{"report.md"}, 0);
    CHECK(temp.parent_path() == std::filesystem::path{"."});
}

TEST_CASE("atomic temp path uses report when target has no filename") {
    const auto temp = atomic_report_temp_path(std::filesystem::path{}, 0);
    CHECK(temp.filename().string().find(".cmake-xray-report.") != std::string::npos);
}

// AP M5-1.7 Tranche C.2: synthetic Windows path-shape Pflichttests fuer
// atomic_report_temp_path. POSIX std::filesystem::path parst weder
// `\\server\share\...` noch `\\?\C:\...` als zusammengesetzten Pfad — der
// Backslash ist auf POSIX kein Pfadtrenner. Der Plan-Pflicht-Skip
// "dokumentierter Windows-API-Skip mit known_limited-Folge" ist in
// docs/user/quality.md unter Plattformstatus dokumentiert; die Tests selbst
// laufen nur auf Windows-Hosts und decken dort den Pfad-Splitting-Vertrag
// (parent_path + filename) fuer UNC und Extended-Length ab.
#ifdef _WIN32
TEST_CASE("atomic temp path keeps the parent UNC root on Windows hosts") {
    const std::filesystem::path target{"\\\\server\\share\\dir\\report.md"};
    const auto temp = atomic_report_temp_path(target, 0);
    CHECK(temp.parent_path() == target.parent_path());
    CHECK(temp.filename().string().rfind(".cmake-xray-report.md.", 0) == 0);
    CHECK(temp.extension() == ".tmp");
}

TEST_CASE("atomic temp path keeps the extended-length parent on Windows hosts") {
    const std::filesystem::path target{"\\\\?\\C:\\very\\long\\path\\report.md"};
    const auto temp = atomic_report_temp_path(target, 0);
    CHECK(temp.parent_path() == target.parent_path());
    CHECK(temp.filename().string().rfind(".cmake-xray-report.md.", 0) == 0);
    CHECK(temp.extension() == ".tmp");
}
#endif

TEST_CASE("atomic writer moves a new target into place via move_new") {
    const TempDir dir;
    RecordingPlatformOps ops;
    AtomicReportWriter writer{ops};

    const auto target = dir.path() / "report.md";
    const auto failure = writer.write_atomic(target, "hello world");

    CHECK_FALSE(failure.has_value());
    CHECK(std::filesystem::exists(target));
    CHECK(read_file(target) == "hello world");
    CHECK(calls_contain_prefix(ops.calls, "move:"));
    CHECK_FALSE(calls_contain_prefix(ops.calls, "replace:"));
}

TEST_CASE("atomic writer replaces an existing target via replace_existing") {
    const TempDir dir;
    RecordingPlatformOps ops;
    AtomicReportWriter writer{ops};

    const auto target = dir.path() / "report.md";
    {
        std::ofstream existing(target);
        existing << "old content";
    }

    const auto failure = writer.write_atomic(target, "new content");

    CHECK_FALSE(failure.has_value());
    CHECK(read_file(target) == "new content");
    CHECK(calls_contain_prefix(ops.calls, "replace:"));
    CHECK_FALSE(calls_contain_prefix(ops.calls, "move:"));
}

TEST_CASE("atomic writer keeps an existing target on replace failure and removes the temp") {
    const TempDir dir;
    RecordingPlatformOps ops;
    ops.replace_error = AtomicFileError{"simulated replace failure"};
    AtomicReportWriter writer{ops};

    const auto target = dir.path() / "report.md";
    {
        std::ofstream existing(target);
        existing << "kept";
    }

    const auto failure = writer.write_atomic(target, "should not land");

    REQUIRE(failure.has_value());
    CHECK(failure->reason.find("simulated replace failure") != std::string::npos);
    CHECK(read_file(target) == "kept");
    CHECK(calls_contain_prefix(ops.calls, "remove:"));
}

TEST_CASE("atomic writer keeps no target on move_new failure and removes the temp") {
    const TempDir dir;
    RecordingPlatformOps ops;
    ops.move_error = AtomicFileError{"simulated move failure"};
    AtomicReportWriter writer{ops};

    const auto target = dir.path() / "report.md";
    const auto failure = writer.write_atomic(target, "should not land");

    REQUIRE(failure.has_value());
    CHECK(failure->reason.find("simulated move failure") != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(target));
    CHECK(calls_contain_prefix(ops.calls, "remove:"));
}

TEST_CASE("atomic writer retries the next attempt when the temp slot is taken") {
    const TempDir dir;
    RecordingPlatformOps ops;
    ops.collide_first_create = true;
    AtomicReportWriter writer{ops};

    const auto target = dir.path() / "report.md";
    const auto failure = writer.write_atomic(target, "after retry");

    CHECK_FALSE(failure.has_value());
    CHECK(read_file(target) == "after retry");
    std::size_t create_calls = 0;
    for (const auto& call : ops.calls) {
        if (call.rfind("create:", 0) == 0) ++create_calls;
    }
    CHECK(create_calls == 2);
}

TEST_CASE("atomic writer reports exhausted temp slots when every attempt collides") {
    const TempDir dir;
    class AlwaysCollidingOps final : public AtomicFilePlatformOps {
    public:
        std::optional<AtomicFileError> create_temp_exclusive(
            const std::filesystem::path& /*temp_path*/) override {
            return AtomicFileError{"collision"};
        }
        std::optional<AtomicFileError> replace_existing(
            const std::filesystem::path&, const std::filesystem::path&) override {
            return std::nullopt;
        }
        std::optional<AtomicFileError> move_new(
            const std::filesystem::path&, const std::filesystem::path&) override {
            return std::nullopt;
        }
        void remove_temp_quiet(const std::filesystem::path&) noexcept override {}
    };
    AlwaysCollidingOps ops;
    AtomicReportWriter writer{ops};

    const auto failure = writer.write_atomic(dir.path() / "report.md", "x");
    REQUIRE(failure.has_value());
    CHECK(failure->reason.find("cannot reserve a temporary report path") != std::string::npos);
}

TEST_CASE("atomic writer surfaces stream write failures and removes the temp") {
    const TempDir dir;
    RecordingPlatformOps ops;
    ops.actually_create = false;  // create_temp_exclusive returns success without making the file
    AtomicReportWriter writer{ops};

    // Target directory has no on-disk temp file, so opening the stream for writing succeeds
    // here on POSIX. To force a write failure we instead point at a path under a missing
    // subdirectory so the stream cannot open.
    const auto target = dir.path() / "missing" / "report.md";
    const auto failure = writer.write_atomic(target, "will not land");

    REQUIRE(failure.has_value());
    CHECK_FALSE(std::filesystem::exists(target));
    CHECK(calls_contain_prefix(ops.calls, "remove:"));
}

// AP M5-1.7 Tranche B Atomic-Replace-Matrix item 7: keine Implementierung
// darf den Zielpfad vor dem Replace-Schritt loeschen. Der Recording-Mock
// schreibt jeden remove_temp_quiet-Aufruf in `calls`; der Test pinnt, dass
// der Writer auf Erfolgs- und Fehlerpfaden ausschliesslich den temporaeren
// Pfad entfernt und den Zielpfad nie berührt.
TEST_CASE("atomic writer never deletes the target path before the replace step") {
    const TempDir dir;
    const auto target = dir.path() / "report.md";

    SUBCASE("success path leaves the target untouched outside replace_existing") {
        RecordingPlatformOps ops;
        AtomicReportWriter writer{ops};
        {
            std::ofstream existing(target);
            existing << "kept";
        }

        const auto failure = writer.write_atomic(target, "fresh");
        REQUIRE_FALSE(failure.has_value());

        const std::string target_remove_prefix = "remove:" + target.string();
        for (const auto& call : ops.calls) {
            CHECK(call.rfind(target_remove_prefix, 0) != 0);
        }
    }

    SUBCASE("replace failure path removes only the temp, never the target") {
        RecordingPlatformOps ops;
        ops.replace_error = AtomicFileError{"simulated replace failure"};
        AtomicReportWriter writer{ops};
        {
            std::ofstream existing(target);
            existing << "kept";
        }

        const auto failure = writer.write_atomic(target, "should not land");
        REQUIRE(failure.has_value());

        const std::string target_remove_prefix = "remove:" + target.string();
        for (const auto& call : ops.calls) {
            CHECK(call.rfind(target_remove_prefix, 0) != 0);
        }
    }
}

TEST_CASE("default platform ops create_temp_exclusive succeeds for a fresh path") {
    const TempDir dir;
    DefaultAtomicFilePlatformOps ops;
    const auto temp = dir.path() / ".cmake-xray-temp.tmp";

    const auto error = ops.create_temp_exclusive(temp);

    CHECK_FALSE(error.has_value());
    CHECK(std::filesystem::exists(temp));
}

TEST_CASE("default platform ops create_temp_exclusive fails when the path already exists") {
    const TempDir dir;
    DefaultAtomicFilePlatformOps ops;
    const auto temp = dir.path() / ".cmake-xray-temp.tmp";
    {
        std::ofstream existing(temp);
    }

    const auto error = ops.create_temp_exclusive(temp);

    REQUIRE(error.has_value());
    CHECK_FALSE(error->message.empty());
}

TEST_CASE("default platform ops replace_existing renames temp over target") {
    const TempDir dir;
    DefaultAtomicFilePlatformOps ops;
    const auto temp = dir.path() / ".cmake-xray-temp.tmp";
    const auto target = dir.path() / "report.md";
    {
        std::ofstream existing(target);
        existing << "old";
    }
    {
        std::ofstream stream(temp);
        stream << "new";
    }

    const auto error = ops.replace_existing(temp, target);

    CHECK_FALSE(error.has_value());
    CHECK(read_file(target) == "new");
    CHECK_FALSE(std::filesystem::exists(temp));
}

TEST_CASE("default platform ops replace_existing fails when the temp does not exist") {
    const TempDir dir;
    DefaultAtomicFilePlatformOps ops;
    const auto error =
        ops.replace_existing(dir.path() / "missing.tmp", dir.path() / "target.md");
    REQUIRE(error.has_value());
}

// AP M5-1.7 Tranche B Atomic-Replace-Matrix item 4: bei einem realen
// Replace-Fehler auf der Host-Plattform (POSIX rename / Windows ReplaceFileW)
// muss der vorhandene Zielinhalt unveraendert bleiben. Der Test setzt einen
// echten Zielinhalt auf, ruft replace_existing mit fehlendem Temp-Pfad auf
// (dadurch scheitert die Plattform-API ohne Seiteneffekt am Ziel) und
// vergleicht die Zielbytes vor und nach dem fehlgeschlagenen Replace.
TEST_CASE("default platform ops replace_existing preserves target bytes when the temp is missing") {
    const TempDir dir;
    DefaultAtomicFilePlatformOps ops;
    const auto target = dir.path() / "report.md";
    const std::string target_content = "kept across replace failure";
    {
        std::ofstream existing(target);
        existing << target_content;
    }

    const auto error = ops.replace_existing(dir.path() / "missing.tmp", target);

    REQUIRE(error.has_value());
    CHECK(read_file(target) == target_content);
}

TEST_CASE("default platform ops move_new renames temp into a fresh target") {
    const TempDir dir;
    DefaultAtomicFilePlatformOps ops;
    const auto temp = dir.path() / ".cmake-xray-temp.tmp";
    const auto target = dir.path() / "report.md";
    {
        std::ofstream stream(temp);
        stream << "fresh";
    }

    const auto error = ops.move_new(temp, target);

    CHECK_FALSE(error.has_value());
    CHECK(read_file(target) == "fresh");
}

TEST_CASE("default platform ops move_new fails when the temp does not exist") {
    const TempDir dir;
    DefaultAtomicFilePlatformOps ops;
    const auto error = ops.move_new(dir.path() / "missing.tmp", dir.path() / "target.md");
    REQUIRE(error.has_value());
}

TEST_CASE("default platform ops remove_temp_quiet ignores missing files") {
    const TempDir dir;
    DefaultAtomicFilePlatformOps ops;
    ops.remove_temp_quiet(dir.path() / "never-existed.tmp");
    // Just verifies no exception escapes; nothing else to check.
    CHECK(true);
}

TEST_CASE("default platform ops remove_temp_quiet deletes existing temp files") {
    const TempDir dir;
    DefaultAtomicFilePlatformOps ops;
    const auto temp = dir.path() / "victim.tmp";
    {
        std::ofstream existing(temp);
    }
    REQUIRE(std::filesystem::exists(temp));
    ops.remove_temp_quiet(temp);
    CHECK_FALSE(std::filesystem::exists(temp));
}

TEST_CASE("string function renderer maps a returned string into RenderResult content") {
    const StringFunctionCliReportRenderer renderer{[] { return std::string{"hello"}; }};
    const auto result = renderer.render();

    REQUIRE(result.content.has_value());
    CHECK(*result.content == "hello");
    CHECK_FALSE(result.error.has_value());
}

TEST_CASE("string function renderer maps caught exceptions into RenderError") {
    const StringFunctionCliReportRenderer renderer{[]() -> std::string {
        throw std::runtime_error("simulated render failure");
    }};
    const auto result = renderer.render();

    CHECK_FALSE(result.content.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->message.find("simulated render failure") != std::string::npos);
}
