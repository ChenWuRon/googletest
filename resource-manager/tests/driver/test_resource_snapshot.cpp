#include <gtest/gtest.h>
#include "resource_manager/driver/resource_snapshot.h"
#include "resource_manager/driver/mock_cgroup_driver.h"

using namespace resource_manager;

class ResourceSnapshotTest : public ::testing::Test {
protected:
    void SetUp() override {
        driver_.createGroup("web");
        driver_.setValue("web", "cpu.max", "100000 100000");
        driver_.setValue("web", "memory.max", "1G");
        driver_.createGroup("db");
        driver_.setValue("db", "memory.max", "500M");
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

    ConfigDomain makeDomain() {
        ConfigDomain domain;
        auto* web = addChild(domain.root(), makeGroup("web"));
        auto* cpuCtrl = addChild(*web, makeController("cpu"));
        addChild(*cpuCtrl, makeItem("cpu.max", "100000 100000"));
        auto* memCtrl = addChild(*web, makeController("memory"));
        addChild(*memCtrl, makeItem("memory.max", "1G"));

        auto* db = addChild(domain.root(), makeGroup("db"));
        auto* dbMemCtrl = addChild(*db, makeController("memory"));
        addChild(*dbMemCtrl, makeItem("memory.max", "500M"));

        return domain;
    }

    MockCgroupDriver driver_;
};

TEST_F(ResourceSnapshotTest, Capture) {
    auto domain = makeDomain();
    auto snapshot = ResourceSnapshot::capture(driver_, domain);

    EXPECT_EQ(snapshot.entries().size(), 3u);
    EXPECT_GT(std::chrono::duration_cast<std::chrono::seconds>(
        snapshot.capturedAt().time_since_epoch()).count(), 0);
}

TEST_F(ResourceSnapshotTest, CaptureValues) {
    auto domain = makeDomain();
    auto snapshot = ResourceSnapshot::capture(driver_, domain);

    bool foundCpuMax = false;
    bool foundWebMem = false;
    bool foundDbMem = false;

    for (const auto& entry : snapshot.entries()) {
        if (entry.cgroupPath == "web" && entry.item == "cpu.max") {
            EXPECT_EQ(entry.value, "100000 100000");
            foundCpuMax = true;
        }
        if (entry.cgroupPath == "web" && entry.item == "memory.max") {
            EXPECT_EQ(entry.value, "1G");
            foundWebMem = true;
        }
        if (entry.cgroupPath == "db" && entry.item == "memory.max") {
            EXPECT_EQ(entry.value, "500M");
            foundDbMem = true;
        }
    }

    EXPECT_TRUE(foundCpuMax);
    EXPECT_TRUE(foundWebMem);
    EXPECT_TRUE(foundDbMem);
}

TEST_F(ResourceSnapshotTest, CompareIdentical) {
    auto domain = makeDomain();
    auto snap1 = ResourceSnapshot::capture(driver_, domain);
    auto snap2 = ResourceSnapshot::capture(driver_, domain);

    auto diff = snap2.compare(snap1);
    EXPECT_TRUE(diff.empty());
    EXPECT_EQ(diff.size(), 0u);
}

TEST_F(ResourceSnapshotTest, CompareChanged) {
    auto domain = makeDomain();
    auto snap1 = ResourceSnapshot::capture(driver_, domain);

    driver_.setValue("web", "cpu.max", "200000 100000");
    auto snap2 = ResourceSnapshot::capture(driver_, domain);

    auto diff = snap2.compare(snap1);
    EXPECT_FALSE(diff.empty());
    EXPECT_EQ(diff.size(), 1u);
    EXPECT_EQ(diff.changes[0].cgroupPath, "web");
    EXPECT_EQ(diff.changes[0].item, "cpu.max");
    EXPECT_EQ(diff.changes[0].oldValue, "100000 100000");
    EXPECT_EQ(diff.changes[0].newValue, "200000 100000");
}

TEST_F(ResourceSnapshotTest, CompareMultipleChanges) {
    auto domain = makeDomain();
    auto snap1 = ResourceSnapshot::capture(driver_, domain);

    driver_.setValue("web", "cpu.max", "200000 100000");
    driver_.setValue("db", "memory.max", "1G");
    auto snap2 = ResourceSnapshot::capture(driver_, domain);

    auto diff = snap2.compare(snap1);
    EXPECT_EQ(diff.size(), 2u);
}

TEST_F(ResourceSnapshotTest, Serialize) {
    auto domain = makeDomain();
    auto snapshot = ResourceSnapshot::capture(driver_, domain);

    std::string text = snapshot.serialize();
    EXPECT_NE(text.find("[web]"), std::string::npos);
    EXPECT_NE(text.find("[db]"), std::string::npos);
    EXPECT_NE(text.find("cpu.max = 100000 100000"), std::string::npos);
    EXPECT_NE(text.find("memory.max = 1G"), std::string::npos);
    EXPECT_NE(text.find("memory.max = 500M"), std::string::npos);
}

TEST_F(ResourceSnapshotTest, CaptureEmptyDomain) {
    ConfigDomain emptyDomain;
    auto snapshot = ResourceSnapshot::capture(driver_, emptyDomain);
    EXPECT_TRUE(snapshot.entries().empty());
}

TEST_F(ResourceSnapshotTest, CaptureMissingGroup) {
    ConfigDomain domain;
    auto* grp = addChild(domain.root(), makeGroup("nonexistent"));
    auto* ctrl = addChild(*grp, makeController("cpu"));
    addChild(*ctrl, makeItem("cpu.max", ""));

    auto snapshot = ResourceSnapshot::capture(driver_, domain);
    ASSERT_EQ(snapshot.entries().size(), 1u);
    EXPECT_TRUE(snapshot.entries()[0].value.empty());
}

TEST_F(ResourceSnapshotTest, Timestamps) {
    auto domain = makeDomain();
    auto snap1 = ResourceSnapshot::capture(driver_, domain);
    auto snap2 = ResourceSnapshot::capture(driver_, domain);

    EXPECT_LE(snap1.capturedAt(), snap2.capturedAt());
}
