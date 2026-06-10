#include <gtest/gtest.h>
#include "resource_manager/discovery/discovery_rules.h"

using namespace resource_manager;

TEST(DiscoveryRulesTest, ExactMatch) {
    EXPECT_TRUE(DiscoveryRules::matchExact("nginx", "nginx"));
    EXPECT_FALSE(DiscoveryRules::matchExact("nginx", "nginx-worker"));
    EXPECT_FALSE(DiscoveryRules::matchExact("nginx", "Nginx"));
    EXPECT_FALSE(DiscoveryRules::matchExact("nginx", "mynginx"));
}

TEST(DiscoveryRulesTest, PrefixMatch) {
    EXPECT_TRUE(DiscoveryRules::matchPrefix("nginx", "nginx"));
    EXPECT_TRUE(DiscoveryRules::matchPrefix("nginx", "nginx-worker"));
    EXPECT_FALSE(DiscoveryRules::matchPrefix("nginx", "mynginx"));
}

TEST(DiscoveryRulesTest, WildcardMatch) {
    EXPECT_TRUE(DiscoveryRules::matchWildcard("nginx-*", "nginx-master"));
    EXPECT_TRUE(DiscoveryRules::matchWildcard("nginx-*", "nginx-worker-1"));
    EXPECT_FALSE(DiscoveryRules::matchWildcard("nginx-*", "nginx"));
    EXPECT_TRUE(DiscoveryRules::matchWildcard("python?script", "python1script"));
    EXPECT_FALSE(DiscoveryRules::matchWildcard("python?script", "python12script"));
    EXPECT_TRUE(DiscoveryRules::matchWildcard("*", "anything"));
}

TEST(DiscoveryRulesTest, MatchDispatch) {
    EXPECT_TRUE(DiscoveryRules::match("nginx", MatchType::Exact, "nginx"));
    EXPECT_TRUE(DiscoveryRules::match("nginx", MatchType::Prefix, "nginx-worker"));
    EXPECT_TRUE(DiscoveryRules::match("nginx-*", MatchType::Wildcard, "nginx-master"));
    EXPECT_FALSE(DiscoveryRules::match("nginx", MatchType::Exact, "redis"));
}

TEST(DiscoveryRulesTest, ParseType) {
    EXPECT_EQ(DiscoveryRules::parseType("exact"), MatchType::Exact);
    EXPECT_EQ(DiscoveryRules::parseType("Exact"), MatchType::Exact);
    EXPECT_EQ(DiscoveryRules::parseType("prefix"), MatchType::Prefix);
    EXPECT_EQ(DiscoveryRules::parseType("PREFIX"), MatchType::Prefix);
    EXPECT_EQ(DiscoveryRules::parseType("wildcard"), MatchType::Wildcard);
    EXPECT_EQ(DiscoveryRules::parseType("unknown"), MatchType::Exact);
}

TEST(DiscoveryRulesTest, WildcardPatterns) {
    EXPECT_TRUE(DiscoveryRules::matchWildcard("", ""));
    EXPECT_TRUE(DiscoveryRules::matchWildcard("a", "a"));
    EXPECT_FALSE(DiscoveryRules::matchWildcard("a", "b"));
    EXPECT_TRUE(DiscoveryRules::matchWildcard("a*", "abc"));
    EXPECT_TRUE(DiscoveryRules::matchWildcard("*c", "abc"));
    EXPECT_TRUE(DiscoveryRules::matchWildcard("a*c", "abc"));
    EXPECT_TRUE(DiscoveryRules::matchWildcard("a?c", "abc"));
    EXPECT_FALSE(DiscoveryRules::matchWildcard("a?c", "ab"));
}
