#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>

#include "resource_manager/core/config_repository.h"
#include "resource_manager/discovery/discovery_service.h"
#include "resource_manager/discovery/discovery_rules.h"
#include "resource_manager/attach/attach_engine.h"
#include "resource_manager/attach/attach_policy.h"
#include "resource_manager/monitor/monitor.h"
#include "resource_manager/recovery/recovery_manager.h"
#include "resource_manager/driver/mock_cgroup_driver.h"
#include "resource_manager/state/runtime_state_manager.h"

using namespace resource_manager;

// ─── Mock Discovery for Integration Tests ──────────────────────────────────

class MockIntegrationDiscovery : public IProcessDiscovery {
public:
    std::vector<ProcessInfo> procs;

    std::optional<ProcessInfo> findProcess(const MatchRule& rule) override {
        MatchType type = DiscoveryRules::parseType(rule.type);
        for (const auto& p : procs) {
            if (DiscoveryRules::match(rule.pattern, type, p.comm)) {
                return p;
            }
        }
        return std::nullopt;
    }

    std::optional<std::vector<ProcessInfo>> findProcesses(const MatchRule& rule) override {
        std::vector<ProcessInfo> matched;
        MatchType type = DiscoveryRules::parseType(rule.type);
        for (const auto& p : procs) {
            if (DiscoveryRules::match(rule.pattern, type, p.comm)) {
                matched.push_back(p);
            }
        }
        if (matched.empty()) return std::nullopt;
        return matched;
    }

    std::optional<std::vector<ThreadInfo>> findThreads(int) override {
        return std::vector<ThreadInfo>{};
    }

    bool exists(int pid) override {
        for (const auto& p : procs) {
            if (p.pid == pid) return true;
        }
        return false;
    }

    void addProcess(int pid, const std::string& comm) {
        procs.push_back({pid, comm, comm, "", {}});
    }

    void removeProcess(int pid) {
        procs.erase(
            std::remove_if(procs.begin(), procs.end(),
                [pid](const ProcessInfo& p) { return p.pid == pid; }),
            procs.end());
    }
};

// ─── Helpers ───────────────────────────────────────────────────────────────

static std::unique_ptr<ConfigNode> makeGroup(const std::string& name) {
    return std::make_unique<ConfigNode>(ConfigNodeType::GROUP, name);
}

static std::unique_ptr<ConfigNode> makeController(const std::string& name) {
    return std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, name);
}

static std::unique_ptr<ConfigNode> makeItem(const std::string& name, const std::string& value) {
    return std::make_unique<ConfigNode>(ConfigNodeType::ITEM, name, value);
}

// ═══════════════════════════════════════════════════════════════════════════
// IT-001: Valid Config → Parse → Validate → Success
// ═══════════════════════════════════════════════════════════════════════════

TEST(IntegrationTest, IT001_ValidConfig_ParseAndValidate) {
    ConfigRepository repo;

    bool ok = repo.loadFromString(
        "group web-server {\n"
        "    mode service;\n"
        "    match \"nginx\" {\n"
        "        type prefix;\n"
        "    }\n"
        "    controller cpu {\n"
        "        item cpu.max = \"100000 100000\";\n"
        "        item cpu.weight = \"100\";\n"
        "    }\n"
        "    controller memory {\n"
        "        item memory.max = \"1G\";\n"
        "    }\n"
        "}\n"
    );

    EXPECT_TRUE(ok);
    EXPECT_TRUE(repo.errors().empty());

    // ConfigState metadata populated
    const auto& cs = repo.getConfigState();
    EXPECT_EQ(cs.source, "memory");
    EXPECT_GT(cs.version, 0u);

    // Domain tree structure is valid
    const auto& root = repo.getRoot().root();
    EXPECT_EQ(root.type(), ConfigNodeType::ROOT);

    const auto* group = root.findChild("web-server");
    ASSERT_NE(group, nullptr);
    EXPECT_EQ(group->type(), ConfigNodeType::GROUP);

    const auto* cpu = group->findChild("cpu");
    ASSERT_NE(cpu, nullptr);
    EXPECT_EQ(cpu->type(), ConfigNodeType::CONTROLLER);

    const auto* maxItem = cpu->findChild("cpu.max");
    ASSERT_NE(maxItem, nullptr);
    EXPECT_EQ(maxItem->type(), ConfigNodeType::ITEM);
    EXPECT_EQ(maxItem->value(), "100000 100000");

    const auto* mem = group->findChild("memory");
    ASSERT_NE(mem, nullptr);

    const auto* memMax = mem->findChild("memory.max");
    ASSERT_NE(memMax, nullptr);
    EXPECT_EQ(memMax->value(), "1G");
}

// ═══════════════════════════════════════════════════════════════════════════
// IT-002: Invalid Config → Parser Error
// ═══════════════════════════════════════════════════════════════════════════

TEST(IntegrationTest, IT002_InvalidConfig_ParserError) {
    ConfigRepository repo;

    // Missing group name
    bool ok = repo.loadFromString(
        "group {\n"
        "}\n"
    );

    EXPECT_FALSE(ok);
    EXPECT_FALSE(repo.errors().empty());
}

TEST(IntegrationTest, IT002b_MissingClosingBrace) {
    ConfigRepository repo;

    bool ok = repo.loadFromString(
        "group web {\n"
        "    controller cpu {\n"
        "        item cpu.max = \"100000\";\n"
    );

    EXPECT_FALSE(ok);
    EXPECT_FALSE(repo.errors().empty());
}

TEST(IntegrationTest, IT002c_InvalidItemSyntax) {
    ConfigRepository repo;

    bool ok = repo.loadFromString(
        "group web {\n"
        "    controller cpu {\n"
        "        item cpu.max 100000;\n"
        "    }\n"
        "}\n"
    );

    EXPECT_FALSE(ok);
    EXPECT_FALSE(repo.errors().empty());
}

// ═══════════════════════════════════════════════════════════════════════════
// IT-003: Discovery → RuntimeState Created
// ═══════════════════════════════════════════════════════════════════════════

TEST(IntegrationTest, IT003_Discovery_CreatesRuntimeState) {
    auto mock = std::make_unique<MockIntegrationDiscovery>();
    mock->addProcess(100, "nginx");

    RuntimeStateManager stateManager;
    DiscoveryService service(std::move(mock), stateManager);

    MatchRule rule{"nginx", "exact"};
    auto result = service.discoverSingle(rule, MatchType::Exact, "IT003");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->pid, 100);
    EXPECT_EQ(result->comm, "nginx");

    // RuntimeState was created in the manager
    auto state = stateManager.findByPid(100);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->processState().processName, "nginx");
    EXPECT_EQ(state->processState().discoveryStatus, DiscoveryStatus::Discovered);

    // Event was published
    EXPECT_EQ(service.events().size(), 1u);
    EXPECT_EQ(service.events()[0].type(), EventType::ProcessDiscovered);
}

TEST(IntegrationTest, IT003b_Discovery_NoMatch_NoState) {
    auto mock = std::make_unique<MockIntegrationDiscovery>();
    mock->addProcess(100, "nginx");

    RuntimeStateManager stateManager;
    DiscoveryService service(std::move(mock), stateManager);

    MatchRule rule{"redis", "exact"};
    auto result = service.discoverSingle(rule, MatchType::Exact, "IT003b");

    EXPECT_FALSE(result.has_value());

    // No runtime state should exist
    auto state = stateManager.findByPid(100);
    EXPECT_FALSE(state.has_value());
    EXPECT_TRUE(service.events().empty());
}

// ═══════════════════════════════════════════════════════════════════════════
// IT-004: Attach → PID attached to target group
// ═══════════════════════════════════════════════════════════════════════════

TEST(IntegrationTest, IT004_Attach_PidAttachedToGroup) {
    auto mockDriver = std::make_unique<MockCgroupDriver>();
    auto* driverPtr = mockDriver.get();

    AttachEngine engine(std::move(mockDriver));

    // Build config tree: group → cpu → cpu.max
    auto group = makeGroup("test_group");
    auto* cpuCtrl = group->addChild(makeController("cpu"));
    cpuCtrl->addChild(makeItem("cpu.max", "100000 100000"));
    cpuCtrl->addChild(makeItem("cpu.weight", "100"));

    RuntimeState state;
    state.updatePid(1234, "test_group");

    // Verify initial state
    EXPECT_EQ(state.processState().attachStatus, AttachStatus::None);

    auto err = engine.attach(*group, state);
    EXPECT_FALSE(err.has_value());

    // RuntimeState updated
    EXPECT_EQ(state.processState().attachStatus, AttachStatus::Attached);
    EXPECT_EQ(state.processState().attachedGroupPath, "test_group");

    // Cgroup driver has the values
    EXPECT_TRUE(driverPtr->exists("test_group"));

    auto cpuMax = driverPtr->getValue("test_group", "cpu.max");
    ASSERT_TRUE(cpuMax.has_value());
    EXPECT_EQ(*cpuMax, "100000 100000");

    auto cpuWeight = driverPtr->getValue("test_group", "cpu.weight");
    ASSERT_TRUE(cpuWeight.has_value());
    EXPECT_EQ(*cpuWeight, "100");
}

TEST(IntegrationTest, IT004b_Attach_MultipleControllers) {
    auto mockDriver = std::make_unique<MockCgroupDriver>();
    auto* driverPtr = mockDriver.get();

    AttachEngine engine(std::move(mockDriver));

    auto group = makeGroup("multi_ctrl");
    auto* cpu = group->addChild(makeController("cpu"));
    cpu->addChild(makeItem("cpu.max", "200000 100000"));
    auto* mem = group->addChild(makeController("memory"));
    mem->addChild(makeItem("memory.max", "512M"));
    mem->addChild(makeItem("memory.high", "400M"));

    RuntimeState state;
    state.updatePid(5678, "multi_ctrl");

    auto err = engine.attach(*group, state);
    EXPECT_FALSE(err.has_value());
    EXPECT_EQ(state.processState().attachStatus, AttachStatus::Attached);

    // All values persisted in mock
    EXPECT_EQ(*driverPtr->getValue("multi_ctrl", "cpu.max"), "200000 100000");
    EXPECT_EQ(*driverPtr->getValue("multi_ctrl", "memory.max"), "512M");
    EXPECT_EQ(*driverPtr->getValue("multi_ctrl", "memory.high"), "400M");
}

// ═══════════════════════════════════════════════════════════════════════════
// IT-005: Process Exit → RuntimeState Lost
// ═══════════════════════════════════════════════════════════════════════════

class ProcessExitTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_ = std::make_unique<MockIntegrationDiscovery>();
        mock_->addProcess(100, "test_proc");
        mockPtr_ = mock_.get();

        stateManager_.registerProcess("test_proc", 100);

        discoveryService_ = std::make_unique<DiscoveryService>(std::move(mock_), stateManager_);
        monitor_ = std::make_unique<Monitor>(stateManager_, *discoveryService_,
            std::chrono::milliseconds(50));
    }

    std::unique_ptr<MockIntegrationDiscovery> mock_;
    MockIntegrationDiscovery* mockPtr_;
    RuntimeStateManager stateManager_;
    std::unique_ptr<DiscoveryService> discoveryService_;
    std::unique_ptr<Monitor> monitor_;
};

TEST_F(ProcessExitTest, InitialPollNoEvents) {
    auto events = monitor_->poll();
    EXPECT_TRUE(events.empty());
}

TEST_F(ProcessExitTest, ProcessExitTriggersLost) {
    // Verify initial state
    auto state = stateManager_.findByPid(100);
    ASSERT_TRUE(state.has_value());

    // Simulate process exit
    mockPtr_->removeProcess(100);

    auto events = monitor_->poll();
    EXPECT_FALSE(events.empty());

    bool foundLost = false;
    for (const auto& ev : events) {
        if (ev.type() == EventType::ProcessLost && ev.pid() == 100) {
            foundLost = true;
            break;
        }
    }
    EXPECT_TRUE(foundLost);
}

TEST_F(ProcessExitTest, PIDChangeDetected) {
    mockPtr_->removeProcess(100);
    mockPtr_->addProcess(200, "test_proc");

    auto events = monitor_->poll();
    EXPECT_FALSE(events.empty());

    bool foundRestarted = false;
    for (const auto& ev : events) {
        if (ev.type() == EventType::ProcessRestarted) {
            foundRestarted = true;
            break;
        }
    }
    EXPECT_TRUE(foundRestarted);
}

// ═══════════════════════════════════════════════════════════════════════════
// IT-006: Full Recovery Workflow
//   Discover → Attach → Process Exit → Rediscover → RecoveryManager →
//   AttachEngine → Recovered
// ═══════════════════════════════════════════════════════════════════════════

class RecoveryWorkflowTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Load config with match rule
        configRepo_.loadFromString(
            "group recovery_group {\n"
            "    match \"recovery_proc\" {\n"
            "        type exact;\n"
            "    }\n"
            "    controller cpu {\n"
            "        item cpu.max = \"100000 100000\";\n"
            "    }\n"
            "}\n"
        );

        // Set up discovery with initial process
        auto discMock = std::make_unique<MockIntegrationDiscovery>();
        discMock->addProcess(100, "recovery_proc");
        discMockPtr_ = discMock.get();

        discoveryService_ = std::make_unique<DiscoveryService>(std::move(discMock), stateManager_);

        // Mock driver for cgroup operations
        auto drv = std::make_unique<MockCgroupDriver>();
        driverPtr_ = drv.get();
        attachEngine_ = std::make_unique<AttachEngine>(std::move(drv));

        recoveryManager_ = std::make_unique<RecoveryManager>(
            stateManager_, *discoveryService_, *attachEngine_, configRepo_);
    }

    ConfigRepository configRepo_;
    RuntimeStateManager stateManager_;
    MockIntegrationDiscovery* discMockPtr_;
    std::unique_ptr<DiscoveryService> discoveryService_;
    MockCgroupDriver* driverPtr_;
    std::unique_ptr<AttachEngine> attachEngine_;
    std::unique_ptr<RecoveryManager> recoveryManager_;
};

TEST_F(RecoveryWorkflowTest, IT006_FullRecoveryWorkflow) {
    const auto& root = configRepo_.getRoot().root();
    const auto* groupNode = root.findChild("recovery_group");
    ASSERT_NE(groupNode, nullptr);

    // ── Phase 1: Register process under the group name ────────────────
    stateManager_.registerProcess("recovery_group", 100);
    stateManager_.setProcessDiscoveryStatus("recovery_group", DiscoveryStatus::Discovered);

    auto state = stateManager_.findByPid(100);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->processState().discoveryStatus, DiscoveryStatus::Discovered);

    // ── Phase 2: Attach ────────────────────────────────────────────────
    RuntimeState attachState;
    attachState.updatePid(100, "recovery_group");

    auto attachErr = attachEngine_->attach(*groupNode, attachState);
    EXPECT_FALSE(attachErr.has_value());
    EXPECT_EQ(attachState.processState().attachStatus, AttachStatus::Attached);
    EXPECT_EQ(attachState.processState().attachedGroupPath, "recovery_group");

    // Verify cgroup values in mock driver
    EXPECT_TRUE(driverPtr_->exists("recovery_group"));
    auto val = driverPtr_->getValue("recovery_group", "cpu.max");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "100000 100000");

    // ── Phase 3: Process Exit ──────────────────────────────────────────
    auto lost = stateManager_.markProcessLost("recovery_group", 100);
    EXPECT_TRUE(lost);

    state = stateManager_.findByName("recovery_group");
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->processState().discoveryStatus, DiscoveryStatus::Missing);
    EXPECT_EQ(state->processState().attachStatus, AttachStatus::Pending);
    EXPECT_EQ(state->processState().recoveryStatus, RecoveryState::Detecting);

    // ── Phase 4: Rediscover & Recover ──────────────────────────────────
    discMockPtr_->removeProcess(100);
    discMockPtr_->addProcess(200, "recovery_proc");

    auto recoverErr = recoveryManager_->recoverProcess("recovery_group");
    EXPECT_FALSE(recoverErr.has_value()) << "Recovery failed: "
        << (recoverErr ? recoverErr->message : "");

    // Verify new PID and recovery status
    state = stateManager_.findByPid(200);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->processState().pid, 200);
    EXPECT_EQ(state->processState().recoveryStatus, RecoveryState::Recovered);
    EXPECT_EQ(state->processState().attachedGroupPath, "recovery_group");

    // Verify cgroup values still in place
    EXPECT_TRUE(driverPtr_->exists("recovery_group"));
    auto val2 = driverPtr_->getValue("recovery_group", "cpu.max");
    ASSERT_TRUE(val2.has_value());
    EXPECT_EQ(*val2, "100000 100000");

    // Verify recovery event published
    auto events = recoveryManager_->events();
    bool foundSuccess = false;
    for (const auto& ev : events) {
        if (ev.type() == EventType::RecoverySucceeded) {
            foundSuccess = true;
            break;
        }
    }
    EXPECT_TRUE(foundSuccess);
}

// ═══════════════════════════════════════════════════════════════════════════
// IT-007: Driver Failure → Recovery failure state
// ═══════════════════════════════════════════════════════════════════════════

class DriverFailureTest : public ::testing::Test {
protected:
    void SetUp() override {
        configRepo_.loadFromString(
            "group fail_group {\n"
            "    match \"fail_proc\" {\n"
            "        type exact;\n"
            "    }\n"
            "    controller cpu {\n"
            "        item cpu.max = \"100000 100000\";\n"
            "    }\n"
            "}\n"
        );

        auto discMock = std::make_unique<MockIntegrationDiscovery>();
        discMock->addProcess(100, "fail_proc");
        discMockPtr_ = discMock.get();

        discoveryService_ = std::make_unique<DiscoveryService>(std::move(discMock), stateManager_);

        auto drv = std::make_unique<MockCgroupDriver>();
        driverPtr_ = drv.get();
        attachEngine_ = std::make_unique<AttachEngine>(std::move(drv));

        recoveryManager_ = std::make_unique<RecoveryManager>(
            stateManager_, *discoveryService_, *attachEngine_, configRepo_);
    }

    ConfigRepository configRepo_;
    RuntimeStateManager stateManager_;
    MockIntegrationDiscovery* discMockPtr_;
    std::unique_ptr<DiscoveryService> discoveryService_;
    MockCgroupDriver* driverPtr_;
    std::unique_ptr<AttachEngine> attachEngine_;
    std::unique_ptr<RecoveryManager> recoveryManager_;
};

TEST_F(DriverFailureTest, IT007_DriverFailure_RecoveryFails) {
    ASSERT_TRUE(stateManager_.registerProcess("fail_group", 100));
    stateManager_.setProcessDiscoveryStatus("fail_group", DiscoveryStatus::Discovered);

    const auto& root = configRepo_.getRoot().root();
    const auto* groupNode = root.findChild("fail_group");
    ASSERT_NE(groupNode, nullptr);

    // Attach first so cgroup state exists
    RuntimeState attachState;
    attachState.updatePid(100, "fail_group");
    auto attachErr = attachEngine_->attach(*groupNode, attachState);
    EXPECT_FALSE(attachErr.has_value());

    // Simulate process exit
    discMockPtr_->removeProcess(100);
    ASSERT_TRUE(stateManager_.markProcessLost("fail_group", 100));

    // Inject a non-retryable driver error.
    // ControllerNotSupported causes reattach to abort immediately
    // without internal retries (AttachEngine::reattach checks this code).
    driverPtr_->setNextError(Error(ErrorCode::ControllerNotSupported,
        "simulated driver failure", "IT007"));

    // New process appears with same identity
    discMockPtr_->addProcess(200, "fail_proc");

    // Recovery finds the new process but driver fails immediately
    auto recoverErr = recoveryManager_->recoverProcess("fail_group");
    ASSERT_TRUE(recoverErr.has_value());
    EXPECT_EQ(recoverErr->code, ErrorCode::ControllerNotSupported);

    // RuntimeState reflects failure
    auto state = stateManager_.findByName("fail_group");
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->processState().recoveryStatus, RecoveryState::Failed);

    // RecoveryFailed event published
    auto events = recoveryManager_->events();
    bool foundFailed = false;
    for (const auto& ev : events) {
        if (ev.type() == EventType::RecoveryFailed) {
            foundFailed = true;
            break;
        }
    }
    EXPECT_TRUE(foundFailed);
}

TEST_F(DriverFailureTest, IT007b_DriverFailure_RetriesExhausted) {
    ASSERT_TRUE(stateManager_.registerProcess("fail_group", 100));
    stateManager_.setProcessDiscoveryStatus("fail_group", DiscoveryStatus::Discovered);

    const auto& root = configRepo_.getRoot().root();
    const auto* groupNode = root.findChild("fail_group");
    ASSERT_NE(groupNode, nullptr);

    RuntimeState attachState;
    attachState.updatePid(100, "fail_group");
    attachEngine_->attach(*groupNode, attachState);

    // Simulate process exit and add replacement
    discMockPtr_->removeProcess(100);
    ASSERT_TRUE(stateManager_.markProcessLost("fail_group", 100));
    discMockPtr_->addProcess(200, "fail_proc");

    // recoverProcess internally calls reattach, which retries 3 times.
    // Using ControllerNotSupported (non-retryable) fails immediately.
    // Inject one error per recoverProcess call. Since setNextError
    // overwrites, we inject right before each call.
    for (int i = 0; i < 3; i++) {
        driverPtr_->setNextError(Error(ErrorCode::ControllerNotSupported,
            "retry " + std::to_string(i + 1), "IT007b"));
        auto err = recoveryManager_->recoverProcess("fail_group");
        ASSERT_TRUE(err.has_value()) << "Expected failure at retry " << (i + 1);
    }

    // After 3 consecutive failures, state is Failed
    auto state = stateManager_.findByName("fail_group");
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->processState().recoveryStatus, RecoveryState::Failed);
}
