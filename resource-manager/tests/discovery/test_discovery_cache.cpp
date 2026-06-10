#include <gtest/gtest.h>
#include <thread>

#include "resource_manager/discovery/discovery_cache.h"

using namespace resource_manager;

TEST(DiscoveryCacheTest, CacheHit) {
    DiscoveryCache cache(std::chrono::seconds(60));
    cache.store(1234, "nginx");

    auto result = cache.lookup(1234);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->pid, 1234);
    EXPECT_EQ(result->name, "nginx");
}

TEST(DiscoveryCacheTest, CacheMiss) {
    DiscoveryCache cache;
    auto result = cache.lookup(9999);
    EXPECT_FALSE(result.has_value());
}

TEST(DiscoveryCacheTest, Expiration) {
    DiscoveryCache cache(std::chrono::milliseconds(10));
    cache.store(1234, "nginx");

    auto result = cache.lookup(1234);
    EXPECT_TRUE(result.has_value());

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    result = cache.lookup(1234);
    EXPECT_FALSE(result.has_value());
}

TEST(DiscoveryCacheTest, Refresh) {
    DiscoveryCache cache(std::chrono::milliseconds(100));
    cache.store(1234, "nginx");

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    cache.refresh(1234);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    auto result = cache.lookup(1234);
    EXPECT_TRUE(result.has_value());
}

TEST(DiscoveryCacheTest, Eviction) {
    DiscoveryCache cache;
    cache.store(1234, "nginx");
    EXPECT_EQ(cache.size(), 1u);

    cache.evict(1234);
    EXPECT_EQ(cache.size(), 0u);
    EXPECT_FALSE(cache.lookup(1234).has_value());
}

TEST(DiscoveryCacheTest, Clear) {
    DiscoveryCache cache;
    cache.store(1234, "nginx");
    cache.store(5678, "redis");
    EXPECT_EQ(cache.size(), 2u);

    cache.clear();
    EXPECT_EQ(cache.size(), 0u);
}

TEST(DiscoveryCacheTest, RefreshNonexistent) {
    DiscoveryCache cache;
    cache.refresh(9999);
    EXPECT_EQ(cache.size(), 0u);
}
