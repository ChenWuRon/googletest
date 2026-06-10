#pragma once

#include <string>
#include <cstddef>

namespace resource_manager {

enum class TokenType {
    GROUP,
    CONTROLLER,
    ITEM,
    MODE,
    MATCH,
    TYPE,

    IDENTIFIER,

    STRING,

    LBRACE,
    RBRACE,
    SEMICOLON,
    EQUALS,

    ERROR,
    END_OF_FILE,
};

struct Token {
    TokenType type;
    std::string lexeme;
    std::size_t line;
    std::size_t column;
};

std::string token_type_name(TokenType type);

} // namespace resource_manager
