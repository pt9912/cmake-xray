#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <string_view>

#include "adapters/output/html_report_adapter.h"
#include "hexagon/model/analysis_result.h"
#include "hexagon/model/application_info.h"
#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/diagnostic.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/model/include_classification.h"
#include "hexagon/model/observation_source.h"
#include "hexagon/model/report_format_version.h"
#include "hexagon/model/report_inputs.h"
#include "hexagon/model/target_info.h"
#include "hexagon/model/translation_unit.h"

namespace {

using xray::adapters::output::html_escape_attribute;
using xray::adapters::output::html_escape_text;
using xray::adapters::output::html_report_css;
using xray::adapters::output::HtmlReportAdapter;
using xray::adapters::output::normalize_html_whitespace;
using xray::adapters::output::render_attribute;
using xray::adapters::output::render_text;
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
    return result;
}

TranslationUnitReference reference(std::string source_path, std::string directory,
                                   std::string unique_key) {
    auto source_path_key = unique_key;
    return {std::move(source_path), std::move(directory),
            std::move(source_path_key), std::move(unique_key)};
}

RankedTranslationUnit ranked_tu(std::string source_path, std::string directory,
                                std::string unique_key) {
    return RankedTranslationUnit{
        reference(std::move(source_path), std::move(directory), std::move(unique_key)),
        1, 0, 0, 0, {}, {},
    };
}

}  // namespace

// ---- HTML escaping ------------------------------------------------------

TEST_CASE("html_escape_text escapes the three text-node specials") {
    CHECK(html_escape_text("plain text") == "plain text");
    CHECK(html_escape_text("a & b") == "a &amp; b");
    CHECK(html_escape_text("a < b") == "a &lt; b");
    CHECK(html_escape_text("a > b") == "a &gt; b");
    CHECK(html_escape_text("a&b<c>d") == "a&amp;b&lt;c&gt;d");
}

TEST_CASE("html_escape_text leaves quote and apostrophe untouched") {
    // Per spec/report-html.md, text nodes do not need to escape " or '.
    // Only attribute values do.
    CHECK(html_escape_text("a \"b\" c") == "a \"b\" c");
    CHECK(html_escape_text("it's fine") == "it's fine");
}

TEST_CASE("html_escape_text neutralizes <script>-like input") {
    CHECK(html_escape_text("<script>alert(1)</script>") ==
          "&lt;script&gt;alert(1)&lt;/script&gt;");
}

TEST_CASE("html_escape_attribute escapes the five attribute specials") {
    CHECK(html_escape_attribute("plain") == "plain");
    CHECK(html_escape_attribute("a & b") == "a &amp; b");
    CHECK(html_escape_attribute("a < b") == "a &lt; b");
    CHECK(html_escape_attribute("a > b") == "a &gt; b");
    CHECK(html_escape_attribute("a \"b\" c") == "a &quot;b&quot; c");
    CHECK(html_escape_attribute("it's fine") == "it&#39;s fine");
}

TEST_CASE("html_escape_attribute does not pre-decode existing entities") {
    // A single pass must escape `&` first regardless of what follows. We
    // never round-trip through pre-escaped data, so `&amp;` in the input
    // must become `&amp;amp;` rather than being left as the original
    // entity.
    CHECK(html_escape_attribute("&amp;") == "&amp;amp;");
}

TEST_CASE("html_escape_text and _attribute leave UTF-8 high bytes untouched") {
    // The contract escapes only ASCII specials; UTF-8 multi-byte sequences
    // pass through verbatim so non-Latin paths and target names render
    // correctly.
    const std::string utf8 = "caf\xc3\xa9";  // "café"
    CHECK(html_escape_text(utf8) == utf8);
    CHECK(html_escape_attribute(utf8) == utf8);
}

// ---- Whitespace normalization ------------------------------------------

TEST_CASE("normalize_html_whitespace turns LF into a visible separator") {
    CHECK(normalize_html_whitespace("a\nb") == "a / b");
    CHECK(normalize_html_whitespace("first\nsecond\nthird") ==
          "first / second / third");
}

TEST_CASE("normalize_html_whitespace collapses CRLF and lone CR to one separator") {
    // \r\n must collapse to a single " / " — lone \r also becomes " / ".
    CHECK(normalize_html_whitespace("a\r\nb") == "a / b");
    CHECK(normalize_html_whitespace("a\rb") == "a / b");
    CHECK(normalize_html_whitespace("a\r\n\r\nb") == "a /  / b");
}

TEST_CASE("normalize_html_whitespace turns tabs into single spaces") {
    CHECK(normalize_html_whitespace("a\tb") == "a b");
    CHECK(normalize_html_whitespace("a\t\tb") == "a  b");
}

TEST_CASE("normalize_html_whitespace turns other ASCII control bytes into spaces") {
    // Every control byte below 0x20 except the ones explicitly handled
    // (\n, \r, \t) becomes a single space. Pin a representative subset.
    CHECK(normalize_html_whitespace(std::string{"a\x01"} + "b") == "a b");
    CHECK(normalize_html_whitespace(std::string{"a\x07"} + "b") == "a b");
    CHECK(normalize_html_whitespace(std::string{"a\x1F"} + "b") == "a b");
}

TEST_CASE("normalize_html_whitespace leaves printable bytes intact") {
    CHECK(normalize_html_whitespace("foo bar baz") == "foo bar baz");
    CHECK(normalize_html_whitespace("a/b\\c") == "a/b\\c");
}

// ---- render_text / render_attribute composition ------------------------

TEST_CASE("render_text normalizes whitespace before HTML escaping") {
    // The composition order matters: " / " separator inserted by
    // normalization must flow through escape unchanged because the slash
    // and spaces are not escape candidates.
    CHECK(render_text("a\nb") == "a / b");
    CHECK(render_text("<a>\n<b>") == "&lt;a&gt; / &lt;b&gt;");
    CHECK(render_text("&\t<") == "&amp; &lt;");
}

TEST_CASE("render_attribute normalizes whitespace before attribute escaping") {
    CHECK(render_attribute("a\nb") == "a / b");
    CHECK(render_attribute("\"a\"\n'b'") == "&quot;a&quot; / &#39;b&#39;");
    CHECK(render_attribute("&\t<\t>") == "&amp; &lt; &gt;");
}

// ---- CSS contract -------------------------------------------------------

TEST_CASE("html_report_css is non-empty and free of forbidden external resources") {
    const auto css = std::string{html_report_css()};
    REQUIRE(!css.empty());
    // No external resources or animations per spec/report-html.md.
    CHECK(css.find("@import") == std::string::npos);
    CHECK(css.find("url(") == std::string::npos);
    CHECK(css.find("@keyframes") == std::string::npos);
    CHECK(css.find("animation:") == std::string::npos);
    CHECK(css.find("animation-name") == std::string::npos);
    // No inline JavaScript via expression(...) or javascript: links.
    CHECK(css.find("expression(") == std::string::npos);
    CHECK(css.find("javascript:") == std::string::npos);
}

TEST_CASE("html_report_css uses static system fontstacks only") {
    const auto css = std::string{html_report_css()};
    CHECK(css.find("system-ui") != std::string::npos);
    CHECK(css.find("ui-monospace") != std::string::npos);
    // No external font-face declarations or font-loading patterns.
    CHECK(css.find("@font-face") == std::string::npos);
    CHECK(css.find("font-display") == std::string::npos);
    CHECK(css.find("src:") == std::string::npos);
    // No CDN or data: URI based font/image embedding.
    CHECK(css.find("fonts.googleapis") == std::string::npos);
    CHECK(css.find("data:") == std::string::npos);
    // No bare URL schemes that could fetch resources at render time.
    CHECK(css.find("http:") == std::string::npos);
    CHECK(css.find("https:") == std::string::npos);
}

TEST_CASE("html_report_css carries print-friendly defaults") {
    const auto css = std::string{html_report_css()};
    CHECK(css.find("@media print") != std::string::npos);
}

// ---- Tranche A skeleton structure --------------------------------------

TEST_CASE("HTML analyze report emits the documented document scaffold") {
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(make_minimal_analysis_result(), 3);
    REQUIRE(!report.empty());

    CHECK(report.find("<!doctype html>") != std::string::npos);
    CHECK(report.find("<html lang=\"en\">") != std::string::npos);
    CHECK(report.find("<head>") != std::string::npos);
    CHECK(report.find("<body>") != std::string::npos);
    CHECK(report.find("<main class=\"report\" data-report-type=\"analyze\"") !=
          std::string::npos);
    CHECK(report.find("<title>cmake-xray analyze report</title>") != std::string::npos);
    CHECK(report.find("<h1>cmake-xray analyze report</h1>") != std::string::npos);
}

TEST_CASE("HTML impact report emits the documented document scaffold") {
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_impact_report(make_minimal_impact_result());
    REQUIRE(!report.empty());

    CHECK(report.find("<!doctype html>") != std::string::npos);
    CHECK(report.find("<html lang=\"en\">") != std::string::npos);
    CHECK(report.find("<main class=\"report\" data-report-type=\"impact\"") !=
          std::string::npos);
    CHECK(report.find("<title>cmake-xray impact report</title>") != std::string::npos);
    CHECK(report.find("<h1>cmake-xray impact report</h1>") != std::string::npos);
}

TEST_CASE("HTML head order follows spec/report-html.md") {
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(make_minimal_analysis_result(), 3);
    const auto pos_charset = report.find("meta charset");
    const auto pos_viewport = report.find("meta name=\"viewport\"");
    const auto pos_format_version = report.find("xray-report-format-version");
    const auto pos_title = report.find("<title>");
    const auto pos_style = report.find("<style>");
    REQUIRE(pos_charset != std::string::npos);
    REQUIRE(pos_viewport != std::string::npos);
    REQUIRE(pos_format_version != std::string::npos);
    REQUIRE(pos_title != std::string::npos);
    REQUIRE(pos_style != std::string::npos);
    CHECK(pos_charset < pos_viewport);
    CHECK(pos_viewport < pos_format_version);
    CHECK(pos_format_version < pos_title);
    CHECK(pos_title < pos_style);
}

TEST_CASE("HTML report carries the kReportFormatVersion meta and data-attribute") {
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(make_minimal_analysis_result(), 3);
    const auto version = std::to_string(static_cast<std::size_t>(kReportFormatVersion));
    CHECK(report.find("xray-report-format-version\" content=\"" + version + "\"") !=
          std::string::npos);
    CHECK(report.find("data-format-version=\"" + version + "\"") != std::string::npos);
}

TEST_CASE("HTML report contains exactly one h1 and exactly one main") {
    const HtmlReportAdapter adapter;
    const auto analyze = adapter.write_analysis_report(make_minimal_analysis_result(), 3);
    const auto impact = adapter.write_impact_report(make_minimal_impact_result());
    auto count = [](std::string_view haystack, std::string_view needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = haystack.find(needle, pos)) != std::string_view::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };
    CHECK(count(analyze, "<h1") == 1);
    CHECK(count(analyze, "<main ") == 1);
    CHECK(count(impact, "<h1") == 1);
    CHECK(count(impact, "<main ") == 1);
}

TEST_CASE("HTML report contains no script tag and no external resource references") {
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(make_minimal_analysis_result(), 3);
    CHECK(report.find("<script") == std::string::npos);
    CHECK(report.find("</script>") == std::string::npos);
    CHECK(report.find("<iframe") == std::string::npos);
    CHECK(report.find("<link ") == std::string::npos);
    // No URL-loading patterns inside the document.
    CHECK(report.find("http://") == std::string::npos);
    CHECK(report.find("https://") == std::string::npos);
    // No HTML comments.
    CHECK(report.find("<!--") == std::string::npos);
}

TEST_CASE("HTML report inlines the static CSS string verbatim") {
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(make_minimal_analysis_result(), 3);
    const auto css = std::string{html_report_css()};
    CHECK(report.find(css) != std::string::npos);
}

TEST_CASE("HTML report ends with a newline-terminated </html>") {
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(make_minimal_analysis_result(), 3);
    REQUIRE(!report.empty());
    CHECK(report.back() == '\n');
    CHECK(report.find("</html>") != std::string::npos);
}

// ---- Tranche B: analyze pflichtsections --------------------------------

TEST_CASE("HTML analyze report emits all six pflichtsections in documented order") {
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(make_minimal_analysis_result(), 3);
    const auto pos_summary = report.find("class=\"summary\"");
    const auto pos_inputs = report.find("class=\"inputs\"");
    const auto pos_ranking = report.find("class=\"ranking\"");
    const auto pos_hotspots = report.find("class=\"hotspots\"");
    const auto pos_targets = report.find("class=\"target-metadata\"");
    const auto pos_diags = report.find("class=\"diagnostics\"");
    REQUIRE(pos_summary != std::string::npos);
    REQUIRE(pos_inputs != std::string::npos);
    REQUIRE(pos_ranking != std::string::npos);
    REQUIRE(pos_hotspots != std::string::npos);
    REQUIRE(pos_targets != std::string::npos);
    REQUIRE(pos_diags != std::string::npos);
    CHECK(pos_summary < pos_inputs);
    CHECK(pos_inputs < pos_ranking);
    CHECK(pos_ranking < pos_hotspots);
    CHECK(pos_hotspots < pos_targets);
    CHECK(pos_targets < pos_diags);
}

TEST_CASE("HTML analyze inputs section uses the documented dt labels and not provided defaults") {
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(make_minimal_analysis_result(), 3);
    CHECK(report.find("<dt>compile_database_path</dt>") != std::string::npos);
    CHECK(report.find("<dt>compile_database_source</dt>") != std::string::npos);
    CHECK(report.find("<dt>cmake_file_api_path</dt>") != std::string::npos);
    CHECK(report.find("<dt>cmake_file_api_resolved_path</dt>") != std::string::npos);
    CHECK(report.find("<dt>cmake_file_api_source</dt>") != std::string::npos);
    // changed_file is impact-only.
    CHECK(report.find("<dt>changed_file</dt>") == std::string::npos);
    // Defaults render as "not provided" rather than empty strings.
    CHECK(report.find("<dd>not provided</dd>") != std::string::npos);
}

TEST_CASE("HTML analyze inputs section renders cli source token verbatim") {
    AnalysisResult result = make_minimal_analysis_result();
    result.inputs.compile_database_path = "tests/e2e/testdata/m2/basic_project/compile_commands.json";
    result.inputs.compile_database_source = ReportInputSource::cli;
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    CHECK(report.find("<dd>tests/e2e/testdata/m2/basic_project/compile_commands.json</dd>") !=
          std::string::npos);
    CHECK(report.find("<dd>cli</dd>") != std::string::npos);
}

TEST_CASE("HTML analyze ranking section renders the pflichtspalten") {
    AnalysisResult result = make_minimal_analysis_result();
    auto tu = ranked_tu("src/app.cpp", "build", "src/app.cpp|build");
    tu.rank = 1;
    tu.arg_count = 4;
    tu.include_path_count = 2;
    tu.define_count = 1;
    result.translation_units = {tu};
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    CHECK(report.find("<th scope=\"col\">Rank</th>") != std::string::npos);
    CHECK(report.find("<th scope=\"col\">Source path</th>") != std::string::npos);
    CHECK(report.find("<th scope=\"col\">Directory</th>") != std::string::npos);
    CHECK(report.find("<th scope=\"col\">Score</th>") != std::string::npos);
    CHECK(report.find("<th scope=\"col\">Arguments</th>") != std::string::npos);
    CHECK(report.find("<th scope=\"col\">Include paths</th>") != std::string::npos);
    CHECK(report.find("<th scope=\"col\">Defines</th>") != std::string::npos);
    CHECK(report.find("<th scope=\"col\">Targets</th>") != std::string::npos);
    CHECK(report.find("<th scope=\"col\">Diagnostics</th>") != std::string::npos);
    // Score is sum of arg_count + include_path_count + define_count.
    CHECK(report.find("<td>4</td><td>2</td><td>1</td>") != std::string::npos);
    CHECK(report.find("<td>7</td>") != std::string::npos);  // score
    CHECK(report.find("src/app.cpp") != std::string::npos);
}

TEST_CASE("HTML analyze ranking respects top_limit and emits Showing N of M hint") {
    AnalysisResult result = make_minimal_analysis_result();
    for (int i = 0; i < 5; ++i) {
        const auto path = "src/file" + std::to_string(i) + ".cpp";
        auto tu = ranked_tu(path, "build", path + "|build");
        tu.rank = static_cast<std::size_t>(i + 1);
        result.translation_units.push_back(tu);
    }
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 2);
    CHECK(report.find("Showing 2 of 5 entries.") != std::string::npos);
    CHECK(report.find("src/file0.cpp") != std::string::npos);
    CHECK(report.find("src/file1.cpp") != std::string::npos);
    CHECK(report.find("src/file3.cpp") == std::string::npos);
}

TEST_CASE("HTML analyze ranking emits leersatz when no translation units") {
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(make_minimal_analysis_result(), 3);
    CHECK(report.find("No translation units to report.") != std::string::npos);
}

TEST_CASE("HTML analyze hotspots section emits leersatz when no hotspots") {
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(make_minimal_analysis_result(), 3);
    CHECK(report.find("No include hotspots to report.") != std::string::npos);
}

TEST_CASE("HTML analyze hotspots section renders header and affected counts") {
    AnalysisResult result = make_minimal_analysis_result();
    result.include_hotspots = {
        IncludeHotspot{"include/common.h",
                       {reference("src/a.cpp", "build", "src/a.cpp|build"),
                        reference("src/b.cpp", "build", "src/b.cpp|build")},
                       {}},
    };
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    CHECK(report.find("<th scope=\"col\">Header</th>") != std::string::npos);
    CHECK(report.find("<th scope=\"col\">Affected translation units</th>") !=
          std::string::npos);
    CHECK(report.find("<th scope=\"col\">Translation unit context</th>") !=
          std::string::npos);
    CHECK(report.find("include/common.h") != std::string::npos);
}

TEST_CASE("HTML analyze hotspots context truncation emits Showing N of M hint per hotspot") {
    AnalysisResult result = make_minimal_analysis_result();
    std::vector<TranslationUnitReference> affected;
    for (int i = 0; i < 6; ++i) {
        const auto path = "src/affected" + std::to_string(i) + ".cpp";
        affected.push_back(reference(path, "build", path + "|build"));
    }
    result.include_hotspots = {IncludeHotspot{"include/header.h", affected, {}}};
    const HtmlReportAdapter adapter;
    // top_limit=2 → context per hotspot is also capped at 2 of 6.
    const auto report = adapter.write_analysis_report(result, 2);
    CHECK(report.find("Showing 2 of 6 translation units.") != std::string::npos);
}

TEST_CASE("HTML analyze target metadata leersatz when no metadata loaded") {
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(make_minimal_analysis_result(), 3);
    CHECK(report.find("No target metadata loaded.") != std::string::npos);
}

TEST_CASE("HTML analyze target metadata renders sorted unique targets") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_metadata = TargetMetadataStatus::loaded;
    result.target_assignments = {
        TargetAssignment{"src/a.cpp|build",
                         {TargetInfo{"core", "STATIC_LIBRARY", "core::STATIC_LIBRARY"}}},
        TargetAssignment{"src/b.cpp|build",
                         {TargetInfo{"app", "EXECUTABLE", "app::EXECUTABLE"}}},
    };
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    // Targets sort by display_name ascending → app before core.
    const auto pos_app = report.find("app</td>");
    const auto pos_core = report.find("core</td>");
    REQUIRE(pos_app != std::string::npos);
    REQUIRE(pos_core != std::string::npos);
    CHECK(pos_app < pos_core);
    CHECK(report.find("EXECUTABLE</td>") != std::string::npos);
    CHECK(report.find("STATIC_LIBRARY</td>") != std::string::npos);
}

TEST_CASE("HTML analyze diagnostics section renders severity badges") {
    AnalysisResult result = make_minimal_analysis_result();
    result.diagnostics = {
        Diagnostic{DiagnosticSeverity::warning, "could not resolve include"},
        Diagnostic{DiagnosticSeverity::note, "conditional includes may be missing"},
    };
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    CHECK(report.find("<span class=\"badge\">warning</span>") != std::string::npos);
    CHECK(report.find("<span class=\"badge\">note</span>") != std::string::npos);
    CHECK(report.find("could not resolve include") != std::string::npos);
}

TEST_CASE("HTML analyze diagnostics section emits leersatz when empty") {
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(make_minimal_analysis_result(), 3);
    CHECK(report.find("No diagnostics.") != std::string::npos);
}

// ---- Tranche B: impact pflichtsections ---------------------------------

TEST_CASE("HTML impact report emits all six pflichtsections in documented order") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = "include/common.h";
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    const auto pos_summary = report.find("class=\"summary\"");
    const auto pos_inputs = report.find("class=\"inputs\"");
    const auto pos_direct_tus = report.find("class=\"impact-direct-tus\"");
    const auto pos_heuristic_tus = report.find("class=\"impact-heuristic-tus\"");
    const auto pos_direct_targets = report.find("class=\"impact-direct-targets\"");
    const auto pos_heuristic_targets = report.find("class=\"impact-heuristic-targets\"");
    const auto pos_diags = report.find("class=\"diagnostics\"");
    REQUIRE(pos_summary != std::string::npos);
    REQUIRE(pos_inputs != std::string::npos);
    REQUIRE(pos_direct_tus != std::string::npos);
    REQUIRE(pos_heuristic_tus != std::string::npos);
    REQUIRE(pos_direct_targets != std::string::npos);
    REQUIRE(pos_heuristic_targets != std::string::npos);
    REQUIRE(pos_diags != std::string::npos);
    CHECK(pos_summary < pos_inputs);
    CHECK(pos_inputs < pos_direct_tus);
    CHECK(pos_direct_tus < pos_heuristic_tus);
    CHECK(pos_heuristic_tus < pos_direct_targets);
    CHECK(pos_direct_targets < pos_heuristic_targets);
    CHECK(pos_heuristic_targets < pos_diags);
}

TEST_CASE("HTML impact inputs section includes changed_file and changed_file_source") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = "include/common/shared.h";
    result.inputs.changed_file_source = ChangedFileSource::compile_database_directory;
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    CHECK(report.find("<dt>changed_file</dt>") != std::string::npos);
    CHECK(report.find("include/common/shared.h") != std::string::npos);
    CHECK(report.find("<dt>changed_file_source</dt>") != std::string::npos);
    CHECK(report.find("compile_database_directory") != std::string::npos);
}

TEST_CASE("HTML impact directly-affected TUs emit leersatz when none") {
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_impact_report(make_minimal_impact_result());
    CHECK(report.find("No directly affected translation units.") != std::string::npos);
    CHECK(report.find("No heuristically affected translation units.") != std::string::npos);
}

TEST_CASE("HTML impact directly-affected TUs render row per direct TU") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = "src/main.cpp";
    result.affected_translation_units = {
        ImpactedTranslationUnit{
            reference("src/main.cpp", "build", "src/main.cpp|build"),
            ImpactKind::direct,
            {},
        },
        ImpactedTranslationUnit{
            reference("src/lib.cpp", "build", "src/lib.cpp|build"),
            ImpactKind::heuristic,
            {},
        },
    };
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    // Both sections render their respective TU rows.
    CHECK(report.find("src/main.cpp") != std::string::npos);
    CHECK(report.find("src/lib.cpp") != std::string::npos);
    // Heuristic section must not contain the direct row and vice versa.
    const auto direct_section_start = report.find("class=\"impact-direct-tus\"");
    const auto heuristic_section_start =
        report.find("class=\"impact-heuristic-tus\"", direct_section_start);
    REQUIRE(direct_section_start != std::string::npos);
    REQUIRE(heuristic_section_start != std::string::npos);
    const auto direct_section =
        report.substr(direct_section_start, heuristic_section_start - direct_section_start);
    CHECK(direct_section.find("src/main.cpp") != std::string::npos);
    CHECK(direct_section.find("src/lib.cpp") == std::string::npos);
}

TEST_CASE("HTML impact target sections emit leersaetze when no affected targets") {
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_impact_report(make_minimal_impact_result());
    CHECK(report.find("No directly affected targets.") != std::string::npos);
    CHECK(report.find("No heuristically affected targets.") != std::string::npos);
}

TEST_CASE("HTML impact target sections split direct and heuristic with badges") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = "src/main.cpp";
    result.affected_targets = {
        ImpactedTarget{TargetInfo{"app", "EXECUTABLE", "app::EXECUTABLE"},
                       TargetImpactClassification::direct},
        ImpactedTarget{TargetInfo{"core", "STATIC_LIBRARY", "core::STATIC_LIBRARY"},
                       TargetImpactClassification::heuristic},
    };
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    CHECK(report.find("badge-direct") != std::string::npos);
    CHECK(report.find("badge-heuristic") != std::string::npos);
    CHECK(report.find("app") != std::string::npos);
    CHECK(report.find("core") != std::string::npos);
}

// ---- Escape behaviour through render_text inside sections --------------

TEST_CASE("HTML report escapes user-derived strings in section text nodes") {
    AnalysisResult result = make_minimal_analysis_result();
    auto tu = ranked_tu("src/<tag>.cpp", "build & dir", "src/<tag>.cpp|build & dir");
    tu.rank = 1;
    result.translation_units = {tu};
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    CHECK(report.find("src/&lt;tag&gt;.cpp") != std::string::npos);
    CHECK(report.find("build &amp; dir") != std::string::npos);
    // Raw < > & must not survive into the rendered DOM as control bytes.
    CHECK(report.find("<tag>") == std::string::npos);
    CHECK(report.find("build & dir</td>") == std::string::npos);
}

TEST_CASE("HTML report normalizes whitespace in diagnostics before escaping") {
    AnalysisResult result = make_minimal_analysis_result();
    result.diagnostics = {
        Diagnostic{DiagnosticSeverity::warning, "line one\nline two\rline three\tend"},
    };
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    // \n and \r → " / ", \t → " ". After escape the slashes/spaces flow
    // through unchanged.
    CHECK(report.find("line one / line two / line three end") != std::string::npos);
}

TEST_CASE("HTML report neutralizes <script>-like input from the model") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = "<script>alert(1)</script>";
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    CHECK(report.find("&lt;script&gt;alert(1)&lt;/script&gt;") != std::string::npos);
    CHECK(report.find("<script>alert") == std::string::npos);
}

// ---- Tranche B: branch coverage for the static text mappings -----------

TEST_CASE("HTML impact inputs section emits each ChangedFileSource value") {
    const HtmlReportAdapter adapter;
    auto run_with = [&](ChangedFileSource source) {
        auto result = make_minimal_impact_result();
        result.inputs.changed_file = "src/main.cpp";
        result.inputs.changed_file_source = source;
        return adapter.write_impact_report(result);
    };
    CHECK(run_with(ChangedFileSource::compile_database_directory)
              .find("compile_database_directory") != std::string::npos);
    CHECK(run_with(ChangedFileSource::file_api_source_root).find("file_api_source_root") !=
          std::string::npos);
    CHECK(run_with(ChangedFileSource::cli_absolute).find("cli_absolute") != std::string::npos);
    CHECK(run_with(ChangedFileSource::unresolved_file_api_source_root)
              .find("unresolved_file_api_source_root") != std::string::npos);
}

TEST_CASE("HTML impact summary maps the uncertain classification when heuristic and empty") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = "include/missing.h";
    result.heuristic = true;
    REQUIRE(result.affected_translation_units.empty());
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    CHECK(report.find("<dt>classification</dt><dd>uncertain</dd>") != std::string::npos);
}

TEST_CASE("HTML target_label falls back to unique_key when display_name is empty") {
    AnalysisResult result = make_minimal_analysis_result();
    auto tu = ranked_tu("src/app.cpp", "build", "src/app.cpp|build");
    tu.rank = 1;
    // Empty display_name + non-empty unique_key → label uses unique_key.
    tu.targets = {TargetInfo{"", "EXECUTABLE", "anonymous::EXEC"}};
    result.translation_units = {tu};
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    CHECK(report.find("anonymous::EXEC") != std::string::npos);
}

TEST_CASE("HTML target_label falls back to type when display_name and unique_key are empty") {
    AnalysisResult result = make_minimal_analysis_result();
    auto tu = ranked_tu("src/app.cpp", "build", "src/app.cpp|build");
    tu.rank = 1;
    tu.targets = {TargetInfo{"", "STATIC_LIBRARY", ""}};
    result.translation_units = {tu};
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    // The label cell renders the type as the label text; the type column
    // (badge) renders STATIC_LIBRARY as well, so the type appears at least
    // twice.
    CHECK(report.find("STATIC_LIBRARY") != std::string::npos);
}

TEST_CASE("HTML target sort tie-breaks display_name ties by type then unique_key") {
    // The targets_cell renders display_name + a type badge, so the
    // unique_key tie-break is exercised during sort but not visible in
    // output. We assert the sort routine runs (covers the type-comparison
    // and unique_key-comparison branches) by feeding three entries that
    // share display_name and span both type and unique_key tie-breaks, and
    // by verifying the EXECUTABLE row precedes the STATIC_LIBRARY rows in
    // the rendered cell.
    AnalysisResult result = make_minimal_analysis_result();
    auto tu = ranked_tu("src/app.cpp", "build", "src/app.cpp|build");
    tu.rank = 1;
    tu.targets = {
        TargetInfo{"shared", "STATIC_LIBRARY", "shared::STATIC_b"},
        TargetInfo{"shared", "EXECUTABLE", "shared::EXEC"},
        TargetInfo{"shared", "STATIC_LIBRARY", "shared::STATIC_a"},
    };
    result.translation_units = {tu};
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    // The first type badge after the targets cell opens is EXECUTABLE
    // (display_name asc → all "shared", then type asc → E < S).
    const auto cell_open = report.find("<td><ul><li>shared");
    REQUIRE(cell_open != std::string::npos);
    const auto first_type_pos = report.find("<span class=\"badge\">", cell_open);
    REQUIRE(first_type_pos != std::string::npos);
    CHECK(report.find("EXECUTABLE", first_type_pos) ==
          first_type_pos + std::string("<span class=\"badge\">").size());
}

TEST_CASE("HTML hotspots section emits Showing N of M hotspot count when truncating") {
    AnalysisResult result = make_minimal_analysis_result();
    for (int i = 0; i < 4; ++i) {
        const auto header = "include/h" + std::to_string(i) + ".h";
        result.include_hotspots.push_back(IncludeHotspot{
            header, {reference("src/a.cpp", "build", "src/a.cpp|build"),
                     reference("src/b.cpp", "build", "src/b.cpp|build")},
            {}});
    }
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 2);
    CHECK(report.find("Showing 2 of 4 entries.") != std::string::npos);
}

TEST_CASE("HTML hotspot context cell shows the empty fallback for hotspots without TUs") {
    AnalysisResult result = make_minimal_analysis_result();
    // Construct a hotspot with an empty affected_translation_units list.
    // The analyzer never emits this in production, but the adapter must
    // still render the context cell defensively because the model permits
    // it.
    result.include_hotspots = {IncludeHotspot{"include/header.h", {}, {}}};
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    CHECK(report.find("<span class=\"empty\">no translation units</span>") !=
          std::string::npos);
}

TEST_CASE("HTML hotspot context cell renders target badge when assignment is present") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_assignments = {
        TargetAssignment{"src/a.cpp|build",
                         {TargetInfo{"app", "EXECUTABLE", "app::EXECUTABLE"}}},
    };
    result.include_hotspots = {
        IncludeHotspot{"include/header.h",
                       {reference("src/a.cpp", "build", "src/a.cpp|build")},
                       {}},
    };
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    // The hotspot's context cell carries the target's display_name in a
    // badge alongside the source path.
    CHECK(report.find("include/header.h") != std::string::npos);
    CHECK(report.find("<span class=\"badge\">app</span>") != std::string::npos);
}

TEST_CASE("HTML target metadata section reports loaded-but-empty discovery") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_metadata = TargetMetadataStatus::loaded;
    // No target_assignments, so the by_key map ends up empty.
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    CHECK(report.find("No targets discovered.") != std::string::npos);
}

// ---- Tranche D: platform path edge cases --------------------------------
//
// AP M5-1.4 Tranche D (plan ~793) hardens the adapter against host-divergent
// path strings the e2e fixtures cannot easily exercise without per-platform
// goldens. Adapter tests use synthetic strings so they stay stable on every
// host; the e2e_binary path-resolver behaviour for backslash and absolute
// drive prefixes is covered separately by analyze_escaping[_windows].html
// and impact_absolute[_windows].html.

TEST_CASE("HTML render_text passes Windows drive paths through verbatim") {
    // Forward-slash drive paths are common in CMake-generated compile_commands;
    // backslash drive paths can appear when the resolver is bypassed. Neither
    // form contains HTML specials, so render_text should leave them untouched
    // beyond the slash they already carry.
    CHECK(render_text("C:/project/src/main.cpp") == "C:/project/src/main.cpp");
    CHECK(render_text("D:/build/lib/core.cpp") == "D:/build/lib/core.cpp");
    CHECK(render_text("C:\\project\\src\\main.cpp") ==
          "C:\\project\\src\\main.cpp");
    // Drive root and bare drive letter must not gain extra characters either.
    CHECK(render_text("C:/") == "C:/");
    CHECK(render_text("C:\\") == "C:\\");
}

TEST_CASE("HTML render_attribute escapes Windows drive paths in attributes") {
    // Attribute values do not escape backslash, but they do escape double
    // quotes; a path like `C:\\"weird"\\file.cpp` must come out fully
    // attribute-safe.
    CHECK(render_attribute("C:/project/src/main.cpp") ==
          "C:/project/src/main.cpp");
    CHECK(render_attribute("C:\\project\\src\\main.cpp") ==
          "C:\\project\\src\\main.cpp");
    CHECK(render_attribute("C:/path/with \"quote\".cpp") ==
          "C:/path/with &quot;quote&quot;.cpp");
}

TEST_CASE("HTML render_text passes UNC paths through verbatim") {
    // UNC paths use leading backslashes which neither HTML escaping nor
    // whitespace normalisation should touch. The slash-style equivalent
    // (//server/share) is also part of the wild and must survive intact.
    CHECK(render_text("\\\\server\\share\\folder\\file.cpp") ==
          "\\\\server\\share\\folder\\file.cpp");
    CHECK(render_text("//server/share/folder/file.cpp") ==
          "//server/share/folder/file.cpp");
    // UNC with HTML specials in the share name still escapes the specials
    // while preserving the leading backslashes.
    CHECK(render_text("\\\\srv\\<share>\\file.cpp") ==
          "\\\\srv\\&lt;share&gt;\\file.cpp");
}

TEST_CASE("HTML render_text handles mixed-slash paths without collapsing them") {
    CHECK(render_text("C:/project\\src/main.cpp") ==
          "C:/project\\src/main.cpp");
    CHECK(render_text("\\\\server/share\\folder/file.cpp") ==
          "\\\\server/share\\folder/file.cpp");
}

// AP M5-1.7 Tranche C.2: synthetic UNC/Extended-Length-Pflichtfaelle fuer
// HTML-Render. Plan-M5-1-7.md "Windows-Bewertung" verlangt mindestens einen
// Adapter-, CLI- oder Golden-Pfadfall fuer UNC und Extended-Length; der
// synthetische Fall ist ohne SMB-Share oder Long-Path-Provisionierung
// pflichtbar und deckt Anzeige- plus Escape-Logik ab.
TEST_CASE("HTML render_attribute escapes UNC paths in attributes") {
    // Attribute-context counterpart to "render_text passes UNC paths
    // through verbatim". Backslash itself is not an attribute special, but
    // any HTML special inside the share or directory name must escape
    // while the leading double backslash survives.
    CHECK(render_attribute("\\\\server\\share\\folder\\file.cpp") ==
          "\\\\server\\share\\folder\\file.cpp");
    CHECK(render_attribute("\\\\srv\\<share>\\file.cpp") ==
          "\\\\srv\\&lt;share&gt;\\file.cpp");
    CHECK(render_attribute("\\\\srv\\share\\with \"quote\".cpp") ==
          "\\\\srv\\share\\with &quot;quote&quot;.cpp");
}

TEST_CASE("HTML render_text and render_attribute pass extended-length paths through verbatim") {
    // Extended-length paths use the \\?\ namespace prefix on Windows. The
    // HTML adapter must not collapse the leading backslashes, must not
    // strip the literal `?`, and must keep the rest of the path intact in
    // both text and attribute contexts.
    CHECK(render_text("\\\\?\\C:\\very\\long\\path\\file.cpp") ==
          "\\\\?\\C:\\very\\long\\path\\file.cpp");
    CHECK(render_attribute("\\\\?\\C:\\very\\long\\path\\file.cpp") ==
          "\\\\?\\C:\\very\\long\\path\\file.cpp");
    // \\?\UNC\ form addresses an extended-length UNC path; the same rules
    // apply, leaving every character including the embedded `?` intact.
    CHECK(render_text("\\\\?\\UNC\\server\\share\\folder\\file.cpp") ==
          "\\\\?\\UNC\\server\\share\\folder\\file.cpp");
    CHECK(render_attribute("\\\\?\\UNC\\server\\share\\folder\\file.cpp") ==
          "\\\\?\\UNC\\server\\share\\folder\\file.cpp");
}

// ---- Tranche D: ASCII escaping edge cases -------------------------------

TEST_CASE("normalize_html_whitespace turns every ASCII control byte 0x00..0x1F into a space") {
    // \t, \n, \r have dedicated rules (single space, " / ", " / ") that earlier
    // tests already pin down. The remaining control bytes must collapse to a
    // single space so they cannot leak as raw control characters into the
    // rendered document. Cover the edges (NUL, BEL, BS, FF, ESC, US) plus a
    // representative middle to keep the loop coverage explicit.
    for (int byte : {0x00, 0x01, 0x07, 0x08, 0x0B, 0x0C, 0x0E, 0x10,
                     0x11, 0x1A, 0x1B, 0x1F}) {
        std::string input;
        input.push_back('a');
        input.push_back(static_cast<char>(byte));
        input.push_back('b');
        const auto out = normalize_html_whitespace(input);
        CHECK_MESSAGE(out == "a b",
                      "byte " << byte << " produced " << out);
    }
}

TEST_CASE("normalize_html_whitespace keeps DEL (0x7F) as itself per the documented contract") {
    // The contract bound is `< 0x20`, so DEL (0x7F) and the rest of printable
    // ASCII must pass through unchanged. The HTML escape stage neither adds
    // nor removes meaning to those bytes either, so render_text must mirror.
    std::string input;
    input.push_back('a');
    input.push_back(static_cast<char>(0x7F));
    input.push_back('b');
    CHECK(normalize_html_whitespace(input) == input);
    CHECK(render_text(input) == input);
}

TEST_CASE("normalize_html_whitespace collapses sequential and mixed newlines deterministically") {
    // Three explicit newlines produce three " / " separators concatenated.
    // The renderer must not deduplicate them - sequential blank "lines" in
    // user-facing diagnostics would otherwise lose their structure. Each
    // " / " carries leading and trailing spaces, so adjacent separators show
    // a double space between the slashes.
    CHECK(normalize_html_whitespace("a\n\n\nb") == "a /  /  / b");
    // Mixed CRLF/LF/CR collapse into one separator each so the displayed
    // separator count matches the logical line count. \r\n is consumed as a
    // single \n; lone \r and \n each produce one separator.
    CHECK(normalize_html_whitespace("a\r\nb\nc\rd") == "a / b / c / d");
}

TEST_CASE("normalize_html_whitespace collapses tabs and control bytes adjacent to newlines without doubling separators") {
    // \t between two LFs must NOT introduce a phantom separator; each LF
    // becomes " / " and the tab becomes a single space, so the output is
    // " /  / " with one space between (tab) and not three separators.
    CHECK(normalize_html_whitespace("a\n\tb") == "a /  b");
    CHECK(normalize_html_whitespace("a\r\n\r\nb") == "a /  / b");
}

TEST_CASE("html_escape_text leaves UTF-8 high bytes and DEL unchanged") {
    // Bytes >= 0x7F are passed through so multi-byte UTF-8 characters
    // (Cyrillic, CJK, emoji surrogates) survive. Earlier tests already cover
    // the printable ASCII passthrough; this case nails the boundary.
    const std::string ru = "\xD1\x82\xD0\xB5\xD1\x81\xD1\x82";  // "тест"
    CHECK(html_escape_text(ru) == ru);
    std::string with_del = "a";
    with_del.push_back(static_cast<char>(0x7F));
    with_del.push_back('b');
    CHECK(html_escape_text(with_del) == with_del);
}

// ---- Tranche D: boundary regression tests -------------------------------

TEST_CASE("HTML target label falls back to unique_key when display_name is empty") {
    // target_label() prefers display_name, then unique_key, then type. An
    // empty display_name with a populated unique_key must therefore render
    // as the unique_key in the target metadata table.
    AnalysisResult result = make_minimal_analysis_result();
    result.target_metadata = TargetMetadataStatus::loaded;
    result.target_assignments = {
        TargetAssignment{"src/a.cpp|build",
                         {TargetInfo{"", "EXECUTABLE", "fallback-key::EXECUTABLE"}}},
    };
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    CHECK(report.find("fallback-key::EXECUTABLE") != std::string::npos);
}

TEST_CASE("HTML target label falls back to type when display_name and unique_key are empty") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_metadata = TargetMetadataStatus::loaded;
    result.target_assignments = {
        TargetAssignment{"src/a.cpp|build",
                         {TargetInfo{"", "OBJECT_LIBRARY", ""}}},
    };
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    // Both the badge (type column) and the table cell label render the type
    // string; we just assert the fallback wrote *something* visible.
    CHECK(report.find("OBJECT_LIBRARY") != std::string::npos);
}

TEST_CASE("HTML analyze report keeps targets sorted by display_name then type then unique_key") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_metadata = TargetMetadataStatus::loaded;
    // Same display_name "core" with different types must stay distinguishable
    // and sort by type as the tie-breaker (EXECUTABLE before STATIC_LIBRARY
    // alphabetically). unique_key is the final tie-breaker.
    result.target_assignments = {
        TargetAssignment{"src/a.cpp|build",
                         {TargetInfo{"core", "STATIC_LIBRARY", "core::STATIC"},
                          TargetInfo{"core", "EXECUTABLE", "core::EXEC"},
                          TargetInfo{"app", "EXECUTABLE", "app::EXEC"}}},
    };
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    const auto pos_app = report.find(">app<");
    const auto pos_core_first = report.find(">core<");
    REQUIRE(pos_app != std::string::npos);
    REQUIRE(pos_core_first != std::string::npos);
    // app sorts first (display_name app < core).
    CHECK(pos_app < pos_core_first);
    // Among the two "core" rows, EXECUTABLE comes before STATIC_LIBRARY in
    // the table because type is the secondary sort key.
    const auto pos_core_exec = report.find(">EXECUTABLE</td></tr>", pos_core_first);
    const auto pos_core_static = report.find(">STATIC_LIBRARY</td></tr>",
                                              pos_core_first);
    REQUIRE(pos_core_exec != std::string::npos);
    REQUIRE(pos_core_static != std::string::npos);
    CHECK(pos_core_exec < pos_core_static);
}

TEST_CASE("HTML impact report renders both direct and heuristic sections in fixed order") {
    // Same ImpactResult populating direct and heuristic translation units;
    // the document must keep the documented section order even when both
    // sides have content. This pins down the regression the C-review flagged
    // (no single golden carries direct + heuristic together).
    ImpactResult result = make_minimal_impact_result();
    result.inputs.changed_file = std::string{"include/common/config.h"};
    result.inputs.changed_file_source =
        ChangedFileSource::compile_database_directory;
    result.affected_translation_units = {
        ImpactedTranslationUnit{
            reference("src/direct.cpp", "build/direct", "src/direct.cpp|build/direct"),
            ImpactKind::direct, {}},
        ImpactedTranslationUnit{
            reference("src/heuristic.cpp", "build/heuristic",
                      "src/heuristic.cpp|build/heuristic"),
            ImpactKind::heuristic, {}},
    };
    result.affected_targets = {
        ImpactedTarget{TargetInfo{"app-direct", "EXECUTABLE", "app-direct::EXEC"},
                       TargetImpactClassification::direct},
        ImpactedTarget{TargetInfo{"app-heuristic", "EXECUTABLE",
                                   "app-heuristic::EXEC"},
                       TargetImpactClassification::heuristic},
    };
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    const auto pos_direct_tus = report.find("class=\"impact-direct-tus\"");
    const auto pos_heuristic_tus = report.find("class=\"impact-heuristic-tus\"");
    const auto pos_direct_tgts = report.find("class=\"impact-direct-targets\"");
    const auto pos_heuristic_tgts =
        report.find("class=\"impact-heuristic-targets\"");
    REQUIRE(pos_direct_tus != std::string::npos);
    REQUIRE(pos_heuristic_tus != std::string::npos);
    REQUIRE(pos_direct_tgts != std::string::npos);
    REQUIRE(pos_heuristic_tgts != std::string::npos);
    CHECK(pos_direct_tus < pos_heuristic_tus);
    CHECK(pos_heuristic_tus < pos_direct_tgts);
    CHECK(pos_direct_tgts < pos_heuristic_tgts);
    // Both kinds populate their sections, no leersaetze for these.
    CHECK(report.find("src/direct.cpp") != std::string::npos);
    CHECK(report.find("src/heuristic.cpp") != std::string::npos);
    CHECK(report.find("badge-direct") != std::string::npos);
    CHECK(report.find("badge-heuristic") != std::string::npos);
}

// ---- M6 AP 1.2 Tranche A.3: Target Graph / Target Hubs / Target Graph
// Reference sections. Status matrix per docs/planning/plan-M6-1-2.md "HTML-Adapter
// (Status-Matrix Reporttyp x Status)" plus disambiguation regressions.

namespace m6_html {

using xray::hexagon::model::TargetDependency;
using xray::hexagon::model::TargetDependencyKind;
using xray::hexagon::model::TargetDependencyResolution;
using xray::hexagon::model::TargetGraphStatus;

TargetDependency resolved_edge(std::string from_uk, std::string from_dn,
                               std::string to_uk, std::string to_dn) {
    return TargetDependency{std::move(from_uk),       std::move(from_dn),
                            std::move(to_uk),         std::move(to_dn),
                            TargetDependencyKind::direct,
                            TargetDependencyResolution::resolved};
}

TargetDependency external_edge(std::string from_uk, std::string from_dn,
                               std::string to_uk, std::string to_dn) {
    return TargetDependency{std::move(from_uk),       std::move(from_dn),
                            std::move(to_uk),         std::move(to_dn),
                            TargetDependencyKind::direct,
                            TargetDependencyResolution::external};
}

}  // namespace m6_html

TEST_CASE("HTML analyze v2: not_loaded keeps Target Graph and Target Hubs sections with empty paragraphs") {
    // Compile-DB-only run: target_graph_status defaults to not_loaded, both
    // sections must remain present with h2 and the documented empty paragraph.
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(make_minimal_analysis_result(), 3);
    CHECK(report.find("class=\"target-graph\"") != std::string::npos);
    CHECK(report.find("<h2>Target Graph</h2>") != std::string::npos);
    CHECK(report.find("Target graph not loaded.") != std::string::npos);
    CHECK(report.find("class=\"target-hubs\"") != std::string::npos);
    CHECK(report.find("<h2>Target Hubs</h2>") != std::string::npos);
    CHECK(report.find("Target hubs not available.") != std::string::npos);
    // No Target Graph table or Hub table when not_loaded.
    CHECK(report.find("<th scope=\"col\">From</th>") == std::string::npos);
    CHECK(report.find("<th scope=\"col\">Direction</th>") == std::string::npos);
}

TEST_CASE("HTML analyze v2: pflichtsection order extends to eight sections including Target Graph and Target Hubs") {
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(make_minimal_analysis_result(), 3);
    const auto pos_targets = report.find("class=\"target-metadata\"");
    const auto pos_graph = report.find("class=\"target-graph\"");
    const auto pos_hubs = report.find("class=\"target-hubs\"");
    const auto pos_diags = report.find("class=\"diagnostics\"");
    REQUIRE(pos_targets != std::string::npos);
    REQUIRE(pos_graph != std::string::npos);
    REQUIRE(pos_hubs != std::string::npos);
    REQUIRE(pos_diags != std::string::npos);
    CHECK(pos_targets < pos_graph);
    CHECK(pos_graph < pos_hubs);
    CHECK(pos_hubs < pos_diags);
}

TEST_CASE("HTML analyze v2: loaded status emits status badge, populated edge table and hubs with thresholds") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_graph_status = m6_html::TargetGraphStatus::loaded;
    result.target_graph.nodes = {
        TargetInfo{"app", "EXECUTABLE", "app::EXECUTABLE"},
        TargetInfo{"core", "STATIC_LIBRARY", "core::STATIC_LIBRARY"},
    };
    result.target_graph.edges = {
        m6_html::resolved_edge("app::EXECUTABLE", "app",
                               "core::STATIC_LIBRARY", "core"),
    };
    result.target_hubs_in.push_back({"core", "STATIC_LIBRARY", "core::STATIC_LIBRARY"});
    result.target_hubs_out.push_back({"app", "EXECUTABLE", "app::EXECUTABLE"});

    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);

    CHECK(report.find("Status: <span class=\"badge badge--loaded\">loaded</span>") !=
          std::string::npos);
    CHECK(report.find("<th scope=\"col\">From</th>") != std::string::npos);
    CHECK(report.find("<th scope=\"col\">Resolution</th>") != std::string::npos);
    CHECK(report.find("<th scope=\"col\">External target</th>") != std::string::npos);
    CHECK(report.find("<td>app</td><td>core</td><td>resolved</td><td></td>") !=
          std::string::npos);
    // No leersatz when edges are present.
    CHECK(report.find("No direct target dependencies.") == std::string::npos);
    // Target Hubs section: thresholds line + 2 hub rows.
    CHECK(report.find("Incoming threshold: 10. Outgoing threshold: 10.") !=
          std::string::npos);
    CHECK(report.find("<th scope=\"col\">Direction</th>") != std::string::npos);
    CHECK(report.find("<th scope=\"col\">Threshold</th>") != std::string::npos);
    CHECK(report.find("<th scope=\"col\">Targets</th>") != std::string::npos);
    CHECK(report.find("<td>Inbound</td><td>10</td><td>core</td>") != std::string::npos);
    CHECK(report.find("<td>Outbound</td><td>10</td><td>app</td>") != std::string::npos);
}

TEST_CASE("HTML analyze v2: partial status with external edge fills External target column") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_graph_status = m6_html::TargetGraphStatus::partial;
    result.target_graph.nodes = {
        TargetInfo{"app", "EXECUTABLE", "app::EXECUTABLE"},
    };
    result.target_graph.edges = {
        m6_html::external_edge("app::EXECUTABLE", "app",
                               "<external>::ghost", "ghost"),
    };

    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    CHECK(report.find("Status: <span class=\"badge badge--partial\">partial</span>") !=
          std::string::npos);
    // External row carries the raw_id in the External target column.
    CHECK(report.find("<td>app</td><td>ghost</td><td>external</td><td>ghost</td>") !=
          std::string::npos);
}

TEST_CASE("HTML analyze v2: loaded status with empty edge list emits No direct target dependencies leersatz") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_graph_status = m6_html::TargetGraphStatus::loaded;
    // Nodes loaded but no edges: leersatz, no table.
    result.target_graph.nodes = {
        TargetInfo{"only_one", "EXECUTABLE", "only_one::EXECUTABLE"},
    };
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    CHECK(report.find("Status: <span class=\"badge badge--loaded\">loaded</span>") !=
          std::string::npos);
    CHECK(report.find("No direct target dependencies.") != std::string::npos);
    CHECK(report.find("<th scope=\"col\">From</th>") == std::string::npos);
}

TEST_CASE("HTML analyze v2: empty inbound and outbound hub vectors render leersaetze in Targets cell") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_graph_status = m6_html::TargetGraphStatus::loaded;
    // Status loaded but no hubs computed → cell carries the leersatz text.
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    CHECK(report.find("<td>Inbound</td><td>10</td><td>No incoming hubs.</td>") !=
          std::string::npos);
    CHECK(report.find("<td>Outbound</td><td>10</td><td>No outgoing hubs.</td>") !=
          std::string::npos);
}

TEST_CASE("HTML analyze v2: hub list disambiguation suffix uses identity_key for collisions") {
    AnalysisResult result = make_minimal_analysis_result();
    result.target_graph_status = m6_html::TargetGraphStatus::loaded;
    // Two hubs share display_name="foo" but live under different unique_keys.
    // Adapter consumes the model in the AP-1.1 sort order
    // (unique_key, display_name, type); foo::EXECUTABLE precedes
    // foo::STATIC_LIBRARY, so we feed the test input in the same order to
    // mirror production data flow.
    result.target_hubs_in = {
        TargetInfo{"foo", "EXECUTABLE", "foo::EXECUTABLE"},
        TargetInfo{"foo", "STATIC_LIBRARY", "foo::STATIC_LIBRARY"},
    };
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    // Both entries get [key: ...] suffix; comma-separated inside the Targets
    // cell, byte-stable in the model's pre-sorted order.
    CHECK(report.find(
              "<td>Inbound</td><td>10</td>"
              "<td>foo [key: foo::EXECUTABLE], foo [key: foo::STATIC_LIBRARY]</td>") !=
          std::string::npos);
}

TEST_CASE("HTML analyze v2: external edge raw_id with HTML specials is escaped without leaking tags") {
    // Plan-Test 'evil<script>' regression: <external>::raw_id strings get HTML
    // escaped to &lt;external&gt;::raw_id; raw <script> tags must never reach
    // the rendered DOM.
    AnalysisResult result = make_minimal_analysis_result();
    result.target_graph_status = m6_html::TargetGraphStatus::partial;
    result.target_graph.edges = {
        m6_html::external_edge("app::EXECUTABLE", "app",
                               "<external>::evil<script>", "evil<script>"),
    };
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    // The raw_id appears HTML-escaped both in the To and the External target
    // cells.
    CHECK(report.find("evil&lt;script&gt;") != std::string::npos);
    // No raw <script> tags survive into the output.
    CHECK(report.find("<script>") == std::string::npos);
}

TEST_CASE("HTML analyze v2: mixed disambiguation across to-column escapes <external>::* keys") {
    // Plan mischfall: edge to internal "foo" (foo::STATIC_LIBRARY) and edge
    // to external raw_id "foo" (<external>::foo) both appear in the same To
    // column. Both rows get [key: ...] suffix; HTML adapter must HTML-escape
    // the < and > in the external suffix to &lt;external&gt;::foo.
    AnalysisResult result = make_minimal_analysis_result();
    result.target_graph_status = m6_html::TargetGraphStatus::loaded;
    result.target_graph.nodes = {
        TargetInfo{"app", "EXECUTABLE", "app::EXECUTABLE"},
        TargetInfo{"foo", "STATIC_LIBRARY", "foo::STATIC_LIBRARY"},
    };
    result.target_graph.edges = {
        m6_html::resolved_edge("app::EXECUTABLE", "app",
                               "foo::STATIC_LIBRARY", "foo"),
        m6_html::external_edge("app::EXECUTABLE", "app",
                               "<external>::foo", "foo"),
    };
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);
    // Internal foo row: To cell shows "foo [key: foo::STATIC_LIBRARY]".
    CHECK(report.find("<td>foo [key: foo::STATIC_LIBRARY]</td>") != std::string::npos);
    // External foo row: To cell shows "foo [key: &lt;external&gt;::foo]".
    CHECK(report.find("<td>foo [key: &lt;external&gt;::foo]</td>") != std::string::npos);
    // Suffix angle brackets are escaped, never raw.
    CHECK(report.find("[key: <external>::") == std::string::npos);
}

TEST_CASE("HTML impact v2: not_loaded keeps Target Graph Reference section with empty paragraph and no Hub section") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = std::string{"include/x.h"};
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    CHECK(report.find("class=\"target-graph-reference\"") != std::string::npos);
    CHECK(report.find("<h2>Target Graph Reference</h2>") != std::string::npos);
    CHECK(report.find("Target graph not loaded.") != std::string::npos);
    // Impact never carries a hub section.
    CHECK(report.find("class=\"target-hubs\"") == std::string::npos);
    CHECK(report.find("<h2>Target Hubs</h2>") == std::string::npos);
}

TEST_CASE("HTML impact v2: pflichtsection order extends with Target Graph Reference between targets and diagnostics") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = std::string{"include/x.h"};
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    const auto pos_heuristic_tgts =
        report.find("class=\"impact-heuristic-targets\"");
    const auto pos_graph_ref = report.find("class=\"target-graph-reference\"");
    const auto pos_diags = report.find("class=\"diagnostics\"");
    REQUIRE(pos_heuristic_tgts != std::string::npos);
    REQUIRE(pos_graph_ref != std::string::npos);
    REQUIRE(pos_diags != std::string::npos);
    CHECK(pos_heuristic_tgts < pos_graph_ref);
    CHECK(pos_graph_ref < pos_diags);
}

TEST_CASE("HTML impact v2: loaded status renders Target Graph Reference table with edges") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = std::string{"include/x.h"};
    result.target_graph_status = m6_html::TargetGraphStatus::loaded;
    result.target_graph.nodes = {
        TargetInfo{"app", "EXECUTABLE", "app::EXECUTABLE"},
        TargetInfo{"core", "STATIC_LIBRARY", "core::STATIC_LIBRARY"},
    };
    result.target_graph.edges = {
        m6_html::resolved_edge("app::EXECUTABLE", "app",
                               "core::STATIC_LIBRARY", "core"),
    };
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    CHECK(report.find("Status: <span class=\"badge badge--loaded\">loaded</span>") !=
          std::string::npos);
    CHECK(report.find("<th scope=\"col\">From</th>") != std::string::npos);
    CHECK(report.find("<td>app</td><td>core</td><td>resolved</td><td></td>") !=
          std::string::npos);
}

TEST_CASE("HTML impact v2: partial status with external edge fills External target column") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = std::string{"include/x.h"};
    result.target_graph_status = m6_html::TargetGraphStatus::partial;
    result.target_graph.edges = {
        m6_html::external_edge("app::EXECUTABLE", "app",
                               "<external>::boost", "boost"),
    };
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    CHECK(report.find("Status: <span class=\"badge badge--partial\">partial</span>") !=
          std::string::npos);
    CHECK(report.find("<td>app</td><td>boost</td><td>external</td><td>boost</td>") !=
          std::string::npos);
}

// ---- AP M6-1.3 A.4: Prioritised Affected Targets section --------------

TEST_CASE("HTML impact v3: not_loaded keeps Prioritised Affected Targets section with depth header and skipped paragraph") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = std::string{"include/x.h"};
    result.impact_target_depth_requested = 2;
    result.impact_target_depth_effective = 0;  // service forces 0 on not_loaded
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    CHECK(report.find("class=\"prioritised-affected-targets\"") != std::string::npos);
    CHECK(report.find("<h2>Prioritised Affected Targets</h2>") != std::string::npos);
    CHECK(report.find("Requested depth: 2. Effective depth: 0 (no graph).") !=
          std::string::npos);
    CHECK(report.find("Target graph not loaded; prioritisation skipped.") !=
          std::string::npos);
    // No table rendered when not_loaded.
    CHECK(report.find("<th scope=\"col\">Priority class</th>") == std::string::npos);
}

TEST_CASE("HTML impact v3: loaded with empty prioritised list shows the leersatz, no table") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = std::string{"include/x.h"};
    result.target_graph_status = xray::hexagon::model::TargetGraphStatus::loaded;
    result.impact_target_depth_requested = 2;
    result.impact_target_depth_effective = 1;
    // prioritized_affected_targets stays empty.
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    CHECK(report.find("Requested depth: 2. Effective depth: 1.") !=
          std::string::npos);
    CHECK(report.find("No prioritised targets.") != std::string::npos);
    CHECK(report.find("<th scope=\"col\">Priority class</th>") == std::string::npos);
}

TEST_CASE("HTML impact v3: loaded with populated prioritised list emits the documented six-column table") {
    using xray::hexagon::model::PrioritizedImpactedTarget;
    using xray::hexagon::model::TargetEvidenceStrength;
    using xray::hexagon::model::TargetGraphStatus;
    using xray::hexagon::model::TargetPriorityClass;
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = std::string{"src/hub.cpp"};
    result.target_graph_status = TargetGraphStatus::loaded;
    result.impact_target_depth_requested = 2;
    result.impact_target_depth_effective = 1;
    result.prioritized_affected_targets = {
        PrioritizedImpactedTarget{
            TargetInfo{"hub", "STATIC_LIBRARY", "hub::STATIC_LIBRARY"},
            TargetPriorityClass::direct, 0, TargetEvidenceStrength::direct},
        PrioritizedImpactedTarget{
            TargetInfo{"app", "EXECUTABLE", "app::EXECUTABLE"},
            TargetPriorityClass::direct_dependent, 1,
            TargetEvidenceStrength::heuristic},
    };
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    CHECK(report.find("<th scope=\"col\">Display name</th>") != std::string::npos);
    CHECK(report.find("<th scope=\"col\">Type</th>") != std::string::npos);
    CHECK(report.find("<th scope=\"col\">Priority class</th>") != std::string::npos);
    CHECK(report.find("<th scope=\"col\">Graph distance</th>") != std::string::npos);
    CHECK(report.find("<th scope=\"col\">Evidence strength</th>") != std::string::npos);
    CHECK(report.find("<th scope=\"col\">Unique key</th>") != std::string::npos);
    CHECK(report.find("<td>hub</td><td>STATIC_LIBRARY</td><td>direct</td>"
                      "<td>0</td><td>direct</td><td>hub::STATIC_LIBRARY</td>") !=
          std::string::npos);
    CHECK(report.find("<td>app</td><td>EXECUTABLE</td><td>direct_dependent</td>"
                      "<td>1</td><td>heuristic</td><td>app::EXECUTABLE</td>") !=
          std::string::npos);
}

TEST_CASE("HTML impact v3: section ordering pins Prioritised Affected Targets between Target Graph Reference and Diagnostics") {
    auto result = make_minimal_impact_result();
    result.inputs.changed_file = std::string{"include/x.h"};
    const HtmlReportAdapter adapter;
    const auto report = adapter.write_impact_report(result);
    const auto pos_graph_ref = report.find("class=\"target-graph-reference\"");
    const auto pos_prioritised = report.find("class=\"prioritised-affected-targets\"");
    const auto pos_diags = report.find("class=\"diagnostics\"");
    REQUIRE(pos_graph_ref != std::string::npos);
    REQUIRE(pos_prioritised != std::string::npos);
    REQUIRE(pos_diags != std::string::npos);
    CHECK(pos_graph_ref < pos_prioritised);
    CHECK(pos_prioritised < pos_diags);
}

TEST_CASE("HTML analyze v4: include filter line carries scope, depth and excluded counts (AP M6-1.4 A.5)") {
    auto result = make_minimal_analysis_result();
    result.include_hotspot_excluded_unknown_count = 5;
    result.include_hotspot_excluded_mixed_count = 3;

    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);

    CHECK(report.find("class=\"include-filter\"") != std::string::npos);
    CHECK(report.find("badge--all") != std::string::npos);
    CHECK(report.find("Excluded: 5 unknown, 3 mixed.") != std::string::npos);
}

TEST_CASE("HTML analyze v4: budget-reached note appears when include_node_budget_reached is true (AP M6-1.4 A.5)") {
    auto result = make_minimal_analysis_result();
    result.include_node_budget_reached = true;
    result.include_node_budget_effective = 8420;

    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);

    CHECK(report.find("class=\"include-budget-note\"") != std::string::npos);
    CHECK(report.find("include analysis stopped at 8420 nodes (budget reached)") !=
          std::string::npos);
}

TEST_CASE("HTML analyze v4: hotspot rows carry origin and depth_kind columns with badge classes (AP M6-1.4 A.5)") {
    auto result = make_minimal_analysis_result();
    auto external_mixed = IncludeHotspot{
        "include/external.h",
        {xray::hexagon::model::TranslationUnitReference{"src/a.cpp", "build", "src/a.cpp",
                                                         "src/a.cpp|build"},
         xray::hexagon::model::TranslationUnitReference{"src/b.cpp", "build", "src/b.cpp",
                                                         "src/b.cpp|build"}},
        {}};
    external_mixed.origin = xray::hexagon::model::IncludeOrigin::external;
    external_mixed.depth_kind = xray::hexagon::model::IncludeDepthKind::mixed;
    result.include_hotspots = {external_mixed};

    const HtmlReportAdapter adapter;
    const auto report = adapter.write_analysis_report(result, 3);

    CHECK(report.find("<th scope=\"col\">Origin</th>") != std::string::npos);
    CHECK(report.find("<th scope=\"col\">Depth</th>") != std::string::npos);
    CHECK(report.find("class=\"badge badge--external\">external") != std::string::npos);
    CHECK(report.find("class=\"badge badge--mixed\">mixed") != std::string::npos);
}
