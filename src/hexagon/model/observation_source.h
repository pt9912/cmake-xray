#pragma once

namespace xray::hexagon::model {

enum class ObservationSource {
    exact,
    derived,
};

enum class TargetMetadataStatus {
    not_loaded,
    loaded,
    partial,
};

}  // namespace xray::hexagon::model
