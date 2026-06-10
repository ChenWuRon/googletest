#include <gtest/gtest.h>
#include "resource_manager/attach/attach_policy.h"

using namespace resource_manager;

TEST(AttachPolicyTest, AttachAll) {
    AttachPolicy policy(AttachPolicyType::AttachAll);

    EXPECT_TRUE(policy.shouldAttachProcess());
    EXPECT_TRUE(policy.shouldAttachThread({100, "worker"}));
    EXPECT_EQ(policy.type(), AttachPolicyType::AttachAll);
}

TEST(AttachPolicyTest, AttachMainOnly) {
    AttachPolicy policy(AttachPolicyType::AttachMainOnly);

    EXPECT_TRUE(policy.shouldAttachProcess());
    EXPECT_FALSE(policy.shouldAttachThread({100, "worker"}));
}

TEST(AttachPolicyTest, AttachThreadsOnly) {
    AttachPolicy policy(AttachPolicyType::AttachThreadsOnly);

    EXPECT_FALSE(policy.shouldAttachProcess());
    EXPECT_TRUE(policy.shouldAttachThread({101, "worker-1"}));
}

TEST(AttachPolicyTest, FilterThreads) {
    AttachPolicy policy(AttachPolicyType::AttachMainOnly);

    std::vector<ThreadState> threads = {
        {100, "main"},
        {101, "worker-1"},
        {102, "worker-2"}
    };

    auto filtered = policy.filterThreads(threads);
    EXPECT_TRUE(filtered.empty());
}

TEST(AttachPolicyTest, FilterThreadsAll) {
    AttachPolicy policy(AttachPolicyType::AttachAll);

    std::vector<ThreadState> threads = {
        {100, "main"},
        {101, "worker-1"}
    };

    auto filtered = policy.filterThreads(threads);
    EXPECT_EQ(filtered.size(), 2u);
}

TEST(AttachPolicyTest, CustomSelection) {
    AttachPolicy policy(AttachPolicyType::CustomSelection);

    policy.setCustomFilter([](const ThreadState& t) {
        return t.threadName.find("worker") != std::string::npos;
    });

    EXPECT_FALSE(policy.shouldAttachThread({100, "main"}));
    EXPECT_TRUE(policy.shouldAttachThread({101, "worker-1"}));
}

TEST(AttachPolicyTest, PolicyType) {
    EXPECT_EQ(AttachPolicy(AttachPolicyType::AttachAll).type(), AttachPolicyType::AttachAll);
    EXPECT_EQ(AttachPolicy(AttachPolicyType::AttachMainOnly).type(), AttachPolicyType::AttachMainOnly);
    EXPECT_EQ(AttachPolicy(AttachPolicyType::AttachThreadsOnly).type(), AttachPolicyType::AttachThreadsOnly);
    EXPECT_EQ(AttachPolicy(AttachPolicyType::CustomSelection).type(), AttachPolicyType::CustomSelection);
}
