#include <gtest/gtest.h>
#include "resource_manager/state/runtime_repository.h"

using namespace resource_manager;

TEST(RuntimeRepositoryTest, Insert) {
    RuntimeRepository repo;

    EXPECT_TRUE(repo.registerProcess("nginx", 1234));
    EXPECT_TRUE(repo.registerProcess("redis", 5678));

    auto all = repo.getAll();
    EXPECT_EQ(all.size(), 2u);
}

TEST(RuntimeRepositoryTest, Remove) {
    RuntimeRepository repo;

    EXPECT_TRUE(repo.registerProcess("nginx", 1234));
    EXPECT_TRUE(repo.removeProcess(1234));

    auto result = repo.findByPid(1234);
    EXPECT_FALSE(result.has_value());
}

TEST(RuntimeRepositoryTest, Lookup) {
    RuntimeRepository repo;
    repo.registerProcess("nginx", 1234);

    auto byPid = repo.findByPid(1234);
    ASSERT_TRUE(byPid.has_value());
    EXPECT_EQ(byPid->processState().pid, 1234);
    EXPECT_EQ(byPid->processState().processName, "nginx");

    auto byName = repo.findByName("nginx");
    ASSERT_TRUE(byName.has_value());
    EXPECT_EQ(byName->processState().pid, 1234);

    auto notFound = repo.findByPid(9999);
    EXPECT_FALSE(notFound.has_value());

    auto nameNotFound = repo.findByName("unknown");
    EXPECT_FALSE(nameNotFound.has_value());
}

TEST(RuntimeRepositoryTest, DuplicateRegistration) {
    RuntimeRepository repo;

    EXPECT_TRUE(repo.registerProcess("nginx", 1234));
    EXPECT_FALSE(repo.registerProcess("nginx", 1234));
    EXPECT_FALSE(repo.registerProcess("nginx", 5678));
    EXPECT_FALSE(repo.registerProcess("redis", 1234));
}

TEST(RuntimeRepositoryTest, RemoveNonexistent) {
    RuntimeRepository repo;
    EXPECT_FALSE(repo.removeProcess(9999));
}
