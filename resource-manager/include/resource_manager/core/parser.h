#pragma once

#include <string>
#include <vector>
#include <cstddef>

#include "resource_manager/core/config_domain.h"
#include "resource_manager/lexer/token.h"

namespace resource_manager {

struct ParseError {
    std::size_t line;
    std::size_t column;
    std::string message;
};

struct ParseResult {
    ConfigDomain domain;
    std::vector<ParseError> errors;
};

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);

    ParseResult parse();

private:
    std::size_t pos_;
    std::vector<Token> tokens_;
    std::vector<ParseError> errors_;

    const Token& peek() const;
    const Token& advance();
    bool match(TokenType type);
    bool check(TokenType type) const;
    bool is_at_end() const;
    const Token& previous() const;

    void error(const Token& token, std::string message);

    ConfigNode* parse_group(ConfigNode& parent);
    void parse_group_body(ConfigNode& group);
    void parse_mode_statement();
    void parse_match_statement();
    void parse_controller(ConfigNode& parent);
    void parse_item(ConfigNode& parent);

    void synchronize_group_body();
    void synchronize_controller_body();
    void synchronize_to_semicolon();
};

} // namespace resource_manager
