#include <gtest/gtest.h>
#include <memory>

#include "resource_manager/recovery/recovery_manager.h"
#include "resource_manager/discovery/discovery_rules.h"

using namespace resource_manager;

class MockRecoveryDiscovery : public IProcessDiscovery {
public:
    std::optional<ProcessInfo> findProcess(const MatchRule& rule) override {
        for (const auto& info : processes_) {
            if (DiscoveryRules::match(info.comm, MatchType::Exact, rule.pattern)) {
                return info;
            }
        }
        return std::nullopt;
    }

    std::optional<std::vector<ProcessInfo>> findProcesses(const MatchRule& rule) override {
        std::vector<ProcessInfo> results;
        for (const auto& info : processes_) {
            if (DiscoveryRules::match(info.comm, MatchType::Exact, rule.pattern)) {
                results.push_back(info);
            }
        }
        return results;
    }

    std::optional<std::vector<ThreadInfo>> findThreads(int pid) override { return std::vector<ThreadInfo>{}; }
    bool exists(int pid) override {
        for (const auto& info : processes_) {
            if (info.pid == pid) return true;
        }
        return false;
    }

    void addProcess(int pid, const std::string& name) {
        processes_.push_back({pid, name, name, "", {}});
    }

    void removeProcess(int pid) {
        processes_.erase(
            std::remove_if(processes_.begin(), processes_.end(),
                [pid](const ProcessInfo& p) { return p.pid == pid; }),
            processes_.end());
    }

private:
    std::vector<ProcessInfo> processes_;
};

class RecoveryManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto mock = std::make_unique<MockRecoveryDiscovery>();
        mockPtr_ = mock.get();
        mockPtr_->addProcess(100, "proc_a");
        mockPtr_->addProcess(200, "proc_b");

        repo_.registerProcess("proc_a", 100);
        repo_.registerProcess("proc_b", 200);

        discoveryService_ = std::make_unique<DiscoveryService>(std::move(mock), repo_);
        recoveryManager_ = std::make_unique<RecoveryManager>(repo_, *discoveryService_);
    }

    RuntimeRepository repo_;
    MockRecoveryDiscovery* mockPtr_;
    std::unique_ptr<DiscoveryService> discoveryService_;
    std::unique_ptr<RecoveryManager> recoveryManager_;
};

TEST_F(RecoveryManagerTest, RecoverExistingProcess) {
    auto err = recoveryManager_->recoverProcess("proc_a");
    EXPECT_FALSE(err.has_value());

    auto state = repo_.findByPid(100);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->processState().recoveryStatus, RecoveryState::Recovered);
}

TEST_F(RecoveryManagerTest, RecoverNonexistentProcess) {
    auto err = recoveryManager_->recoverProcess("nonexistent");
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->code, ErrorCode::ProcessNotFound);
}

TEST_F(RecoveryManagerTest, RecoverWithNewPID) {
    mockPtr_->removeProcess(100);
    mockPtr_->addProcess(300, "proc_a");

    auto err = recoveryManager_->recoverProcess("proc_a");
    EXPECT_FALSE(err.has_value());

    auto state = repo_.findByPid(300);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->processState().pid, 300);
    EXPECT_EQ(state->processState().recoveryStatus, RecoveryState::Recovered);
}

TEST_F(RecoveryManagerTest, RecoverAll) {
    repo_.registerProcess("proc_c", 300);
    auto state = repo_.findByName("proc_c");
    ASSERT_TRUE(state.has_value());
    state->processState().discoveryStatus = DiscoveryStatus::Missing;

    mockPtr_->addProcess(300, "proc_c");

    auto err = recoveryManager_->recoverAll();
    EXPECT_FALSE(err.has_value());
}

TEST_F(RecoveryManagerTest, RecoverWithRetrySuccess) {
    auto err = recoveryManager_->recoverProcessWithRetry("proc_a", 3);
    EXPECT_FALSE(err.has_value());
}

TEST_F(RecoveryManagerTest, RecoverWithRetryAllFail) {
    auto err = recoveryManager_->recoverProcessWithRetry("nonexistent", 2);
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->code, ErrorCode::RecoveryFailed);
}

TEST_F(RecoveryManagerTest, EventsPublished) {
    recoveryManager_->recoverProcess("proc_a");

    auto events = recoveryManager_->events();
    EXPECT_FALSE(events.empty());

    bool foundSuccess = false;
    for (const auto& ev : events) {
        if (ev.type() == EventType::RecoverySucceeded) {
            foundSuccess = true;
            break;
        }
    }
    EXPECT_TRUE(foundSuccess);
}

TEST_F(RecoveryManagerTest, RecoveryFailedEvent) {
    recoveryManager_->recoverProcess("nonexistent");

    auto events = recoveryManager_->events();
    EXPECT_FALSE(events.empty());

    bool foundFailed = false;
    for (const auto& ev : events) {
        if (ev.type() == EventType::RecoveryFailed) {
            foundFailed = true;
            break;
        }
    }
    EXPECT_TRUE(foundFailed);
}
