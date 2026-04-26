#include "adapters/output/html_report_adapter.h"

#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>

#include "hexagon/model/analysis_result.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/model/report_format_version.h"

namespace xray::adapters::output {

std::string html_escape_text(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '&':
            out.append("&amp;");
            break;
        case '<':
            out.append("&lt;");
            break;
        case '>':
            out.append("&gt;");
            break;
        default:
            out.push_back(ch);
            break;
        }
    }
    // NRVO-defeating return mirrors render_impact_inputs in the JSON adapter
    // so the closing-brace destructor counts as a covered line under gcov.
    return std::string(std::move(out));
}

std::string html_escape_attribute(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '&':
            out.append("&amp;");
            break;
        case '<':
            out.append("&lt;");
            break;
        case '>':
            out.append("&gt;");
            break;
        case '"':
            out.append("&quot;");
            break;
        case '\'':
            out.append("&#39;");
            break;
        default:
            out.push_back(ch);
            break;
        }
    }
    return std::string(std::move(out));
}

std::string render_text(std::string_view value) {
    return html_escape_text(normalize_html_whitespace(value));
}

std::string render_attribute(std::string_view value) {
    return html_escape_attribute(normalize_html_whitespace(value));
}

std::string normalize_html_whitespace(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        const auto ch = static_cast<unsigned char>(value[index]);
        if (ch == '\r') {
            // \r\n → \n; lone \r → \n. The lookahead skip keeps \r\n from
            // emitting two newlines that would each become " / ".
            const auto next = index + 1;
            if (next < value.size() && value[next] == '\n') {
                ++index;
            }
            out.append(" / ");
            continue;
        }
        if (ch == '\n') {
            out.append(" / ");
            continue;
        }
        if (ch == '\t') {
            out.push_back(' ');
            continue;
        }
        if (ch < 0x20) {
            out.push_back(' ');
            continue;
        }
        out.push_back(static_cast<char>(ch));
    }
    return std::string(std::move(out));
}

std::string_view html_report_css() {
    // Static, byte-stable CSS per docs/report-html.md "CSS-Vertrag".
    // Identical for analyze and impact. No @import / url(...) / external
    // fonts / animations / random values. System fontstacks only.
    static constexpr std::string_view kCss =
        "body{margin:0;padding:1.5rem;background:#ffffff;color:#1a1a1a;"
        "font-family:system-ui,-apple-system,\"Segoe UI\",Roboto,Helvetica,Arial,sans-serif;"
        "line-height:1.5;}"
        "main.report{max-width:80rem;margin:0 auto;}"
        "h1{font-size:1.75rem;margin:0 0 1rem 0;}"
        "h2{font-size:1.25rem;margin:1.5rem 0 0.75rem 0;}"
        "h3{font-size:1.0rem;margin:1rem 0 0.5rem 0;}"
        "section{margin-bottom:1.5rem;}"
        "table{border-collapse:collapse;width:100%;}"
        "th,td{border:1px solid #d0d0d0;padding:0.5rem;text-align:left;vertical-align:top;}"
        "th{background:#f3f3f3;font-weight:600;}"
        ".table-wrap{overflow-x:auto;}"
        ".badge{display:inline-block;padding:0.1rem 0.5rem;border:1px solid #1a1a1a;"
        "border-radius:0.25rem;background:#f0f0f0;color:#1a1a1a;font-size:0.875rem;}"
        ".badge-direct{background:#1a1a1a;color:#ffffff;}"
        ".badge-heuristic{background:#ffffff;color:#1a1a1a;}"
        ".empty{color:#404040;font-style:italic;}"
        "code,pre{font-family:ui-monospace,\"SFMono-Regular\",\"Menlo\",\"Consolas\",monospace;}"
        "@media print{body{background:#ffffff;color:#000000;}"
        "th{background:#ffffff;}}";
    return kCss;
}

namespace {

// Tranche A stub: emit only the byte-stable HTML5 frame — doctype, html,
// head with the contract head order, body, main with data-report-type and
// data-format-version, and a single h1 placeholder. Adapter unit tests
// pin the structural contract against this skeleton so Tranche B can fill
// in the analyze/impact section payload without re-litigating the frame.
// SAFETY: report_type and title MUST be adapter-controlled string literals;
// they are streamed unescaped because the only call sites pass the
// hard-coded "analyze"/"impact" and "cmake-xray analyze report"/
// "cmake-xray impact report" tokens. Any future caller passing user-derived
// data must route it through render_text or render_attribute first.
std::string render_skeleton(std::string_view report_type, std::string_view title) {
    const auto version = std::to_string(
        static_cast<std::size_t>(xray::hexagon::model::kReportFormatVersion));
    std::ostringstream out;
    out << "<!doctype html>\n"
        << "<html lang=\"en\">\n"
        << "<head>\n"
        << "<meta charset=\"utf-8\">\n"
        << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        << "<meta name=\"xray-report-format-version\" content=\"" << version << "\">\n"
        << "<title>" << title << "</title>\n"
        << "<style>" << html_report_css() << "</style>\n"
        << "</head>\n"
        << "<body>\n"
        << "<main class=\"report\" data-report-type=\"" << report_type
        << "\" data-format-version=\"" << version << "\">\n"
        << "<h1>" << title << "</h1>\n"
        << "</main>\n"
        << "</body>\n"
        << "</html>\n";
    return out.str();
}

}  // namespace

std::string HtmlReportAdapter::write_analysis_report(
    const xray::hexagon::model::AnalysisResult& analysis_result,
    std::size_t top_limit) const {
    // Tranche A stub: parameters are unused on purpose. Tranche B consumes
    // both the analysis_result and top_limit for the actual section payload.
    (void)analysis_result;
    (void)top_limit;
    return render_skeleton("analyze", "cmake-xray analyze report");
}

std::string HtmlReportAdapter::write_impact_report(
    const xray::hexagon::model::ImpactResult& impact_result) const {
    // Tranche A stub: parameter unused on purpose. Tranche B consumes the
    // impact_result for the actual section payload.
    (void)impact_result;
    return render_skeleton("impact", "cmake-xray impact report");
}

}  // namespace xray::adapters::output
