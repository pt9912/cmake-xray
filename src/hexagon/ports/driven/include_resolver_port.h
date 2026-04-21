#pragma once

namespace xray::hexagon::ports::driven {

class IncludeResolverPort {
public:
    virtual ~IncludeResolverPort() = default;

    virtual bool can_resolve_includes() const = 0;
};

}  // namespace xray::hexagon::ports::driven
