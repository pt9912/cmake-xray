#include <doctest/doctest.h>

#include <string>
#include <string_view>

#include "adapters/output/console_report_adapter.h"
#include "hexagon/model/analysis_result.h"
#include "hexagon/model/application_info.h"
#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/model/include_hotspot.h"
#include "hexagon/model/translation_unit.h"

namespace {

using xray::adapters::output::ConsoleReportAdapter;
using xray::hexagon::model::AnalysisResult;
using xray::hexagon::model::CompileDatabaseError;
using xray::hexagon::model::CompileDatabaseResult;
using xray::hexagon::model::ImpactKind;
using xray::hexagon::model::ImpactResult;
using xray::hexagon::model::ImpactedTranslationUnit;
using xray::hexagon::model::IncludeHotspot;
using xray::hexagon::model::RankedTranslationUnit;
using xray::hexagon::model::TranslationUnitReference;

TranslationUnitReference make_reference(std::string source_path, std::string directory,
                                        std::string unique_key) {
    return {
        .source_path = std::move(source_path),
        .directory = std::move(directory),
        .source_path_key = unique_key,
        .unique_key = std::move(unique_key),
    };
}

AnalysisResult make_analysis_result() {
    AnalysisResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.include_analysis_heuristic = true;
    result.translation_units = {
        RankedTranslationUnit{
            .reference = make_reference("src/app/main.cpp", "build/debug",
                                        "src/app/main.cpp|build/debug"),
            .rank = 1,
            .arg_count = 8,
            .include_path_count = 2,
            .define_count = 1,
            .diagnostics = {},
        },
    };
    result.include_hotspots = {
        IncludeHotspot{
            .header_path = "include/common/config.h",
            .affected_translation_units =
                {
                    make_reference("src/app/main.cpp", "build/debug",
                                   "src/app/main.cpp|build/debug"),
                    make_reference("src/app/main.cpp", "build/release",
                                   "src/app/main.cpp|build/release"),
                    make_reference("src/lib/core.cpp", "build/lib",
                                   "src/lib/core.cpp|build/lib"),
                },
            .diagnostics = {},
        },
    };
    return result;
}

std::size_t count_occurrences(std::string_view text, std::string_view needle) {
    std::size_t count = 0;
    std::size_t position = text.find(needle);
    while (position != std::string_view::npos) {
        ++count;
        position = text.find(needle, position + needle.size());
    }
    return count;
}

}  // namespace

TEST_CASE("console report adapter keeps full hotspot mapping for emitted hotspots") {
    const ConsoleReportAdapter adapter;

    const auto report = adapter.write_analysis_report(make_analysis_result(), 1);

    CHECK(report.find("include hotspots [heuristic]") != std::string::npos);
    CHECK(report.find("top 1 of 1 include hotspots") != std::string::npos);
    CHECK(report.find("src/app/main.cpp [directory: build/debug]") != std::string::npos);
    CHECK(report.find("src/app/main.cpp [directory: build/release]") != std::string::npos);
    CHECK(report.find("src/lib/core.cpp [directory: build/lib]") != std::string::npos);
}

TEST_CASE("console report adapter disambiguates duplicate impact observations") {
    const ConsoleReportAdapter adapter;
    ImpactResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.changed_file = "src/app/main.cpp";
    result.affected_translation_units = {
        ImpactedTranslationUnit{
            make_reference("src/app/main.cpp", "build/debug", "src/app/main.cpp|build/debug"),
            ImpactKind::direct,
        },
        ImpactedTranslationUnit{
            make_reference("src/app/main.cpp", "build/release",
                           "src/app/main.cpp|build/release"),
            ImpactKind::direct,
        },
    };

    const auto report = adapter.write_impact_report(result);

    CHECK(report.find("affected translation units: 2") != std::string::npos);
    CHECK(report.find("src/app/main.cpp [directory: build/debug] [direct]") !=
          std::string::npos);
    CHECK(report.find("src/app/main.cpp [directory: build/release] [direct]") !=
          std::string::npos);
}

TEST_CASE("console report adapter keeps omitted translation-unit diagnostics visible with top limit") {
    const ConsoleReportAdapter adapter;
    auto result = make_analysis_result();
    result.translation_units.push_back(
        RankedTranslationUnit{
            .reference = make_reference("src/tools/tool.cpp", "build/tools",
                                        "src/tools/tool.cpp|build/tools"),
            .rank = 2,
            .arg_count = 2,
            .include_path_count = 1,
            .define_count = 0,
            .diagnostics = {{{xray::hexagon::model::DiagnosticSeverity::warning,
                              "could not resolve include \"generated/late.h\" from src/tools/tool.cpp"}}},
        });
    result.diagnostics = {
        {xray::hexagon::model::DiagnosticSeverity::warning,
         "could not resolve include \"generated/late.h\" from src/tools/tool.cpp"},
        {xray::hexagon::model::DiagnosticSeverity::note,
         "include-based results are heuristic; conditional or generated includes may be missing"},
    };

    const auto report = adapter.write_analysis_report(result, 1);

    CHECK(report.find("top 1 of 2 translation units") != std::string::npos);
    CHECK(report.find("warning: could not resolve include \"generated/late.h\" from src/tools/tool.cpp") !=
          std::string::npos);
    CHECK(count_occurrences(report,
                            "could not resolve include \"generated/late.h\" from src/tools/tool.cpp") ==
          1);
}

TEST_CASE("console report adapter deduplicates displayed diagnostics and handles empty hotspots") {
    const ConsoleReportAdapter adapter;
    auto result = make_analysis_result();
    result.translation_units = {
        RankedTranslationUnit{
            .reference = make_reference("src/app/main.cpp", "build/debug",
                                        "src/app/main.cpp|build/debug"),
            .rank = 1,
            .arg_count = 8,
            .include_path_count = 2,
            .define_count = 1,
            .diagnostics =
                {
                    {xray::hexagon::model::DiagnosticSeverity::warning,
                     "could not resolve include \"generated/version.h\" from src/app/main.cpp"},
                },
        },
        RankedTranslationUnit{
            .reference = make_reference("src/app/alt.cpp", "build/release",
                                        "src/app/alt.cpp|build/release"),
            .rank = 2,
            .arg_count = 7,
            .include_path_count = 2,
            .define_count = 1,
            .diagnostics =
                {
                    {xray::hexagon::model::DiagnosticSeverity::warning,
                     "could not resolve include \"generated/version.h\" from src/app/main.cpp"},
                },
        },
    };
    result.translation_units[0].diagnostics = {
        {xray::hexagon::model::DiagnosticSeverity::warning,
         "could not resolve include \"generated/version.h\" from src/app/main.cpp"},
    };
    result.include_hotspots.clear();
    result.diagnostics = {
        {xray::hexagon::model::DiagnosticSeverity::warning,
         "could not resolve include \"generated/version.h\" from src/app/main.cpp"},
        {xray::hexagon::model::DiagnosticSeverity::note,
         "include-based results are heuristic; conditional or generated includes may be missing"},
    };

    const auto report = adapter.write_analysis_report(result, 2);

    CHECK(report.find("no include hotspots found") != std::string::npos);
    CHECK(count_occurrences(
              report,
              "could not resolve include \"generated/version.h\" from src/app/main.cpp") == 2);
}
