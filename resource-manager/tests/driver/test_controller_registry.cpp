#include <gtest/gtest.h>
#include "resource_manager/driver/control_file_type.h"

using namespace resource_manager;

class ControllerRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        ControllerRegistry::instance().registerBuiltins();
    }
};

TEST_F(ControllerRegistryTest, Registration) {
    auto& reg = ControllerRegistry::instance();

    ControllerDefinition ctrl;
    ctrl.name = "test_ctrl";
    ctrl.min_version = 2;
    ctrl.max_version = 2;
    ctrl.items["test.item"] = {"test.item", ValueType::String, 2, "default"};

    reg.registerController(ctrl);

    auto found = reg.findController("test_ctrl");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "test_ctrl");
    EXPECT_EQ(found->items.size(), 1u);
}

TEST_F(ControllerRegistryTest, Lookup) {
    auto& reg = ControllerRegistry::instance();

    auto notFound = reg.findController("nonexistent");
    EXPECT_EQ(notFound, nullptr);

    auto cpu = reg.findController("cpu");
    EXPECT_NE(cpu, nullptr);
}

TEST_F(ControllerRegistryTest, DuplicateRegistration) {
    auto& reg = ControllerRegistry::instance();

    ControllerDefinition ctrl;
    ctrl.name = "dup_ctrl";
    ctrl.min_version = 2;
    ctrl.max_version = 2;

    reg.registerController(ctrl);

    ControllerDefinition ctrl2;
    ctrl2.name = "dup_ctrl";
    ctrl2.min_version = 1;
    ctrl2.max_version = 2;
    ctrl2.items["item1"] = {"item1", ValueType::Int, 2, "0"};

    reg.registerController(ctrl2);

    auto found = reg.findController("dup_ctrl");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->min_version, 1);
    EXPECT_EQ(found->items.size(), 1u);
}

TEST_F(ControllerRegistryTest, ListControllers) {
    auto& reg = ControllerRegistry::instance();

    auto all = reg.listControllers();
    EXPECT_GE(all.size(), 2u);

    bool hasCpu = false;
    for (const auto& name : all) {
        if (name == "cpu") hasCpu = true;
    }
    EXPECT_TRUE(hasCpu);
}

TEST_F(ControllerRegistryTest, Builtins) {
    auto& reg = ControllerRegistry::instance();

    auto cpu = reg.findController("cpu");
    ASSERT_NE(cpu, nullptr);

    auto cpuMax = reg.findControlFile("cpu", "cpu.max");
    ASSERT_NE(cpuMax, nullptr);
    EXPECT_EQ(cpuMax->value_type, ValueType::Quota);
    EXPECT_EQ(cpuMax->default_value, "max 100000");

    auto memMax = reg.findControlFile("memory", "memory.max");
    ASSERT_NE(memMax, nullptr);
    EXPECT_EQ(memMax->value_type, ValueType::Size);

    auto pidsMax = reg.findControlFile("pids", "pids.max");
    ASSERT_NE(pidsMax, nullptr);
    EXPECT_EQ(pidsMax->value_type, ValueType::Int);
}

TEST_F(ControllerRegistryTest, FindControlFileNotFound) {
    auto& reg = ControllerRegistry::instance();

    auto notFound = reg.findControlFile("cpu", "nonexistent.item");
    EXPECT_EQ(notFound, nullptr);

    auto ctrlNotFound = reg.findControlFile("nonexistent", "cpu.max");
    EXPECT_EQ(ctrlNotFound, nullptr);
}
