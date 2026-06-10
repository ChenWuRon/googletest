#include <gtest/gtest.h>
#include <memory>

#include "resource_manager/discovery/discovery_service.h"
#include "resource_manager/discovery/discovery_rules.h"
#include "resource_manager/state/runtime_repository.h"

using namespace resource_manager;

namespace {

class MockDiscovery : public IProcessDiscovery {
public:
    std::vector<ProcessInfo> procs;

    std::optional<ProcessInfo> findProcess(const MatchRule& rule) override {
        for (const auto& p : procs) {
            if (DiscoveryRules::match(rule.pattern, MatchType::Exact, p.comm)) {
                return p;
            }
        }
        return std::nullopt;
    }

    std::optional<std::vector<ProcessInfo>> findProcesses(const MatchRule& rule) override {
        std::vector<ProcessInfo> matched;
        for (const auto& p : procs) {
            if (DiscoveryRules::match(rule.pattern, MatchType::Exact, p.comm)) {
                matched.push_back(p);
            }
        }
        if (matched.empty()) return std::nullopt;
        return matched;
    }

    std::optional<std::vector<ThreadInfo>> findThreads(int) override {
        return std::nullopt;
    }

    bool exists(int pid) override {
        for (const auto& p : procs) {
            if (p.pid == pid) return true;
        }
        return false;
    }
};

} // namespace

TEST(DiscoveryServiceTest, DiscoverSingle) {
    auto mock = std::make_unique<MockDiscovery>();
    mock->procs.push_back({1234, "nginx", "", "", {}});

    RuntimeRepository repo;
    DiscoveryService service(std::move(mock), repo);

    MatchRule rule{"nginx", "exact"};
    auto result = service.discoverSingle(rule, MatchType::Exact, "test");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->pid, 1234);
    EXPECT_EQ(result->comm, "nginx");

    auto found = repo.findByPid(1234);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->processState().processName, "nginx");

    EXPECT_EQ(service.events().size(), 1u);
    EXPECT_EQ(service.events()[0].type(), EventType::ProcessDiscovered);
}

TEST(DiscoveryServiceTest, DiscoverSingleNoMatch) {
    auto mock = std::make_unique<MockDiscovery>();
    RuntimeRepository repo;
    DiscoveryService service(std::move(mock), repo);

    MatchRule rule{"nonexistent", "exact"};
    auto result = service.discoverSingle(rule, MatchType::Exact, "test");

    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(service.events().empty());
}

TEST(DiscoveryServiceTest, DiscoverAll) {
    auto mock = std::make_unique<MockDiscovery>();
    mock->procs.push_back({1234, "nginx", "", "", {}});
    mock->procs.push_back({5678, "redis", "", "", {}});

    RuntimeRepository repo;
    DiscoveryService service(std::move(mock), repo);

    MatchRule rule{"nginx", "exact"};
    auto results = service.discoverAll(rule, MatchType::Exact, "test");

    EXPECT_EQ(results.size(), 1u);
    EXPECT_EQ(service.events().size(), 1u);
}

TEST(DiscoveryServiceTest, DiscoverAllEmpty) {
    auto mock = std::make_unique<MockDiscovery>();
    RuntimeRepository repo;
    DiscoveryService service(std::move(mock), repo);

    MatchRule rule{"nginx", "exact"};
    auto results = service.discoverAll(rule, MatchType::Exact, "test");

    EXPECT_TRUE(results.empty());
    EXPECT_TRUE(service.events().empty());
}
