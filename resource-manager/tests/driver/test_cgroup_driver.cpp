#include <gtest/gtest.h>
#include "resource_manager/driver/icgroup_driver.h"
#include "resource_manager/driver/mock_cgroup_driver.h"

using namespace resource_manager;

TEST(CgroupDriverTest, FactoryCreateMock) {
    auto driver = ICgroupDriver::create(0);
    ASSERT_NE(driver, nullptr);
    EXPECT_TRUE(driver->createGroup("test").has_value() == false);
}

TEST(CgroupDriverTest, FactoryCreateInvalid) {
    auto driver = ICgroupDriver::create(99);
    EXPECT_EQ(driver, nullptr);
}

TEST(CgroupDriverTest, MockCreateGroup) {
    MockCgroupDriver driver;
    EXPECT_FALSE(driver.exists("test"));

    auto err = driver.createGroup("test");
    EXPECT_FALSE(err.has_value());
    EXPECT_TRUE(driver.exists("test"));
}

TEST(CgroupDriverTest, MockRemoveGroup) {
    MockCgroupDriver driver;
    driver.createGroup("test");
    EXPECT_TRUE(driver.exists("test"));

    driver.removeGroup("test");
    EXPECT_FALSE(driver.exists("test"));
}

TEST(CgroupDriverTest, MockSetAndGetValue) {
    MockCgroupDriver driver;
    driver.createGroup("test");

    auto err = driver.setValue("test", "cpu.max", "100000 100000");
    EXPECT_FALSE(err.has_value());

    auto val = driver.getValue("test", "cpu.max");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "100000 100000");
}

TEST(CgroupDriverTest, MockAttachProcess) {
    MockCgroupDriver driver;
    driver.createGroup("test");

    auto err = driver.attachProcess("test", 1234);
    EXPECT_FALSE(err.has_value());

    auto val = driver.getValue("test", "cgroup.procs");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "1234");
}

TEST(CgroupDriverTest, MockAttachThread) {
    MockCgroupDriver driver;
    driver.createGroup("test");

    auto err = driver.attachThread("test", 5678);
    EXPECT_FALSE(err.has_value());

    auto val = driver.getValue("test", "cgroup.threads");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "5678");
}

TEST(CgroupDriverTest, MockEnableController) {
    MockCgroupDriver driver;
    driver.createGroup("test");

    auto err = driver.enableController("test", "cpu");
    EXPECT_FALSE(err.has_value());
}

TEST(CgroupDriverTest, MockListGroups) {
    MockCgroupDriver driver;
    driver.createGroup("web/cpu");
    driver.createGroup("web/memory");
    driver.createGroup("db/cpu");

    auto all = driver.listGroups("");
    EXPECT_EQ(all.size(), 3u);

    auto web = driver.listGroups("web");
    EXPECT_EQ(web.size(), 2u);
}

TEST(CgroupDriverTest, MockGetValueNotFound) {
    MockCgroupDriver driver;
    auto val = driver.getValue("nonexistent", "cpu.max");
    EXPECT_FALSE(val.has_value());
}

TEST(CgroupDriverTest, MockOperationsLog) {
    MockCgroupDriver driver;
    driver.createGroup("test");
    driver.setValue("test", "cpu.max", "100000");
    driver.attachProcess("test", 1234);

    auto ops = driver.operations();
    EXPECT_EQ(ops.size(), 4u);
    EXPECT_EQ(ops[0].method, "createGroup");
    EXPECT_EQ(ops[1].method, "setValue");
    EXPECT_EQ(ops[2].method, "attachProcess");
    EXPECT_EQ(ops[3].method, "setValue");
}

TEST(CgroupDriverTest, MockErrorInjection) {
    MockCgroupDriver driver;
    driver.setNextError(Error(ErrorCode::CgroupCreateFailed, "injected", "test"));

    auto err = driver.createGroup("test");
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->code, ErrorCode::CgroupCreateFailed);
}

TEST(CgroupDriverTest, MockSetValueWithoutGroup) {
    MockCgroupDriver driver;
    auto err = driver.setValue("nonexistent", "cpu.max", "100000");
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->code, ErrorCode::CgroupWriteFailed);
}
