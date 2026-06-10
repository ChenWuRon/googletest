#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>

#include "resource_manager/monitor/monitor.h"
#include "resource_manager/discovery/discovery_cache.h"

using namespace resource_manager;

class MockMonitorDiscovery : public IProcessDiscovery {
public:
    explicit MockMonitorDiscovery(std::vector<int> activePids)
        : activePids_(std::move(activePids)) {}

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

    std::optional<std::vector<ThreadInfo>> findThreads(int pid) override {
        return std::vector<ThreadInfo>{};
    }

    bool exists(int pid) override {
        return std::find(activePids_.begin(), activePids_.end(), pid) != activePids_.end();
    }

    void addProcess(int pid, const std::string& name) {
        processes_.push_back({pid, name, name, "", {}});
        if (std::find(activePids_.begin(), activePids_.end(), pid) == activePids_.end()) {
            activePids_.push_back(pid);
        }
    }

    void removeProcess(int pid) {
        activePids_.erase(
            std::remove(activePids_.begin(), activePids_.end(), pid),
            activePids_.end());
        processes_.erase(
            std::remove_if(processes_.begin(), processes_.end(),
                [pid](const ProcessInfo& p) { return p.pid == pid; }),
            processes_.end());
    }

private:
    std::vector<int> activePids_;
    std::vector<ProcessInfo> processes_;
};

class MonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto mock = std::make_unique<MockMonitorDiscovery>(
            std::vector<int>{100, 101});
        mockPtr_ = mock.get();
        mockPtr_->addProcess(100, "proc_a");
        mockPtr_->addProcess(101, "proc_b");

        stateManager_.registerProcess("proc_a", 100);
        stateManager_.registerProcess("proc_b", 101);

        discoveryService_ = std::make_unique<DiscoveryService>(std::move(mock), stateManager_);
    }

    RuntimeStateManager stateManager_;
    MockMonitorDiscovery* mockPtr_;
    std::unique_ptr<DiscoveryService> discoveryService_;
};

TEST_F(MonitorTest, PollNoChanges) {
    Monitor monitor(stateManager_, *discoveryService_, std::chrono::milliseconds(100));

    auto events = monitor.poll();
    EXPECT_TRUE(events.empty());
}

TEST_F(MonitorTest, PollProcessLost) {
    Monitor monitor(stateManager_, *discoveryService_, std::chrono::milliseconds(100));

    mockPtr_->removeProcess(100);

    auto events = monitor.poll();
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

TEST_F(MonitorTest, PollPIDChanged) {
    Monitor monitor(stateManager_, *discoveryService_, std::chrono::milliseconds(100));

    mockPtr_->removeProcess(100);
    mockPtr_->addProcess(200, "proc_a");

    auto events = monitor.poll();
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

TEST_F(MonitorTest, StartStop) {
    Monitor monitor(stateManager_, *discoveryService_, std::chrono::milliseconds(50));

    EXPECT_FALSE(monitor.isRunning());
    monitor.start();
    EXPECT_TRUE(monitor.isRunning());

    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    monitor.stop();
    EXPECT_FALSE(monitor.isRunning());
}

TEST_F(MonitorTest, EventsAccumulate) {
    Monitor monitor(stateManager_, *discoveryService_, std::chrono::milliseconds(100));

    mockPtr_->removeProcess(100);
    monitor.poll();

    auto events = monitor.events();
    EXPECT_FALSE(events.empty());
}

TEST_F(MonitorTest, PollWithAllProcessesAlive) {
    Monitor monitor(stateManager_, *discoveryService_, std::chrono::milliseconds(100));

    mockPtr_->addProcess(100, "proc_a");
    mockPtr_->addProcess(101, "proc_b");

    auto events = monitor.poll();
    EXPECT_TRUE(events.empty());
}
