#include <gtest/gtest.h>
#include "resource_manager/core/parser.h"

using namespace resource_manager;

// ── helpers ─────────────────────────────────────────────────────────────

Token make(TokenType type, std::string lexeme,
           std::size_t line = 1, std::size_t col = 1) {
    return {type, std::move(lexeme), line, col};
}

std::vector<Token> tokens_from(std::initializer_list<Token> ts) {
    std::vector<Token> out(ts);
    out.push_back(make(TokenType::END_OF_FILE, ""));
    return out;
}

// ── Empty / Minimal ─────────────────────────────────────────────────────

TEST(ParserTest, EmptyInput) {
    Parser parser({make(TokenType::END_OF_FILE, "")});
    auto result = parser.parse();
    EXPECT_TRUE(result.errors.empty());
    EXPECT_TRUE(result.domain.root().children().empty());
}

TEST(ParserTest, SingleGroupEmptyBody) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "web"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_TRUE(result.errors.empty());

    auto* g = result.domain.root().findChild("web");
    ASSERT_NE(g, nullptr);
    EXPECT_EQ(g->type(), ConfigNodeType::GROUP);
    EXPECT_TRUE(g->children().empty());
}

TEST(ParserTest, TwoGroupsEmptyBody) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "a"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "b"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_TRUE(result.errors.empty());
    EXPECT_NE(result.domain.root().findChild("a"), nullptr);
    EXPECT_NE(result.domain.root().findChild("b"), nullptr);
}

// ── Mode statement ──────────────────────────────────────────────────────

TEST(ParserTest, GroupWithMode) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "app"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::MODE, "mode"),
        make(TokenType::IDENTIFIER, "service"),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_TRUE(result.errors.empty());
    ASSERT_NE(result.domain.root().findChild("app"), nullptr);
}

TEST(ParserTest, GroupWithModeMissingValue) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "x"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::MODE, "mode"),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_FALSE(result.errors.empty());
}

TEST(ParserTest, GroupWithModeMissingSemicolon) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "x"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::MODE, "mode"),
        make(TokenType::IDENTIFIER, "service"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_FALSE(result.errors.empty());
}

// ── Match statement ─────────────────────────────────────────────────────

TEST(ParserTest, GroupWithMatch) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "app"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::MATCH, "match"),
        make(TokenType::STRING, "nginx"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::TYPE, "type"),
        make(TokenType::IDENTIFIER, "prefix"),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_TRUE(result.errors.empty());

    auto* g = result.domain.root().findChild("app");
    ASSERT_NE(g, nullptr);
    auto rule = g->getMatchRule();
    ASSERT_TRUE(rule.has_value());
    EXPECT_EQ(rule->pattern, "nginx");
    EXPECT_EQ(rule->type, "prefix");
}

TEST(ParserTest, GroupWithMatchMissingPattern) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "x"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::MATCH, "match"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::TYPE, "type"),
        make(TokenType::IDENTIFIER, "prefix"),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_FALSE(result.errors.empty());
}

// ── Controller / Item ───────────────────────────────────────────────────

TEST(ParserTest, SingleControllerSingleItem) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "svc"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::CONTROLLER, "controller"),
        make(TokenType::IDENTIFIER, "cpu"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::ITEM, "item"),
        make(TokenType::IDENTIFIER, "cpu.max"),
        make(TokenType::EQUALS, "="),
        make(TokenType::STRING, "100000 100000"),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    ASSERT_TRUE(result.errors.empty());

    auto* g = result.domain.root().findChild("svc");
    ASSERT_NE(g, nullptr);
    auto* c = g->findChild("cpu");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->type(), ConfigNodeType::CONTROLLER);
    auto* i = c->findChild("cpu.max");
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->type(), ConfigNodeType::ITEM);
    EXPECT_EQ(i->value(), "100000 100000");
}

TEST(ParserTest, ControllerWithMultipleItems) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "svc"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::CONTROLLER, "controller"),
        make(TokenType::IDENTIFIER, "cpu"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::ITEM, "item"),
        make(TokenType::IDENTIFIER, "cpu.max"),
        make(TokenType::EQUALS, "="),
        make(TokenType::STRING, "100000 100000"),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::ITEM, "item"),
        make(TokenType::IDENTIFIER, "cpu.weight"),
        make(TokenType::EQUALS, "="),
        make(TokenType::STRING, "100"),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    ASSERT_TRUE(result.errors.empty());

    auto* c = result.domain.root().findChild("svc")->findChild("cpu");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->children().size(), 2);
    EXPECT_EQ(c->findChild("cpu.max")->value(), "100000 100000");
    EXPECT_EQ(c->findChild("cpu.weight")->value(), "100");
}

TEST(ParserTest, MultipleControllers) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "svc"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::CONTROLLER, "controller"),
        make(TokenType::IDENTIFIER, "cpu"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::CONTROLLER, "controller"),
        make(TokenType::IDENTIFIER, "memory"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    ASSERT_TRUE(result.errors.empty());

    auto* g = result.domain.root().findChild("svc");
    ASSERT_NE(g, nullptr);
    EXPECT_EQ(g->children().size(), 2);
    EXPECT_NE(g->findChild("cpu"), nullptr);
    EXPECT_NE(g->findChild("memory"), nullptr);
}

// ── Full example ────────────────────────────────────────────────────────

TEST(ParserTest, FullExample) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "web-server"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::MODE, "mode"),
        make(TokenType::IDENTIFIER, "service"),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::MATCH, "match"),
        make(TokenType::STRING, "nginx"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::TYPE, "type"),
        make(TokenType::IDENTIFIER, "prefix"),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::CONTROLLER, "controller"),
        make(TokenType::IDENTIFIER, "cpu"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::ITEM, "item"),
        make(TokenType::IDENTIFIER, "cpu.max"),
        make(TokenType::EQUALS, "="),
        make(TokenType::STRING, "100000 100000"),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::ITEM, "item"),
        make(TokenType::IDENTIFIER, "cpu.weight"),
        make(TokenType::EQUALS, "="),
        make(TokenType::STRING, "100"),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::CONTROLLER, "controller"),
        make(TokenType::IDENTIFIER, "memory"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::ITEM, "item"),
        make(TokenType::IDENTIFIER, "memory.max"),
        make(TokenType::EQUALS, "="),
        make(TokenType::STRING, "1G"),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::ITEM, "item"),
        make(TokenType::IDENTIFIER, "memory.high"),
        make(TokenType::EQUALS, "="),
        make(TokenType::STRING, "800M"),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "db-cluster"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::MODE, "mode"),
        make(TokenType::IDENTIFIER, "entity"),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::MATCH, "match"),
        make(TokenType::STRING, "mysqld"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::TYPE, "type"),
        make(TokenType::IDENTIFIER, "exact"),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::CONTROLLER, "controller"),
        make(TokenType::IDENTIFIER, "cpu"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::ITEM, "item"),
        make(TokenType::IDENTIFIER, "cpu.max"),
        make(TokenType::EQUALS, "="),
        make(TokenType::STRING, "200000 100000"),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::CONTROLLER, "controller"),
        make(TokenType::IDENTIFIER, "pids"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::ITEM, "item"),
        make(TokenType::IDENTIFIER, "pids.max"),
        make(TokenType::EQUALS, "="),
        make(TokenType::STRING, "1024"),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    ASSERT_TRUE(result.errors.empty());

    auto* web = result.domain.root().findChild("web-server");
    ASSERT_NE(web, nullptr);
    EXPECT_EQ(web->type(), ConfigNodeType::GROUP);
    EXPECT_EQ(web->children().size(), 2);
    ASSERT_NE(web->findChild("cpu"), nullptr);
    ASSERT_NE(web->findChild("memory"), nullptr);
    ASSERT_NE(web->findChild("cpu")->findChild("cpu.max"), nullptr);
    EXPECT_EQ(web->findChild("cpu")->findChild("cpu.max")->value(), "100000 100000");

    {
        auto rule = web->getMatchRule();
        ASSERT_TRUE(rule.has_value());
        EXPECT_EQ(rule->pattern, "nginx");
        EXPECT_EQ(rule->type, "prefix");
    }

    auto* db = result.domain.root().findChild("db-cluster");
    ASSERT_NE(db, nullptr);
    EXPECT_EQ(db->children().size(), 2);
    ASSERT_NE(db->findChild("pids"), nullptr);
    EXPECT_EQ(db->findChild("pids")->findChild("pids.max")->value(), "1024");

    {
        auto rule = db->getMatchRule();
        ASSERT_TRUE(rule.has_value());
        EXPECT_EQ(rule->pattern, "mysqld");
        EXPECT_EQ(rule->type, "exact");
    }
}

TEST(ParserTest, ExampleConfEquivalence) {
    // This is the example.conf content as tokens
    auto ts = tokens_from({
        make(TokenType::GROUP, "group", 4,1),
        make(TokenType::IDENTIFIER, "web-server", 4,6),
        make(TokenType::LBRACE, "{", 4,17),
        make(TokenType::MODE, "mode", 5,5),
        make(TokenType::IDENTIFIER, "service", 5,10),
        make(TokenType::SEMICOLON, ";", 5,17),
        make(TokenType::MATCH, "match", 7,5),
        make(TokenType::STRING, "nginx", 7,11),
        make(TokenType::LBRACE, "{", 7,18),
        make(TokenType::TYPE, "type", 8,9),
        make(TokenType::IDENTIFIER, "prefix", 8,14),
        make(TokenType::SEMICOLON, ";", 8,20),
        make(TokenType::RBRACE, "}", 9,5),
        make(TokenType::CONTROLLER, "controller", 11,5),
        make(TokenType::IDENTIFIER, "cpu", 11,16),
        make(TokenType::LBRACE, "{", 11,20),
        make(TokenType::ITEM, "item", 12,9),
        make(TokenType::IDENTIFIER, "cpu.max", 12,14),
        make(TokenType::EQUALS, "=", 12,22),
        make(TokenType::STRING, "100000 100000", 12,24),
        make(TokenType::SEMICOLON, ";", 12,39),
        make(TokenType::ITEM, "item", 13,9),
        make(TokenType::IDENTIFIER, "cpu.weight", 13,14),
        make(TokenType::EQUALS, "=", 13,25),
        make(TokenType::STRING, "100", 13,27),
        make(TokenType::SEMICOLON, ";", 13,32),
        make(TokenType::RBRACE, "}", 14,5),
        make(TokenType::CONTROLLER, "controller", 16,5),
        make(TokenType::IDENTIFIER, "memory", 16,16),
        make(TokenType::LBRACE, "{", 16,23),
        make(TokenType::ITEM, "item", 17,9),
        make(TokenType::IDENTIFIER, "memory.max", 17,14),
        make(TokenType::EQUALS, "=", 17,25),
        make(TokenType::STRING, "1G", 17,27),
        make(TokenType::SEMICOLON, ";", 17,31),
        make(TokenType::ITEM, "item", 18,9),
        make(TokenType::IDENTIFIER, "memory.high", 18,14),
        make(TokenType::EQUALS, "=", 18,26),
        make(TokenType::STRING, "800M", 18,28),
        make(TokenType::SEMICOLON, ";", 18,33),
        make(TokenType::RBRACE, "}", 19,5),
        make(TokenType::CONTROLLER, "controller", 21,5),
        make(TokenType::IDENTIFIER, "cpuset", 21,16),
        make(TokenType::LBRACE, "{", 21,23),
        make(TokenType::ITEM, "item", 22,9),
        make(TokenType::IDENTIFIER, "cpuset.cpus", 22,14),
        make(TokenType::EQUALS, "=", 22,26),
        make(TokenType::STRING, "0-3", 22,28),
        make(TokenType::SEMICOLON, ";", 22,33),
        make(TokenType::ITEM, "item", 23,9),
        make(TokenType::IDENTIFIER, "cpuset.mems", 23,14),
        make(TokenType::EQUALS, "=", 23,26),
        make(TokenType::STRING, "0", 23,28),
        make(TokenType::SEMICOLON, ";", 23,31),
        make(TokenType::RBRACE, "}", 24,5),
        make(TokenType::RBRACE, "}", 25,1),
    });
    Parser parser(ts);
    auto result = parser.parse();
    ASSERT_TRUE(result.errors.empty());

    auto* web = result.domain.root().findChild("web-server");
    ASSERT_NE(web, nullptr);
    EXPECT_EQ(web->children().size(), 3); // cpu, memory, cpuset
    EXPECT_NE(web->findChild("cpuset"), nullptr);
    EXPECT_EQ(web->findChild("cpuset")->findChild("cpuset.cpus")->value(), "0-3");

    auto rule = web->getMatchRule();
    ASSERT_TRUE(rule.has_value());
    EXPECT_EQ(rule->pattern, "nginx");
    EXPECT_EQ(rule->type, "prefix");
}

// ── Duplicates ──────────────────────────────────────────────────────────

TEST(ParserTest, DuplicateGroup) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "dup"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "dup"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    ASSERT_FALSE(result.errors.empty());
    bool found_dup_error = false;
    for (const auto& e : result.errors) {
        if (e.message.find("duplicate") != std::string::npos) {
            found_dup_error = true;
            break;
        }
    }
    EXPECT_TRUE(found_dup_error);
}

TEST(ParserTest, DuplicateController) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "svc"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::CONTROLLER, "controller"),
        make(TokenType::IDENTIFIER, "cpu"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::CONTROLLER, "controller"),
        make(TokenType::IDENTIFIER, "cpu"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    ASSERT_FALSE(result.errors.empty());
    bool found_dup = false;
    for (const auto& e : result.errors) {
        if (e.message.find("duplicate") != std::string::npos) found_dup = true;
    }
    EXPECT_TRUE(found_dup);
}

TEST(ParserTest, DuplicateItem) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "svc"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::CONTROLLER, "controller"),
        make(TokenType::IDENTIFIER, "cpu"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::ITEM, "item"),
        make(TokenType::IDENTIFIER, "cpu.max"),
        make(TokenType::EQUALS, "="),
        make(TokenType::STRING, "100"),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::ITEM, "item"),
        make(TokenType::IDENTIFIER, "cpu.max"),
        make(TokenType::EQUALS, "="),
        make(TokenType::STRING, "200"),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    ASSERT_FALSE(result.errors.empty());
    bool found_dup = false;
    for (const auto& e : result.errors) {
        if (e.message.find("duplicate") != std::string::npos) found_dup = true;
    }
    EXPECT_TRUE(found_dup);

    auto* c = result.domain.root().findChild("svc")->findChild("cpu");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->children().size(), 1);
}

// ── Missing elements ────────────────────────────────────────────────────

TEST(ParserTest, MissingGroupName) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_FALSE(result.errors.empty());
}

TEST(ParserTest, MissingOpeningBraceAfterGroup) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "svc"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_FALSE(result.errors.empty());
}

TEST(ParserTest, MissingClosingBrace) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "svc"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::RBRACE, "}"),
        // no second RBRACE for group — group body is closed prematurely
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "other"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();

    EXPECT_NE(result.domain.root().findChild("svc"), nullptr);
    EXPECT_NE(result.domain.root().findChild("other"), nullptr);
}

TEST(ParserTest, MissingItemEquals) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "svc"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::CONTROLLER, "controller"),
        make(TokenType::IDENTIFIER, "cpu"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::ITEM, "item"),
        make(TokenType::IDENTIFIER, "cpu.max"),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_FALSE(result.errors.empty());
}

TEST(ParserTest, MissingItemValue) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "svc"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::CONTROLLER, "controller"),
        make(TokenType::IDENTIFIER, "cpu"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::ITEM, "item"),
        make(TokenType::IDENTIFIER, "cpu.max"),
        make(TokenType::EQUALS, "="),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_FALSE(result.errors.empty());
}

TEST(ParserTest, MissingItemSemicolon) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "svc"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::CONTROLLER, "controller"),
        make(TokenType::IDENTIFIER, "cpu"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::ITEM, "item"),
        make(TokenType::IDENTIFIER, "cpu.max"),
        make(TokenType::EQUALS, "="),
        make(TokenType::STRING, "100"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_FALSE(result.errors.empty());
}

// ── Nested errors ───────────────────────────────────────────────────────

TEST(ParserTest, NestedGroupError) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "outer"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "inner"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_FALSE(result.errors.empty());

    auto* outer = result.domain.root().findChild("outer");
    ASSERT_NE(outer, nullptr);
}

TEST(ParserTest, NestedControllerError) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "svc"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::CONTROLLER, "controller"),
        make(TokenType::IDENTIFIER, "cpu"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::CONTROLLER, "controller"),
        make(TokenType::IDENTIFIER, "nested"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_FALSE(result.errors.empty());
}

// ── Error token in stream ───────────────────────────────────────────────

TEST(ParserTest, TokenStreamWithError) {
    // The token stream already contains an ERROR token; parser should report it
    auto ts = tokens_from({
        make(TokenType::ERROR, "@"),   // becomes unexpected token at top level
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "svc"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_FALSE(result.errors.empty());
}

// ── Error recovery: unexpected token sync ───────────────────────────────

TEST(ParserTest, UnexpectedTokenInGroupBody) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "svc"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::IDENTIFIER, "bogus"),
        make(TokenType::CONTROLLER, "controller"),
        make(TokenType::IDENTIFIER, "cpu"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_FALSE(result.errors.empty());

    auto* g = result.domain.root().findChild("svc");
    ASSERT_NE(g, nullptr);
    auto* c = g->findChild("cpu");
    EXPECT_NE(c, nullptr);
}

TEST(ParserTest, UnexpectedTokenInControllerBody) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "svc"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::CONTROLLER, "controller"),
        make(TokenType::IDENTIFIER, "cpu"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::MODE, "mode"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_FALSE(result.errors.empty());
}

// ── Line / Column accuracy ──────────────────────────────────────────────

TEST(ParserTest, ErrorLineColumn) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group", 4, 1),
        make(TokenType::IDENTIFIER, "dup", 4, 7),
        make(TokenType::LBRACE, "{", 4, 11),
        make(TokenType::RBRACE, "}", 4, 12),
        make(TokenType::GROUP, "group", 8, 1),
        make(TokenType::IDENTIFIER, "dup", 8, 7),
        make(TokenType::LBRACE, "{", 8, 11),
        make(TokenType::RBRACE, "}", 8, 12),
    });
    Parser parser(ts);
    auto result = parser.parse();
    ASSERT_FALSE(result.errors.empty());

    bool found_dup = false;
    for (const auto& e : result.errors) {
        if (e.message.find("duplicate") != std::string::npos) {
            EXPECT_EQ(e.line, 8);
            EXPECT_EQ(e.column, 7);
            found_dup = true;
        }
    }
    EXPECT_TRUE(found_dup);
}

// ── Empty string value ──────────────────────────────────────────────────

TEST(ParserTest, ItemEmptyStringValue) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "svc"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::CONTROLLER, "controller"),
        make(TokenType::IDENTIFIER, "test"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::ITEM, "item"),
        make(TokenType::IDENTIFIER, "flag"),
        make(TokenType::EQUALS, "="),
        make(TokenType::STRING, ""),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    ASSERT_TRUE(result.errors.empty());

    auto* i = result.domain.root().findChild("svc")->findChild("test")->findChild("flag");
    ASSERT_NE(i, nullptr);
    EXPECT_TRUE(i->value().empty());
}

// ── Match with type wildcard ────────────────────────────────────────────

TEST(ParserTest, MatchTypeWildcard) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "svc"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::MATCH, "match"),
        make(TokenType::STRING, "nginx*"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::TYPE, "type"),
        make(TokenType::IDENTIFIER, "wildcard"),
        make(TokenType::SEMICOLON, ";"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_TRUE(result.errors.empty());

    auto* g = result.domain.root().findChild("svc");
    ASSERT_NE(g, nullptr);
    auto rule = g->getMatchRule();
    ASSERT_TRUE(rule.has_value());
    EXPECT_EQ(rule->pattern, "nginx*");
    EXPECT_EQ(rule->type, "wildcard");
}

// ── Multiple errors collected ───────────────────────────────────────────

TEST(ParserTest, MultipleErrorsCollected) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "svc"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::MODE, "mode"),
        // missing mode value and semicolon
        make(TokenType::CONTROLLER, "controller"),
        make(TokenType::IDENTIFIER, "cpu"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::ITEM, "item"),
        make(TokenType::IDENTIFIER, "x"),
        make(TokenType::EQUALS, "="),
        // missing value
        make(TokenType::RBRACE, "}"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_GE(result.errors.size(), 2);
}

// ── Unexpected EOF ──────────────────────────────────────────────────────

TEST(ParserTest, UnexpectedEofInGroup) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "svc"),
        make(TokenType::LBRACE, "{"),
        // no RBRACE before EOF
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_FALSE(result.errors.empty());
}

TEST(ParserTest, UnexpectedEofInMatch) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "svc"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::MATCH, "match"),
        make(TokenType::STRING, "pat"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::TYPE, "type"),
        make(TokenType::IDENTIFIER, "exact"),
        // no semicolon, no RBRACE
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_FALSE(result.errors.empty());
}

// ── Extra tokens after last group ───────────────────────────────────────

TEST(ParserTest, ExtraTokensAfterValidGroup) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "svc"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::IDENTIFIER, "stray"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_FALSE(result.errors.empty());
}

// ── Single controller without items ─────────────────────────────────────

TEST(ParserTest, ControllerNoItems) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "svc"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::CONTROLLER, "controller"),
        make(TokenType::IDENTIFIER, "cpu"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::RBRACE, "}"),
        make(TokenType::RBRACE, "}"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_TRUE(result.errors.empty());

    auto* c = result.domain.root().findChild("svc")->findChild("cpu");
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->children().empty());
}

// ── Group with mode and match and controller ────────────────────────────

TEST(ParserTest, GroupWithModeMatchController) {
    auto ts = tokens_from({
        make(TokenType::GROUP, "group"),
        make(TokenType::IDENTIFIER, "full"),
        make(TokenType::LBRACE, "{"),
        make(TokenType::MODE, "mode", 2, 5),
        make(TokenType::IDENTIFIER, "service", 2, 10),
        make(TokenType::SEMICOLON, ";", 2, 18),
        make(TokenType::MATCH, "match", 3, 5),
        make(TokenType::STRING, "app", 3, 11),
        make(TokenType::LBRACE, "{", 3, 16),
        make(TokenType::TYPE, "type", 4, 9),
        make(TokenType::IDENTIFIER, "exact", 4, 14),
        make(TokenType::SEMICOLON, ";", 4, 20),
        make(TokenType::RBRACE, "}", 5, 5),
        make(TokenType::CONTROLLER, "controller", 7, 5),
        make(TokenType::IDENTIFIER, "cpu", 7, 16),
        make(TokenType::LBRACE, "{", 7, 20),
        make(TokenType::RBRACE, "}", 8, 5),
        make(TokenType::RBRACE, "}", 10, 1),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_TRUE(result.errors.empty());
    auto* full = result.domain.root().findChild("full");
    ASSERT_NE(full, nullptr);

    auto rule = full->getMatchRule();
    ASSERT_TRUE(rule.has_value());
    EXPECT_EQ(rule->pattern, "app");
    EXPECT_EQ(rule->type, "exact");
}

// ── Only unexpected token at top level ──────────────────────────────────

TEST(ParserTest, OnlyUnexpectedAtTopLevel) {
    auto ts = tokens_from({
        make(TokenType::IDENTIFIER, "stray"),
    });
    Parser parser(ts);
    auto result = parser.parse();
    EXPECT_FALSE(result.errors.empty());
    EXPECT_TRUE(result.domain.root().children().empty());
}
