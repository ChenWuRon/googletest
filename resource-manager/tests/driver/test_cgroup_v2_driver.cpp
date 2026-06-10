#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

#include "resource_manager/driver/cgroup_v2_driver.h"

using namespace resource_manager;

namespace fs = std::filesystem;

class CgroupV2DriverTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmpDir = fs::temp_directory_path() / "rq_test_cgroupv2";
        fs::remove_all(tmpDir);
        fs::create_directories(tmpDir);

        createCgroup("mygroup");
    }

    void TearDown() override {
        fs::remove_all(tmpDir);
    }

    void createCgroup(const std::string& name) {
        auto dir = tmpDir / name;
        fs::create_directories(dir);
        std::ofstream(dir / "cgroup.procs") << "";
        std::ofstream(dir / "cgroup.threads") << "";
        std::ofstream(dir / "cgroup.subtree_control") << "";
    }

    fs::path tmpDir;
};

TEST_F(CgroupV2DriverTest, CreateGroup) {
    CgroupV2Driver driver(tmpDir.string());
    auto err = driver.createGroup("newgroup");
    EXPECT_FALSE(err.has_value());
    EXPECT_TRUE(fs::is_directory(tmpDir / "newgroup"));
}

TEST_F(CgroupV2DriverTest, CreateGroupAlreadyExists) {
    CgroupV2Driver driver(tmpDir.string());
    auto err = driver.createGroup("mygroup");
    EXPECT_FALSE(err.has_value());
}

TEST_F(CgroupV2DriverTest, RemoveGroup) {
    CgroupV2Driver driver(tmpDir.string());
    auto err = driver.removeGroup("mygroup");
    EXPECT_FALSE(err.has_value());
    EXPECT_FALSE(fs::exists(tmpDir / "mygroup"));
}

TEST_F(CgroupV2DriverTest, Exists) {
    CgroupV2Driver driver(tmpDir.string());
    EXPECT_TRUE(driver.exists("mygroup"));
    EXPECT_FALSE(driver.exists("nonexistent"));
}

TEST_F(CgroupV2DriverTest, SetAndGetValue) {
    CgroupV2Driver driver(tmpDir.string());
    driver.createGroup("testmem");

    auto err = driver.setValue("testmem", "memory.max", "1G");
    EXPECT_FALSE(err.has_value());

    auto val = driver.getValue("testmem", "memory.max");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "1G");
}

TEST_F(CgroupV2DriverTest, AttachProcess) {
    CgroupV2Driver driver(tmpDir.string());

    auto err = driver.attachProcess("mygroup", ::getpid());
    EXPECT_FALSE(err.has_value());

    auto val = driver.getValue("mygroup", "cgroup.procs");
    ASSERT_TRUE(val.has_value());
}

TEST_F(CgroupV2DriverTest, AttachThread) {
    CgroupV2Driver driver(tmpDir.string());

    auto err = driver.attachThread("mygroup", ::getpid());
    EXPECT_FALSE(err.has_value());

    auto val = driver.getValue("mygroup", "cgroup.threads");
    ASSERT_TRUE(val.has_value());
}

TEST_F(CgroupV2DriverTest, EnableController) {
    CgroupV2Driver driver(tmpDir.string());
    createCgroup("subgroup");

    auto err = driver.enableController("subgroup", "cpu");
    EXPECT_FALSE(err.has_value());

    std::ifstream in(tmpDir / "subgroup" / "cgroup.subtree_control");
    std::string content;
    std::getline(in, content);
    EXPECT_EQ(content, "+cpu");
}

TEST_F(CgroupV2DriverTest, ListGroups) {
    CgroupV2Driver driver(tmpDir.string());
    driver.createGroup("alpha");
    driver.createGroup("beta");

    auto groups = driver.listGroups("");
    EXPECT_GE(groups.size(), 3u);
}

TEST_F(CgroupV2DriverTest, GetValueNotFound) {
    CgroupV2Driver driver(tmpDir.string());
    auto val = driver.getValue("mygroup", "nonexistent.file");
    EXPECT_FALSE(val.has_value());
}

TEST_F(CgroupV2DriverTest, RemoveGroupNonexistent) {
    CgroupV2Driver driver(tmpDir.string());
    auto err = driver.removeGroup("nonexistent");
    EXPECT_FALSE(err.has_value());
}
