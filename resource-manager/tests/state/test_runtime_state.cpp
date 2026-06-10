#include <gtest/gtest.h>
#include "resource_manager/state/runtime_state.h"

using namespace resource_manager;

TEST(RuntimeStateTest, ProcessCreation) {
    RuntimeState state;
    EXPECT_EQ(state.processState().pid, 0);
    EXPECT_EQ(state.processState().processName, "");
    EXPECT_FALSE(state.processState().attached);
    EXPECT_EQ(state.processState().discoveryStatus, DiscoveryStatus::Unknown);
    EXPECT_EQ(state.processState().recoveryStatus, RecoveryState::None);
    EXPECT_EQ(state.processState().retryCount, 0);
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
    EXPECT_FALSE(state.processState().attached);

    state.markAttached();
    EXPECT_TRUE(state.processState().attached);

    state.markDetached();
    EXPECT_FALSE(state.processState().attached);
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
