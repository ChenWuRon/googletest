#include <gtest/gtest.h>
#include <fstream>

#include "resource_manager/core/config_loader.h"

using namespace resource_manager;

// ── loadFromString ──────────────────────────────────────────────────────

TEST(ConfigLoaderTest, LoadFromString) {
    ConfigLoader loader;
    auto result = loader.loadFromString("hello config");
    EXPECT_EQ(result, "hello config");
}

TEST(ConfigLoaderTest, LoadFromStringEmpty) {
    ConfigLoader loader;
    auto result = loader.loadFromString("");
    EXPECT_TRUE(result.empty());
}

TEST(ConfigLoaderTest, LoadFromStringMultiline) {
    ConfigLoader loader;
    std::string multi = "group app {\n    mode service;\n}\n";
    auto result = loader.loadFromString(multi);
    EXPECT_EQ(result, multi);
}

// ── loadFromFile ────────────────────────────────────────────────────────

TEST(ConfigLoaderTest, LoadFromFile) {
    {
        std::ofstream out("/tmp/test_loader_file.conf");
        out << "group app { }";
    }
    ConfigLoader loader;
    auto result = loader.loadFromFile("/tmp/test_loader_file.conf");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "group app { }");
}

TEST(ConfigLoaderTest, LoadFromFileMultiline) {
    {
        std::ofstream out("/tmp/test_loader_multiline.conf");
        out << "group app {\n    mode service;\n}\n";
    }
    ConfigLoader loader;
    auto result = loader.loadFromFile("/tmp/test_loader_multiline.conf");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "group app {\n    mode service;\n}\n");
}

TEST(ConfigLoaderTest, LoadFromFileNotFound) {
    ConfigLoader loader;
    auto result = loader.loadFromFile("/tmp/nonexistent_loader_test_file.conf");
    EXPECT_FALSE(result.has_value());
}

TEST(ConfigLoaderTest, LoadFromFileEmpty) {
    {
        std::ofstream out("/tmp/test_loader_empty.conf");
    }
    ConfigLoader loader;
    auto result = loader.loadFromFile("/tmp/test_loader_empty.conf");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}

// ── loadFromString static ───────────────────────────────────────────────

TEST(ConfigLoaderTest, StaticLoadFromString) {
    auto result = ConfigLoader::loadFromString("static test");
    EXPECT_EQ(result, "static test");
}
