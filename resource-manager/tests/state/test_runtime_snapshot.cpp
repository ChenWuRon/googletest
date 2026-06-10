#include <gtest/gtest.h>
#include "resource_manager/state/runtime_snapshot.h"

using namespace resource_manager;

TEST(RuntimeSnapshotTest, SnapshotConsistency) {
    RuntimeState state;
    state.updatePid(1234, "nginx");
    state.markAttached();

    ThreadState t1{100, "worker-1"};
    ThreadState t2{101, "worker-2"};
    state.addThread(t1);
    state.addThread(t2);

    auto snapshot = RuntimeSnapshot::capture(state);

    EXPECT_EQ(snapshot.pid(), 1234);
    EXPECT_EQ(snapshot.processName(), "nginx");
    EXPECT_TRUE(snapshot.attached());
    EXPECT_EQ(snapshot.threads().size(), 2u);
}

TEST(RuntimeSnapshotTest, SnapshotComparison) {
    RuntimeState state;
    state.updatePid(1234, "nginx");
    state.processState().recoveryStatus = RecoveryState::None;

    auto snap1 = RuntimeSnapshot::capture(state);

    state.updatePid(5678, "nginx");
    auto snap2 = RuntimeSnapshot::capture(state);

    auto diff = snap1.compare(snap2);
    EXPECT_TRUE(diff.pidChanged);
    EXPECT_FALSE(diff.processNameChanged);
    EXPECT_TRUE(diff.anyChanged());

    auto snap3 = RuntimeSnapshot::capture(state);
    auto noDiff = snap2.compare(snap3);
    EXPECT_FALSE(noDiff.anyChanged());
}

TEST(RuntimeSnapshotTest, SnapshotSerialization) {
    RuntimeState state;
    state.updatePid(1234, "nginx");
    state.markAttached();

    auto snapshot = RuntimeSnapshot::capture(state);
    auto serialized = snapshot.serialize();

    EXPECT_NE(serialized.find("RuntimeSnapshot"), std::string::npos);
    EXPECT_NE(serialized.find("pid=1234"), std::string::npos);
    EXPECT_NE(serialized.find("processName=nginx"), std::string::npos);
    EXPECT_NE(serialized.find("attached=true"), std::string::npos);
    EXPECT_NE(serialized.find("retryCount=0"), std::string::npos);
    EXPECT_NE(serialized.find("threads=[]"), std::string::npos);
}
