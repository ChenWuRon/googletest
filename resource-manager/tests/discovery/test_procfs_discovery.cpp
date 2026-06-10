#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <unistd.h>

#include "resource_manager/discovery/procfs_discovery.h"

using namespace resource_manager;

namespace fs = std::filesystem;

class ProcfsDiscoveryTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmpDir = fs::temp_directory_path() / "rq_test_procfs";
        fs::remove_all(tmpDir);
        createProcEntry(100, "nginx", "nginx master process");
        createProcEntry(200, "redis-server", "redis-server *:6379");
        createTaskDir(100, {100, 101});
    }

    void TearDown() override {
        fs::remove_all(tmpDir);
    }

    void createProcEntry(int pid, const std::string& comm, const std::string& cmdline) {
        auto dir = tmpDir / std::to_string(pid);
        fs::create_directories(dir);

        std::ofstream(dir / "comm") << comm << "\n";
        std::ofstream(dir / "cmdline") << cmdline;
    }

    void createTaskDir(int pid, const std::vector<int>& tids) {
        auto taskDir = tmpDir / std::to_string(pid) / "task";
        fs::create_directories(taskDir);

        for (int tid : tids) {
            auto tidDir = taskDir / std::to_string(tid);
            fs::create_directories(tidDir);
            auto baseComm = (tid == pid) ? "nginx" : "nginx-worker";
            std::ofstream(tidDir / "comm") << baseComm << "\n";
        }
    }

    fs::path tmpDir;
};

TEST_F(ProcfsDiscoveryTest, FindProcessExact) {
    ProcfsDiscovery discovery(tmpDir.string());
    MatchRule rule{"nginx", "exact"};

    auto result = discovery.findProcess(rule);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->pid, 100);
    EXPECT_EQ(result->comm, "nginx");
}

TEST_F(ProcfsDiscoveryTest, FindProcessNotFound) {
    ProcfsDiscovery discovery(tmpDir.string());
    MatchRule rule{"unknown", "exact"};

    auto result = discovery.findProcess(rule);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ProcfsDiscoveryTest, FindProcessesPrefix) {
    createProcEntry(300, "nginx-worker", "nginx worker process");
    ProcfsDiscovery discovery(tmpDir.string());
    MatchRule rule{"nginx", "prefix"};

    auto result = discovery.findProcesses(rule);
    ASSERT_TRUE(result.has_value());
    EXPECT_GE(result->size(), 2u);
}

TEST_F(ProcfsDiscoveryTest, FindThreads) {
    ProcfsDiscovery discovery(tmpDir.string());

    auto result = discovery.findThreads(100);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 2u);
}

TEST_F(ProcfsDiscoveryTest, FindThreadsNotFound) {
    ProcfsDiscovery discovery(tmpDir.string());

    auto result = discovery.findThreads(999);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ProcfsDiscoveryTest, Exists) {
    ProcfsDiscovery discovery(tmpDir.string());
    // Our own process always exists
    EXPECT_TRUE(discovery.exists(::getpid()));
    // Extremely high PID is unlikely to exist
    EXPECT_FALSE(discovery.exists(99999999));
}

TEST_F(ProcfsDiscoveryTest, ExistsWithCustomPath) {
    ProcfsDiscovery discovery(tmpDir.string());
    EXPECT_FALSE(discovery.exists(100));
}
