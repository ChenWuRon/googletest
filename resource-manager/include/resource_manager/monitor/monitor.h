#pragma once

// ADR-008 Recovery
// Monitor detects PID changes and triggers Recovery flow.
// Collaborates with Discovery and AttachEngine.

#include <memory>
#include <functional>

#include "resource_manager/state/runtime_state.h"
#include "resource_manager/discovery/iprocess_discovery.h"
#include "resource_manager/attach/attach_engine.h"

namespace resource_manager {

class Monitor {
public:
    Monitor(
        std::unique_ptr<IProcessDiscovery> discovery,
        std::unique_ptr<AttachEngine> attach_engine
    );
    ~Monitor() = default;

    void start();
    void stop();
    void check(); // single check cycle

private:
    std::unique_ptr<IProcessDiscovery> discovery_;
    std::unique_ptr<AttachEngine> attach_engine_;
    bool running_;
};

} // namespace resource_manager
