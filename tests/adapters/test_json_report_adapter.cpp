#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "adapters/output/json_report_adapter.h"
#include "hexagon/model/analysis_result.h"
#include "hexagon/model/application_info.h"
#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/model/include_hotspot.h"
#include "hexagon/model/report_format_version.h"
#include "hexagon/model/report_inputs.h"
#include "hexagon/model/translation_unit.h"

namespace {

using xray::adapters::output::JsonReportAdapter;
using xray::hexagon::model::AnalysisResult;
using xray::hexagon::model::ChangedFileSource;
using xray::hexagon::model::CompileDatabaseError;
using xray::hexagon::model::CompileDatabaseResult;
using xray::hexagon::model::Diagnostic;
using xray::hexagon::model::DiagnosticSeverity;
using xray::hexagon::model::ImpactKind;
using xray::hexagon::model::ImpactResult;
using xray::hexagon::model::ImpactedTarget;
using xray::hexagon::model::ImpactedTranslationUnit;
using xray::hexagon::model::IncludeHotspot;
using xray::hexagon::model::ObservationSource;
using xray::hexagon::model::RankedTranslationUnit;
using xray::hexagon::model::ReportInputs;
using xray::hexagon::model::ReportInputSource;
using xray::hexagon::model::TargetAssignment;
using xray::hexagon::model::TargetImpactClassification;
using xray::hexagon::model::TargetInfo;
using xray::hexagon::model::TargetMetadataStatus;
using xray::hexagon::model::TranslationUnitReference;
using xray::hexagon::model::kReportFormatVersion;

TranslationUnitReference reference(std::string source_path, std::string directory,
                                   std::string unique_key) {
    // source_path_key and unique_key share the same value in test fixtures;
    // copy once into source_path_key, then move the original into unique_key.
    auto source_path_key = unique_key;
    return {std::move(source_path), std::move(directory),
            std::move(source_path_key), std::move(unique_key)};
}

AnalysisResult make_analysis_result_with_heuristic(bool heuristic) {
    AnalysisResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.compile_database_path = "build/compile_commands.json";
    result.inputs = ReportInputs{
        std::optional<std::string>{"build/compile_commands.json"},
        ReportInputSource::cli,
        std::nullopt,
        std::nullopt,
        ReportInputSource::not_provided,
        std::nullopt,
        std::nullopt,
    };
    result.include_analysis_heuristic = heuristic;
    result.translation_units = {
        RankedTranslationUnit{
            reference("src/app/main.cpp", "build/debug",
                      "src/app/main.cpp|build/debug"),
            1, 8, 2, 1, {}, {},
        },
        RankedTranslationUnit{
            reference("src/lib/core.cpp", "build/lib",
                      "src/lib/core.cpp|build/lib"),
            2, 4, 1, 0, {}, {},
        },
    };
    result.include_hotspots = {
        IncludeHotspot{
            "include/common/config.h",
            {
                reference("src/lib/core.cpp", "build/lib",
                          "src/lib/core.cpp|build/lib"),
                reference("src/app/main.cpp", "build/debug",
                          "src/app/main.cpp|build/debug"),
            },
            {{DiagnosticSeverity::note, "hotspot detail"}},
        },
    };
    result.diagnostics = {
        {DiagnosticSeverity::warning, "compile database was normalized"},
    };
    return result;
}

ImpactResult make_impact_result() {
    ImpactResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.compile_database_path = "build/compile_commands.json";
    result.changed_file = "src/lib.cpp";
    result.changed_file_key = "src/lib.cpp";
    result.inputs = ReportInputs{
        std::optional<std::string>{"build/compile_commands.json"},
        ReportInputSource::cli,
        std::nullopt,
        std::nullopt,
        ReportInputSource::not_provided,
        std::optional<std::string>{"src/lib.cpp"},
        std::optional<ChangedFileSource>{ChangedFileSource::compile_database_directory},
    };
    result.heuristic = true;
    result.affected_translation_units = {
        ImpactedTranslationUnit{
            reference("src/zlast.cpp", "build/zlast", "src/zlast.cpp|build/zlast"),
            ImpactKind::heuristic,
            {},
        },
        ImpactedTranslationUnit{
            reference("src/aone.cpp", "build/aone", "src/aone.cpp|build/aone"),
            ImpactKind::direct,
            {},
        },
        ImpactedTranslationUnit{
            reference("src/btwo.cpp", "build/btwo", "src/btwo.cpp|build/btwo"),
            ImpactKind::heuristic,
            {},
        },
    };
    result.diagnostics = {{DiagnosticSeverity::note, "impact note"}};
    return result;
}

nlohmann::json parse(const std::string& text) {
    return nlohmann::json::parse(text);
}

}  // namespace

TEST_CASE("json analyze report has stable top-level structure and metadata") {
    const JsonReportAdapter adapter;
    const auto report = adapter.write_analysis_report(
        make_analysis_result_with_heuristic(true), 10);
    REQUIRE(!report.empty());
    REQUIRE(report.back() == '\n');
    const auto doc = parse(report);

    CHECK(doc["format"] == "cmake-xray.analysis");
    CHECK(doc["format_version"].get<int>() == kReportFormatVersion);
    CHECK(doc["report_type"] == "analyze");
    REQUIRE(doc.contains("inputs"));
    REQUIRE(doc.contains("summary"));
    REQUIRE(doc.contains("translation_unit_ranking"));
    REQUIRE(doc.contains("include_hotspots"));
    REQUIRE(doc.contains("diagnostics"));
}

TEST_CASE("json analyze inputs serialize cli-or-null and resolved file api path") {
    AnalysisResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.inputs = ReportInputs{
        std::nullopt, ReportInputSource::not_provided,
        std::optional<std::string>{"build"}, std::optional<std::string>{"build/.cmake/api/v1/reply"},
        ReportInputSource::cli, std::nullopt, std::nullopt,
    };

    const JsonReportAdapter adapter;
    const auto doc = parse(adapter.write_analysis_report(result, 5));

    const auto& inputs = doc["inputs"];
    CHECK(inputs["compile_database_path"].is_null());
    CHECK(inputs["compile_database_source"].is_null());
    CHECK(inputs["cmake_file_api_path"] == "build");
    CHECK(inputs["cmake_file_api_resolved_path"] == "build/.cmake/api/v1/reply");
    CHECK(inputs["cmake_file_api_source"] == "cli");
}

TEST_CASE("json analyze summary booleans honor include_analysis_heuristic flag") {
    const JsonReportAdapter adapter;
    const auto doc_true = parse(
        adapter.write_analysis_report(make_analysis_result_with_heuristic(true), 5));
    const auto doc_false = parse(
        adapter.write_analysis_report(make_analysis_result_with_heuristic(false), 5));

    CHECK(doc_true["summary"]["include_analysis_heuristic"].get<bool>() == true);
    CHECK(doc_false["summary"]["include_analysis_heuristic"].get<bool>() == false);
    CHECK(doc_true["summary"]["observation_source"] == "exact");
    CHECK(doc_true["summary"]["target_metadata"] == "not_loaded");
}

TEST_CASE("json analyze ranking exposes limit/total/returned/truncated and item shape") {
    const JsonReportAdapter adapter;
    const auto doc = parse(
        adapter.write_analysis_report(make_analysis_result_with_heuristic(true), 1));

    const auto& ranking = doc["translation_unit_ranking"];
    CHECK(ranking["limit"].get<int>() == 1);
    CHECK(ranking["total_count"].get<int>() == 2);
    CHECK(ranking["returned_count"].get<int>() == 1);
    CHECK(ranking["truncated"].get<bool>() == true);
    REQUIRE(ranking["items"].size() == 1);
    const auto& item = ranking["items"][0];
    CHECK(item["rank"].get<int>() == 1);
    CHECK(item["reference"]["source_path"] == "src/app/main.cpp");
    CHECK(item["reference"]["directory"] == "build/debug");
    CHECK(item["metrics"]["arg_count"].get<int>() == 8);
    CHECK(item["metrics"]["include_path_count"].get<int>() == 2);
    CHECK(item["metrics"]["define_count"].get<int>() == 1);
    CHECK(item["metrics"]["score"].get<int>() == 11);
    CHECK(item["targets"].is_array());
    CHECK(item["targets"].empty());
    CHECK(item["diagnostics"].is_array());
    CHECK(item["diagnostics"].empty());
}

TEST_CASE("json analyze report-level diagnostics carry severity and message verbatim") {
    const JsonReportAdapter adapter;
    const auto doc = parse(
        adapter.write_analysis_report(make_analysis_result_with_heuristic(true), 5));

    REQUIRE(doc["diagnostics"].size() == 1);
    CHECK(doc["diagnostics"][0]["severity"] == "warning");
    CHECK(doc["diagnostics"][0]["message"] == "compile database was normalized");
}

TEST_CASE("json analyze ranking item carries diagnostics and target details verbatim") {
    AnalysisResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.translation_units = {
        RankedTranslationUnit{
            reference("src/app.cpp", "build", "src/app.cpp|build"),
            1, 5, 3, 2,
            {{DiagnosticSeverity::warning, "missing include"}},
            {TargetInfo{"app", "EXECUTABLE", "app::EXE"},
             TargetInfo{"app", "STATIC_LIBRARY", "app::STATIC"}},
        },
    };

    const JsonReportAdapter adapter;
    const auto doc = parse(adapter.write_analysis_report(result, 1));
    const auto& item = doc["translation_unit_ranking"]["items"][0];
    REQUIRE(item["diagnostics"].size() == 1);
    CHECK(item["diagnostics"][0]["severity"] == "warning");
    CHECK(item["diagnostics"][0]["message"] == "missing include");
    REQUIRE(item["targets"].size() == 2);
    CHECK(item["targets"][0]["display_name"] == "app");
    CHECK(item["targets"][0]["type"] == "EXECUTABLE");
    CHECK(item["targets"][1]["type"] == "STATIC_LIBRARY");
}

TEST_CASE("json analyze ranking sort tie-breaks rank by reference source_path") {
    AnalysisResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.translation_units = {
        RankedTranslationUnit{reference("z/last.cpp", "build", "z|build"), 1, 0, 0, 0, {}, {}},
        RankedTranslationUnit{reference("a/first.cpp", "build", "a|build"), 1, 0, 0, 0, {}, {}},
    };

    const JsonReportAdapter adapter;
    const auto doc = parse(adapter.write_analysis_report(result, 10));
    const auto& items = doc["translation_unit_ranking"]["items"];
    REQUIRE(items.size() == 2);
    CHECK(items[0]["reference"]["source_path"] == "a/first.cpp");
    CHECK(items[1]["reference"]["source_path"] == "z/last.cpp");
}

TEST_CASE("json analyze hotspot affected list is sorted and limit-aware") {
    const JsonReportAdapter adapter;
    const auto doc = parse(
        adapter.write_analysis_report(make_analysis_result_with_heuristic(true), 1));

    const auto& hotspots = doc["include_hotspots"];
    REQUIRE(hotspots["items"].size() == 1);
    const auto& hotspot = hotspots["items"][0];
    CHECK(hotspot["header_path"] == "include/common/config.h");
    CHECK(hotspot["affected_total_count"].get<int>() == 2);
    CHECK(hotspot["affected_returned_count"].get<int>() == 1);
    CHECK(hotspot["affected_truncated"].get<bool>() == true);
    REQUIRE(hotspot["affected_translation_units"].size() == 1);
    CHECK(hotspot["affected_translation_units"][0]["reference"]["source_path"]
          == "src/app/main.cpp");
}

TEST_CASE("json analyze hotspot items sort by affected count then header_path") {
    AnalysisResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.include_hotspots = {
        IncludeHotspot{"include/zeta.h",
                       {reference("a.cpp", "b", "a|b")}, {}},
        IncludeHotspot{"include/alpha.h",
                       {reference("a.cpp", "b", "a|b")}, {}},
        IncludeHotspot{"include/heavy.h",
                       {reference("a.cpp", "b", "a|b"),
                        reference("c.cpp", "d", "c|d"),
                        reference("e.cpp", "f", "e|f")}, {}},
    };

    const JsonReportAdapter adapter;
    const auto doc = parse(adapter.write_analysis_report(result, 10));
    const auto& items = doc["include_hotspots"]["items"];
    REQUIRE(items.size() == 3);
    CHECK(items[0]["header_path"] == "include/heavy.h");
    CHECK(items[1]["header_path"] == "include/alpha.h");
    CHECK(items[2]["header_path"] == "include/zeta.h");
}

TEST_CASE("json impact report exposes contract fields and impact-specific inputs") {
    const JsonReportAdapter adapter;
    const auto report = adapter.write_impact_report(make_impact_result());
    REQUIRE(!report.empty());
    REQUIRE(report.back() == '\n');
    const auto doc = parse(report);

    CHECK(doc["format"] == "cmake-xray.impact");
    CHECK(doc["format_version"].get<int>() == kReportFormatVersion);
    CHECK(doc["report_type"] == "impact");
    CHECK(doc["inputs"]["changed_file"] == "src/lib.cpp");
    CHECK(doc["inputs"]["changed_file_source"] == "compile_database_directory");
    CHECK(doc.contains("directly_affected_translation_units"));
    CHECK(doc.contains("heuristically_affected_translation_units"));
    CHECK(doc.contains("directly_affected_targets"));
    CHECK(doc.contains("heuristically_affected_targets"));
}

TEST_CASE("json impact summary classification follows heuristic and emptiness rules") {
    const JsonReportAdapter adapter;
    auto direct = make_impact_result();
    direct.heuristic = false;
    auto uncertain = make_impact_result();
    uncertain.heuristic = true;
    uncertain.affected_translation_units.clear();

    const auto doc_heuristic = parse(adapter.write_impact_report(make_impact_result()));
    const auto doc_direct = parse(adapter.write_impact_report(direct));
    const auto doc_uncertain = parse(adapter.write_impact_report(uncertain));
    CHECK(doc_heuristic["summary"]["classification"] == "heuristic");
    CHECK(doc_direct["summary"]["classification"] == "direct");
    CHECK(doc_uncertain["summary"]["classification"] == "uncertain");
}

TEST_CASE("json impact translation unit lists are split by kind and sorted") {
    const JsonReportAdapter adapter;
    const auto doc = parse(adapter.write_impact_report(make_impact_result()));

    const auto& direct = doc["directly_affected_translation_units"];
    REQUIRE(direct.size() == 1);
    CHECK(direct[0]["reference"]["source_path"] == "src/aone.cpp");

    const auto& heuristic = doc["heuristically_affected_translation_units"];
    REQUIRE(heuristic.size() == 2);
    CHECK(heuristic[0]["reference"]["source_path"] == "src/btwo.cpp");
    CHECK(heuristic[1]["reference"]["source_path"] == "src/zlast.cpp");
}

TEST_CASE("json impact target lists are split by classification and sorted") {
    auto result = make_impact_result();
    result.target_metadata = TargetMetadataStatus::loaded;
    result.affected_targets = {
        ImpactedTarget{TargetInfo{"zlib", "STATIC_LIBRARY", "zlib::STATIC"},
                       TargetImpactClassification::heuristic},
        ImpactedTarget{TargetInfo{"app", "EXECUTABLE", "app::EXECUTABLE"},
                       TargetImpactClassification::direct},
        ImpactedTarget{TargetInfo{"core", "SHARED_LIBRARY", "core::SHARED"},
                       TargetImpactClassification::heuristic},
    };

    const JsonReportAdapter adapter;
    const auto doc = parse(adapter.write_impact_report(result));

    const auto& direct = doc["directly_affected_targets"];
    REQUIRE(direct.size() == 1);
    CHECK(direct[0]["display_name"] == "app");
    CHECK(direct[0]["type"] == "EXECUTABLE");

    const auto& heuristic = doc["heuristically_affected_targets"];
    REQUIRE(heuristic.size() == 2);
    CHECK(heuristic[0]["display_name"] == "core");
    CHECK(heuristic[1]["display_name"] == "zlib");

    CHECK(doc["summary"]["affected_target_count"].get<int>() == 3);
    CHECK(doc["summary"]["direct_target_count"].get<int>() == 1);
    CHECK(doc["summary"]["heuristic_target_count"].get<int>() == 2);
}

TEST_CASE("json impact inputs serialize file_api_source_root and cli_absolute") {
    auto file_api_only = make_impact_result();
    file_api_only.inputs = ReportInputs{
        std::nullopt, ReportInputSource::not_provided,
        std::optional<std::string>{"build"}, std::optional<std::string>{"build/.cmake/api/v1/reply"},
        ReportInputSource::cli, std::optional<std::string>{"src/lib.cpp"},
        std::optional<ChangedFileSource>{ChangedFileSource::file_api_source_root},
    };

    auto absolute = make_impact_result();
    absolute.inputs.changed_file = std::string{"/project/src/lib.cpp"};
    absolute.inputs.changed_file_source = ChangedFileSource::cli_absolute;

    const JsonReportAdapter adapter;
    const auto doc_file_api = parse(adapter.write_impact_report(file_api_only));
    const auto doc_absolute = parse(adapter.write_impact_report(absolute));

    CHECK(doc_file_api["inputs"]["changed_file_source"] == "file_api_source_root");
    CHECK(doc_absolute["inputs"]["changed_file_source"] == "cli_absolute");
    CHECK(doc_absolute["inputs"]["changed_file"] == "/project/src/lib.cpp");
}

TEST_CASE("json target list tie-breaks identical display_name by type") {
    auto result = make_impact_result();
    result.target_metadata = TargetMetadataStatus::loaded;
    result.affected_targets = {
        ImpactedTarget{TargetInfo{"shared", "STATIC_LIBRARY", "shared::STATIC"},
                       TargetImpactClassification::direct},
        ImpactedTarget{TargetInfo{"shared", "EXECUTABLE", "shared::EXECUTABLE"},
                       TargetImpactClassification::direct},
    };

    const JsonReportAdapter adapter;
    const auto doc = parse(adapter.write_impact_report(result));
    const auto& direct = doc["directly_affected_targets"];
    REQUIRE(direct.size() == 2);
    CHECK(direct[0]["display_name"] == "shared");
    CHECK(direct[0]["type"] == "EXECUTABLE");
    CHECK(direct[1]["type"] == "STATIC_LIBRARY");
}

TEST_CASE("json reference list tie-breaks identical source_path by directory") {
    AnalysisResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.translation_units = {
        RankedTranslationUnit{reference("src/lib.cpp", "build/zeta", "z"), 1, 0, 0, 0, {}, {}},
        RankedTranslationUnit{reference("src/lib.cpp", "build/alpha", "a"), 1, 0, 0, 0, {}, {}},
    };

    const JsonReportAdapter adapter;
    const auto doc = parse(adapter.write_analysis_report(result, 10));
    const auto& items = doc["translation_unit_ranking"]["items"];
    REQUIRE(items.size() == 2);
    CHECK(items[0]["reference"]["directory"] == "build/alpha");
    CHECK(items[1]["reference"]["directory"] == "build/zeta");
}

TEST_CASE("json hotspot affected list resolves targets via target_assignments") {
    AnalysisResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.target_metadata = TargetMetadataStatus::loaded;
    result.target_assignments = {
        TargetAssignment{"src/main.cpp|build", {TargetInfo{"app", "EXECUTABLE", "app::EXE"}}},
    };
    result.include_hotspots = {
        IncludeHotspot{"include/header.h",
                       {reference("src/main.cpp", "build", "src/main.cpp|build")},
                       {}},
    };

    const JsonReportAdapter adapter;
    const auto doc = parse(adapter.write_analysis_report(result, 10));
    const auto& items = doc["include_hotspots"]["items"];
    REQUIRE(items.size() == 1);
    REQUIRE(items[0]["affected_translation_units"].size() == 1);
    const auto& targets =
        items[0]["affected_translation_units"][0]["targets"];
    REQUIRE(targets.size() == 1);
    CHECK(targets[0]["display_name"] == "app");
}

TEST_CASE("json summary maps partial target_metadata") {
    auto result = make_analysis_result_with_heuristic(true);
    result.target_metadata = TargetMetadataStatus::partial;
    result.observation_source = ObservationSource::derived;

    const JsonReportAdapter adapter;
    const auto doc = parse(adapter.write_analysis_report(result, 5));
    CHECK(doc["summary"]["target_metadata"] == "partial");
    CHECK(doc["summary"]["observation_source"] == "derived");
}

TEST_CASE("schema rejects rendered JSON whose changed_file_source is unresolved_file_api_source_root") {
    // The unresolved_file_api_source_root model value is an internal AP 1.1
    // error provenance. JSON v1 deliberately excludes it; the CLI emits a
    // text-only error in that case. This test pins both halves of the gate:
    //   1. The adapter renders the model value verbatim, so a stray code path
    //      that produces it cannot silently substitute another value.
    //   2. The schema's ChangedFileSource enum does not list it, so the
    //      rendered JSON would fail schema validation.
    auto result = make_impact_result();
    result.inputs.changed_file_source =
        ChangedFileSource::unresolved_file_api_source_root;

    const JsonReportAdapter adapter;
    const auto doc = parse(adapter.write_impact_report(result));
    CHECK(doc["inputs"]["changed_file_source"] == "unresolved_file_api_source_root");

    std::ifstream stream{XRAY_REPORT_JSON_SCHEMA_PATH};
    REQUIRE(stream.is_open());
    nlohmann::json schema;
    stream >> schema;
    const auto enum_values =
        schema.at("$defs").at("ChangedFileSource").at("enum")
            .get<std::vector<std::string>>();
    CHECK(std::find(enum_values.begin(), enum_values.end(),
                    std::string{"unresolved_file_api_source_root"}) ==
          enum_values.end());
}
