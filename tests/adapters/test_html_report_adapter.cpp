#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <string_view>

#include "adapters/output/html_report_adapter.h"
#include "hexagon/model/analysis_result.h"
#include "hexagon/model/application_info.h"
#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/model/report_format_version.h"

namespace {

using xray::adapters::output::html_escape_attribute;
using xray::adapters::output::html_escape_text;
using xray::adapters::output::html_report_css;
using xray::adapters::output::HtmlReportAdapter;
using xray::adapters::output::normalize_html_whitespace;
using xray::adapters::output::render_attribute;
using xray::adapters::output::render_text;
using xray::hexagon::model::AnalysisResult;
using xray::hexagon::model::CompileDatabaseError;
using xray::hexagon::model::CompileDatabaseResult;
using xray::hexagon::model::ImpactResult;
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
    // Per docs/report-html.md, text nodes do not need to escape " or '.
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
    // No external resources or animations per docs/report-html.md.
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

TEST_CASE("HTML head order follows docs/report-html.md") {
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
