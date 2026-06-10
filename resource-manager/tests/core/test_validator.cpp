#include <gtest/gtest.h>
#include "resource_manager/core/validator.h"

using namespace resource_manager;

// ── helpers ─────────────────────────────────────────────────────────────

ConfigDomain make_valid_domain() {
    ConfigDomain domain;
    auto* g = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "app", "", 2, 1));
    auto* c = g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu", "", 3, 5));
    c->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "cpu.max", "100", 4, 9));
    return domain;
}

// ── Valid ───────────────────────────────────────────────────────────────

TEST(ValidatorTest, ValidTree) {
    auto domain = make_valid_domain();
    Validator v;
    auto result = v.validate(domain);
    EXPECT_TRUE(result.valid());
    EXPECT_TRUE(result.errors.empty());
}

TEST(ValidatorTest, EmptyDomain) {
    ConfigDomain domain;
    Validator v;
    auto result = v.validate(domain);
    EXPECT_TRUE(result.valid());
}

TEST(ValidatorTest, MultipleGroups) {
    ConfigDomain domain;
    auto* g1 = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "web", "", 1, 1));
    auto* c1 = g1->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu", "", 2, 5));
    c1->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "cpu.max", "100", 3, 9));

    auto* g2 = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "db", "", 5, 1));
    auto* c2 = g2->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "memory", "", 6, 5));
    c2->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "memory.max", "1G", 7, 9));

    Validator v;
    auto result = v.validate(domain);
    EXPECT_TRUE(result.valid());
}

TEST(ValidatorTest, MultipleControllersAndItems) {
    ConfigDomain domain;
    auto* g = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "app", "", 1, 1));
    auto* cpu = g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu", "", 2, 5));
    cpu->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "cpu.max", "100", 3, 9));
    cpu->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "cpu.weight", "50", 4, 9));
    auto* mem = g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "memory", "", 6, 5));
    mem->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "memory.max", "512M", 7, 9));

    Validator v;
    auto result = v.validate(domain);
    EXPECT_TRUE(result.valid());
}

// ── Hierarchy violations ────────────────────────────────────────────────

TEST(ValidatorTest, ItemUnderGroup) {
    ConfigDomain domain;
    auto* g = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "app", "", 1, 1));
    g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "cpu.max", "100", 2, 5));

    Validator v;
    auto result = v.validate(domain);
    ASSERT_FALSE(result.valid());
    EXPECT_EQ(result.errors.size(), 1);
    EXPECT_NE(result.errors[0].message.find("ITEM"), std::string::npos);
    EXPECT_NE(result.errors[0].message.find("CONTROLLER"), std::string::npos);
}

TEST(ValidatorTest, ControllerUnderRoot) {
    ConfigDomain domain;
    domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu", "", 1, 1));

    Validator v;
    auto result = v.validate(domain);
    ASSERT_FALSE(result.valid());
    EXPECT_NE(result.errors[0].message.find("CONTROLLER"), std::string::npos);
    EXPECT_NE(result.errors[0].message.find("GROUP"), std::string::npos);
}

TEST(ValidatorTest, GroupUnderGroup) {
    ConfigDomain domain;
    auto* g = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "outer", "", 1, 1));
    g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "inner", "", 2, 5));

    Validator v;
    auto result = v.validate(domain);
    ASSERT_FALSE(result.valid());
    EXPECT_NE(result.errors[0].message.find("GROUP"), std::string::npos);
    EXPECT_NE(result.errors[0].message.find("ROOT"), std::string::npos);
}

TEST(ValidatorTest, ControllerUnderController) {
    ConfigDomain domain;
    auto* g = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "app", "", 1, 1));
    auto* c = g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu", "", 2, 5));
    c->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "nested", "", 3, 9));

    Validator v;
    auto result = v.validate(domain);
    ASSERT_FALSE(result.valid());
    EXPECT_NE(result.errors[0].message.find("CONTROLLER"), std::string::npos);
    EXPECT_NE(result.errors[0].message.find("GROUP"), std::string::npos);
}

TEST(ValidatorTest, ItemUnderRoot) {
    ConfigDomain domain;
    domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "direct", "val", 1, 1));

    Validator v;
    auto result = v.validate(domain);
    ASSERT_FALSE(result.valid());
    EXPECT_NE(result.errors[0].message.find("ITEM"), std::string::npos);
    EXPECT_NE(result.errors[0].message.find("CONTROLLER"), std::string::npos);
}

// ── Missing value ───────────────────────────────────────────────────────

TEST(ValidatorTest, ItemMissingValue) {
    ConfigDomain domain;
    auto* g = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "app", "", 1, 1));
    auto* c = g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu", "", 2, 5));
    c->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "cpu.max", "", 3, 9));

    Validator v;
    auto result = v.validate(domain);
    ASSERT_FALSE(result.valid());
    EXPECT_NE(result.errors[0].message.find("missing"), std::string::npos);
}

TEST(ValidatorTest, SomeItemsMissingSomeHaveValue) {
    ConfigDomain domain;
    auto* g = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "app", "", 1, 1));
    auto* c = g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu", "", 2, 5));
    c->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "cpu.max", "", 3, 9));
    c->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "cpu.weight", "100", 4, 9));

    Validator v;
    auto result = v.validate(domain);
    ASSERT_FALSE(result.valid());
    EXPECT_EQ(result.errors.size(), 1);
}

// ── Multiple errors ─────────────────────────────────────────────────────

TEST(ValidatorTest, MultipleStructuralErrors) {
    ConfigDomain domain;
    auto* g = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "app", "", 1, 1));

    // ITEM under GROUP
    g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "bad_item", "x", 2, 5));

    // CONTROLLER under CONTROLLER
    auto* c = g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu", "", 3, 5));
    c->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "nested", "", 4, 9));

    // ITEM under GROUP with missing value
    g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "no_val", "", 5, 5));

    Validator v;
    auto result = v.validate(domain);
    EXPECT_GE(result.errors.size(), 3);
}

// ── Error metadata ──────────────────────────────────────────────────────

TEST(ValidatorTest, ErrorContainsLineColumnPath) {
    ConfigDomain domain;
    auto* g = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "app", "", 5, 3));
    g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "bad", "val", 7, 9));

    Validator v;
    auto result = v.validate(domain);
    ASSERT_FALSE(result.valid());

    const auto& e = result.errors[0];
    EXPECT_EQ(e.line, 7);
    EXPECT_EQ(e.column, 9);
    EXPECT_NE(e.path.find("/app/bad"), std::string::npos);
}

// ── Mixed valid and invalid ─────────────────────────────────────────────

TEST(ValidatorTest, MixedTree) {
    ConfigDomain domain;

    // Valid group
    auto* g1 = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "valid", "", 1, 1));
    auto* c1 = g1->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu", "", 2, 5));
    c1->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "cpu.max", "100", 3, 9));

    // Invalid group: ITEM under GROUP
    auto* g2 = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "invalid", "", 5, 1));
    g2->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "bad_item", "x", 6, 5));

    Validator v;
    auto result = v.validate(domain);
    ASSERT_FALSE(result.valid());
    EXPECT_EQ(result.errors.size(), 1);
}

// ── Group name edge cases ───────────────────────────────────────────────

TEST(ValidatorTest, GroupNameContainsPathMessage) {
    ConfigDomain domain;
    auto* g = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "good", "", 1, 1));
    auto* c = g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "ctrl", "", 2, 5));
    c->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "oops", "", 3, 9));

    Validator v;
    auto result = v.validate(domain);
    ASSERT_FALSE(result.valid());
    bool found = false;
    for (const auto& e : result.errors) {
        if (e.message.find("GROUP") != std::string::npos &&
            e.message.find("oops") != std::string::npos) {
            found = true;
        }
    }
    EXPECT_TRUE(found);
}
