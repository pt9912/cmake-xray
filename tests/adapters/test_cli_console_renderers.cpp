#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <string_view>

#include "adapters/cli/cli_console_renderers.h"
#include "hexagon/model/analysis_result.h"
#include "hexagon/model/application_info.h"
#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/diagnostic.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/model/include_hotspot.h"
#include "hexagon/model/observation_source.h"
#include "hexagon/model/report_inputs.h"
#include "hexagon/model/target_info.h"
#include "hexagon/model/translation_unit.h"

namespace {

using xray::adapters::cli::render_console_quiet_analyze;
using xray::adapters::cli::render_console_quiet_impact;
using xray::hexagon::model::AnalysisResult;
using xray::hexagon::model::ChangedFileSource;
using xray::hexagon::model::CompileDatabaseError;
using xray::hexagon::model::CompileDatabaseResult;
using xray::hexagon::model::Diagnostic;
using xray::hexagon::model::DiagnosticSeverity;
using xray::hexagon::model::ImpactedTarget;
using xray::hexagon::model::ImpactedTranslationUnit;
using xray::hexagon::model::ImpactKind;
using xray::hexagon::model::ImpactResult;
using xray::hexagon::model::IncludeHotspot;
using xray::hexagon::model::RankedTranslationUnit;
using xray::hexagon::model::ReportInputs;
using xray::hexagon::model::ReportInputSource;
using xray::hexagon::model::TargetImpactClassification;
using xray::hexagon::model::TargetInfo;
using xray::hexagon::model::TargetMetadataStatus;
using xray::hexagon::model::TranslationUnitReference;

AnalysisResult make_minimal_analysis_result() {
    AnalysisResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    return result;
}

ImpactResult make_minimal_impact_result() {
    ImpactResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.inputs.changed_file = std::string{"src/changed.cpp"};
    result.inputs.changed_file_source = ChangedFileSource::compile_database_directory;
    return result;
}

RankedTranslationUnit make_ranked_translation_unit(std::string source_path,
                                                   std::string directory,
                                                   std::vector<Diagnostic> diagnostics = {}) {
    RankedTranslationUnit tu{};
    tu.reference = TranslationUnitReference{source_path, directory, source_path,
                                              source_path + "|" + directory};
    tu.rank = 1;
    tu.diagnostics = std::move(diagnostics);
    return tu;
}

std::size_t count_lines(std::string_view text) {
    std::size_t count = 0;
    for (const char ch : text) {
        if (ch == '\n') ++count;
    }
    return count;
}

}  // namespace

// ---- Tranche B.1: Quiet Analyze ----------------------------------------

TEST_CASE("Quiet analyze emits the documented pflichtzeilen in order with no targets and no warnings") {
    AnalysisResult result = make_minimal_analysis_result();
    result.translation_units.push_back(make_ranked_translation_unit("src/a.cpp", "build"));
    result.translation_units.push_back(make_ranked_translation_unit("src/b.cpp", "build"));
    const auto out = render_console_quiet_analyze(result, /*top_limit=*/10);
    CHECK(out ==
          "analysis: ok\n"
          "translation units: 2\n"
          "ranking entries shown: 2 of 2\n"
          "include hotspots shown: 0 of 0\n"
          "diagnostics: 0\n");
}

TEST_CASE("Quiet analyze emits target metadata only when status is not not_loaded") {
    AnalysisResult result = make_minimal_analysis_result();
    result.translation_units.push_back(make_ranked_translation_unit("src/a.cpp", "build"));
    SUBCASE("not_loaded -> line absent") {
        result.target_metadata = TargetMetadataStatus::not_loaded;
        const auto out = render_console_quiet_analyze(result, 10);
        CHECK(out.find("target metadata:") == std::string::npos);
    }
    SUBCASE("partial -> line present with partial label") {
        result.target_metadata = TargetMetadataStatus::partial;
        const auto out = render_console_quiet_analyze(result, 10);
        CHECK(out.find("target metadata: partial\n") != std::string::npos);
    }
    SUBCASE("loaded -> line present with loaded label") {
        result.target_metadata = TargetMetadataStatus::loaded;
        const auto out = render_console_quiet_analyze(result, 10);
        CHECK(out.find("target metadata: loaded\n") != std::string::npos);
    }
}

TEST_CASE("Quiet analyze emits warnings: present only when at least one warning is in the result") {
    AnalysisResult result = make_minimal_analysis_result();
    result.translation_units.push_back(make_ranked_translation_unit("src/a.cpp", "build"));
    SUBCASE("no warnings -> line absent") {
        const auto out = render_console_quiet_analyze(result, 10);
        CHECK(out.find("warnings:") == std::string::npos);
    }
    SUBCASE("report-level warning -> line present") {
        result.diagnostics.push_back(
            Diagnostic{DiagnosticSeverity::warning, "missing thing"});
        const auto out = render_console_quiet_analyze(result, 10);
        CHECK(out.find("warnings: present\n") != std::string::npos);
    }
    SUBCASE("only note severity -> line absent") {
        result.diagnostics.push_back(Diagnostic{DiagnosticSeverity::note, "hint"});
        const auto out = render_console_quiet_analyze(result, 10);
        CHECK(out.find("warnings:") == std::string::npos);
    }
    SUBCASE("per-TU warning lifts the line") {
        result.translation_units = {make_ranked_translation_unit(
            "src/a.cpp", "build",
            {Diagnostic{DiagnosticSeverity::warning, "tu warning"}})};
        const auto out = render_console_quiet_analyze(result, 10);
        CHECK(out.find("warnings: present\n") != std::string::npos);
    }
}

TEST_CASE("Quiet analyze top_limit only affects the shown counters, not totals") {
    AnalysisResult result = make_minimal_analysis_result();
    for (int i = 0; i < 7; ++i) {
        result.translation_units.push_back(
            make_ranked_translation_unit("src/" + std::to_string(i) + ".cpp", "build"));
    }
    result.include_hotspots = {
        IncludeHotspot{"include/a.h", {}, {}},
        IncludeHotspot{"include/b.h", {}, {}},
        IncludeHotspot{"include/c.h", {}, {}},
    };
    const auto out = render_console_quiet_analyze(result, /*top_limit=*/2);
    CHECK(out.find("translation units: 7\n") != std::string::npos);
    CHECK(out.find("ranking entries shown: 2 of 7\n") != std::string::npos);
    CHECK(out.find("include hotspots shown: 2 of 3\n") != std::string::npos);
}

TEST_CASE("Quiet analyze never lists individual translation units, hotspots or targets") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_metadata = TargetMetadataStatus::loaded;
    result.translation_units = {make_ranked_translation_unit("src/leaks.cpp", "build/leaks")};
    result.include_hotspots = {IncludeHotspot{"include/leaks.h", {}, {}}};
    const auto out = render_console_quiet_analyze(result, 10);
    CHECK(out.find("src/leaks.cpp") == std::string::npos);
    CHECK(out.find("include/leaks.h") == std::string::npos);
    CHECK(out.find("build/leaks") == std::string::npos);
}

TEST_CASE("Quiet analyze ends with exactly one trailing newline and no doubled newlines") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_metadata = TargetMetadataStatus::loaded;
    result.diagnostics.push_back(Diagnostic{DiagnosticSeverity::warning, "w"});
    const auto out = render_console_quiet_analyze(result, 10);
    REQUIRE_FALSE(out.empty());
    CHECK(out.back() == '\n');
    CHECK(out.find("\n\n") == std::string::npos);
}

// ---- Tranche B.1: Quiet Impact -----------------------------------------

TEST_CASE("Quiet impact emits the documented pflichtzeilen in order for direct match without targets") {
    ImpactResult result = make_minimal_impact_result();
    result.inputs.changed_file = std::string{"include/common/config.h"};
    result.heuristic = false;
    result.affected_translation_units.push_back(ImpactedTranslationUnit{
        TranslationUnitReference{"src/a.cpp", "build", "src/a.cpp", "src/a.cpp|build"},
        ImpactKind::direct, {}});
    const auto out = render_console_quiet_impact(result);
    CHECK(out ==
          "impact: ok\n"
          "changed file: include/common/config.h\n"
          "affected translation units: 1\n"
          "classification: direct\n"
          "affected targets: 0\n"
          "diagnostics: 0\n");
}

TEST_CASE("Quiet impact prints classification heuristic when the result is heuristic") {
    ImpactResult result = make_minimal_impact_result();
    result.heuristic = true;
    const auto out = render_console_quiet_impact(result);
    CHECK(out.find("classification: heuristic\n") != std::string::npos);
}

TEST_CASE("Quiet impact emits target metadata only when status is not not_loaded") {
    ImpactResult result = make_minimal_impact_result();
    SUBCASE("not_loaded -> line absent") {
        result.target_metadata = TargetMetadataStatus::not_loaded;
        const auto out = render_console_quiet_impact(result);
        CHECK(out.find("target metadata:") == std::string::npos);
    }
    SUBCASE("loaded -> line present") {
        result.target_metadata = TargetMetadataStatus::loaded;
        const auto out = render_console_quiet_impact(result);
        CHECK(out.find("target metadata: loaded\n") != std::string::npos);
    }
}

TEST_CASE("Quiet impact emits warnings: present only when at least one warning exists") {
    ImpactResult result = make_minimal_impact_result();
    SUBCASE("no warnings -> line absent") {
        const auto out = render_console_quiet_impact(result);
        CHECK(out.find("warnings:") == std::string::npos);
    }
    SUBCASE("report-level warning -> line present") {
        result.diagnostics.push_back(Diagnostic{DiagnosticSeverity::warning, "w"});
        const auto out = render_console_quiet_impact(result);
        CHECK(out.find("warnings: present\n") != std::string::npos);
    }
    SUBCASE("only note severity -> line absent") {
        result.diagnostics.push_back(Diagnostic{DiagnosticSeverity::note, "hint"});
        const auto out = render_console_quiet_impact(result);
        CHECK(out.find("warnings:") == std::string::npos);
    }
}

TEST_CASE("Quiet impact never lists individual translation units or targets") {
    ImpactResult result = make_minimal_impact_result();
    result.target_metadata = TargetMetadataStatus::loaded;
    result.affected_translation_units.push_back(ImpactedTranslationUnit{
        TranslationUnitReference{"src/leaks.cpp", "build/leaks", "src/leaks.cpp",
                                  "src/leaks.cpp|build/leaks"},
        ImpactKind::heuristic, {}});
    result.affected_targets.push_back(
        ImpactedTarget{TargetInfo{"leaky_target", "EXECUTABLE", "leaky_target::EXECUTABLE"},
                       TargetImpactClassification::heuristic});
    const auto out = render_console_quiet_impact(result);
    CHECK(out.find("src/leaks.cpp") == std::string::npos);
    CHECK(out.find("leaky_target") == std::string::npos);
    CHECK(out.find("build/leaks") == std::string::npos);
}

TEST_CASE("Quiet impact ends with exactly one trailing newline and no doubled newlines") {
    ImpactResult result = make_minimal_impact_result();
    result.target_metadata = TargetMetadataStatus::loaded;
    result.diagnostics.push_back(Diagnostic{DiagnosticSeverity::warning, "w"});
    const auto out = render_console_quiet_impact(result);
    REQUIRE_FALSE(out.empty());
    CHECK(out.back() == '\n');
    CHECK(out.find("\n\n") == std::string::npos);
}

TEST_CASE("Quiet impact pflichtzeilen count is six (plus optional lines)") {
    // The contract documents exactly six pflichtzeilen for impact (impact: ok,
    // changed file, affected translation units, classification, affected
    // targets, diagnostics). With the two optional lines (target metadata,
    // warnings: present) absent, the output must therefore have six newlines.
    ImpactResult result = make_minimal_impact_result();
    const auto out = render_console_quiet_impact(result);
    CHECK(count_lines(out) == 6);
}

TEST_CASE("Quiet analyze pflichtzeilen count is five (plus optional lines)") {
    AnalysisResult result = make_minimal_analysis_result();
    const auto out = render_console_quiet_analyze(result, 10);
    CHECK(count_lines(out) == 5);
}
