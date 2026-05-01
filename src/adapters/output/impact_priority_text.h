#pragma once

#include <string>

#include "hexagon/model/impact_result.h"

namespace xray::adapters::output {

// AP M6-1.3 A.3: presentation-layer string mappings for the AP-1.3
// priority enums. The strings ("direct", "direct_dependent",
// "transitive_dependent" / "direct", "heuristic", "uncertain") are
// part of the JSON and DOT contracts; centralising them here keeps the
// JSON and DOT adapters byte-stable to each other and gives tests a
// single anchor for the documented mappings.

inline std::string priority_class_text_v3(
    xray::hexagon::model::TargetPriorityClass priority_class) {
    using xray::hexagon::model::TargetPriorityClass;
    if (priority_class == TargetPriorityClass::direct) return "direct";
    if (priority_class == TargetPriorityClass::direct_dependent) return "direct_dependent";
    return "transitive_dependent";
}

inline std::string evidence_strength_text_v3(
    xray::hexagon::model::TargetEvidenceStrength evidence_strength) {
    using xray::hexagon::model::TargetEvidenceStrength;
    if (evidence_strength == TargetEvidenceStrength::direct) return "direct";
    if (evidence_strength == TargetEvidenceStrength::heuristic) return "heuristic";
    return "uncertain";
}

}  // namespace xray::adapters::output
