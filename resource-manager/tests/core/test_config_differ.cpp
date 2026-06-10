#include <gtest/gtest.h>
#include "resource_manager/core/config_differ.h"

using namespace resource_manager;

// ── Helpers ─────────────────────────────────────────────────────────────

ConfigDomain make_domain(const std::initializer_list<
    std::tuple<std::string, std::string, std::string, std::string>> items) {
    ConfigDomain domain;
    for (const auto& [group, ctrl, item, val] : items) {
        auto* g = domain.root().findChild(group);
        if (!g) {
            g = domain.root().addChild(
                std::make_unique<ConfigNode>(ConfigNodeType::GROUP, group));
        }
        auto* c = g->findChild(ctrl);
        if (!c) {
            c = g->addChild(
                std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, ctrl));
        }
        c->addChild(
            std::make_unique<ConfigNode>(ConfigNodeType::ITEM, item, val));
    }
    return domain;
}

// ── Identical ───────────────────────────────────────────────────────────

TEST(ConfigDifferTest, IdenticalDomains) {
    auto d = make_domain({
        {"app", "cpu", "cpu.max", "100"},
        {"app", "cpu", "cpu.weight", "50"},
    });
    auto result = ConfigDiffer::diff(d, d);
    EXPECT_FALSE(result.has_changes());
    EXPECT_TRUE(result.added.empty());
    EXPECT_TRUE(result.removed.empty());
    EXPECT_TRUE(result.modified.empty());
}

TEST(ConfigDifferTest, BothEmpty) {
    ConfigDomain a, b;
    auto result = ConfigDiffer::diff(a, b);
    EXPECT_FALSE(result.has_changes());
}

// ── Added ───────────────────────────────────────────────────────────────

TEST(ConfigDifferTest, AddedGroup) {
    auto old_d = make_domain({});
    auto new_d = make_domain({{"app", "cpu", "cpu.max", "100"}});
    auto result = ConfigDiffer::diff(old_d, new_d);
    ASSERT_EQ(result.added.size(), 2);
    EXPECT_EQ(result.added[0].path, "/app");
    EXPECT_EQ(result.added[0].node_type, ConfigNodeType::GROUP);
    EXPECT_EQ(result.added[1].path, "/app/cpu");
    EXPECT_EQ(result.added[1].node_type, ConfigNodeType::CONTROLLER);
    EXPECT_TRUE(result.removed.empty());
    EXPECT_TRUE(result.modified.empty());
}

TEST(ConfigDifferTest, AddedItem) {
    auto old_d = make_domain({{"app", "cpu", "cpu.max", "100"}});
    auto new_d = make_domain({
        {"app", "cpu", "cpu.max", "100"},
        {"app", "cpu", "cpu.weight", "50"},
    });
    auto result = ConfigDiffer::diff(old_d, new_d);
    ASSERT_EQ(result.added.size(), 1);
    EXPECT_EQ(result.added[0].path, "/app/cpu/cpu.weight");
    EXPECT_EQ(result.added[0].node_type, ConfigNodeType::ITEM);
}

TEST(ConfigDifferTest, AddedController) {
    auto old_d = make_domain({{"app", "cpu", "cpu.max", "100"}});
    auto new_d = make_domain({
        {"app", "cpu", "cpu.max", "100"},
        {"app", "memory", "memory.max", "1G"},
    });
    auto result = ConfigDiffer::diff(old_d, new_d);
    ASSERT_EQ(result.added.size(), 2);
    EXPECT_EQ(result.added[0].path, "/app/memory");
    EXPECT_EQ(result.added[0].node_type, ConfigNodeType::CONTROLLER);
    EXPECT_EQ(result.added[1].path, "/app/memory/memory.max");
    EXPECT_EQ(result.added[1].node_type, ConfigNodeType::ITEM);
}

// ── Removed ─────────────────────────────────────────────────────────────

TEST(ConfigDifferTest, RemovedGroup) {
    auto old_d = make_domain({{"app", "cpu", "cpu.max", "100"}});
    auto new_d = make_domain({});
    auto result = ConfigDiffer::diff(old_d, new_d);
    ASSERT_EQ(result.removed.size(), 2);
    EXPECT_EQ(result.removed[0].path, "/app");
    EXPECT_EQ(result.removed[0].node_type, ConfigNodeType::GROUP);
    EXPECT_EQ(result.removed[1].path, "/app/cpu");
    EXPECT_EQ(result.removed[1].node_type, ConfigNodeType::CONTROLLER);
}

TEST(ConfigDifferTest, RemovedItem) {
    auto old_d = make_domain({
        {"app", "cpu", "cpu.max", "100"},
        {"app", "cpu", "cpu.weight", "50"},
    });
    auto new_d = make_domain({{"app", "cpu", "cpu.max", "100"}});
    auto result = ConfigDiffer::diff(old_d, new_d);
    ASSERT_EQ(result.removed.size(), 1);
    EXPECT_EQ(result.removed[0].path, "/app/cpu/cpu.weight");
    EXPECT_EQ(result.removed[0].node_type, ConfigNodeType::ITEM);
}

TEST(ConfigDifferTest, RemovedController) {
    auto old_d = make_domain({
        {"app", "cpu", "cpu.max", "100"},
        {"app", "memory", "memory.max", "1G"},
    });
    auto new_d = make_domain({{"app", "cpu", "cpu.max", "100"}});
    auto result = ConfigDiffer::diff(old_d, new_d);
    ASSERT_EQ(result.removed.size(), 2);
    EXPECT_EQ(result.removed[0].path, "/app/memory");
    EXPECT_EQ(result.removed[0].node_type, ConfigNodeType::CONTROLLER);
    EXPECT_EQ(result.removed[1].path, "/app/memory/memory.max");
    EXPECT_EQ(result.removed[1].node_type, ConfigNodeType::ITEM);
}

// ── Modified ────────────────────────────────────────────────────────────

TEST(ConfigDifferTest, ModifiedItemValue) {
    auto old_d = make_domain({{"app", "cpu", "cpu.max", "100"}});
    auto new_d = make_domain({{"app", "cpu", "cpu.max", "200"}});
    auto result = ConfigDiffer::diff(old_d, new_d);
    ASSERT_EQ(result.modified.size(), 1);
    EXPECT_EQ(result.modified[0].path, "/app/cpu/cpu.max");
    EXPECT_EQ(result.modified[0].old_value, "100");
    EXPECT_EQ(result.modified[0].new_value, "200");
}

TEST(ConfigDifferTest, ModifiedItemValueToEmpty) {
    auto old_d = make_domain({{"app", "cpu", "cpu.max", "100"}});
    auto new_d = make_domain({{"app", "cpu", "cpu.max", ""}});
    auto result = ConfigDiffer::diff(old_d, new_d);
    ASSERT_EQ(result.modified.size(), 1);
    EXPECT_EQ(result.modified[0].old_value, "100");
    EXPECT_EQ(result.modified[0].new_value, "");
}

TEST(ConfigDifferTest, ModifiedItemValueFromEmpty) {
    auto old_d = make_domain({{"app", "cpu", "cpu.max", ""}});
    auto new_d = make_domain({{"app", "cpu", "cpu.max", "100"}});
    auto result = ConfigDiffer::diff(old_d, new_d);
    ASSERT_EQ(result.modified.size(), 1);
    EXPECT_EQ(result.modified[0].old_value, "");
    EXPECT_EQ(result.modified[0].new_value, "100");
}

// ── Mixed ───────────────────────────────────────────────────────────────

TEST(ConfigDifferTest, MixedChanges) {
    auto old_d = make_domain({
        {"a", "cpu", "cpu.max", "100"},
        {"a", "cpu", "cpu.weight", "50"},
        {"b", "mem", "memory.max", "1G"},
    });
    auto new_d = make_domain({
        {"a", "cpu", "cpu.max", "200"},         // modified
        {"a", "cpu", "cpu.weight", "50"},        // unchanged
        {"a", "mem", "memory.max", "512M"},      // added controller+item
        {"c", "pids", "pids.max", "1024"},        // added group+controller+item
    });

    auto result = ConfigDiffer::diff(old_d, new_d);

    // Added: group c (/c, /c/pids, /c/pids/pids.max), controller mem in a (/a/mem, /a/mem/memory.max)
    ASSERT_EQ(result.added.size(), 5);

    // Removed: group b (/b, /b/mem, /b/mem/memory.max), item cpu.weight in a — no, cpu.weight is unchanged
    // Actually removed is group b + its contents
    ASSERT_EQ(result.removed.size(), 3);

    // Modified: cpu.max in a
    ASSERT_EQ(result.modified.size(), 1);
    EXPECT_EQ(result.modified[0].path, "/a/cpu/cpu.max");
    EXPECT_EQ(result.modified[0].old_value, "100");
    EXPECT_EQ(result.modified[0].new_value, "200");

    EXPECT_TRUE(result.has_changes());
}

// ── No changes with different object identity ───────────────────────────

TEST(ConfigDifferTest, IdenticalContentDifferentObjects) {
    auto d1 = make_domain({{"app", "cpu", "cpu.max", "100"}});
    auto d2 = make_domain({{"app", "cpu", "cpu.max", "100"}});
    auto result = ConfigDiffer::diff(d1, d2);
    EXPECT_FALSE(result.has_changes());
}

// ── Multiple items in same controller ───────────────────────────────────

TEST(ConfigDifferTest, MultipleItemsInOneController) {
    auto old_d = make_domain({
        {"app", "cpu", "cpu.max", "100"},
        {"app", "cpu", "cpu.weight", "50"},
    });
    auto new_d = make_domain({
        {"app", "cpu", "cpu.max", "200"},
        {"app", "cpu", "cpu.weight", "75"},
        {"app", "cpu", "cpu.idle", "0"},
    });
    auto result = ConfigDiffer::diff(old_d, new_d);

    ASSERT_EQ(result.modified.size(), 2);
    EXPECT_EQ(result.added.size(), 1);
    EXPECT_EQ(result.added[0].path, "/app/cpu/cpu.idle");
    EXPECT_TRUE(result.removed.empty());
}

// ── Empty domain ────────────────────────────────────────────────────────

TEST(ConfigDifferTest, FromEmptyToNonEmpty) {
    ConfigDomain empty;
    auto d = make_domain({{"app", "cpu", "cpu.max", "100"}});
    auto result = ConfigDiffer::diff(empty, d);
    EXPECT_EQ(result.added.size(), 3);
    EXPECT_TRUE(result.has_changes());
}

TEST(ConfigDifferTest, FromNonEmptyToEmpty) {
    ConfigDomain empty;
    auto d = make_domain({{"app", "cpu", "cpu.max", "100"}});
    auto result = ConfigDiffer::diff(d, empty);
    EXPECT_EQ(result.removed.size(), 3);
    EXPECT_TRUE(result.has_changes());
}
