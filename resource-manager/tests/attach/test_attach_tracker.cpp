#include <gtest/gtest.h>
#include "resource_manager/attach/attach_tracker.h"

using namespace resource_manager;

TEST(AttachTrackerTest, RegisterAttach) {
    AttachTracker tracker;
    tracker.registerAttach(1234, 1234, "web");

    EXPECT_TRUE(tracker.isAttached(1234));
    EXPECT_TRUE(tracker.isThreadAttached(1234, 1234));

    auto record = tracker.queryAttach(1234, 1234);
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->pid, 1234);
    EXPECT_EQ(record->tid, 1234);
    EXPECT_EQ(record->groupPath, "web");
    EXPECT_EQ(record->status, AttachStatus::Attached);
}

TEST(AttachTrackerTest, RemoveAttach) {
    AttachTracker tracker;
    tracker.registerAttach(1234, 1234, "web");
    EXPECT_EQ(tracker.size(), 1u);

    tracker.removeAttach(1234, 1234);
    EXPECT_EQ(tracker.size(), 0u);
}

TEST(AttachTrackerTest, QueryAttach) {
    AttachTracker tracker;
    tracker.registerAttach(1234, 1234, "web");
    tracker.registerAttach(1234, 100, "web");

    auto byPid = tracker.queryByPid(1234);
    EXPECT_EQ(byPid.size(), 2u);

    auto byGroup = tracker.queryByGroup("web");
    EXPECT_EQ(byGroup.size(), 2u);

    auto notFound = tracker.queryAttach(9999, 9999);
    EXPECT_FALSE(notFound.has_value());
}

TEST(AttachTrackerTest, AttachStateConsistency) {
    AttachTracker tracker;

    EXPECT_FALSE(tracker.isAttached(1234));

    tracker.registerAttach(1234, 1234, "web");
    EXPECT_TRUE(tracker.isAttached(1234));

    tracker.markFailed(1234, 1234);
    EXPECT_FALSE(tracker.isAttached(1234));

    auto record = tracker.queryAttach(1234, 1234);
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->status, AttachStatus::Failed);
}

TEST(AttachTrackerTest, MultipleThreads) {
    AttachTracker tracker;

    tracker.registerAttach(1234, 1234, "web");
    tracker.registerAttach(1234, 100, "web");
    tracker.registerAttach(1234, 101, "web");

    EXPECT_TRUE(tracker.isThreadAttached(1234, 100));
    EXPECT_TRUE(tracker.isThreadAttached(1234, 101));
    EXPECT_EQ(tracker.size(), 3u);

    tracker.removeAttach(1234, 100);
    EXPECT_EQ(tracker.size(), 2u);
    EXPECT_FALSE(tracker.isThreadAttached(1234, 100));
}

TEST(AttachTrackerTest, UpdateExisting) {
    AttachTracker tracker;

    tracker.registerAttach(1234, 1234, "web");
    tracker.registerAttach(1234, 1234, "db");

    auto record = tracker.queryAttach(1234, 1234);
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->groupPath, "db");
    EXPECT_EQ(tracker.size(), 1u);
}
