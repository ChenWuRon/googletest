#pragma once

#include <memory>

#include "resource_manager/core/config_domain.h"
#include "resource_manager/state/runtime_state.h"
#include "resource_manager/driver/icgroup_driver.h"
#include "resource_manager/attach/attach_policy.h"
#include "resource_manager/error/error.h"

namespace resource_manager {

class AttachEngine {
public:
    AttachEngine(
        std::unique_ptr<ICgroupDriver> driver,
        std::unique_ptr<AttachPolicy> policy = std::make_unique<AttachPolicy>(AttachPolicyType::AttachAll)
    );

    std::optional<Error> attach(const ConfigNode& group, RuntimeState& state);
    std::optional<Error> detach(RuntimeState& state);
    std::optional<Error> reattach(const ConfigNode& group, RuntimeState& state);

private:
    std::optional<Error> executeAttach(const std::string& cgroupPath, const ConfigNode& group, RuntimeState& state);

    std::unique_ptr<ICgroupDriver> driver_;
    std::unique_ptr<AttachPolicy> policy_;
};

} // namespace resource_manager
