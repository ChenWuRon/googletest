#include <gtest/gtest.h>
#include "resource_manager/monitor/process_watcher.h"

using namespace resource_manager;

TEST(ProcessWatcherTest, DetectCreated) {
    ProcessWatcher watcher;
    RuntimeStateManager stateManager;
    stateManager.registerProcess("test_proc", 100);

    std::vector<ProcessInfo> discovered;
    discovered.push_back({101, "test_proc", "", "", {}});
    discovered.push_back({102, "new_proc", "", "", {}});

    auto changes = watcher.detectDiscoveryChanges(discovered, stateManager);
    EXPECT_FALSE(changes.empty());

    bool foundCreated = false;
    for (const auto& c : changes.changes) {
        if (c.type == ProcessChangeType::ProcessCreated && c.processName == "new_proc") {
            foundCreated = true;
            break;
        }
    }
    EXPECT_TRUE(foundCreated);
}

TEST(ProcessWatcherTest, DetectLost) {
    ProcessWatcher watcher;
    RuntimeStateManager stateManager;
    stateManager.registerProcess("active_proc", 100);
    stateManager.registerProcess("lost_proc", 101);

    std::vector<ProcessInfo> discovered;
    discovered.push_back({100, "active_proc", "", "", {}});

    auto changes = watcher.detectDiscoveryChanges(discovered, stateManager);
    EXPECT_FALSE(changes.empty());

    bool foundLost = false;
    for (const auto& c : changes.changes) {
        if (c.type == ProcessChangeType::ProcessLost && c.processName == "lost_proc") {
            foundLost = true;
            break;
        }
    }
    EXPECT_TRUE(foundLost);
}

TEST(ProcessWatcherTest, DetectPIDChanged) {
    ProcessWatcher watcher;
    RuntimeStateManager stateManager;
    stateManager.registerProcess("test_proc", 100);

    std::vector<ProcessInfo> discovered;
    discovered.push_back({200, "test_proc", "", "", {}});

    auto changes = watcher.detectDiscoveryChanges(discovered, stateManager);
    EXPECT_FALSE(changes.empty());

    bool foundPidChange = false;
    for (const auto& c : changes.changes) {
        if (c.type == ProcessChangeType::PIDChanged &&
            c.processName == "test_proc" &&
            c.oldPid == 100 && c.pid == 200)
        {
            foundPidChange = true;
            break;
        }
    }
    EXPECT_TRUE(foundPidChange);
}

TEST(ProcessWatcherTest, NoChanges) {
    ProcessWatcher watcher;
    RuntimeStateManager stateManager;
    stateManager.registerProcess("test_proc", 100);

    std::vector<ProcessInfo> discovered;
    discovered.push_back({100, "test_proc", "", "", {}});

    auto changes = watcher.detectDiscoveryChanges(discovered, stateManager);
    EXPECT_TRUE(changes.empty());
}

TEST(ProcessWatcherTest, EmptyResult) {
    ProcessChangeSet cs;
    EXPECT_TRUE(cs.empty());
    EXPECT_EQ(cs.size(), 0u);
    cs.clear();
    EXPECT_TRUE(cs.empty());
}

TEST(ProcessWatcherTest, SnapshotChangesProcessCreated) {
    ProcessWatcher watcher;

    RuntimeState oldState;
    auto oldSnap = RuntimeSnapshot::capture(oldState);

    RuntimeState newState;
    newState.updatePid(100, "test_proc");
    auto newSnap = RuntimeSnapshot::capture(newState);

    auto changes = watcher.detectSnapshotChanges(oldSnap, newSnap);
    EXPECT_FALSE(changes.empty());

    bool foundCreated = false;
    for (const auto& c : changes.changes) {
        if (c.type == ProcessChangeType::ProcessCreated && c.pid == 100) {
            foundCreated = true;
            break;
        }
    }
    EXPECT_TRUE(foundCreated);
}

TEST(ProcessWatcherTest, SnapshotChangesPIDChanged) {
    ProcessWatcher watcher;

    RuntimeState oldState;
    oldState.updatePid(100, "test_proc");
    auto oldSnap = RuntimeSnapshot::capture(oldState);

    RuntimeState newState;
    newState.updatePid(200, "test_proc");
    auto newSnap = RuntimeSnapshot::capture(newState);

    auto changes = watcher.detectSnapshotChanges(oldSnap, newSnap);
    EXPECT_FALSE(changes.empty());

    bool foundPidChange = false;
    for (const auto& c : changes.changes) {
        if (c.type == ProcessChangeType::PIDChanged &&
            c.oldPid == 100 && c.pid == 200)
        {
            foundPidChange = true;
            break;
        }
    }
    EXPECT_TRUE(foundPidChange);
}

TEST(ProcessWatcherTest, SnapshotChangesThreadChanged) {
    ProcessWatcher watcher;

    RuntimeState oldState;
    oldState.updatePid(100, "test_proc");
    oldState.addThread({101, "worker-1"});
    oldState.addThread({102, "worker-2"});
    auto oldSnap = RuntimeSnapshot::capture(oldState);

    RuntimeState newState;
    newState.updatePid(100, "test_proc");
    newState.addThread({101, "worker-1"});
    newState.addThread({103, "worker-3"});
    auto newSnap = RuntimeSnapshot::capture(newState);

    auto changes = watcher.detectSnapshotChanges(oldSnap, newSnap);
    EXPECT_FALSE(changes.empty());

    bool foundThreadChange = false;
    for (const auto& c : changes.changes) {
        if (c.type == ProcessChangeType::ThreadChanged) {
            foundThreadChange = true;
            break;
        }
    }
    EXPECT_TRUE(foundThreadChange);
}

TEST(ProcessWatcherTest, SnapshotChangesNoChange) {
    ProcessWatcher watcher;

    RuntimeState state;
    state.updatePid(100, "test_proc");
    state.addThread({101, "worker"});
    auto snap1 = RuntimeSnapshot::capture(state);
    auto snap2 = RuntimeSnapshot::capture(state);

    auto changes = watcher.detectSnapshotChanges(snap1, snap2);
    EXPECT_TRUE(changes.empty());
}
