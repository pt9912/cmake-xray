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
#include "hexagon/model/include_classification.h"
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
using xray::hexagon::model::TargetAssignment;
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

// ---- Tranche B.2: Verbose Analyze --------------------------------------

namespace {

using xray::adapters::cli::render_console_verbose_analyze;
using xray::adapters::cli::render_console_verbose_impact;

}  // namespace

TEST_CASE("Verbose analyze emits the seven pflichtsections in documented order") {
    AnalysisResult result = make_minimal_analysis_result();
    const auto out = render_console_verbose_analyze(result, 10);
    const auto pos_summary = out.find("verbose analysis summary\n");
    const auto pos_inputs = out.find("\ninputs\n");
    const auto pos_observation = out.find("\nobservation\n");
    const auto pos_ranking = out.find("\ntranslation unit ranking\n");
    const auto pos_hotspots = out.find("\ninclude hotspots\n");
    const auto pos_targets = out.find("\ntargets\n");
    const auto pos_diagnostics = out.find("\ndiagnostics\n");
    REQUIRE(pos_summary != std::string::npos);
    REQUIRE(pos_inputs != std::string::npos);
    REQUIRE(pos_observation != std::string::npos);
    REQUIRE(pos_ranking != std::string::npos);
    REQUIRE(pos_hotspots != std::string::npos);
    REQUIRE(pos_targets != std::string::npos);
    REQUIRE(pos_diagnostics != std::string::npos);
    CHECK(pos_summary < pos_inputs);
    CHECK(pos_inputs < pos_observation);
    CHECK(pos_observation < pos_ranking);
    CHECK(pos_ranking < pos_hotspots);
    CHECK(pos_hotspots < pos_targets);
    CHECK(pos_targets < pos_diagnostics);
}

TEST_CASE("Verbose analyze inputs section reads ReportInputs and uses 'not provided' for missing fields") {
    AnalysisResult result = make_minimal_analysis_result();
    // ReportInputs default: every optional is std::nullopt, sources are
    // not_provided. The inputs section must therefore say "not provided" for
    // every path field and "not_provided" for every source field.
    const auto out = render_console_verbose_analyze(result, 3);
    CHECK(out.find("compile_database_path: not provided\n") != std::string::npos);
    CHECK(out.find("cmake_file_api_path: not provided\n") != std::string::npos);
    CHECK(out.find("cmake_file_api_resolved_path: not provided\n") != std::string::npos);
    CHECK(out.find("compile_database_source: not_provided\n") != std::string::npos);
    CHECK(out.find("cmake_file_api_source: not_provided\n") != std::string::npos);
}

TEST_CASE("Verbose analyze observation section emits exactly the three documented values") {
    AnalysisResult result = make_minimal_analysis_result();
    result.observation_source = xray::hexagon::model::ObservationSource::derived;
    result.target_metadata = TargetMetadataStatus::partial;
    result.include_analysis_heuristic = true;
    const auto out = render_console_verbose_analyze(result, 3);
    CHECK(out.find("observation_source: derived\n") != std::string::npos);
    CHECK(out.find("target_metadata: partial\n") != std::string::npos);
    CHECK(out.find("include_analysis_heuristic: true\n") != std::string::npos);
}

TEST_CASE("Verbose analyze ranking is sorted by rank then source_path then directory then unique_key") {
    AnalysisResult result = make_minimal_analysis_result();
    auto tu1 = make_ranked_translation_unit("src/zeta.cpp", "build");
    tu1.rank = 1;
    auto tu2 = make_ranked_translation_unit("src/alpha.cpp", "build");
    tu2.rank = 2;
    auto tu3 = make_ranked_translation_unit("src/alpha.cpp", "build/extra");
    tu3.rank = 2;
    result.translation_units = {tu2, tu3, tu1};  // Out of order on purpose.
    const auto out = render_console_verbose_analyze(result, 10);
    const auto pos_zeta = out.find("zeta.cpp");
    const auto pos_alpha_build = out.find("alpha.cpp [directory: build]");
    const auto pos_alpha_extra = out.find("alpha.cpp [directory: build/extra]");
    REQUIRE(pos_zeta != std::string::npos);
    REQUIRE(pos_alpha_build != std::string::npos);
    REQUIRE(pos_alpha_extra != std::string::npos);
    // rank 1 (zeta) comes before rank 2; among rank 2, alpha sorts before
    // alpha-with-different-directory because directory is the third sort
    // key and "build" < "build/extra".
    CHECK(pos_zeta < pos_alpha_build);
    CHECK(pos_alpha_build < pos_alpha_extra);
}

TEST_CASE("Verbose analyze hotspots are sorted by affected count desc then header_path asc") {
    AnalysisResult result = make_minimal_analysis_result();
    TranslationUnitReference dummy{"src/x.cpp", "build", "src/x.cpp", "src/x.cpp|build"};
    result.include_hotspots = {
        IncludeHotspot{"include/zeta.h", {dummy}, {}},                       // 1 affected
        IncludeHotspot{"include/alpha.h", {dummy, dummy, dummy}, {}},        // 3 affected
        IncludeHotspot{"include/beta.h", {dummy, dummy, dummy}, {}},         // 3 affected (tie)
    };
    const auto out = render_console_verbose_analyze(result, 10);
    const auto pos_alpha = out.find("include/alpha.h");
    const auto pos_beta = out.find("include/beta.h");
    const auto pos_zeta = out.find("include/zeta.h");
    REQUIRE(pos_alpha != std::string::npos);
    REQUIRE(pos_beta != std::string::npos);
    REQUIRE(pos_zeta != std::string::npos);
    // 3-affected entries (alpha, beta) come before 1-affected (zeta); tie
    // between alpha and beta is broken by header_path asc.
    CHECK(pos_alpha < pos_beta);
    CHECK(pos_beta < pos_zeta);
}

TEST_CASE("Verbose analyze diagnostics section normalises newlines in messages to ' / '") {
    AnalysisResult result = make_minimal_analysis_result();
    result.diagnostics.push_back(
        Diagnostic{DiagnosticSeverity::warning, "first line\nsecond line"});
    const auto out = render_console_verbose_analyze(result, 3);
    CHECK(out.find("warning: first line / second line\n") != std::string::npos);
    CHECK(out.find("\nsecond line") == std::string::npos);
}

TEST_CASE("Verbose analyze top_limit affects ranking and hotspot listings") {
    AnalysisResult result = make_minimal_analysis_result();
    for (int i = 0; i < 5; ++i) {
        auto tu = make_ranked_translation_unit("src/" + std::to_string(i) + ".cpp", "build");
        tu.rank = static_cast<std::size_t>(i + 1);
        result.translation_units.push_back(tu);
    }
    const auto out = render_console_verbose_analyze(result, 2);
    CHECK(out.find("showing 2 of 5 (top_limit=2)") != std::string::npos);
    CHECK(out.find("src/0.cpp") != std::string::npos);
    CHECK(out.find("src/1.cpp") != std::string::npos);
    CHECK(out.find("src/2.cpp") == std::string::npos);
}

TEST_CASE("Verbose analyze empty sections print documented placeholder lines") {
    AnalysisResult result = make_minimal_analysis_result();
    const auto out = render_console_verbose_analyze(result, 5);
    CHECK(out.find("no translation units to report") != std::string::npos);
    CHECK(out.find("no include hotspots") != std::string::npos);
    CHECK(out.find("no target metadata loaded") != std::string::npos);
    CHECK(out.find("no diagnostics") != std::string::npos);
}

TEST_CASE("Verbose analyze ends with exactly one trailing newline") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_metadata = TargetMetadataStatus::loaded;
    result.diagnostics.push_back(Diagnostic{DiagnosticSeverity::warning, "w"});
    const auto out = render_console_verbose_analyze(result, 10);
    REQUIRE_FALSE(out.empty());
    CHECK(out.back() == '\n');
    REQUIRE(out.size() >= 2);
    // Single trailing newline: last char '\n', second-to-last not '\n'.
    CHECK(out[out.size() - 2] != '\n');
}

// ---- Tranche B.2: Verbose Impact ---------------------------------------

TEST_CASE("Verbose impact emits the eight pflichtsections in documented order") {
    ImpactResult result = make_minimal_impact_result();
    const auto out = render_console_verbose_impact(result);
    const auto pos_summary = out.find("verbose impact summary\n");
    const auto pos_inputs = out.find("\ninputs\n");
    const auto pos_observation = out.find("\nobservation\n");
    const auto pos_direct_tus = out.find("\ndirectly affected translation units\n");
    const auto pos_heuristic_tus = out.find("\nheuristically affected translation units\n");
    const auto pos_direct_targets = out.find("\ndirectly affected targets\n");
    const auto pos_heuristic_targets = out.find("\nheuristically affected targets\n");
    const auto pos_diagnostics = out.find("\ndiagnostics\n");
    REQUIRE(pos_summary != std::string::npos);
    REQUIRE(pos_inputs != std::string::npos);
    REQUIRE(pos_observation != std::string::npos);
    REQUIRE(pos_direct_tus != std::string::npos);
    REQUIRE(pos_heuristic_tus != std::string::npos);
    REQUIRE(pos_direct_targets != std::string::npos);
    REQUIRE(pos_heuristic_targets != std::string::npos);
    REQUIRE(pos_diagnostics != std::string::npos);
    CHECK(pos_summary < pos_inputs);
    CHECK(pos_inputs < pos_observation);
    CHECK(pos_observation < pos_direct_tus);
    CHECK(pos_direct_tus < pos_heuristic_tus);
    CHECK(pos_heuristic_tus < pos_direct_targets);
    CHECK(pos_direct_targets < pos_heuristic_targets);
    CHECK(pos_heuristic_targets < pos_diagnostics);
}

TEST_CASE("Verbose impact populates direct and heuristic sections together when both kinds present") {
    // AP M5-1.5 Tranche B.2 Sub-Risiko: a synthetic ImpactResult with both
    // direct and heuristic translation units / targets exercises the
    // section ordering for the case real fixtures cannot construct
    // (changed_file is either source or header, not both).
    ImpactResult result = make_minimal_impact_result();
    result.affected_translation_units = {
        ImpactedTranslationUnit{
            TranslationUnitReference{"src/direct.cpp", "build", "src/direct.cpp",
                                      "src/direct.cpp|build"},
            ImpactKind::direct, {}},
        ImpactedTranslationUnit{
            TranslationUnitReference{"src/heuristic.cpp", "build", "src/heuristic.cpp",
                                      "src/heuristic.cpp|build"},
            ImpactKind::heuristic, {}},
    };
    result.affected_targets = {
        ImpactedTarget{TargetInfo{"app-direct", "EXECUTABLE", "app-direct"},
                       TargetImpactClassification::direct},
        ImpactedTarget{TargetInfo{"app-heuristic", "EXECUTABLE", "app-heuristic"},
                       TargetImpactClassification::heuristic},
    };
    const auto out = render_console_verbose_impact(result);
    const auto pos_direct_tu = out.find("src/direct.cpp");
    const auto pos_heuristic_tu = out.find("src/heuristic.cpp");
    const auto pos_direct_target = out.find("app-direct");
    const auto pos_heuristic_target = out.find("app-heuristic");
    REQUIRE(pos_direct_tu != std::string::npos);
    REQUIRE(pos_heuristic_tu != std::string::npos);
    REQUIRE(pos_direct_target != std::string::npos);
    REQUIRE(pos_heuristic_target != std::string::npos);
    // Direct TU section comes before heuristic TU section.
    CHECK(pos_direct_tu < pos_heuristic_tu);
    // Heuristic TUs come before direct targets (section order).
    CHECK(pos_heuristic_tu < pos_direct_target);
    CHECK(pos_direct_target < pos_heuristic_target);
}

TEST_CASE("Verbose impact targets are sorted by display_name then type then unique_key") {
    ImpactResult result = make_minimal_impact_result();
    result.affected_targets = {
        ImpactedTarget{TargetInfo{"core", "STATIC_LIBRARY", "core::STATIC"},
                       TargetImpactClassification::direct},
        ImpactedTarget{TargetInfo{"core", "EXECUTABLE", "core::EXEC"},
                       TargetImpactClassification::direct},
        ImpactedTarget{TargetInfo{"app", "EXECUTABLE", "app::EXEC"},
                       TargetImpactClassification::direct},
    };
    const auto out = render_console_verbose_impact(result);
    const auto pos_app = out.find("- app [type:");
    const auto pos_core_exec = out.find("- core [type: EXECUTABLE]");
    const auto pos_core_static = out.find("- core [type: STATIC_LIBRARY]");
    REQUIRE(pos_app != std::string::npos);
    REQUIRE(pos_core_exec != std::string::npos);
    REQUIRE(pos_core_static != std::string::npos);
    CHECK(pos_app < pos_core_exec);
    CHECK(pos_core_exec < pos_core_static);
}

TEST_CASE("Verbose impact observation section omits include_analysis_heuristic") {
    // Plan: impact observation shows only Observation Source and Target
    // Metadata Status; include_analysis_heuristic is analyze-only.
    ImpactResult result = make_minimal_impact_result();
    const auto out = render_console_verbose_impact(result);
    CHECK(out.find("observation_source:") != std::string::npos);
    CHECK(out.find("target_metadata:") != std::string::npos);
    CHECK(out.find("include_analysis_heuristic:") == std::string::npos);
}

TEST_CASE("Verbose impact ends with exactly one trailing newline") {
    ImpactResult result = make_minimal_impact_result();
    const auto out = render_console_verbose_impact(result);
    REQUIRE_FALSE(out.empty());
    CHECK(out.back() == '\n');
    REQUIRE(out.size() >= 2);
    CHECK(out[out.size() - 2] != '\n');
}

// ---- Tranche B.2 coverage: branches not exercised by the section tests --

TEST_CASE("Verbose diagnostic-message normaliser collapses CRLF and lone CR to a single ' / '") {
    AnalysisResult result = make_minimal_analysis_result();
    result.diagnostics.push_back(
        Diagnostic{DiagnosticSeverity::warning, "first\r\nsecond\rthird"});
    const auto out = render_console_verbose_analyze(result, 3);
    // \r\n and \r both become " / "; no raw CR or LF must leak into the
    // rendered text.
    CHECK(out.find("first / second / third") != std::string::npos);
    CHECK(out.find("\r") == std::string::npos);
}

TEST_CASE("Verbose diagnostic-message normaliser turns control bytes into a single space") {
    AnalysisResult result = make_minimal_analysis_result();
    std::string with_controls;
    with_controls += 'a';
    with_controls += '\t';
    with_controls += 'b';
    with_controls += static_cast<char>(0x07);  // BEL
    with_controls += 'c';
    result.diagnostics.push_back(Diagnostic{DiagnosticSeverity::note, with_controls});
    const auto out = render_console_verbose_analyze(result, 3);
    CHECK(out.find("a b c") != std::string::npos);
    CHECK(out.find('\t') == std::string::npos);
    CHECK(out.find(static_cast<char>(0x07)) == std::string::npos);
}

TEST_CASE("Verbose ranking tie-breaker: identical rank/source_path/directory compare unique_key") {
    AnalysisResult result = make_minimal_analysis_result();
    auto tu1 = make_ranked_translation_unit("src/same.cpp", "build");
    tu1.rank = 1;
    tu1.reference.unique_key = "src/same.cpp|build|zzz";
    auto tu2 = make_ranked_translation_unit("src/same.cpp", "build");
    tu2.rank = 1;
    tu2.reference.unique_key = "src/same.cpp|build|aaa";
    result.translation_units = {tu1, tu2};
    const auto out = render_console_verbose_analyze(result, 10);
    // The unique_key tie-breaker is internal; we just confirm the renderer
    // does not crash and emits both entries.
    const auto first_pos = out.find("src/same.cpp [directory: build]");
    REQUIRE(first_pos != std::string::npos);
    const auto second_pos = out.find("src/same.cpp [directory: build]", first_pos + 1);
    CHECK(second_pos != std::string::npos);
}

TEST_CASE("Verbose hotspot-context tie-breaker: identical source_path/directory compare unique_key") {
    AnalysisResult result = make_minimal_analysis_result();
    TranslationUnitReference ref_a{"src/x.cpp", "build", "src/x.cpp", "src/x.cpp|build|aaa"};
    TranslationUnitReference ref_z{"src/x.cpp", "build", "src/x.cpp", "src/x.cpp|build|zzz"};
    result.include_hotspots = {IncludeHotspot{"include/h.h", {ref_z, ref_a}, {}}};
    const auto out = render_console_verbose_analyze(result, 10);
    CHECK(out.find("src/x.cpp [directory: build]") != std::string::npos);
}

TEST_CASE("Verbose impact tie-breaker: identical source_path/directory compare unique_key") {
    ImpactResult result = make_minimal_impact_result();
    result.affected_translation_units = {
        ImpactedTranslationUnit{
            TranslationUnitReference{"src/dup.cpp", "build", "src/dup.cpp",
                                      "src/dup.cpp|build|zzz"},
            ImpactKind::heuristic, {}},
        ImpactedTranslationUnit{
            TranslationUnitReference{"src/dup.cpp", "build", "src/dup.cpp",
                                      "src/dup.cpp|build|aaa"},
            ImpactKind::heuristic, {}},
    };
    const auto out = render_console_verbose_impact(result);
    const auto first_pos = out.find("src/dup.cpp [directory: build]");
    REQUIRE(first_pos != std::string::npos);
    const auto second_pos = out.find("src/dup.cpp [directory: build]", first_pos + 1);
    CHECK(second_pos != std::string::npos);
}

TEST_CASE("Verbose targets sort tie-breaker: identical display_name and type compare unique_key") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_metadata = TargetMetadataStatus::loaded;
    result.target_assignments = {
        TargetAssignment{"obs1",
                         {TargetInfo{"core", "EXECUTABLE", "core::EXEC::zzz"}}},
        TargetAssignment{"obs2",
                         {TargetInfo{"core", "EXECUTABLE", "core::EXEC::aaa"}}},
    };
    const auto out = render_console_verbose_analyze(result, 5);
    // Both targets render even though display_name and type are identical;
    // unique_key is the third sort key.
    const auto first_core = out.find("- core [type: EXECUTABLE]");
    REQUIRE(first_core != std::string::npos);
    const auto second_core = out.find("- core [type: EXECUTABLE]", first_core + 1);
    CHECK(second_core != std::string::npos);
}

TEST_CASE("Verbose ranking sort: equal rank with different source_path uses source_path tie-breaker") {
    AnalysisResult result = make_minimal_analysis_result();
    auto tu_zeta = make_ranked_translation_unit("src/zeta.cpp", "build");
    tu_zeta.rank = 5;
    auto tu_alpha = make_ranked_translation_unit("src/alpha.cpp", "build");
    tu_alpha.rank = 5;
    result.translation_units = {tu_zeta, tu_alpha};  // out of source_path order
    const auto out = render_console_verbose_analyze(result, 10);
    const auto pos_alpha = out.find("src/alpha.cpp");
    const auto pos_zeta = out.find("src/zeta.cpp");
    REQUIRE(pos_alpha != std::string::npos);
    REQUIRE(pos_zeta != std::string::npos);
    CHECK(pos_alpha < pos_zeta);
}

TEST_CASE("Verbose hotspot context sort: equal source_path with different directory uses directory tie-breaker") {
    AnalysisResult result = make_minimal_analysis_result();
    TranslationUnitReference ref_release{"src/x.cpp", "build/release", "src/x.cpp",
                                          "src/x.cpp|build/release"};
    TranslationUnitReference ref_debug{"src/x.cpp", "build/debug", "src/x.cpp",
                                        "src/x.cpp|build/debug"};
    result.include_hotspots = {IncludeHotspot{"include/h.h", {ref_release, ref_debug}, {}}};
    const auto out = render_console_verbose_analyze(result, 10);
    const auto pos_debug = out.find("[directory: build/debug]");
    const auto pos_release = out.find("[directory: build/release]");
    REQUIRE(pos_debug != std::string::npos);
    REQUIRE(pos_release != std::string::npos);
    CHECK(pos_debug < pos_release);
}

TEST_CASE("Verbose impact TU sort: equal source_path with different directory uses directory tie-breaker") {
    ImpactResult result = make_minimal_impact_result();
    result.affected_translation_units = {
        ImpactedTranslationUnit{
            TranslationUnitReference{"src/dup.cpp", "build/release", "src/dup.cpp",
                                      "src/dup.cpp|build/release"},
            ImpactKind::heuristic, {}},
        ImpactedTranslationUnit{
            TranslationUnitReference{"src/dup.cpp", "build/debug", "src/dup.cpp",
                                      "src/dup.cpp|build/debug"},
            ImpactKind::heuristic, {}},
    };
    const auto out = render_console_verbose_impact(result);
    const auto pos_debug = out.find("[directory: build/debug]");
    const auto pos_release = out.find("[directory: build/release]");
    REQUIRE(pos_debug != std::string::npos);
    REQUIRE(pos_release != std::string::npos);
    CHECK(pos_debug < pos_release);
}

TEST_CASE("Verbose inputs section renders 'not provided' when compile_database_path is std::nullopt") {
    AnalysisResult result = make_minimal_analysis_result();
    // Default ReportInputs has all optionals as std::nullopt; this exercises
    // the optional_or_not_provided fallback in the inputs section.
    REQUIRE_FALSE(result.inputs.compile_database_path.has_value());
    const auto out = render_console_verbose_analyze(result, 5);
    CHECK(out.find("compile_database_path: not provided\n") != std::string::npos);
}

TEST_CASE("Verbose impact inputs section labels every ChangedFileSource enum value") {
    ImpactResult result = make_minimal_impact_result();
    SUBCASE("compile_database_directory") {
        result.inputs.changed_file_source = ChangedFileSource::compile_database_directory;
        const auto out = render_console_verbose_impact(result);
        CHECK(out.find("changed_file_source: compile_database_directory\n") !=
              std::string::npos);
    }
    SUBCASE("file_api_source_root") {
        result.inputs.changed_file_source = ChangedFileSource::file_api_source_root;
        const auto out = render_console_verbose_impact(result);
        CHECK(out.find("changed_file_source: file_api_source_root\n") !=
              std::string::npos);
    }
    SUBCASE("cli_absolute") {
        result.inputs.changed_file_source = ChangedFileSource::cli_absolute;
        const auto out = render_console_verbose_impact(result);
        CHECK(out.find("changed_file_source: cli_absolute\n") != std::string::npos);
    }
    SUBCASE("unresolved_file_api_source_root") {
        result.inputs.changed_file_source =
            ChangedFileSource::unresolved_file_api_source_root;
        const auto out = render_console_verbose_impact(result);
        CHECK(out.find("changed_file_source: unresolved_file_api_source_root\n") !=
              std::string::npos);
    }
    SUBCASE("std::nullopt falls back to 'not provided'") {
        result.inputs.changed_file_source.reset();
        const auto out = render_console_verbose_impact(result);
        CHECK(out.find("changed_file_source: not provided\n") != std::string::npos);
    }
}
