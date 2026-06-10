#include <gtest/gtest.h>
#include "resource_manager/monitor/runtime_reconciler.h"

using namespace resource_manager;

TEST(RuntimeReconcilerTest, ProcessCreated) {
    RuntimeRepository repo;
    RuntimeReconciler reconciler;

    ProcessChangeSet changes;
    ProcessChange change;
    change.type = ProcessChangeType::ProcessCreated;
    change.pid = 100;
    change.processName = "test_proc";
    changes.changes.push_back(change);

    auto results = reconciler.reconcile(repo, changes);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].action, ReconciliationAction::UpdatePid);
    EXPECT_EQ(results[0].pid, 100);

    auto state = repo.findByPid(100);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->processState().processName, "test_proc");
}

TEST(RuntimeReconcilerTest, ProcessLost) {
    RuntimeRepository repo;
    repo.registerProcess("test_proc", 100);

    RuntimeReconciler reconciler;

    ProcessChangeSet changes;
    ProcessChange change;
    change.type = ProcessChangeType::ProcessLost;
    change.pid = 100;
    change.processName = "test_proc";
    changes.changes.push_back(change);

    auto results = reconciler.reconcile(repo, changes);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].action, ReconciliationAction::MarkLost);

    auto state = repo.findByPid(100);
    EXPECT_FALSE(state.has_value());
}

TEST(RuntimeReconcilerTest, PIDChanged) {
    RuntimeRepository repo;
    repo.registerProcess("test_proc", 100);

    RuntimeReconciler reconciler;

    ProcessChangeSet changes;
    ProcessChange change;
    change.type = ProcessChangeType::PIDChanged;
    change.pid = 200;
    change.oldPid = 100;
    change.processName = "test_proc";
    changes.changes.push_back(change);

    auto results = reconciler.reconcile(repo, changes);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].action, ReconciliationAction::UpdatePid);
    EXPECT_EQ(results[0].pid, 200);
    EXPECT_EQ(results[0].oldPid, 100);

    auto state = repo.findByPid(200);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->processState().pid, 200);
}

TEST(RuntimeReconcilerTest, ThreadChanged) {
    RuntimeRepository repo;
    repo.registerProcess("test_proc", 100);

    RuntimeReconciler reconciler;

    ProcessChangeSet changes;
    ProcessChange change;
    change.type = ProcessChangeType::ThreadChanged;
    change.pid = 100;
    change.processName = "test_proc";
    change.oldThreads = {{101, "old_worker"}};
    change.newThreads = {{102, "new_worker"}};
    changes.changes.push_back(change);

    auto results = reconciler.reconcile(repo, changes);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].action, ReconciliationAction::NoChange);
}

TEST(RuntimeReconcilerTest, NoChangesWithEmptyChanges) {
    RuntimeRepository repo;
    repo.registerProcess("test_proc", 100);

    RuntimeReconciler reconciler;
    ProcessChangeSet changes;

    auto results = reconciler.reconcile(repo, changes);
    EXPECT_TRUE(results.empty());
}

TEST(RuntimeReconcilerTest, MultipleChanges) {
    RuntimeRepository repo;
    repo.registerProcess("proc_a", 100);
    repo.registerProcess("proc_b", 101);

    RuntimeReconciler reconciler;

    ProcessChangeSet changes;
    ProcessChange c1;
    c1.type = ProcessChangeType::ProcessLost;
    c1.pid = 100;
    c1.processName = "proc_a";
    changes.changes.push_back(c1);

    ProcessChange c2;
    c2.type = ProcessChangeType::ProcessCreated;
    c2.pid = 200;
    c2.processName = "proc_c";
    changes.changes.push_back(c2);

    auto results = reconciler.reconcile(repo, changes);
    ASSERT_EQ(results.size(), 2u);

    EXPECT_FALSE(repo.findByPid(100).has_value());
    EXPECT_TRUE(repo.findByPid(200).has_value());
}
