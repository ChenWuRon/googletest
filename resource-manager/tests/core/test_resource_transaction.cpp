#include <gtest/gtest.h>
#include "resource_manager/core/resource_transaction.h"
#include "resource_manager/driver/mock_cgroup_driver.h"

using namespace resource_manager;

TEST(ResourceTransactionTest, SuccessfulCommit) {
    MockCgroupDriver driver;
    driver.createGroup("web");
    driver.setValue("web", "cpu.max", "50000 100000");

    ResourceTransaction txn(driver);

    auto err = txn.begin();
    EXPECT_FALSE(err.has_value());

    err = txn.apply("web", "cpu.max", "100000 100000");
    EXPECT_FALSE(err.has_value());

    err = txn.apply("web", "memory.max", "1G");
    EXPECT_FALSE(err.has_value());

    err = txn.commit();
    EXPECT_FALSE(err.has_value());

    auto val = driver.getValue("web", "cpu.max");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "100000 100000");

    auto mem = driver.getValue("web", "memory.max");
    ASSERT_TRUE(mem.has_value());
    EXPECT_EQ(*mem, "1G");
}

TEST(ResourceTransactionTest, Rollback) {
    MockCgroupDriver driver;
    driver.createGroup("web");
    driver.setValue("web", "cpu.max", "50000 100000");
    driver.setValue("web", "memory.max", "500M");

    ResourceTransaction txn(driver);

    auto err = txn.begin();
    EXPECT_FALSE(err.has_value());

    err = txn.apply("web", "cpu.max", "100000 100000");
    EXPECT_FALSE(err.has_value());

    err = txn.apply("web", "memory.max", "1G");
    EXPECT_FALSE(err.has_value());

    err = txn.rollback();
    EXPECT_FALSE(err.has_value());

    auto cpu = driver.getValue("web", "cpu.max");
    ASSERT_TRUE(cpu.has_value());
    EXPECT_EQ(*cpu, "50000 100000");

    auto mem = driver.getValue("web", "memory.max");
    ASSERT_TRUE(mem.has_value());
    EXPECT_EQ(*mem, "500M");
}

TEST(ResourceTransactionTest, PartialFailure) {
    MockCgroupDriver driver;
    driver.createGroup("web");

    ResourceTransaction txn(driver);
    txn.begin();

    auto err = txn.apply("web", "cpu.max", "100000");
    EXPECT_FALSE(err.has_value());

    driver.setNextError(Error(ErrorCode::CgroupWriteFailed, "injected", "test"));
    err = txn.apply("web", "memory.max", "1G");
    ASSERT_TRUE(err.has_value());

    err = txn.rollback();
    EXPECT_FALSE(err.has_value());

    auto cpu = driver.getValue("web", "cpu.max");
    ASSERT_TRUE(cpu.has_value());
    EXPECT_EQ(*cpu, "");
}

TEST(ResourceTransactionTest, TransactionNotStarted) {
    MockCgroupDriver driver;

    ResourceTransaction txn(driver);
    EXPECT_FALSE(txn.isActive());

    auto err = txn.apply("web", "cpu.max", "100000");
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->code, ErrorCode::InternalError);

    err = txn.commit();
    ASSERT_TRUE(err.has_value());

    err = txn.rollback();
    ASSERT_TRUE(err.has_value());
}

TEST(ResourceTransactionTest, BeginClearsPrevious) {
    MockCgroupDriver driver;
    driver.createGroup("web");
    driver.setValue("web", "cpu.max", "old");

    ResourceTransaction txn(driver);
    txn.begin();
    txn.apply("web", "cpu.max", "new");
    txn.commit();

    txn.begin();
    txn.apply("web", "cpu.max", "newer");
    txn.commit();

    auto val = driver.getValue("web", "cpu.max");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "newer");
}
