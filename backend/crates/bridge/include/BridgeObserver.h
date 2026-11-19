#pragma once

#include <cstdint>
#include <string>

namespace audioticon::bridge {

struct RenderStatusDto;

class BridgeObserver {
public:
    virtual ~BridgeObserver() = default;
    virtual void on_progress(RenderStatusDto status) const;
};

} // namespace audioticon::bridge
