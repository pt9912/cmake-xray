#include <doctest/doctest.h>

#include <string>

#include "adapters/output/markdown_report_adapter.h"
#include "hexagon/model/analysis_result.h"
#include "hexagon/model/application_info.h"
#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/model/include_hotspot.h"
#include "hexagon/model/translation_unit.h"

namespace {

using xray::adapters::output::MarkdownReportAdapter;
using xray::hexagon::model::AnalysisResult;
using xray::hexagon::model::CompileDatabaseError;
using xray::hexagon::model::CompileDatabaseResult;
using xray::hexagon::model::Diagnostic;
using xray::hexagon::model::DiagnosticSeverity;
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
    result.compile_database_path = "tests/e2e/testdata/m3/report_project/compile_commands.json";
    result.include_analysis_heuristic = true;
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
                    {DiagnosticSeverity::warning,
                     "could not resolve include \"generated/version.h\" from src/app/main.cpp"},
                },
        },
        RankedTranslationUnit{
            .reference = make_reference("src/lib/core.cpp", "build/lib",
                                        "src/lib/core.cpp|build/lib"),
            .rank = 2,
            .arg_count = 4,
            .include_path_count = 1,
            .define_count = 0,
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
                    make_reference("src/lib/core.cpp", "build/lib",
                                   "src/lib/core.cpp|build/lib"),
                },
            .diagnostics =
                {
                    {DiagnosticSeverity::note, "hotspot detail"},
                },
        },
    };
    result.diagnostics = {
        {DiagnosticSeverity::warning, "compile database was normalized"},
        {DiagnosticSeverity::note,
         "include-based results are heuristic; conditional or generated includes may be missing"},
    };
    return result;
}

ImpactResult make_impact_result() {
    ImpactResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.compile_database_path =
        "tests/e2e/testdata/m3/report_impact_header/compile_commands.json";
    result.changed_file = "include/common/config.h";
    result.heuristic = true;
    result.affected_translation_units = {
        ImpactedTranslationUnit{
            make_reference("src/app/main.cpp", "build/debug", "src/app/main.cpp|build/debug"),
            ImpactKind::heuristic,
        },
    };
    result.diagnostics = {
        {DiagnosticSeverity::warning,
         "could not resolve include \"generated/version.h\" from src/app/main.cpp"},
    };
    return result;
}

}  // namespace

TEST_CASE("markdown report adapter renders project reports with exact section structure") {
    const MarkdownReportAdapter adapter;

    const auto report = adapter.write_analysis_report(make_analysis_result(), 1);

    CHECK(report ==
          "# Project Analysis Report\n"
          "\n"
          "- Report type: analyze\n"
          "- Compile database: tests/e2e/testdata/m3/report\\_project/compile\\_commands.json\n"
          "- Translation units: 2\n"
          "- Translation unit ranking entries: 1 of 2\n"
          "- Include hotspot entries: 1 of 1\n"
          "- Top limit: 1\n"
          "- Include analysis heuristic: yes\n"
          "\n"
          "## Translation Unit Ranking\n"
          "1. src/app/main.cpp [directory: build/debug]\n"
          "    Metrics: arg_count=8, include_path_count=2, define_count=1\n"
          "    Diagnostics:\n"
          "    - warning: could not resolve include \"generated/version.h\" from src/app/main.cpp\n"
          "\n"
          "## Include Hotspots\n"
          "1. Header: include/common/config.h\n"
          "    Affected translation units: 2\n"
          "    Translation units:\n"
          "    - src/app/main.cpp [directory: build/debug]\n"
          "    - src/lib/core.cpp [directory: build/lib]\n"
          "    Diagnostics:\n"
          "    - note: hotspot detail\n"
          "\n"
          "## Diagnostics\n"
          "- warning: compile database was normalized\n"
          "- note: include-based results are heuristic; conditional or generated includes may be missing\n");
}

TEST_CASE("markdown report adapter renders heuristic and uncertain impact classifications") {
    const MarkdownReportAdapter adapter;

    const auto heuristic_report = adapter.write_impact_report(make_impact_result());
    CHECK(heuristic_report.find("- Impact classification: heuristic\n") != std::string::npos);
    CHECK(heuristic_report.find("## Heuristically Affected Translation Units\n- src/app/main.cpp [directory: build/debug]\n") !=
          std::string::npos);

    auto uncertain_result = make_impact_result();
    uncertain_result.affected_translation_units.clear();
    uncertain_result.diagnostics = {
        {DiagnosticSeverity::note, "conditional or generated includes may be missing from this result"},
    };

    const auto uncertain_report = adapter.write_impact_report(uncertain_result);
    CHECK(uncertain_report ==
          "# Impact Analysis Report\n"
          "\n"
          "- Report type: impact\n"
          "- Compile database: tests/e2e/testdata/m3/report\\_impact\\_header/compile\\_commands.json\n"
          "- Changed file: include/common/config.h\n"
          "- Affected translation units: 0\n"
          "- Impact classification: uncertain\n"
          "\n"
          "No affected translation units found.\n"
          "\n"
          "## Directly Affected Translation Units\n"
          "No directly affected translation units.\n"
          "\n"
          "## Heuristically Affected Translation Units\n"
          "No heuristically affected translation units.\n"
          "\n"
          "## Diagnostics\n"
          "- note: conditional or generated includes may be missing from this result\n");
}

TEST_CASE("markdown report adapter escapes markdown-sensitive paths and messages") {
    const MarkdownReportAdapter adapter;
    auto result = make_analysis_result();
    result.compile_database_path = "#tmp/report_[1].json";
    result.translation_units[0].reference.source_path = "- [main]_file.cpp";
    result.translation_units[0].diagnostics = {
        {DiagnosticSeverity::warning, "1. [list] *item*"},
    };
    result.include_hotspots.clear();
    result.diagnostics = {
        {DiagnosticSeverity::note, "#heading style"},
    };

    const auto report = adapter.write_analysis_report(result, 1);

    CHECK(report.find("- Compile database: \\#tmp/report\\_\\[1\\].json\n") !=
          std::string::npos);
    CHECK(report.find("1. \\- \\[main\\]\\_file.cpp [directory: build/debug]\n") !=
          std::string::npos);
    CHECK(report.find("- warning: 1\\. \\[list\\] \\*item\\*\n") != std::string::npos);
    CHECK(report.find("- note: \\#heading style\n") != std::string::npos);
}

TEST_CASE("markdown report adapter renders direct impact classification without hits") {
    const MarkdownReportAdapter adapter;
    ImpactResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.compile_database_path = "compile_commands.json";
    result.changed_file = "src/app/main.cpp";
    result.heuristic = false;

    const auto report = adapter.write_impact_report(result);

    CHECK(report.find("- Impact classification: direct\n") != std::string::npos);
    CHECK(report.find("No affected translation units found.\n") != std::string::npos);
}
