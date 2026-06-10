#include <gtest/gtest.h>
#include <fstream>

#include "resource_manager/core/config_repository.h"

using namespace resource_manager;

// ── Fixture: temporary config file ──────────────────────────────────────

class ConfigRepositoryTest : public ::testing::Test {
protected:
    void write_config(const std::string& content) {
        std::ofstream out(path_);
        out << content;
    }

    std::string path_ = "/tmp/test_requtao_config.conf";
};

// ── Default state ───────────────────────────────────────────────────────

TEST_F(ConfigRepositoryTest, DefaultDomainIsEmpty) {
    ConfigRepository repo;
    EXPECT_TRUE(repo.getRoot().root().children().empty());
    EXPECT_TRUE(repo.errors().empty());
}

// ── replace ─────────────────────────────────────────────────────────────

TEST_F(ConfigRepositoryTest, ReplaceSetsDomain) {
    ConfigRepository repo;

    ConfigDomain domain;
    auto* g = domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "app", "", 1, 1));
    auto* c = g->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, "cpu", "", 2, 5));
    c->addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::ITEM, "cpu.max", "100", 3, 9));

    repo.replace(std::move(domain));
    EXPECT_FALSE(repo.getRoot().root().children().empty());
    EXPECT_TRUE(repo.errors().empty());
}

TEST_F(ConfigRepositoryTest, ReplaceOverwritesFullDomain) {
    ConfigRepository repo;

    ConfigDomain d1;
    d1.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "web", "", 1, 1));
    repo.replace(std::move(d1));
    EXPECT_NE(repo.getRoot().root().findChild("web"), nullptr);

    ConfigDomain d2;
    d2.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "db", "", 1, 1));
    repo.replace(std::move(d2));
    EXPECT_EQ(repo.getRoot().root().findChild("web"), nullptr);
    EXPECT_NE(repo.getRoot().root().findChild("db"), nullptr);
}

TEST_F(ConfigRepositoryTest, ReplaceWithEmptyDomain) {
    ConfigRepository repo;

    ConfigDomain d1;
    d1.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "web", "", 1, 1));
    repo.replace(std::move(d1));
    EXPECT_FALSE(repo.getRoot().root().children().empty());

    repo.replace(ConfigDomain{});
    EXPECT_TRUE(repo.getRoot().root().children().empty());
}

// ── getRoot immutable ───────────────────────────────────────────────────

TEST_F(ConfigRepositoryTest, GetRootReturnsConstRef) {
    ConfigDomain domain;
    domain.root().addChild(
        std::make_unique<ConfigNode>(ConfigNodeType::GROUP, "app", "", 1, 1));

    ConfigRepository repo;
    repo.replace(std::move(domain));

    const auto& root = repo.getRoot();
    EXPECT_NE(root.root().findChild("app"), nullptr);
    EXPECT_EQ(root.root().children().size(), 1);
}

// ── load ────────────────────────────────────────────────────────────────

TEST_F(ConfigRepositoryTest, LoadValidFile) {
    write_config(R"(group app {
    controller cpu {
        item cpu.max = "100000 100000";
    }
})");
    ConfigRepository repo;
    bool ok = repo.load(path_);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(repo.errors().empty());

    const auto& root = repo.getRoot().root();
    auto* g = root.findChild("app");
    ASSERT_NE(g, nullptr);
    auto* c = g->findChild("cpu");
    ASSERT_NE(c, nullptr);
    auto* i = c->findChild("cpu.max");
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->value(), "100000 100000");
}

TEST_F(ConfigRepositoryTest, LoadFileNotFound) {
    ConfigRepository repo;
    bool ok = repo.load("/tmp/nonexistent_requtao_file.conf");
    EXPECT_FALSE(ok);
    EXPECT_FALSE(repo.errors().empty());
}

TEST_F(ConfigRepositoryTest, LoadFileWithSyntaxError) {
    write_config(R"(group app {
    controller cpu {
        item cpu.max = "100"
})"); // missing closing brace
    ConfigRepository repo;
    bool ok = repo.load(path_);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(repo.errors().empty());
}

TEST_F(ConfigRepositoryTest, LoadFileWithLexError) {
    write_config("group app { @ }");
    ConfigRepository repo;
    bool ok = repo.load(path_);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(repo.errors().empty());
}

TEST_F(ConfigRepositoryTest, LoadFileWithStructuralError) {
    // ITEM directly under GROUP (invalid hierarchy)
    write_config(R"(group app {
    item cpu.max = "100";
})");
    ConfigRepository repo;
    bool ok = repo.load(path_);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(repo.errors().empty());

    bool found = false;
    for (const auto& e : repo.errors()) {
        if (e.message.find("must be placed under a CONTROLLER") != std::string::npos) {
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(ConfigRepositoryTest, LoadFailureDoesNotMutateState) {
    write_config(R"(group app {
    controller cpu {
        item cpu.max = "100";
    }
})");
    ConfigRepository repo;
    EXPECT_TRUE(repo.load(path_));
    EXPECT_NE(repo.getRoot().root().findChild("app"), nullptr);

    // Load bad file
    {
        std::ofstream out(path_);
        out << "group app { @ }";
    }
    bool ok = repo.load(path_);
    EXPECT_FALSE(ok);

    // State should remain from previous successful load
    EXPECT_NE(repo.getRoot().root().findChild("app"), nullptr);
}

TEST_F(ConfigRepositoryTest, LoadEmptyFile) {
    write_config("");
    ConfigRepository repo;
    bool ok = repo.load(path_);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(repo.getRoot().root().children().empty());
}

TEST_F(ConfigRepositoryTest, LoadFileWithCommentsOnly) {
    write_config("# this is a comment\n// another comment\n");
    ConfigRepository repo;
    bool ok = repo.load(path_);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(repo.getRoot().root().children().empty());
}

TEST_F(ConfigRepositoryTest, LoadMultipleCalls) {
    write_config(R"(group web {
    controller cpu {
        item cpu.max = "50";
    }
})");
    ConfigRepository repo;
    EXPECT_TRUE(repo.load(path_));
    EXPECT_NE(repo.getRoot().root().findChild("web"), nullptr);

    write_config(R"(group db {
    controller memory {
        item memory.max = "2G";
    }
})");
    EXPECT_TRUE(repo.load(path_));
    // Old state gone
    EXPECT_EQ(repo.getRoot().root().findChild("web"), nullptr);
    EXPECT_NE(repo.getRoot().root().findChild("db"), nullptr);
}

// ── Errors are cleared on each operation ────────────────────────────────

TEST_F(ConfigRepositoryTest, ErrorsClearedOnReplace) {
    ConfigRepository repo;

    // Trigger an error
    repo.load("/tmp/nonexistent_requtao_file.conf");
    EXPECT_FALSE(repo.errors().empty());

    // replace clears errors
    ConfigDomain domain;
    repo.replace(std::move(domain));
    EXPECT_TRUE(repo.errors().empty());
}

TEST_F(ConfigRepositoryTest, ErrorsClearedOnLoad) {
    ConfigRepository repo;
    repo.load("/tmp/nonexistent_requtao_file.conf");
    EXPECT_FALSE(repo.errors().empty());

    write_config("group app { }");
    repo.load(path_);
    EXPECT_TRUE(repo.errors().empty());
}
