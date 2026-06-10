#include <gtest/gtest.h>
#include "resource_manager/driver/resource_validator.h"

using namespace resource_manager;

class ResourceValidatorTest : public ::testing::Test {
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

    ConfigNode* addChild(ConfigNode& parent, std::unique_ptr<ConfigNode> child) {
        return parent.addChild(std::move(child));
    }

    ResourceValidator validator{ControllerRegistry::instance()};
};

TEST_F(ResourceValidatorTest, ValidCpuMax) {
    auto err = validator.validate("cpu", "cpu.max", "100000 100000");
    EXPECT_FALSE(err.has_value());
}

TEST_F(ResourceValidatorTest, ValidCpuMaxWithMax) {
    auto err = validator.validate("cpu", "cpu.max", "max 100000");
    EXPECT_FALSE(err.has_value());
}

TEST_F(ResourceValidatorTest, InvalidCpuMax) {
    auto err = validator.validate("cpu", "cpu.max", "abc");
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->code, ErrorCode::InvalidControlValue);
}

TEST_F(ResourceValidatorTest, EmptyCpuMax) {
    auto err = validator.validate("cpu", "cpu.max", "");
    ASSERT_TRUE(err.has_value());
}

TEST_F(ResourceValidatorTest, ValidCpuWeight) {
    auto err = validator.validate("cpu", "cpu.weight", "100");
    EXPECT_FALSE(err.has_value());
}

TEST_F(ResourceValidatorTest, CpuWeightLowerBound) {
    auto err = validator.validate("cpu", "cpu.weight", "1");
    EXPECT_FALSE(err.has_value());
}

TEST_F(ResourceValidatorTest, CpuWeightUpperBound) {
    auto err = validator.validate("cpu", "cpu.weight", "10000");
    EXPECT_FALSE(err.has_value());
}

TEST_F(ResourceValidatorTest, InvalidCpuWeightZero) {
    auto err = validator.validate("cpu", "cpu.weight", "0");
    ASSERT_TRUE(err.has_value());
}

TEST_F(ResourceValidatorTest, InvalidCpuWeightTooHigh) {
    auto err = validator.validate("cpu", "cpu.weight", "10001");
    ASSERT_TRUE(err.has_value());
}

TEST_F(ResourceValidatorTest, InvalidCpuWeightAlpha) {
    auto err = validator.validate("cpu", "cpu.weight", "abc");
    ASSERT_TRUE(err.has_value());
}

TEST_F(ResourceValidatorTest, ValidMemoryMax) {
    auto err = validator.validate("memory", "memory.max", "1G");
    EXPECT_FALSE(err.has_value());
}

TEST_F(ResourceValidatorTest, ValidMemoryMaxWithMax) {
    auto err = validator.validate("memory", "memory.max", "max");
    EXPECT_FALSE(err.has_value());
}

TEST_F(ResourceValidatorTest, ValidMemoryMaxBytes) {
    auto err = validator.validate("memory", "memory.max", "1073741824");
    EXPECT_FALSE(err.has_value());
}

TEST_F(ResourceValidatorTest, InvalidMemoryMax) {
    auto err = validator.validate("memory", "memory.max", "1GB");
    ASSERT_TRUE(err.has_value());
}

TEST_F(ResourceValidatorTest, InvalidMemoryMaxNegative) {
    auto err = validator.validate("memory", "memory.max", "-1");
    ASSERT_TRUE(err.has_value());
}

TEST_F(ResourceValidatorTest, ValidCpusetCpus) {
    auto err = validator.validate("cpuset", "cpuset.cpus", "0-3");
    EXPECT_FALSE(err.has_value());
}

TEST_F(ResourceValidatorTest, ValidCpusetCpusList) {
    auto err = validator.validate("cpuset", "cpuset.cpus", "0,2,4");
    EXPECT_FALSE(err.has_value());
}

TEST_F(ResourceValidatorTest, ValidCpusetCpusSingle) {
    auto err = validator.validate("cpuset", "cpuset.cpus", "0");
    EXPECT_FALSE(err.has_value());
}

TEST_F(ResourceValidatorTest, InvalidCpusetCpus) {
    auto err = validator.validate("cpuset", "cpuset.cpus", "a");
    ASSERT_TRUE(err.has_value());
}

TEST_F(ResourceValidatorTest, InvalidCpusetCpusNegative) {
    auto err = validator.validate("cpuset", "cpuset.cpus", "-1");
    ASSERT_TRUE(err.has_value());
}

TEST_F(ResourceValidatorTest, ValidPidsMaxInt) {
    auto err = validator.validate("pids", "pids.max", "1024");
    EXPECT_FALSE(err.has_value());
}

TEST_F(ResourceValidatorTest, ValidPidsMax) {
    auto err = validator.validate("pids", "pids.max", "max");
    EXPECT_FALSE(err.has_value());
}

TEST_F(ResourceValidatorTest, InvalidPidsMax) {
    auto err = validator.validate("pids", "pids.max", "-1");
    ASSERT_TRUE(err.has_value());
}

TEST_F(ResourceValidatorTest, InvalidPidsMaxAlpha) {
    auto err = validator.validate("pids", "pids.max", "abc");
    ASSERT_TRUE(err.has_value());
}

TEST_F(ResourceValidatorTest, CpusetPartitionMember) {
    auto err = validator.validate("cpuset", "cpuset.cpus.partition", "member");
    EXPECT_FALSE(err.has_value());
}

TEST_F(ResourceValidatorTest, CpusetPartitionRoot) {
    auto err = validator.validate("cpuset", "cpuset.cpus.partition", "root");
    EXPECT_FALSE(err.has_value());
}

TEST_F(ResourceValidatorTest, CpusetPartitionInvalid) {
    auto err = validator.validate("cpuset", "cpuset.cpus.partition", "foo");
    ASSERT_TRUE(err.has_value());
}

TEST_F(ResourceValidatorTest, UnknownController) {
    auto err = validator.validate("unknown", "some.item", "value");
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->code, ErrorCode::InvalidControlValue);
}

TEST_F(ResourceValidatorTest, UnknownItem) {
    auto err = validator.validate("cpu", "nonexistent", "value");
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->code, ErrorCode::InvalidControlValue);
}

TEST_F(ResourceValidatorTest, ValidDomain) {
    ConfigDomain domain;
    auto* grp = addChild(domain.root(), makeGroup("web"));
    auto* ctrl = addChild(*grp, makeController("cpu"));
    addChild(*ctrl, makeItem("cpu.max", "100000 100000"));

    auto err = validator.validate(domain);
    EXPECT_FALSE(err.has_value());
}

TEST_F(ResourceValidatorTest, InvalidDomain) {
    ConfigDomain domain;
    auto* grp = addChild(domain.root(), makeGroup("web"));
    auto* ctrl = addChild(*grp, makeController("cpu"));
    addChild(*ctrl, makeItem("cpu.max", "invalid"));

    auto err = validator.validate(domain);
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->code, ErrorCode::InvalidControlValue);
}

TEST_F(ResourceValidatorTest, UnknownControllerInDomain) {
    ConfigDomain domain;
    auto* grp = addChild(domain.root(), makeGroup("web"));
    auto* ctrl = addChild(*grp, makeController("unknown"));
    addChild(*ctrl, makeItem("x", "y"));

    auto err = validator.validate(domain);
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->code, ErrorCode::ControllerNotSupported);
}

TEST_F(ResourceValidatorTest, ValidIoWeight) {
    auto err = validator.validate("io", "io.weight", "500");
    EXPECT_FALSE(err.has_value());
}

TEST_F(ResourceValidatorTest, ValidCpuWeightNice) {
    auto err = validator.validate("cpu", "cpu.weight.nice", "0");
    EXPECT_FALSE(err.has_value());
}

TEST_F(ResourceValidatorTest, ValidCpuIdle) {
    auto err = validator.validate("cpu", "cpu.idle", "0");
    EXPECT_FALSE(err.has_value());
}

TEST_F(ResourceValidatorTest, InvalidCpuIdle) {
    auto err = validator.validate("cpu", "cpu.idle", "2");
    ASSERT_TRUE(err.has_value());
}

TEST_F(ResourceValidatorTest, ValidMemoryHigh) {
    auto err = validator.validate("memory", "memory.high", "500M");
    EXPECT_FALSE(err.has_value());
}

TEST_F(ResourceValidatorTest, ValidMemoryMin) {
    auto err = validator.validate("memory", "memory.min", "0");
    EXPECT_FALSE(err.has_value());
}

TEST_F(ResourceValidatorTest, MultipleGroups) {
    ConfigDomain domain;
    auto* g1 = addChild(domain.root(), makeGroup("web"));
    auto* c1 = addChild(*g1, makeController("cpu"));
    addChild(*c1, makeItem("cpu.max", "100000 100000"));

    auto* g2 = addChild(domain.root(), makeGroup("db"));
    auto* c2 = addChild(*g2, makeController("memory"));
    addChild(*c2, makeItem("memory.max", "1G"));

    auto err = validator.validate(domain);
    EXPECT_FALSE(err.has_value());
}
