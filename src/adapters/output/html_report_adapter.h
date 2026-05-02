#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "hexagon/ports/driven/report_writer_port.h"

namespace xray::adapters::output {

// AP M5-1.4 Tranche A: contract-fixed helpers exposed for unit tests so the
// HTML escape and whitespace-normalization rules can be pinned without
// instantiating the adapter or rendering full reports. Tranche B fills in
// the actual analyze/impact rendering.

// HTML-escape a text-node value per spec/report-html.md "Escaping": minimum
// escapes for &, <, >. Caller-side helper; Tranche B composes whitespace
// normalization and escaping via render_text below before emitting business
// values.
std::string html_escape_text(std::string_view value);

// HTML-escape an attribute value per spec/report-html.md "Escaping":
// minimum escapes for &, <, >, ", '. Caller-side helper; Tranche B composes
// whitespace normalization and escaping via render_attribute below.
std::string html_escape_attribute(std::string_view value);

// Normalize whitespace inside a business value before HTML escaping per
// spec/report-html.md "Escaping": \r\n and \r → " / ", each \n → " / ",
// each \t → " ", every other ASCII control byte below 0x20 → " ".
std::string normalize_html_whitespace(std::string_view value);

// Compose normalize_html_whitespace + html_escape_text for a business value
// emitted as a text node. Tranche B uses this for every nutzergelieferten
// Wert; the contract requires whitespace normalization BEFORE escaping so
// the inserted " / " and " " separators flow through escaping unchanged.
std::string render_text(std::string_view value);

// Compose normalize_html_whitespace + html_escape_attribute for a business
// value emitted as an attribute value. Same composition order as
// render_text.
std::string render_attribute(std::string_view value);

// The static, byte-stable CSS string emitted in <style> for every HTML
// report, per spec/report-html.md "CSS-Vertrag". Identical for analyze and
// impact, free of @import / url(...) / external resources / animations.
std::string_view html_report_css();

class HtmlReportAdapter final : public xray::hexagon::ports::driven::ReportWriterPort {
public:
    std::string write_analysis_report(
        const xray::hexagon::model::AnalysisResult& analysis_result,
        std::size_t top_limit) const override;
    std::string write_impact_report(
        const xray::hexagon::model::ImpactResult& impact_result) const override;
};

}  // namespace xray::adapters::output
