#include <gtest/gtest.h>
#include <memory>

#include "resource_manager/recovery/recovery_manager.h"
#include "resource_manager/discovery/discovery_rules.h"
#include "resource_manager/driver/mock_cgroup_driver.h"

using namespace resource_manager;

static void setupTestConfig(ConfigRepository& repo) {
    repo.loadFromString(
        "group proc_a {\n"
        "}\n"
        "group proc_b {\n"
        "}\n"
    );
}

class MockRecoveryDiscovery : public IProcessDiscovery {
public:
    std::optional<ProcessInfo> findProcess(const MatchRule& rule) override {
        MatchType type = DiscoveryRules::parseType(rule.type);
        for (const auto& info : processes_) {
            if (DiscoveryRules::match(rule.pattern, type, info.comm)) {
                return info;
            }
        }
        return std::nullopt;
    }

    std::optional<std::vector<ProcessInfo>> findProcesses(const MatchRule& rule) override {
        std::vector<ProcessInfo> results;
        MatchType type = DiscoveryRules::parseType(rule.type);
        for (const auto& info : processes_) {
            if (DiscoveryRules::match(rule.pattern, type, info.comm)) {
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
        setupTestConfig(configRepo_);

        auto mock = std::make_unique<MockRecoveryDiscovery>();
        mockPtr_ = mock.get();
        mockPtr_->addProcess(100, "proc_a");
        mockPtr_->addProcess(200, "proc_b");

        stateManager_.registerProcess("proc_a", 100);
        stateManager_.registerProcess("proc_b", 200);

        discoveryService_ = std::make_unique<DiscoveryService>(std::move(mock), stateManager_);
        attachEngine_ = std::make_unique<AttachEngine>(
            std::make_unique<MockCgroupDriver>());
        recoveryManager_ = std::make_unique<RecoveryManager>(
            stateManager_, *discoveryService_, *attachEngine_, configRepo_);
    }

    ConfigRepository configRepo_;
    RuntimeStateManager stateManager_;
    MockRecoveryDiscovery* mockPtr_;
    std::unique_ptr<DiscoveryService> discoveryService_;
    std::unique_ptr<AttachEngine> attachEngine_;
    std::unique_ptr<RecoveryManager> recoveryManager_;
};

TEST_F(RecoveryManagerTest, RecoverExistingProcess) {
    auto err = recoveryManager_->recoverProcess("proc_a");
    EXPECT_FALSE(err.has_value());

    auto state = stateManager_.findByPid(100);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->processState().recoveryStatus, RecoveryState::Recovered);
    EXPECT_EQ(state->processState().attachedGroupPath, "proc_a");
}

TEST_F(RecoveryManagerTest, RecoverConfigGroupMismatch) {
    mockPtr_->addProcess(400, "no_config_group");
    stateManager_.registerProcess("no_config_group", 400);

    auto err = recoveryManager_->recoverProcess("no_config_group");
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->code, ErrorCode::ConfigNotFound);

    auto state = stateManager_.findByName("no_config_group");
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->processState().recoveryStatus, RecoveryState::Failed);
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

    auto state = stateManager_.findByPid(300);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->processState().pid, 300);
    EXPECT_EQ(state->processState().recoveryStatus, RecoveryState::Recovered);
    EXPECT_EQ(state->processState().attachedGroupPath, "proc_a");
}

TEST_F(RecoveryManagerTest, RecoverAll) {
    stateManager_.registerProcess("proc_c", 300);
    auto state = stateManager_.findByName("proc_c");
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

TEST_F(RecoveryManagerTest, RecoverWithMatchRuleExact) {
    // Add a group with an exact-match rule via config
    configRepo_.loadFromString(
        "group match_exact_group {\n"
        "    match \"exact_proc\" {\n"
        "        type exact;\n"
        "    }\n"
        "}\n"
    );

    stateManager_.registerProcess("match_exact_group", 500);
    mockPtr_->addProcess(500, "exact_proc");

    auto err = recoveryManager_->recoverProcess("match_exact_group");
    EXPECT_FALSE(err.has_value());

    auto state = stateManager_.findByName("match_exact_group");
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->processState().recoveryStatus, RecoveryState::Recovered);
    EXPECT_EQ(state->processState().attachedGroupPath, "match_exact_group");
}

TEST_F(RecoveryManagerTest, RecoverWithMatchRulePrefix) {
    // Add a group with a prefix-match rule via config
    configRepo_.loadFromString(
        "group match_prefix_group {\n"
        "    match \"prefix_proc\" {\n"
        "        type prefix;\n"
        "    }\n"
        "}\n"
    );

    stateManager_.registerProcess("match_prefix_group", 600);
    mockPtr_->addProcess(600, "prefix_proc_123");

    auto err = recoveryManager_->recoverProcess("match_prefix_group");
    EXPECT_FALSE(err.has_value());

    auto state = stateManager_.findByName("match_prefix_group");
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->processState().recoveryStatus, RecoveryState::Recovered);
    EXPECT_EQ(state->processState().attachedGroupPath, "match_prefix_group");
}

TEST_F(RecoveryManagerTest, RecoverWithMatchRuleNotFound) {
    // Group has a match rule but no process matches
    configRepo_.loadFromString(
        "group match_notfound_group {\n"
        "    match \"nonexistent\" {\n"
        "        type exact;\n"
        "    }\n"
        "}\n"
    );

    stateManager_.registerProcess("match_notfound_group", 700);

    auto err = recoveryManager_->recoverProcess("match_notfound_group");
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->code, ErrorCode::ProcessNotFound);

    auto state = stateManager_.findByName("match_notfound_group");
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->processState().recoveryStatus, RecoveryState::Failed);
}


