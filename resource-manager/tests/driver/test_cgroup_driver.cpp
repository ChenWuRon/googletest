#include <gtest/gtest.h>
#include "resource_manager/driver/icgroup_driver.h"

using namespace resource_manager;

TEST(CgroupDriverTest, MockDriver) {
    auto driver = ICgroupDriver::create(0); // mock
    EXPECT_NE(driver, nullptr);
}
