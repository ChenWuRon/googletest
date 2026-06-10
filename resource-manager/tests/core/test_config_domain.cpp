#include <gtest/gtest.h>
#include "resource_manager/core/config_domain.h"

using namespace resource_manager;

// ── ConfigNodeType ─────────────────────────────────────────────────────

TEST(ConfigNodeTypeTest, EnumValues) {
    EXPECT_NE(static_cast<int>(ConfigNodeType::ROOT),
              static_cast<int>(ConfigNodeType::GROUP));
}

// ── ConfigNode ─────────────────────────────────────────────────────────

TEST(ConfigNodeTest, CreateNode) {
    ConfigNode node(ConfigNodeType::GROUP, "my_service");
    EXPECT_EQ(node.type(), ConfigNodeType::GROUP);
    EXPECT_EQ(node.name(), "my_service");
    EXPECT_EQ(node.parent(), nullptr);
    EXPECT_TRUE(node.children().empty());
}

TEST(ConfigNodeTest, CreateRoot) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    EXPECT_TRUE(root.name().empty());
    EXPECT_EQ(root.parent(), nullptr);
    EXPECT_TRUE(root.children().empty());
}

TEST(ConfigNodeTest, AddChildSuccess) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    auto child = std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "svc");
    auto* raw = root.addChild(std::move(child));
    ASSERT_NE(raw, nullptr);
    EXPECT_EQ(raw->type(), ConfigNodeType::GROUP);
    EXPECT_EQ(raw->name(), "svc");
    EXPECT_EQ(raw->parent(), &root);
    EXPECT_EQ(root.children().size(), 1);
    EXPECT_EQ(root.children().begin()->first, "svc");
    EXPECT_EQ(root.children().begin()->second.get(), raw);
}

TEST(ConfigNodeTest, AddChildNullptr) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    auto* raw = root.addChild(nullptr);
    EXPECT_EQ(raw, nullptr);
}

TEST(ConfigNodeTest, AddChildDuplicate) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    auto a = std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "dup");
    root.addChild(std::move(a));
    auto b = std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "dup");
    auto* raw = root.addChild(std::move(b));
    EXPECT_EQ(raw, nullptr);
    EXPECT_EQ(root.children().size(), 1);
}

TEST(ConfigNodeTest, AddChildDifferentNames) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    auto* a = root.addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "a"));
    auto* b = root.addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "b"));
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_NE(a, b);
    EXPECT_EQ(root.children().size(), 2);
}

TEST(ConfigNodeTest, AddChildAnyType) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    auto* g = root.addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "g"));
    ASSERT_NE(g, nullptr);
    auto* c = g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu"));
    ASSERT_NE(c, nullptr);
    auto* i = c->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "cpu.max"));
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(g->parent(), &root);
    EXPECT_EQ(c->parent(), g);
    EXPECT_EQ(i->parent(), c);
}

TEST(ConfigNodeTest, RemoveChildFound) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    root.addChild(std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "svc"));
    EXPECT_EQ(root.children().size(), 1);

    auto removed = root.removeChild("svc");
    ASSERT_NE(removed, nullptr);
    EXPECT_EQ(removed->name(), "svc");
    EXPECT_EQ(removed->parent(), nullptr);
    EXPECT_TRUE(root.children().empty());
}

TEST(ConfigNodeTest, RemoveChildNotFound) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    root.addChild(std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "a"));
    auto removed = root.removeChild("nonexistent");
    EXPECT_EQ(removed, nullptr);
    EXPECT_EQ(root.children().size(), 1);
}

TEST(ConfigNodeTest, RemoveChildFromNonRoot) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    auto* g = root.addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "svc"));
    g->addChild(std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu"));
    EXPECT_EQ(g->children().size(), 1);

    auto removed = g->removeChild("cpu");
    ASSERT_NE(removed, nullptr);
    EXPECT_EQ(removed->name(), "cpu");
    EXPECT_TRUE(g->children().empty());
}

TEST(ConfigNodeTest, RemoveAndReAddSameName) {
    ConfigNode root(ConfigNodeType::ROOT, "");

    root.addChild(std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "x"));
    root.removeChild("x");
    EXPECT_TRUE(root.children().empty());

    auto* raw = root.addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "x"));
    ASSERT_NE(raw, nullptr);
    EXPECT_EQ(root.children().size(), 1);
}

TEST(ConfigNodeTest, FindChildFound) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    auto* raw = root.addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "svc"));
    auto* found = root.findChild("svc");
    EXPECT_EQ(found, raw);
}

TEST(ConfigNodeTest, FindChildNotFound) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    root.addChild(std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "a"));
    auto* found = root.findChild("b");
    EXPECT_EQ(found, nullptr);
}

TEST(ConfigNodeTest, FindChildConst) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    root.addChild(std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "svc"));

    const auto& const_root = root;
    auto* found = const_root.findChild("svc");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name(), "svc");
    EXPECT_EQ(const_root.findChild("x"), nullptr);
}

TEST(ConfigNodeTest, FindChildAfterRemove) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    root.addChild(std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "svc"));
    root.removeChild("svc");
    EXPECT_EQ(root.findChild("svc"), nullptr);
}

TEST(ConfigNodeTest, ParentPointer) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    auto* g = root.addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "g"));
    auto* c = g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "c"));
    EXPECT_EQ(g->parent(), &root);
    EXPECT_EQ(c->parent(), g);
}

TEST(ConfigNodeTest, ParentAfterRemove) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    root.addChild(std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "g"));
    auto removed = root.removeChild("g");
    ASSERT_NE(removed, nullptr);
    EXPECT_EQ(removed->parent(), nullptr);
}

TEST(ConfigNodeTest, IterateChildren) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    root.addChild(std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "a"));
    root.addChild(std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "b"));
    root.addChild(std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "c"));

    int count = 0;
    std::string names;
    for (const auto& [name, node] : root.children()) {
        EXPECT_EQ(node->name(), name);
        names += name;
        count++;
    }
    EXPECT_EQ(count, 3);
    EXPECT_EQ(names, "abc");
}

TEST(ConfigNodeTest, DeepHierarchy) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    auto* g = root.addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "my_app"));
    auto* c = g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu"));
    auto* i1 = c->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "cpu.max"));
    auto* i2 = c->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "cpu.weight"));

    ASSERT_NE(g, nullptr);
    ASSERT_NE(c, nullptr);
    ASSERT_NE(i1, nullptr);
    ASSERT_NE(i2, nullptr);

    EXPECT_EQ(root.children().size(), 1);
    EXPECT_EQ(g->children().size(), 1);
    EXPECT_EQ(c->children().size(), 2);

    EXPECT_EQ(c->parent(), g);
    EXPECT_EQ(i1->parent(), c);
    EXPECT_EQ(i2->parent(), c);

    EXPECT_NE(i1, i2);
}

// ── ConfigDomain ───────────────────────────────────────────────────────

TEST(ConfigDomainTest, Create) {
    ConfigDomain domain;
    EXPECT_EQ(domain.root().type(), ConfigNodeType::ROOT);
    EXPECT_TRUE(domain.root().children().empty());
}

TEST(ConfigDomainTest, RootAccess) {
    ConfigDomain domain;
    auto& root = domain.root();
    EXPECT_EQ(root.type(), ConfigNodeType::ROOT);

    const auto& const_root = const_cast<const ConfigDomain&>(domain).root();
    EXPECT_EQ(const_root.type(), ConfigNodeType::ROOT);
}

TEST(ConfigDomainTest, BuildViaDomain) {
    ConfigDomain domain;
    auto* g = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "svc"));
    auto* c = g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu"));
    auto* i = c->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "cpu.max"));

    ASSERT_NE(g, nullptr);
    ASSERT_NE(c, nullptr);
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->name(), "cpu.max");
}
