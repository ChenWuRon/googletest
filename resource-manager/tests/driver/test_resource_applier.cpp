#include <gtest/gtest.h>
#include "resource_manager/driver/resource_applier.h"
#include "resource_manager/driver/mock_cgroup_driver.h"
#include "resource_manager/driver/control_file_type.h"

using namespace resource_manager;

class ResourceApplierTest : public ::testing::Test {
protected:
    void SetUp() override {
        ControllerRegistry::instance().registerBuiltins();
    }

    std::unique_ptr<ConfigNode> makeGroup(const std::string& name) {
        return std::make_unique<ConfigNode>(ConfigNodeType::GROUP, name);
    }

    std::unique_ptr<ConfigNode> makeController(const std::string& name) {
        return std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, name);
    }

    std::unique_ptr<ConfigNode> makeItem(const std::string& name, const std::string& value) {
        return std::make_unique<ConfigNode>(ConfigNodeType::ITEM, name, value);
    }

    ConfigNode* addGroup(ConfigNode& root, std::unique_ptr<ConfigNode> group) {
        return root.addChild(std::move(group));
    }

    ConfigNode* addController(ConfigNode& group, std::unique_ptr<ConfigNode> ctrl) {
        return group.addChild(std::move(ctrl));
    }

    void addItem(ConfigNode& ctrl, std::unique_ptr<ConfigNode> item) {
        ctrl.addChild(std::move(item));
    }
};

TEST_F(ResourceApplierTest, SuccessfulApply) {
    MockCgroupDriver driver;
    ResourceApplier applier(driver, ControllerRegistry::instance());

    ConfigDomain domain;
    auto& root = domain.root();

    auto* web = addGroup(root, makeGroup("web"));

    auto* cpuCtrl = addController(*web, makeController("cpu"));
    addItem(*cpuCtrl, makeItem("cpu.max", "100000 100000"));
    addItem(*cpuCtrl, makeItem("cpu.weight", "100"));

    auto* memCtrl = addController(*web, makeController("memory"));
    addItem(*memCtrl, makeItem("memory.max", "1G"));

    auto err = applier.apply(domain);
    EXPECT_FALSE(err.has_value());

    EXPECT_TRUE(driver.exists("web"));

    auto cpuMax = driver.getValue("web", "cpu.max");
    ASSERT_TRUE(cpuMax.has_value());
    EXPECT_EQ(*cpuMax, "100000 100000");

    auto memMax = driver.getValue("web", "memory.max");
    ASSERT_TRUE(memMax.has_value());
    EXPECT_EQ(*memMax, "1G");
}

TEST_F(ResourceApplierTest, InvalidController) {
    MockCgroupDriver driver;
    ResourceApplier applier(driver, ControllerRegistry::instance());

    ConfigDomain domain;
    auto& root = domain.root();
    auto* group = addGroup(root, makeGroup("test"));
    auto* badCtrl = addController(*group, makeController("nonexistent_controller"));
    addItem(*badCtrl, makeItem("cpu.max", "100000"));

    auto err = applier.apply(domain);
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->code, ErrorCode::ControllerNotSupported);
}

TEST_F(ResourceApplierTest, InvalidItem) {
    MockCgroupDriver driver;
    ResourceApplier applier(driver, ControllerRegistry::instance());

    ConfigDomain domain;
    auto& root = domain.root();
    auto* group = addGroup(root, makeGroup("test"));
    auto* cpuCtrl = addController(*group, makeController("cpu"));
    addItem(*cpuCtrl, makeItem("nonexistent.item", "100000"));

    auto err = applier.apply(domain);
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->code, ErrorCode::InvalidControlValue);
}

TEST_F(ResourceApplierTest, DriverFailure) {
    MockCgroupDriver driver;
    driver.setNextError(Error(ErrorCode::CgroupCreateFailed, "injected", "test"));
    ResourceApplier applier(driver, ControllerRegistry::instance());

    ConfigDomain domain;
    auto& root = domain.root();
    auto* group = addGroup(root, makeGroup("web"));
    auto* cpuCtrl = addController(*group, makeController("cpu"));
    addItem(*cpuCtrl, makeItem("cpu.max", "100000"));

    auto err = applier.apply(domain);
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->code, ErrorCode::CgroupCreateFailed);
}
