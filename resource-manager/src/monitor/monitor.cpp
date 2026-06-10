#include "resource_manager/monitor/monitor.h"

namespace resource_manager {

Monitor::Monitor(
    std::unique_ptr<IProcessDiscovery> discovery,
    std::unique_ptr<AttachEngine> attach_engine
)
    : discovery_(std::move(discovery))
    , attach_engine_(std::move(attach_engine))
    , running_(false) {}

void Monitor::start() {
    // Phase 3 implementation.
    running_ = true;
}

void Monitor::stop() {
    // Phase 3 implementation.
    running_ = false;
}

void Monitor::check() {
    // Phase 3 implementation.
}

} // namespace resource_manager
