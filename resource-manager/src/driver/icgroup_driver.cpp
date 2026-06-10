#include "resource_manager/driver/icgroup_driver.h"
#include "resource_manager/driver/cgroup_v2_driver.h"
#include "resource_manager/driver/mock_cgroup_driver.h"

namespace resource_manager {

std::unique_ptr<ICgroupDriver> ICgroupDriver::create(int cgroup_version) {
    switch (cgroup_version) {
        case 0:
            return std::make_unique<MockCgroupDriver>();
        case 2:
            return std::make_unique<CgroupV2Driver>();
        default:
            return nullptr;
    }
}

} // namespace resource_manager
