#include <gtest/gtest.h>
#include "resource_manager/core/config_renderer.h"

using namespace resource_manager;

// ── Empty ───────────────────────────────────────────────────────────────

TEST(ConfigRendererTest, EmptyDomain) {
    ConfigDomain domain;
    std::string text = ConfigRenderer::render(domain);
    EXPECT_TRUE(text.empty());
}

// ── Single group, no controller ─────────────────────────────────────────

TEST(ConfigRendererTest, SingleGroupEmpty) {
    ConfigDomain domain;
    domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "app"));
    std::string text = ConfigRenderer::render(domain);
    EXPECT_EQ(text, "group app {\n}\n\n");
}

// ── Group with one controller, one item ─────────────────────────────────

TEST(ConfigRendererTest, SingleControllerSingleItem) {
    ConfigDomain domain;
    auto* g = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "web"));
    auto* c = g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu"));
    c->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "cpu.max", "100000 100000"));

    std::string text = ConfigRenderer::render(domain);
    EXPECT_EQ(text,
        "group web {\n"
        "    controller cpu {\n"
        "        item cpu.max = \"100000 100000\";\n"
        "    }\n"
        "}\n"
        "\n");
}

// ── Multiple items in one controller ────────────────────────────────────

TEST(ConfigRendererTest, MultipleItems) {
    ConfigDomain domain;
    auto* g = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "app"));
    auto* c = g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu"));
    c->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "cpu.max", "100000 100000"));
    c->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "cpu.weight", "100"));

    std::string text = ConfigRenderer::render(domain);
    EXPECT_EQ(text,
        "group app {\n"
        "    controller cpu {\n"
        "        item cpu.max = \"100000 100000\";\n"
        "        item cpu.weight = \"100\";\n"
        "    }\n"
        "}\n"
        "\n");
}

// ── Multiple controllers ────────────────────────────────────────────────

TEST(ConfigRendererTest, MultipleControllers) {
    ConfigDomain domain;
    auto* g = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "app"));
    auto* cpu = g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu"));
    cpu->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "cpu.max", "100"));
    auto* mem = g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "memory"));
    mem->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "memory.max", "1G"));

    std::string text = ConfigRenderer::render(domain);
    EXPECT_EQ(text,
        "group app {\n"
        "    controller cpu {\n"
        "        item cpu.max = \"100\";\n"
        "    }\n"
        "    controller memory {\n"
        "        item memory.max = \"1G\";\n"
        "    }\n"
        "}\n"
        "\n");
}

// ── Multiple groups ─────────────────────────────────────────────────────

TEST(ConfigRendererTest, MultipleGroups) {
    ConfigDomain domain;
    auto* g1 = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "web"));
    g1->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu"));

    auto* g2 = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "db"));
    g2->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "memory"));

    std::string text = ConfigRenderer::render(domain);
    EXPECT_EQ(text,
        "group web {\n"
        "    controller cpu {\n"
        "    }\n"
        "}\n"
        "\n"
        "group db {\n"
        "    controller memory {\n"
        "    }\n"
        "}\n"
        "\n");
}

// ── Item with empty value ───────────────────────────────────────────────

TEST(ConfigRendererTest, ItemEmptyValue) {
    ConfigDomain domain;
    auto* g = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "app"));
    auto* c = g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "test"));
    c->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "flag", ""));

    std::string text = ConfigRenderer::render(domain);
    EXPECT_EQ(text,
        "group app {\n"
        "    controller test {\n"
        "        item flag = \"\";\n"
        "    }\n"
        "}\n"
        "\n");
}

// ── Item with special characters in value ───────────────────────────────

TEST(ConfigRendererTest, ItemSpecialCharsInValue) {
    ConfigDomain domain;
    auto* g = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "app"));
    auto* c = g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "io"));
    c->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "io.max",
                                     "8:16 rbps=1048576 wbps=1048576"));

    std::string text = ConfigRenderer::render(domain);
    EXPECT_EQ(text,
        "group app {\n"
        "    controller io {\n"
        "        item io.max = \"8:16 rbps=1048576 wbps=1048576\";\n"
        "    }\n"
        "}\n"
        "\n");
}

// ── Round-trip: render then re-parse ────────────────────────────────────

TEST(ConfigRendererTest, RenderThenReparse) {
    ConfigDomain domain;
    auto* g = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "web-server"));
    auto* c = g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu"));
    c->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "cpu.max", "100000 100000"));
    c->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "cpu.weight", "100"));
    auto* mem = g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "memory"));
    mem->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "memory.max", "1G"));
    mem->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "memory.high", "800M"));

    std::string rendered = ConfigRenderer::render(domain);

    // Render to text, then verify the text contains expected structure
    EXPECT_NE(rendered.find("group web-server {"), std::string::npos);
    EXPECT_NE(rendered.find("controller cpu {"), std::string::npos);
    EXPECT_NE(rendered.find("controller memory {"), std::string::npos);
    EXPECT_NE(rendered.find("item cpu.max = \"100000 100000\";"), std::string::npos);
    EXPECT_NE(rendered.find("item cpu.weight = \"100\";"), std::string::npos);
    EXPECT_NE(rendered.find("item memory.max = \"1G\";"), std::string::npos);
    EXPECT_NE(rendered.find("item memory.high = \"800M\";"), std::string::npos);
}
