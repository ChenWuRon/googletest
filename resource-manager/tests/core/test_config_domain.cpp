#include <gtest/gtest.h>
#include "resource_manager/core/config_domain.h"

using namespace resource_manager;

// ── ConfigPath Tests ─────────────────────────────────────────────────────

TEST(ConfigPathTest, ParseRoot) {
    auto path = ConfigPath::parse("/");
    ASSERT_TRUE(path.has_value());
    EXPECT_TRUE(path->is_root());
    EXPECT_TRUE(path->empty());
    EXPECT_EQ(path->depth(), 0);
    EXPECT_EQ(path->to_string(), "/");
}

TEST(ConfigPathTest, ParseSingleSegment) {
    auto path = ConfigPath::parse("/ssm_app");
    ASSERT_TRUE(path.has_value());
    EXPECT_FALSE(path->is_root());
    EXPECT_EQ(path->depth(), 1);
    EXPECT_EQ(path->segments()[0], "ssm_app");
    EXPECT_EQ(path->to_string(), "/ssm_app");
}

TEST(ConfigPathTest, ParseMultiSegment) {
    auto path = ConfigPath::parse("/ssm_app/cpu/cpu.max");
    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path->depth(), 3);
    auto segs = path->segments();
    EXPECT_EQ(segs[0], "ssm_app");
    EXPECT_EQ(segs[1], "cpu");
    EXPECT_EQ(segs[2], "cpu.max");
    EXPECT_EQ(path->to_string(), "/ssm_app/cpu/cpu.max");
}

TEST(ConfigPathTest, ParseInvalidEmpty) {
    auto path = ConfigPath::parse("");
    EXPECT_FALSE(path.has_value());
}

TEST(ConfigPathTest, ParseInvalidNoLeadingSlash) {
    auto path = ConfigPath::parse("ssm_app");
    EXPECT_FALSE(path.has_value());
}

TEST(ConfigPathTest, ParseInvalidDoubleSlash) {
    auto path = ConfigPath::parse("/ssm_app//cpu");
    EXPECT_FALSE(path.has_value());
}

TEST(ConfigPathTest, ParseInvalidTrailingSlash) {
    // "/ssm_app/" -> last getline reads empty -> rejected
    auto path = ConfigPath::parse("/ssm_app/");
    EXPECT_FALSE(path.has_value());
}

TEST(ConfigPathTest, ParentPath) {
    auto child = ConfigPath::parse("/ssm_app/cpu");
    ASSERT_TRUE(child.has_value());
    auto p = child->parent();
    EXPECT_EQ(p.to_string(), "/ssm_app");
    auto root = p.parent();
    EXPECT_TRUE(root.is_root());
    EXPECT_EQ(root.to_string(), "/");
    // root parent is root
    auto root_again = root.parent();
    EXPECT_TRUE(root_again.is_root());
}

TEST(ConfigPathTest, ChildPath) {
    auto root = ConfigPath::root();
    auto group = root.child("ssm_app");
    EXPECT_EQ(group.to_string(), "/ssm_app");
    auto ctrl = group.child("cpu");
    EXPECT_EQ(ctrl.to_string(), "/ssm_app/cpu");
}

TEST(ConfigPathTest, Equality) {
    auto a = ConfigPath::parse("/a/b/c");
    auto b = ConfigPath::parse("/a/b/c");
    auto c = ConfigPath::parse("/a/b");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(*a, *b);
    EXPECT_NE(*a, *c);
}

TEST(ConfigPathTest, RootEquality) {
    auto a = ConfigPath::root();
    auto b = ConfigPath::parse("/");
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(a, *b);
}

TEST(ConfigPathTest, PathRoundTrip) {
    const std::string input = "/web-server/cpu/cpu.max";
    auto path = ConfigPath::parse(input);
    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path->to_string(), input);
}

// ── ConfigNodeType Tests ────────────────────────────────────────────────

TEST(ConfigNodeTypeTest, TypeNames) {
    EXPECT_EQ(node_type_name(ConfigNodeType::ROOT), "ROOT");
    EXPECT_EQ(node_type_name(ConfigNodeType::GROUP), "GROUP");
    EXPECT_EQ(node_type_name(ConfigNodeType::CONTROLLER), "CONTROLLER");
    EXPECT_EQ(node_type_name(ConfigNodeType::ITEM), "ITEM");
}

TEST(ConfigNodeTypeTest, ValidParentChild) {
    EXPECT_TRUE(is_valid_parent_child(ConfigNodeType::ROOT, ConfigNodeType::GROUP));
    EXPECT_TRUE(is_valid_parent_child(ConfigNodeType::GROUP, ConfigNodeType::CONTROLLER));
    EXPECT_TRUE(is_valid_parent_child(ConfigNodeType::CONTROLLER, ConfigNodeType::ITEM));

    EXPECT_FALSE(is_valid_parent_child(ConfigNodeType::ROOT, ConfigNodeType::ROOT));
    EXPECT_FALSE(is_valid_parent_child(ConfigNodeType::ROOT, ConfigNodeType::CONTROLLER));
    EXPECT_FALSE(is_valid_parent_child(ConfigNodeType::ROOT, ConfigNodeType::ITEM));
    EXPECT_FALSE(is_valid_parent_child(ConfigNodeType::GROUP, ConfigNodeType::GROUP));
    EXPECT_FALSE(is_valid_parent_child(ConfigNodeType::GROUP, ConfigNodeType::ROOT));
    EXPECT_FALSE(is_valid_parent_child(ConfigNodeType::GROUP, ConfigNodeType::ITEM));
    EXPECT_FALSE(is_valid_parent_child(ConfigNodeType::CONTROLLER, ConfigNodeType::GROUP));
    EXPECT_FALSE(is_valid_parent_child(ConfigNodeType::CONTROLLER, ConfigNodeType::CONTROLLER));
    EXPECT_FALSE(is_valid_parent_child(ConfigNodeType::ITEM, ConfigNodeType::ITEM));
    EXPECT_FALSE(is_valid_parent_child(ConfigNodeType::ITEM, ConfigNodeType::GROUP));
}

// ── ConfigNode Tree Tests ───────────────────────────────────────────────

TEST(ConfigNodeTest, CreateRoot) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    EXPECT_EQ(root.type(), ConfigNodeType::ROOT);
    EXPECT_TRUE(root.name().empty());
    EXPECT_TRUE(root.is_root());
    EXPECT_EQ(root.parent(), nullptr);
    EXPECT_TRUE(root.children().empty());
}

TEST(ConfigNodeTest, AddChildSuccess) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    auto group = std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "ssm_app");
    auto* raw = root.addChild(std::move(group));
    ASSERT_NE(raw, nullptr);
    EXPECT_EQ(raw->type(), ConfigNodeType::GROUP);
    EXPECT_EQ(raw->name(), "ssm_app");
    EXPECT_EQ(raw->parent(), &root);
    EXPECT_EQ(root.children().size(), 1);
}

TEST(ConfigNodeTest, AddChildRejectsInvalidType) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    // cannot add CONTROLLER under ROOT
    auto ctrl = std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu");
    auto* raw = root.addChild(std::move(ctrl));
    EXPECT_EQ(raw, nullptr);
    EXPECT_TRUE(root.children().empty());
}

TEST(ConfigNodeTest, AddChildRejectsDuplicateName) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    auto g1 = std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "my_service");
    auto* r1 = root.addChild(std::move(g1));
    ASSERT_NE(r1, nullptr);

    auto g2 = std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "my_service");
    auto* r2 = root.addChild(std::move(g2));
    EXPECT_EQ(r2, nullptr);
    EXPECT_EQ(root.children().size(), 1);
}

TEST(ConfigNodeTest, AddChildRejectsNullptr) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    auto* raw = root.addChild(nullptr);
    EXPECT_EQ(raw, nullptr);
}

TEST(ConfigNodeTest, BuildFullHierarchy) {
    ConfigNode root(ConfigNodeType::ROOT, "");

    auto group = std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "ssm_app");
    auto* g = root.addChild(std::move(group));
    ASSERT_NE(g, nullptr);

    auto ctrl = std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu");
    auto* c = g->addChild(std::move(ctrl));
    ASSERT_NE(c, nullptr);

    auto item = std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "cpu.max");
    auto* i = c->addChild(std::move(item));
    ASSERT_NE(i, nullptr);

    EXPECT_EQ(i->type(), ConfigNodeType::ITEM);
    EXPECT_EQ(i->name(), "cpu.max");
    EXPECT_EQ(i->parent(), c);
    EXPECT_EQ(c->parent(), g);
    EXPECT_EQ(g->parent(), &root);

    // ITEM cannot have children
    auto leaf = std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "child");
    auto* r = i->addChild(std::move(leaf));
    EXPECT_EQ(r, nullptr);
}

TEST(ConfigNodeTest, FindChild) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    auto group = std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "ssm_app");
    auto* raw = root.addChild(std::move(group));
    ASSERT_NE(raw, nullptr);

    auto* found = root.findChild("ssm_app");
    EXPECT_EQ(found, raw);

    auto* missing = root.findChild("nonexistent");
    EXPECT_EQ(missing, nullptr);
}

TEST(ConfigNodeTest, FindChildConst) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    auto group = std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "ssm_app");
    root.addChild(std::move(group));

    const auto& const_root = root;
    auto* found = const_root.findChild("ssm_app");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name(), "ssm_app");
    EXPECT_EQ(const_root.findChild("x"), nullptr);
}

TEST(ConfigNodeTest, RemoveChild) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    auto group = std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "ssm_app");
    root.addChild(std::move(group));
    EXPECT_EQ(root.children().size(), 1);

    auto removed = root.removeChild("ssm_app");
    ASSERT_NE(removed, nullptr);
    EXPECT_EQ(removed->name(), "ssm_app");
    EXPECT_EQ(removed->parent(), nullptr);
    EXPECT_TRUE(root.children().empty());
}

TEST(ConfigNodeTest, RemoveChildNonexistent) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    auto removed = root.removeChild("nonexistent");
    EXPECT_EQ(removed, nullptr);
}

TEST(ConfigNodeTest, GetPath) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    EXPECT_EQ(root.getPath().to_string(), "/");

    auto g = root.addChild(std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "my_svc"));
    ASSERT_NE(g, nullptr);
    EXPECT_EQ(g->getPath().to_string(), "/my_svc");

    auto c = g->addChild(std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu"));
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->getPath().to_string(), "/my_svc/cpu");

    auto i = c->addChild(std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "cpu.max"));
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->getPath().to_string(), "/my_svc/cpu/cpu.max");
}

TEST(ConfigNodeTest, FindByPath) {
    ConfigNode root(ConfigNodeType::ROOT, "");

    auto g = root.addChild(std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "a"));
    auto c = g->addChild(std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "b"));
    auto i = c->addChild(std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "c"));

    EXPECT_EQ(root.findByPath(ConfigPath::parse("/")->to_string()), &root);
    EXPECT_EQ(root.findByPath(ConfigPath::parse("/a")->to_string()), g);
    EXPECT_EQ(root.findByPath(ConfigPath::parse("/a/b")->to_string()), c);
    EXPECT_EQ(root.findByPath(ConfigPath::parse("/a/b/c")->to_string()), i);
    EXPECT_EQ(root.findByPath(ConfigPath::parse("/nonexistent")->to_string()), nullptr);
}

TEST(ConfigNodeTest, FindByPathConst) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    auto g = root.addChild(std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "svc"));

    const auto& const_root = root;
    auto* found = const_root.findByPath(ConfigPath::parse("/svc")->to_string());
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name(), "svc");
}

TEST(ConfigNodeTest, MultipleGroups) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    auto* g1 = root.addChild(std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "web"));
    auto* g2 = root.addChild(std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "db"));
    ASSERT_NE(g1, nullptr);
    ASSERT_NE(g2, nullptr);
    EXPECT_EQ(root.children().size(), 2);

    auto c1 = g1->addChild(std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu"));
    auto c2 = g2->addChild(std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "memory"));
    ASSERT_NE(c1, nullptr);
    ASSERT_NE(c2, nullptr);
    EXPECT_EQ(g1->children().size(), 1);
    EXPECT_EQ(g2->children().size(), 1);
}

TEST(ConfigNodeTest, IterateChildren) {
    ConfigNode root(ConfigNodeType::ROOT, "");
    root.addChild(std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "a"));
    root.addChild(std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "b"));
    root.addChild(std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "c"));

    int count = 0;
    for (const auto& [name, node] : root.children()) {
        EXPECT_EQ(node->name(), name);
        count++;
    }
    EXPECT_EQ(count, 3);
}

TEST(ConfigNodeTest, GroupDataAccessors) {
    ConfigNode group(ConfigNodeType::GROUP, "svc");
    group.mode().service = ServiceType::Systemd;
    group.mode().ns = NamespaceType::Cgroup;
    group.mode().entity = EntityType::ProcessName;
    group.match_rule().pattern = "nginx";
    group.match_rule().type = "prefix";

    EXPECT_EQ(group.mode().service, ServiceType::Systemd);
    EXPECT_EQ(group.mode().ns, NamespaceType::Cgroup);
    EXPECT_EQ(group.mode().entity, EntityType::ProcessName);
    EXPECT_EQ(group.match_rule().pattern, "nginx");
    EXPECT_EQ(group.match_rule().type, "prefix");
}

TEST(ConfigNodeTest, ItemDataAccessors) {
    ConfigNode item(ConfigNodeType::ITEM, "cpu.max");
    item.value() = "100000 100000";
    item.value_type() = ValueType::Quota;

    EXPECT_EQ(item.value(), "100000 100000");
    EXPECT_EQ(item.value_type(), ValueType::Quota);

    // Modify and read back
    item.value() = "max 100000";
    item.value_type() = ValueType::String;
    EXPECT_EQ(item.value(), "max 100000");
    EXPECT_EQ(item.value_type(), ValueType::String);
}

// ── ConfigDomain Tests ──────────────────────────────────────────────────

TEST(ConfigDomainTest, CreateEmpty) {
    ConfigDomain domain;
    EXPECT_EQ(domain.root().type(), ConfigNodeType::ROOT);
    EXPECT_TRUE(domain.root().is_root());
    EXPECT_EQ(domain.groupCount(), 0);
    EXPECT_EQ(domain.nodeCount(), 1); // only root
}

TEST(ConfigDomainTest, AddGroup) {
    ConfigDomain domain;
    auto* g = domain.addGroup("ssm_app");
    ASSERT_NE(g, nullptr);
    EXPECT_EQ(g->type(), ConfigNodeType::GROUP);
    EXPECT_EQ(g->name(), "ssm_app");
    EXPECT_EQ(g->parent(), &domain.root());
    EXPECT_EQ(domain.groupCount(), 1);
    EXPECT_EQ(domain.nodeCount(), 2); // root + 1 group
}

TEST(ConfigDomainTest, AddGroupDuplicate) {
    ConfigDomain domain;
    auto* g1 = domain.addGroup("ssm_app");
    auto* g2 = domain.addGroup("ssm_app");
    ASSERT_NE(g1, nullptr);
    EXPECT_EQ(g2, nullptr); // duplicate rejected
    EXPECT_EQ(domain.groupCount(), 1);
}

TEST(ConfigDomainTest, AddController) {
    ConfigDomain domain;
    domain.addGroup("ssm_app");
    auto* c = domain.addController("ssm_app", "cpu");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->type(), ConfigNodeType::CONTROLLER);
    EXPECT_EQ(c->name(), "cpu");
    EXPECT_EQ(domain.nodeCount(), 3); // root + group + controller
}

TEST(ConfigDomainTest, AddControllerGroupNotFound) {
    ConfigDomain domain;
    auto* c = domain.addController("nonexistent", "cpu");
    EXPECT_EQ(c, nullptr);
}

TEST(ConfigDomainTest, AddItem) {
    ConfigDomain domain;
    domain.addGroup("ssm_app");
    domain.addController("ssm_app", "cpu");
    auto* i = domain.addItem("ssm_app", "cpu", "cpu.max");
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->type(), ConfigNodeType::ITEM);
    EXPECT_EQ(i->name(), "cpu.max");
    EXPECT_EQ(domain.nodeCount(), 4); // root + group + controller + item
}

TEST(ConfigDomainTest, AddItemParentNotFound) {
    ConfigDomain domain;
    auto* i1 = domain.addItem("nonexistent", "cpu", "cpu.max");
    EXPECT_EQ(i1, nullptr);

    domain.addGroup("svc");
    auto* i2 = domain.addItem("svc", "nonexistent", "cpu.max");
    EXPECT_EQ(i2, nullptr);
}

TEST(ConfigDomainTest, BuildFullTreeViaDomain) {
    ConfigDomain domain;

    auto* g = domain.addGroup("ssm_app");
    ASSERT_NE(g, nullptr);
    g->mode().service = ServiceType::Systemd;
    g->match_rule().pattern = "ssm-app";
    g->match_rule().type = "prefix";

    auto* c = domain.addController("ssm_app", "cpu");
    ASSERT_NE(c, nullptr);

    auto* i = domain.addItem("ssm_app", "cpu", "cpu.max");
    ASSERT_NE(i, nullptr);
    i->value() = "100000 100000";
    i->value_type() = ValueType::Quota;

    // Verify paths
    EXPECT_EQ(g->getPath().to_string(), "/ssm_app");
    EXPECT_EQ(c->getPath().to_string(), "/ssm_app/cpu");
    EXPECT_EQ(i->getPath().to_string(), "/ssm_app/cpu/cpu.max");

    // Verify group data
    EXPECT_EQ(g->mode().service, ServiceType::Systemd);
    EXPECT_EQ(g->match_rule().pattern, "ssm-app");

    // Verify item data
    EXPECT_EQ(i->value(), "100000 100000");
    EXPECT_EQ(i->value_type(), ValueType::Quota);

    EXPECT_EQ(domain.groupCount(), 1);
    EXPECT_EQ(domain.nodeCount(), 4);
}

TEST(ConfigDomainTest, FindByPathFull) {
    ConfigDomain domain;
    domain.addGroup("web");
    domain.addController("web", "cpu");
    domain.addItem("web", "cpu", "cpu.max");
    domain.addItem("web", "cpu", "cpu.weight");

    auto* found_root = domain.findByPath("/");
    ASSERT_NE(found_root, nullptr);
    EXPECT_TRUE(found_root->is_root());

    auto* found_group = domain.findByPath("/web");
    ASSERT_NE(found_group, nullptr);
    EXPECT_EQ(found_group->type(), ConfigNodeType::GROUP);
    EXPECT_EQ(found_group->name(), "web");

    auto* found_ctrl = domain.findByPath("/web/cpu");
    ASSERT_NE(found_ctrl, nullptr);
    EXPECT_EQ(found_ctrl->type(), ConfigNodeType::CONTROLLER);

    auto* found_item1 = domain.findByPath("/web/cpu/cpu.max");
    ASSERT_NE(found_item1, nullptr);
    EXPECT_EQ(found_item1->type(), ConfigNodeType::ITEM);

    auto* found_item2 = domain.findByPath("/web/cpu/cpu.weight");
    ASSERT_NE(found_item2, nullptr);
    EXPECT_EQ(found_item2->name(), "cpu.weight");

    // Not found
    EXPECT_EQ(domain.findByPath("/nonexistent"), nullptr);
    EXPECT_EQ(domain.findByPath("/web/nonexistent/item"), nullptr);

    // Invalid path
    EXPECT_EQ(domain.findByPath(""), nullptr);
}

TEST(ConfigDomainTest, FindByPathConst) {
    ConfigDomain domain;
    domain.addGroup("svc");

    const auto& const_domain = domain;
    auto* found = const_domain.findByPath("/svc");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name(), "svc");

    EXPECT_EQ(const_domain.findByPath("/nonexistent"), nullptr);
}

TEST(ConfigDomainTest, MultipleGroupsAndControllers) {
    ConfigDomain domain;
    domain.addGroup("web");
    domain.addGroup("db");
    domain.addController("web", "cpu");
    domain.addController("web", "memory");
    domain.addController("db", "cpu");
    domain.addItem("web", "cpu", "cpu.max");
    domain.addItem("web", "memory", "memory.max");
    domain.addItem("db", "cpu", "cpu.max");

    EXPECT_EQ(domain.groupCount(), 2);
    EXPECT_EQ(domain.nodeCount(), 9); // root + 2 group + 3 ctrl + 3 item

    // Traverse
    int group_count = 0;
    int controller_count = 0;
    int item_count = 0;

    for (const auto& [gname, gnode] : domain.root().children()) {
        group_count++;
        for (const auto& [cname, cnode] : gnode->children()) {
            controller_count++;
            for (const auto& [iname, inode] : cnode->children()) {
                item_count++;
            }
        }
    }

    EXPECT_EQ(group_count, 2);
    EXPECT_EQ(controller_count, 3);
    EXPECT_EQ(item_count, 3);
}

TEST(ConfigDomainTest, RemoveGroupViaNode) {
    ConfigDomain domain;
    domain.addGroup("svc");
    domain.addController("svc", "cpu");
    EXPECT_EQ(domain.groupCount(), 1);

    auto removed = domain.root().removeChild("svc");
    ASSERT_NE(removed, nullptr);
    EXPECT_EQ(removed->name(), "svc");
    EXPECT_TRUE(domain.root().children().empty());
}

TEST(ConfigDomainTest, NodeCountConsistency) {
    ConfigDomain domain;
    EXPECT_EQ(domain.nodeCount(), 1);

    domain.addGroup("a");
    EXPECT_EQ(domain.nodeCount(), 2);

    domain.addGroup("b");
    EXPECT_EQ(domain.nodeCount(), 3);

    domain.addController("a", "cpu");
    EXPECT_EQ(domain.nodeCount(), 4);

    domain.addItem("a", "cpu", "cpu.max");
    EXPECT_EQ(domain.nodeCount(), 5);

    // Duplicate rejections do not increase count
    domain.addGroup("a");
    EXPECT_EQ(domain.nodeCount(), 5);

    domain.addController("a", "cpu");
    EXPECT_EQ(domain.nodeCount(), 5);
}
