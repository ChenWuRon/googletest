#include "resource_manager/attach/attach_engine.h"

namespace resource_manager {

AttachEngine::AttachEngine(std::unique_ptr<ICgroupDriver> driver)
    : driver_(std::move(driver)) {}

std::optional<Error> AttachEngine::attach(const ConfigNode& group, const RuntimeState& state) {
    // Phase 3 implementation.
    return std::nullopt;
}

std::optional<Error> AttachEngine::detach(const RuntimeState& state) {
    // Phase 3 implementation.
    return std::nullopt;
}

std::optional<Error> AttachEngine::reattach(const ConfigNode& group, RuntimeState& state) {
    // Phase 3 implementation.
    return std::nullopt;
}

} // namespace resource_manager
