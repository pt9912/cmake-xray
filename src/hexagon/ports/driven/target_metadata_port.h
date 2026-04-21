#pragma once

namespace xray::hexagon::ports::driven {

class TargetMetadataPort {
public:
    virtual ~TargetMetadataPort() = default;

    virtual bool has_target_metadata() const = 0;
};

}  // namespace xray::hexagon::ports::driven
