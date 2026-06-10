#pragma once

// ADR-010 Attach Engine
// Orchestrates the attach flow: createGroup -> setValue -> attachTask.
// NOT in Driver, NOT in Monitor. Independent component.

#include <memory>

#include "resource_manager/core/config_domain.h"
#include "resource_manager/state/runtime_state.h"
#include "resource_manager/driver/icgroup_driver.h"
#include "resource_manager/error/error.h"

namespace resource_manager {

class AttachEngine {
public:
    explicit AttachEngine(std::unique_ptr<ICgroupDriver> driver);
    ~AttachEngine() = default;

    std::optional<Error> attach(const Group& group, const RuntimeState& state);
    std::optional<Error> detach(const RuntimeState& state);
    std::optional<Error> reattach(const Group& group, RuntimeState& state);

private:
    std::unique_ptr<ICgroupDriver> driver_;
};

} // namespace resource_manager
