#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "application_info.h"
#include "compile_database_result.h"
#include "diagnostic.h"
#include "observation_source.h"
#include "report_inputs.h"
#include "target_graph.h"
#include "target_info.h"
#include "translation_unit.h"

namespace xray::hexagon::model {

enum class ImpactKind {
    direct,
    heuristic,
};

struct ImpactedTranslationUnit {
    TranslationUnitReference reference;
    ImpactKind kind{ImpactKind::direct};
    std::vector<TargetInfo> targets;
};

enum class TargetImpactClassification {
    direct,
    heuristic,
};

struct ImpactedTarget {
    TargetInfo target;
    TargetImpactClassification classification{TargetImpactClassification::direct};
};

// AP M6-1.3 A.1: priority classes for the reverse-target-graph traversal
// over `impact`. The enum order is intentionally aligned with the
// documented sort rank (direct < direct_dependent < transitive_dependent),
// but sort order itself MUST NOT rely on the enum's ordinal value -- it
// must consult an explicit rank table. See plan-M6-1-3.md "Modellvertrag".
enum class TargetPriorityClass {
    direct,                // graph_distance == 0
    direct_dependent,      // graph_distance == 1
    transitive_dependent,  // graph_distance >= 2
};

// AP M6-1.3 A.1: evidence axis carried through reverse-BFS hops. Sort
// order via explicit rank table (direct < heuristic < uncertain), see
// plan-M6-1-3.md "Modellvertrag".
enum class TargetEvidenceStrength {
    direct,
    heuristic,
    uncertain,
};

struct PrioritizedImpactedTarget {
    TargetInfo target;
    TargetPriorityClass priority_class{TargetPriorityClass::direct};
    std::size_t graph_distance{0};
    TargetEvidenceStrength evidence_strength{TargetEvidenceStrength::direct};
};

// AP M6-1.3 A.1: service-side rejection of an AnalyzeImpactRequest. When
// set on an ImpactResult the request was rejected before the result was
// built; callers (CLI, adapters, tests) must treat the result as
// "rejected, no analysis data" and forward only the error code/message.
// Empty (default-constructed) when the request is valid.
struct ServiceValidationError {
    std::string code;     // e.g. "impact_target_depth_out_of_range"
    std::string message;  // human-readable, contract-pinned phrase
};

struct ImpactResult {
    ApplicationInfo application;
    CompileDatabaseResult compile_database;
    std::string compile_database_path;
    std::string changed_file;
    std::string changed_file_key;
    ReportInputs inputs;
    bool heuristic{false};
    std::vector<ImpactedTranslationUnit> affected_translation_units;
    std::vector<Diagnostic> diagnostics;
    ObservationSource observation_source{ObservationSource::exact};
    TargetMetadataStatus target_metadata{TargetMetadataStatus::not_loaded};
    std::vector<TargetAssignment> target_assignments;
    std::vector<ImpactedTarget> affected_targets;
    TargetGraph target_graph;
    TargetGraphStatus target_graph_status{TargetGraphStatus::not_loaded};
    // AP M6-1.3 A.1 fields. The reverse-BFS over target_graph.edges
    // produces prioritized_affected_targets; impact_target_depth_requested
    // mirrors the (normalised) AnalyzeImpactRequest depth, and
    // impact_target_depth_effective is the actually-reached maximum
    // distance after BFS budget/cycle/short-circuit handling. Adapters
    // never emit `null` for the two depth fields; the analyzer always
    // sets them on every successfully built ImpactResult.
    std::vector<PrioritizedImpactedTarget> prioritized_affected_targets;
    std::size_t impact_target_depth_requested{2};
    std::size_t impact_target_depth_effective{0};
    // Optional service-validation error per AP M6-1.3 A.1. When set, the
    // remaining fields are default-initialized and must not be consumed
    // as analysis data.
    std::optional<ServiceValidationError> service_validation_error;
};

}  // namespace xray::hexagon::model
