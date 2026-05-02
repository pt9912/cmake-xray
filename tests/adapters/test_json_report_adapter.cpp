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
#include "hexagon/model/target_graph.h"
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
using xray::hexagon::model::TargetDependency;
using xray::hexagon::model::TargetDependencyKind;
using xray::hexagon::model::TargetDependencyResolution;
using xray::hexagon::model::TargetGraph;
using xray::hexagon::model::TargetGraphStatus;
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

TEST_CASE("json output escapes JSON-special characters in diagnostic messages") {
    AnalysisResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.diagnostics = {
        {DiagnosticSeverity::warning, "quote: \" backslash: \\ tab:\t newline:\n"},
    };

    const JsonReportAdapter adapter;
    const auto rendered = adapter.write_analysis_report(result, 1);

    // Round-trip: parsing the rendered JSON yields the original message bytes.
    const auto doc = parse(rendered);
    REQUIRE(doc["diagnostics"].size() == 1);
    CHECK(doc["diagnostics"][0]["message"] ==
          "quote: \" backslash: \\ tab:\t newline:\n");

    // Raw JSON carries the escape sequences instead of literal control chars,
    // so a downstream tool that sees the JSON without a parser still gets a
    // valid document.
    CHECK(rendered.find("\\\"") != std::string::npos);
    CHECK(rendered.find("\\\\") != std::string::npos);
    CHECK(rendered.find("\\t") != std::string::npos);
    CHECK(rendered.find("\\n") != std::string::npos);
}

TEST_CASE("json output preserves UTF-8 in paths, diagnostics, and target names") {
    ImpactResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.target_metadata = TargetMetadataStatus::loaded;
    result.affected_translation_units = {
        ImpactedTranslationUnit{
            reference("src/t\xc3\xb6ster.cpp", "build/r\xc3\xa9sum\xc3\xa9",
                      "src/t\xc3\xb6ster.cpp|build"),
            ImpactKind::direct,
            {},
        },
    };
    result.affected_targets = {
        ImpactedTarget{TargetInfo{"\xe6\xb5\x8b\xe8\xaf\x95\xe5\xba\x93",
                                   "STATIC_LIBRARY", "cjk::STATIC"},
                       TargetImpactClassification::direct},
    };
    result.diagnostics = {
        {DiagnosticSeverity::note, "umlaut \xc3\xa4\xc3\xb6\xc3\xbc"},
    };
    result.inputs = ReportInputs{
        std::nullopt, ReportInputSource::not_provided,
        std::nullopt, std::nullopt, ReportInputSource::not_provided,
        std::optional<std::string>{"src/t\xc3\xb6ster.cpp"},
        std::optional<ChangedFileSource>{ChangedFileSource::cli_absolute},
    };
    result.changed_file = "src/t\xc3\xb6ster.cpp";
    result.heuristic = false;

    const JsonReportAdapter adapter;
    const auto rendered = adapter.write_impact_report(result);

    // Round-trip with nlohmann::json: every UTF-8 byte sequence that goes in
    // comes back unchanged.
    const auto doc = parse(rendered);
    CHECK(doc["inputs"]["changed_file"] == "src/t\xc3\xb6ster.cpp");
    REQUIRE(doc["directly_affected_translation_units"].size() == 1);
    CHECK(doc["directly_affected_translation_units"][0]["reference"]["source_path"]
          == "src/t\xc3\xb6ster.cpp");
    CHECK(doc["directly_affected_translation_units"][0]["reference"]["directory"]
          == "build/r\xc3\xa9sum\xc3\xa9");
    REQUIRE(doc["directly_affected_targets"].size() == 1);
    CHECK(doc["directly_affected_targets"][0]["display_name"]
          == "\xe6\xb5\x8b\xe8\xaf\x95\xe5\xba\x93");
    REQUIRE(doc["diagnostics"].size() == 1);
    CHECK(doc["diagnostics"][0]["message"] == "umlaut \xc3\xa4\xc3\xb6\xc3\xbc");

    // UTF-8 bytes appear as raw bytes in the JSON output (nlohmann's default
    // does not switch them to \uXXXX escapes), so the document stays compact
    // and human-grepable.
    CHECK(rendered.find("t\xc3\xb6ster") != std::string::npos);
    CHECK(rendered.find("r\xc3\xa9sum\xc3\xa9") != std::string::npos);
    CHECK(rendered.find("\xe6\xb5\x8b\xe8\xaf\x95\xe5\xba\x93") != std::string::npos);
    CHECK(rendered.find("\xc3\xa4\xc3\xb6\xc3\xbc") != std::string::npos);
}

// AP M5-1.7 Tranche C.2: synthetic UNC/Extended-Length-Pflichtfaelle fuer
// JSON-Render. Plan-M5-1-7.md "Pfad- und Zeilenenden-Vertrag" verlangt,
// dass UNC- und Extended-Length-Pfade in mindestens einem Adapter-, CLI-
// oder Golden-Test abgedeckt sind. Die Tests stellen Round-Trip-Treue
// (nlohmann::json parst die JSON-escapten Backslashes wieder zu raw
// backslashes) und JSON-Escape-Korrektheit (raw `\` wird zu `\\` im Wire-
// Format) sicher; die rohen JSON-Bytes pruefen wir zusaetzlich, damit ein
// versehentliches Re-Quote (z. B. `\\\\\\\\` statt `\\\\`) auffaellt.
TEST_CASE("json output preserves UNC paths verbatim through escape and round-trip") {
    ImpactResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.target_metadata = TargetMetadataStatus::loaded;
    const std::string unc_path = "\\\\server\\share\\dir\\foo.cpp";
    const std::string unc_directory = "\\\\server\\share\\build";
    const std::string unc_unique_key = "\\\\server\\share\\dir|build";
    result.affected_translation_units = {
        ImpactedTranslationUnit{
            reference(unc_path, unc_directory, unc_unique_key),
            ImpactKind::direct,
            {},
        },
    };
    result.inputs = ReportInputs{
        std::nullopt, ReportInputSource::not_provided,
        std::nullopt, std::nullopt, ReportInputSource::not_provided,
        std::optional<std::string>{unc_path},
        std::optional<ChangedFileSource>{ChangedFileSource::cli_absolute},
    };
    result.changed_file = unc_path;
    result.heuristic = false;

    const JsonReportAdapter adapter;
    const auto rendered = adapter.write_impact_report(result);

    const auto doc = parse(rendered);
    CHECK(doc["inputs"]["changed_file"] == unc_path);
    REQUIRE(doc["directly_affected_translation_units"].size() == 1);
    CHECK(doc["directly_affected_translation_units"][0]["reference"]["source_path"]
          == unc_path);
    CHECK(doc["directly_affected_translation_units"][0]["reference"]["directory"]
          == unc_directory);

    // Wire-format check: each raw backslash escapes to \\ in JSON, so the
    // leading double backslash of an UNC path appears as four backslash
    // characters in the rendered string (each `\\` here is one raw byte).
    CHECK(rendered.find("\\\\\\\\server\\\\share\\\\dir\\\\foo.cpp") != std::string::npos);
}

TEST_CASE("json output preserves extended-length paths verbatim through escape and round-trip") {
    ImpactResult result;
    result.application = xray::hexagon::model::application_info();
    result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    result.target_metadata = TargetMetadataStatus::loaded;
    const std::string el_path = "\\\\?\\C:\\very\\long\\path\\foo.cpp";
    const std::string el_directory = "\\\\?\\C:\\very\\long\\build";
    const std::string el_unique_key = "\\\\?\\C:\\very\\long\\path|build";
    result.affected_translation_units = {
        ImpactedTranslationUnit{
            reference(el_path, el_directory, el_unique_key),
            ImpactKind::direct,
            {},
        },
    };
    result.inputs = ReportInputs{
        std::nullopt, ReportInputSource::not_provided,
        std::nullopt, std::nullopt, ReportInputSource::not_provided,
        std::optional<std::string>{el_path},
        std::optional<ChangedFileSource>{ChangedFileSource::cli_absolute},
    };
    result.changed_file = el_path;
    result.heuristic = false;

    const JsonReportAdapter adapter;
    const auto rendered = adapter.write_impact_report(result);

    const auto doc = parse(rendered);
    CHECK(doc["inputs"]["changed_file"] == el_path);
    REQUIRE(doc["directly_affected_translation_units"].size() == 1);
    CHECK(doc["directly_affected_translation_units"][0]["reference"]["source_path"]
          == el_path);
    CHECK(doc["directly_affected_translation_units"][0]["reference"]["directory"]
          == el_directory);

    // The literal `?` survives unchanged (it is not a JSON special), and
    // each backslash escapes to \\ on the wire.
    CHECK(rendered.find("\\\\\\\\?\\\\C:\\\\very\\\\long\\\\path\\\\foo.cpp") != std::string::npos);
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

// --- M6 AP 1.2 A.1: target-graph fields in JSON v2 ---

namespace m6_json {

TargetDependency direct_edge(std::string from_uk, std::string to_uk,
                             TargetDependencyResolution res =
                                 TargetDependencyResolution::resolved) {
    return TargetDependency{from_uk, from_uk, to_uk,        to_uk,
                            TargetDependencyKind::direct, res};
}

}  // namespace m6_json

TEST_CASE("json analyze v2: not_loaded status yields empty target_graph and empty target_hubs at default thresholds") {
    AnalysisResult result = make_analysis_result_with_heuristic(true);
    // Default-constructed: status=not_loaded, graph empty, hubs empty.
    REQUIRE(result.target_graph_status == TargetGraphStatus::not_loaded);

    const JsonReportAdapter adapter;
    const auto rendered = adapter.write_analysis_report(result, 10);
    const auto doc = nlohmann::json::parse(rendered);

    CHECK(doc["format_version"] == kReportFormatVersion);
    CHECK(doc["target_graph_status"] == "not_loaded");
    CHECK(doc["target_graph"]["nodes"].empty());
    CHECK(doc["target_graph"]["edges"].empty());
    CHECK(doc["target_hubs"]["inbound"].empty());
    CHECK(doc["target_hubs"]["outbound"].empty());
    CHECK(doc["target_hubs"]["thresholds"]["in_threshold"] == 10);
    CHECK(doc["target_hubs"]["thresholds"]["out_threshold"] == 10);
}

TEST_CASE("json analyze v2: loaded status with hub propagates nodes, edges and inbound hub list") {
    AnalysisResult result = make_analysis_result_with_heuristic(true);
    result.target_graph_status = TargetGraphStatus::loaded;
    result.target_graph.nodes.push_back({"hub", "STATIC_LIBRARY", "hub::STATIC_LIBRARY"});
    for (int i = 0; i < 10; ++i) {
        const auto src_uk = "src" + std::to_string(i) + "::STATIC_LIBRARY";
        result.target_graph.nodes.push_back({"src" + std::to_string(i),
                                              "STATIC_LIBRARY", src_uk});
        result.target_graph.edges.push_back(
            m6_json::direct_edge(src_uk, "hub::STATIC_LIBRARY"));
    }
    result.target_hubs_in.push_back({"hub", "STATIC_LIBRARY", "hub::STATIC_LIBRARY"});

    const JsonReportAdapter adapter;
    const auto doc = nlohmann::json::parse(adapter.write_analysis_report(result, 10));

    CHECK(doc["target_graph_status"] == "loaded");
    CHECK(doc["target_graph"]["nodes"].size() == 11);
    CHECK(doc["target_graph"]["edges"].size() == 10);
    REQUIRE(doc["target_hubs"]["inbound"].size() == 1);
    CHECK(doc["target_hubs"]["inbound"][0]["unique_key"] == "hub::STATIC_LIBRARY");
    CHECK(doc["target_hubs"]["outbound"].empty());
    // Edge details: from_unique_key, to_unique_key, kind, resolution.
    const auto& first_edge = doc["target_graph"]["edges"][0];
    CHECK(first_edge["from_unique_key"] == "src0::STATIC_LIBRARY");
    CHECK(first_edge["to_unique_key"] == "hub::STATIC_LIBRARY");
    CHECK(first_edge["kind"] == "direct");
    CHECK(first_edge["resolution"] == "resolved");
}

TEST_CASE("json analyze v2: external edges serialize with <external>::* to_unique_key and resolution=external") {
    AnalysisResult result = make_analysis_result_with_heuristic(false);
    result.target_graph_status = TargetGraphStatus::partial;
    result.target_graph.nodes.push_back({"app", "EXECUTABLE", "app::EXECUTABLE"});
    result.target_graph.edges.push_back(TargetDependency{
        "app::EXECUTABLE", "app", "<external>::ghost", "ghost",
        TargetDependencyKind::direct, TargetDependencyResolution::external});

    const JsonReportAdapter adapter;
    const auto doc = nlohmann::json::parse(adapter.write_analysis_report(result, 10));

    CHECK(doc["target_graph_status"] == "partial");
    REQUIRE(doc["target_graph"]["edges"].size() == 1);
    CHECK(doc["target_graph"]["edges"][0]["to_unique_key"] == "<external>::ghost");
    CHECK(doc["target_graph"]["edges"][0]["to_display_name"] == "ghost");
    CHECK(doc["target_graph"]["edges"][0]["resolution"] == "external");
}

TEST_CASE("json impact v2: target_graph is serialized but target_hubs is absent (schema asymmetry)") {
    auto result = make_impact_result();
    result.target_graph_status = TargetGraphStatus::loaded;
    result.target_graph.nodes.push_back({"a", "STATIC_LIBRARY", "a::STATIC_LIBRARY"});
    result.target_graph.nodes.push_back({"b", "STATIC_LIBRARY", "b::STATIC_LIBRARY"});
    result.target_graph.edges.push_back(
        m6_json::direct_edge("a::STATIC_LIBRARY", "b::STATIC_LIBRARY"));

    const JsonReportAdapter adapter;
    const auto doc = nlohmann::json::parse(adapter.write_impact_report(result));

    CHECK(doc["format_version"] == kReportFormatVersion);
    CHECK(doc["target_graph_status"] == "loaded");
    CHECK(doc["target_graph"]["nodes"].size() == 2);
    CHECK(doc["target_graph"]["edges"].size() == 1);
    // The schema-enforced asymmetry: target_hubs must NOT appear in impact JSON.
    CHECK_FALSE(doc.contains("target_hubs"));
}

// ---- AP M6-1.3 A.3: shared text mappings for v3 priority enums --------

#include "adapters/output/impact_priority_text.h"

TEST_CASE("AP1.3 A.3: priority_class_text_v3 maps the three TargetPriorityClass values byte-stably") {
    using xray::adapters::output::priority_class_text_v3;
    using xray::hexagon::model::TargetPriorityClass;
    CHECK(priority_class_text_v3(TargetPriorityClass::direct) == "direct");
    CHECK(priority_class_text_v3(TargetPriorityClass::direct_dependent) ==
          "direct_dependent");
    CHECK(priority_class_text_v3(TargetPriorityClass::transitive_dependent) ==
          "transitive_dependent");
}

TEST_CASE("AP1.3 A.3: evidence_strength_text_v3 maps the three TargetEvidenceStrength values byte-stably") {
    using xray::adapters::output::evidence_strength_text_v3;
    using xray::hexagon::model::TargetEvidenceStrength;
    CHECK(evidence_strength_text_v3(TargetEvidenceStrength::direct) == "direct");
    CHECK(evidence_strength_text_v3(TargetEvidenceStrength::heuristic) ==
          "heuristic");
    CHECK(evidence_strength_text_v3(TargetEvidenceStrength::uncertain) ==
          "uncertain");
}

TEST_CASE("AP1.3 A.4: kReportFormatVersion is 3 once all five adapters render the v3 fields") {
    // AP M6-1.3 A.4 flips the production format-version constant. The
    // schema-side FormatVersion.const must follow in
    // spec/report-json.schema.json; report_json_schema_validation
    // verifies the pair stays in sync.
    static_assert(xray::hexagon::model::kReportFormatVersion == 3);
    CHECK(xray::hexagon::model::kReportFormatVersion == 3);
}
