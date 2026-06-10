#pragma once

#include <string>
#include <optional>

#include "resource_manager/driver/icgroup_driver.h"
#include "resource_manager/driver/control_file_type.h"
#include "resource_manager/core/config_domain.h"

namespace resource_manager {

class ResourceApplier {
public:
    ResourceApplier(ICgroupDriver& driver, ControllerRegistry& registry);

    std::optional<Error> apply(const ConfigDomain& domain);

private:
    std::optional<Error> applyGroup(const ConfigNode& groupNode);
    std::optional<Error> applyItem(const std::string& cgroupPath, const ConfigNode& itemNode);

    ICgroupDriver& driver_;
    ControllerRegistry& registry_;
};

} // namespace resource_manager
