#include <gtest/gtest.h>
#include "resource_manager/state/runtime_state.h"

using namespace resource_manager;

TEST(RuntimeStateTest, ProcessCreation) {
    RuntimeState state;
    EXPECT_EQ(state.processState().pid, 0);
    EXPECT_EQ(state.processState().processName, "");
    EXPECT_EQ(state.processState().attachStatus, AttachStatus::None);
    EXPECT_EQ(state.processState().discoveryStatus, DiscoveryStatus::Unknown);
    EXPECT_EQ(state.processState().recoveryStatus, RecoveryState::None);
    EXPECT_EQ(state.processState().retryCount, 0);
    EXPECT_EQ(state.processState().lastSeenPid, 0);
    EXPECT_TRUE(state.threads().empty());
}

TEST(RuntimeStateTest, PidUpdate) {
    RuntimeState state;
    state.updatePid(1234, "nginx");
    EXPECT_EQ(state.processState().pid, 1234);
    EXPECT_EQ(state.processState().processName, "nginx");
}

TEST(RuntimeStateTest, ThreadTracking) {
    RuntimeState state;

    ThreadState t1{100, "worker-1"};
    ThreadState t2{101, "worker-2"};

    state.addThread(t1);
    state.addThread(t2);
    EXPECT_EQ(state.threads().size(), 2u);

    auto found = state.findThread(100);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->tid, 100);
    EXPECT_EQ(found->threadName, "worker-1");

    EXPECT_FALSE(state.findThread(999).has_value());

    state.removeThread(100);
    EXPECT_EQ(state.threads().size(), 1u);
    EXPECT_FALSE(state.findThread(100).has_value());
}

TEST(RuntimeStateTest, AttachStateChanges) {
    RuntimeState state;
    EXPECT_EQ(state.processState().attachStatus, AttachStatus::None);
    EXPECT_TRUE(state.processState().attachedGroupPath.empty());

    state.markAttached("web");
    EXPECT_EQ(state.processState().attachStatus, AttachStatus::Attached);
    EXPECT_EQ(state.processState().attachedGroupPath, "web");

    state.markDetached();
    EXPECT_EQ(state.processState().attachStatus, AttachStatus::Detached);
}

TEST(RuntimeStateTest, SetAttachStatus) {
    RuntimeState state;
    EXPECT_EQ(state.processState().attachStatus, AttachStatus::None);

    state.setAttachStatus(AttachStatus::Pending);
    EXPECT_EQ(state.processState().attachStatus, AttachStatus::Pending);

    state.setAttachStatus(AttachStatus::Failed);
    EXPECT_EQ(state.processState().attachStatus, AttachStatus::Failed);
}

TEST(RuntimeStateTest, UpdateLastSeen) {
    RuntimeState state;
    state.updatePid(100, "proc");
    EXPECT_EQ(state.processState().lastSeenPid, 0);

    state.updateLastSeen(200);
    EXPECT_EQ(state.processState().pid, 200);
    EXPECT_EQ(state.processState().lastSeenPid, 100);
}

TEST(RuntimeStateTest, RecoveryStateChanges) {
    RuntimeState state;
    EXPECT_EQ(state.processState().recoveryStatus, RecoveryState::None);
    EXPECT_EQ(state.processState().retryCount, 0);

    state.processState().recoveryStatus = RecoveryState::Detecting;
    state.processState().retryCount = 1;

    EXPECT_EQ(state.processState().recoveryStatus, RecoveryState::Detecting);
    EXPECT_EQ(state.processState().retryCount, 1);
}
