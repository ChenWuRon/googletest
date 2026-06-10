#include <gtest/gtest.h>
#include "resource_manager/lexer/lexer.h"

using namespace resource_manager;

// ── token_type_name ─────────────────────────────────────────────────────

TEST(TokenTypeNameTest, AllTypes) {
    EXPECT_EQ(token_type_name(TokenType::GROUP), "GROUP");
    EXPECT_EQ(token_type_name(TokenType::CONTROLLER), "CONTROLLER");
    EXPECT_EQ(token_type_name(TokenType::ITEM), "ITEM");
    EXPECT_EQ(token_type_name(TokenType::MODE), "MODE");
    EXPECT_EQ(token_type_name(TokenType::MATCH), "MATCH");
    EXPECT_EQ(token_type_name(TokenType::TYPE), "TYPE");
    EXPECT_EQ(token_type_name(TokenType::IDENTIFIER), "IDENTIFIER");
    EXPECT_EQ(token_type_name(TokenType::STRING), "STRING");
    EXPECT_EQ(token_type_name(TokenType::LBRACE), "LBRACE");
    EXPECT_EQ(token_type_name(TokenType::RBRACE), "RBRACE");
    EXPECT_EQ(token_type_name(TokenType::SEMICOLON), "SEMICOLON");
    EXPECT_EQ(token_type_name(TokenType::EQUALS), "EQUALS");
    EXPECT_EQ(token_type_name(TokenType::ERROR), "ERROR");
    EXPECT_EQ(token_type_name(TokenType::END_OF_FILE), "END_OF_FILE");
}

// ── Empty / Trivial ─────────────────────────────────────────────────────

TEST(LexerTest, EmptyInput) {
    Lexer lexer("");
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::END_OF_FILE);
}

TEST(LexerTest, OnlyWhitespace) {
    Lexer lexer("   \t  \n  \r  ");
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::END_OF_FILE);
}

TEST(LexerTest, OnlyLineCommentHash) {
    Lexer lexer("# this is a comment");
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::END_OF_FILE);
}

TEST(LexerTest, OnlyLineCommentSlash) {
    Lexer lexer("// this is a comment");
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::END_OF_FILE);
}

// ── Keywords ────────────────────────────────────────────────────────────

TEST(LexerTest, KeywordGroup) {
    Lexer lexer("group");
    auto tokens = lexer.tokenize();
    ASSERT_GE(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::GROUP);
    EXPECT_EQ(tokens[0].lexeme, "group");
}

TEST(LexerTest, KeywordController) {
    Lexer lexer("controller");
    auto tokens = lexer.tokenize();
    EXPECT_EQ(tokens[0].type, TokenType::CONTROLLER);
    EXPECT_EQ(tokens[0].lexeme, "controller");
}

TEST(LexerTest, KeywordItem) {
    Lexer lexer("item");
    auto tokens = lexer.tokenize();
    EXPECT_EQ(tokens[0].type, TokenType::ITEM);
    EXPECT_EQ(tokens[0].lexeme, "item");
}

TEST(LexerTest, KeywordMode) {
    Lexer lexer("mode");
    auto tokens = lexer.tokenize();
    EXPECT_EQ(tokens[0].type, TokenType::MODE);
    EXPECT_EQ(tokens[0].lexeme, "mode");
}

TEST(LexerTest, KeywordMatch) {
    Lexer lexer("match");
    auto tokens = lexer.tokenize();
    EXPECT_EQ(tokens[0].type, TokenType::MATCH);
    EXPECT_EQ(tokens[0].lexeme, "match");
}

TEST(LexerTest, KeywordType) {
    Lexer lexer("type");
    auto tokens = lexer.tokenize();
    EXPECT_EQ(tokens[0].type, TokenType::TYPE);
    EXPECT_EQ(tokens[0].lexeme, "type");
}

// ── Identifiers ─────────────────────────────────────────────────────────

TEST(LexerTest, IdentifierPlain) {
    Lexer lexer("mygroup");
    auto tokens = lexer.tokenize();
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].lexeme, "mygroup");
}

TEST(LexerTest, IdentifierWithHyphen) {
    Lexer lexer("web-server");
    auto tokens = lexer.tokenize();
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].lexeme, "web-server");
}

TEST(LexerTest, IdentifierWithDot) {
    Lexer lexer("cpu.max");
    auto tokens = lexer.tokenize();
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].lexeme, "cpu.max");
}

TEST(LexerTest, IdentifierWithUnderscore) {
    Lexer lexer("my_group_1");
    auto tokens = lexer.tokenize();
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].lexeme, "my_group_1");
}

TEST(LexerTest, IdentifierStartsWithUnderscore) {
    Lexer lexer("_private");
    auto tokens = lexer.tokenize();
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].lexeme, "_private");
}

// ── Punctuation ─────────────────────────────────────────────────────────

TEST(LexerTest, Punctuation) {
    Lexer lexer("{};=");
    auto tokens = lexer.tokenize();
    ASSERT_GE(tokens.size(), 4);
    EXPECT_EQ(tokens[0].type, TokenType::LBRACE);
    EXPECT_EQ(tokens[1].type, TokenType::RBRACE);
    EXPECT_EQ(tokens[2].type, TokenType::SEMICOLON);
    EXPECT_EQ(tokens[3].type, TokenType::EQUALS);
}

// ── Strings ─────────────────────────────────────────────────────────────

TEST(LexerTest, StringBasic) {
    Lexer lexer(R"("hello world")");
    auto tokens = lexer.tokenize();
    ASSERT_GE(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::STRING);
    EXPECT_EQ(tokens[0].lexeme, "hello world");
}

TEST(LexerTest, StringEmpty) {
    Lexer lexer(R"("")");
    auto tokens = lexer.tokenize();
    ASSERT_GE(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::STRING);
    EXPECT_TRUE(tokens[0].lexeme.empty());
}

TEST(LexerTest, StringWithSpecialChars) {
    Lexer lexer(R"("100000 100000")");
    auto tokens = lexer.tokenize();
    EXPECT_EQ(tokens[0].type, TokenType::STRING);
    EXPECT_EQ(tokens[0].lexeme, "100000 100000");
}

TEST(LexerTest, StringUnterminated) {
    Lexer lexer(R"("unterminated)");
    auto tokens = lexer.tokenize();
    EXPECT_EQ(tokens[0].type, TokenType::ERROR);
}

TEST(LexerTest, StringNewlineInside) {
    Lexer lexer("\"line1\nline2\"");
    auto tokens = lexer.tokenize();
    EXPECT_EQ(tokens[0].type, TokenType::ERROR);
}

// ── Comments ────────────────────────────────────────────────────────────

TEST(LexerTest, CommentHashMixed) {
    Lexer lexer("group # comment\ncontroller");
    auto tokens = lexer.tokenize();
    ASSERT_GE(tokens.size(), 2);
    EXPECT_EQ(tokens[0].type, TokenType::GROUP);
    EXPECT_EQ(tokens[1].type, TokenType::CONTROLLER);
}

TEST(LexerTest, CommentSlashMixed) {
    Lexer lexer("group // comment\ncontroller");
    auto tokens = lexer.tokenize();
    ASSERT_GE(tokens.size(), 2);
    EXPECT_EQ(tokens[0].type, TokenType::GROUP);
    EXPECT_EQ(tokens[1].type, TokenType::CONTROLLER);
}

TEST(LexerTest, CommentHashAtEndOfFile) {
    Lexer lexer("item # no newline at end");
    auto tokens = lexer.tokenize();
    ASSERT_GE(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::ITEM);
    EXPECT_EQ(tokens[1].type, TokenType::END_OF_FILE);
}

// ── Error handling ──────────────────────────────────────────────────────

TEST(LexerTest, InvalidCharacter) {
    Lexer lexer("@");
    auto tokens = lexer.tokenize();
    ASSERT_GE(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::ERROR);
    EXPECT_EQ(tokens[0].lexeme, "@");
}

TEST(LexerTest, MultipleInvalidCharacters) {
    Lexer lexer("@$!");
    auto tokens = lexer.tokenize();
    ASSERT_GE(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type, TokenType::ERROR);
    EXPECT_EQ(tokens[1].type, TokenType::ERROR);
    EXPECT_EQ(tokens[2].type, TokenType::ERROR);
}

// ── Line / Column tracking ──────────────────────────────────────────────

TEST(LexerTest, LineColumnBasic) {
    Lexer lexer("group\n\tcontroller");
    auto tokens = lexer.tokenize();
    ASSERT_GE(tokens.size(), 2);
    EXPECT_EQ(tokens[0].type, TokenType::GROUP);
    EXPECT_EQ(tokens[0].line, 1);
    EXPECT_EQ(tokens[0].column, 1);
    EXPECT_EQ(tokens[1].type, TokenType::CONTROLLER);
    EXPECT_EQ(tokens[1].line, 2);
    EXPECT_EQ(tokens[1].column, 2);
}

// ── Full example ────────────────────────────────────────────────────────

TEST(LexerTest, FullGroupExample) {
    std::string input = R"(
group web-server {
    mode service;
    match "nginx" {
        type prefix;
    }
    controller cpu {
        item cpu.max = "100000 100000";
    }
}
)";
    Lexer lexer(input);
    auto tokens = lexer.tokenize();

    auto check_token = [&](std::size_t idx, TokenType type,
                           const std::string& lexeme) {
        ASSERT_LT(idx, tokens.size());
        EXPECT_EQ(tokens[idx].type, type) << "at index " << idx;
        EXPECT_EQ(tokens[idx].lexeme, lexeme) << "at index " << idx;
    };

    std::size_t i = 0;
    check_token(i++, TokenType::GROUP, "group");
    check_token(i++, TokenType::IDENTIFIER, "web-server");
    check_token(i++, TokenType::LBRACE, "{");
    check_token(i++, TokenType::MODE, "mode");
    check_token(i++, TokenType::IDENTIFIER, "service");
    check_token(i++, TokenType::SEMICOLON, ";");
    check_token(i++, TokenType::MATCH, "match");
    check_token(i++, TokenType::STRING, "nginx");
    check_token(i++, TokenType::LBRACE, "{");
    check_token(i++, TokenType::TYPE, "type");
    check_token(i++, TokenType::IDENTIFIER, "prefix");
    check_token(i++, TokenType::SEMICOLON, ";");
    check_token(i++, TokenType::RBRACE, "}");
    check_token(i++, TokenType::CONTROLLER, "controller");
    check_token(i++, TokenType::IDENTIFIER, "cpu");
    check_token(i++, TokenType::LBRACE, "{");
    check_token(i++, TokenType::ITEM, "item");
    check_token(i++, TokenType::IDENTIFIER, "cpu.max");
    check_token(i++, TokenType::EQUALS, "=");
    check_token(i++, TokenType::STRING, "100000 100000");
    check_token(i++, TokenType::SEMICOLON, ";");
    check_token(i++, TokenType::RBRACE, "}");
    check_token(i++, TokenType::RBRACE, "}");
    EXPECT_EQ(tokens[i].type, TokenType::END_OF_FILE);
}

TEST(LexerTest, FullGroupExampleHashComments) {
    std::string input = R"(# Resource Manager Config
group web-server {
    mode service; # inline comment

    match "nginx" {
        type prefix;
    }

    controller cpu {
        item cpu.max = "100000 100000";
        item cpu.weight = "100";
    }

}
)";
    Lexer lexer(input);
    auto tokens = lexer.tokenize();

    EXPECT_EQ(tokens[0].type, TokenType::GROUP);
    EXPECT_EQ(tokens[1].lexeme, "web-server");
    EXPECT_EQ(tokens[2].type, TokenType::LBRACE);
    EXPECT_EQ(tokens[3].type, TokenType::MODE);
    EXPECT_EQ(tokens[4].lexeme, "service");
    EXPECT_EQ(tokens[5].type, TokenType::SEMICOLON);
    // match block
    EXPECT_EQ(tokens[6].type, TokenType::MATCH);
    EXPECT_EQ(tokens[7].lexeme, "nginx");
    EXPECT_EQ(tokens[8].type, TokenType::LBRACE);
    EXPECT_EQ(tokens[9].type, TokenType::TYPE);
    EXPECT_EQ(tokens[10].lexeme, "prefix");
    EXPECT_EQ(tokens[11].type, TokenType::SEMICOLON);
    EXPECT_EQ(tokens[12].type, TokenType::RBRACE);
    // controller cpu
    EXPECT_EQ(tokens[13].type, TokenType::CONTROLLER);
    EXPECT_EQ(tokens[14].lexeme, "cpu");
    EXPECT_EQ(tokens[15].type, TokenType::LBRACE);
    EXPECT_EQ(tokens[16].type, TokenType::ITEM);
    EXPECT_EQ(tokens[17].lexeme, "cpu.max");
    EXPECT_EQ(tokens[18].type, TokenType::EQUALS);
    EXPECT_EQ(tokens[19].lexeme, "100000 100000");
    EXPECT_EQ(tokens[20].type, TokenType::SEMICOLON);
    EXPECT_EQ(tokens[21].type, TokenType::ITEM);
    EXPECT_EQ(tokens[22].lexeme, "cpu.weight");
    EXPECT_EQ(tokens[23].type, TokenType::EQUALS);
    EXPECT_EQ(tokens[24].lexeme, "100");
    EXPECT_EQ(tokens[25].type, TokenType::SEMICOLON);
    EXPECT_EQ(tokens[26].type, TokenType::RBRACE);
    EXPECT_EQ(tokens[27].type, TokenType::RBRACE);
    EXPECT_EQ(tokens[28].type, TokenType::END_OF_FILE);
}

// ── Edge cases ──────────────────────────────────────────────────────────

TEST(LexerTest, SlashNotComment) {
    Lexer lexer("x/y");
    auto tokens = lexer.tokenize();
    ASSERT_GE(tokens.size(), 2);
    EXPECT_EQ(tokens[0].lexeme, "x/y");
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[1].type, TokenType::END_OF_FILE);
}

TEST(LexerTest, SingleSlashAtEnd) {
    Lexer lexer("/");
    auto tokens = lexer.tokenize();
    ASSERT_GE(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::ERROR);
    EXPECT_EQ(tokens[0].lexeme, "/");
}

TEST(LexerTest, TokenOrder) {
    Lexer lexer("group a { item b = \"c\"; }");
    auto tokens = lexer.tokenize();
    ASSERT_GE(tokens.size(), 8);
    EXPECT_EQ(tokens[0].type, TokenType::GROUP);
    EXPECT_EQ(tokens[1].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[2].type, TokenType::LBRACE);
    EXPECT_EQ(tokens[3].type, TokenType::ITEM);
    EXPECT_EQ(tokens[4].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[5].type, TokenType::EQUALS);
    EXPECT_EQ(tokens[6].type, TokenType::STRING);
    EXPECT_EQ(tokens[7].type, TokenType::SEMICOLON);
    EXPECT_EQ(tokens[8].type, TokenType::RBRACE);
    EXPECT_EQ(tokens[9].type, TokenType::END_OF_FILE);
}

// ── Multiple groups ─────────────────────────────────────────────────────

TEST(LexerTest, TwoGroups) {
    std::string input = R"(
group web {
    mode service;
}
group db {
    mode entity;
}
)";
    Lexer lexer(input);
    auto tokens = lexer.tokenize();

    EXPECT_EQ(tokens[0].type, TokenType::GROUP);
    EXPECT_EQ(tokens[1].lexeme, "web");
    EXPECT_EQ(tokens[2].type, TokenType::LBRACE);
    EXPECT_EQ(tokens[3].type, TokenType::MODE);
    EXPECT_EQ(tokens[4].lexeme, "service");
    EXPECT_EQ(tokens[5].type, TokenType::SEMICOLON);
    EXPECT_EQ(tokens[6].type, TokenType::RBRACE);

    EXPECT_EQ(tokens[7].type, TokenType::GROUP);
    EXPECT_EQ(tokens[8].lexeme, "db");
    EXPECT_EQ(tokens[9].type, TokenType::LBRACE);
    EXPECT_EQ(tokens[10].type, TokenType::MODE);
    EXPECT_EQ(tokens[11].lexeme, "entity");
    EXPECT_EQ(tokens[12].type, TokenType::SEMICOLON);
    EXPECT_EQ(tokens[13].type, TokenType::RBRACE);

    EXPECT_EQ(tokens[14].type, TokenType::END_OF_FILE);
}

TEST(LexerTest, MultipleControllers) {
    std::string input = R"(
group app {
    controller cpu {
        item cpu.max = "100000 100000";
    }
    controller memory {
        item memory.max = "1G";
    }
}
)";
    Lexer lexer(input);
    auto tokens = lexer.tokenize();

    EXPECT_EQ(tokens[0].type, TokenType::GROUP);
    EXPECT_EQ(tokens[1].lexeme, "app");

    EXPECT_EQ(tokens[3].type, TokenType::CONTROLLER);
    EXPECT_EQ(tokens[4].lexeme, "cpu");
    EXPECT_EQ(tokens[6].type, TokenType::ITEM);
    EXPECT_EQ(tokens[7].lexeme, "cpu.max");

    EXPECT_EQ(tokens[12].type, TokenType::CONTROLLER);
    EXPECT_EQ(tokens[13].lexeme, "memory");
    EXPECT_EQ(tokens[15].type, TokenType::ITEM);
    EXPECT_EQ(tokens[16].lexeme, "memory.max");
}
