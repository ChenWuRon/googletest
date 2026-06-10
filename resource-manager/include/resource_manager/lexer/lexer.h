#pragma once

#include <string>
#include <vector>

#include "resource_manager/lexer/token.h"

namespace resource_manager {

class Lexer {
public:
    explicit Lexer(std::string source);

    std::vector<Token> tokenize();

private:
    char peek() const;
    char advance();
    void skip_whitespace();
    void skip_line_comment();
    void skip_block_comment();

    Token read_identifier_or_keyword();
    Token read_string();
    Token read_error(std::string message);

    bool is_at_end() const;

    std::string source_;
    std::size_t pos_;
    std::size_t line_;
    std::size_t column_;
};

} // namespace resource_manager
