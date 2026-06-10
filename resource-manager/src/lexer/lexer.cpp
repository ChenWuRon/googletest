#include "resource_manager/lexer/lexer.h"

#include <unordered_map>

namespace resource_manager {

namespace {

const std::unordered_map<std::string, TokenType> keywords = {
    {"group",      TokenType::GROUP},
    {"controller", TokenType::CONTROLLER},
    {"item",       TokenType::ITEM},
    {"mode",       TokenType::MODE},
    {"match",      TokenType::MATCH},
    {"type",       TokenType::TYPE},
};

bool is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

bool is_ident_continue(char c) {
    return is_ident_start(c) ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '.';
}

} // namespace

Lexer::Lexer(std::string source)
    : source_(std::move(source))
    , pos_(0)
    , line_(1)
    , column_(1) {}

bool Lexer::is_at_end() const {
    return pos_ >= source_.size();
}

char Lexer::peek() const {
    return is_at_end() ? '\0' : source_[pos_];
}

char Lexer::advance() {
    char c = source_[pos_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

void Lexer::skip_whitespace() {
    while (!is_at_end()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            advance();
        } else {
            break;
        }
    }
}

void Lexer::skip_line_comment() {
    while (!is_at_end() && peek() != '\n') {
        advance();
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (!is_at_end()) {
        skip_whitespace();
        if (is_at_end()) break;

        std::size_t start_line = line_;
        std::size_t start_col  = column_;
        char c = peek();

        auto add_token = [&](TokenType type, std::string lexeme) {
            tokens.push_back({type, std::move(lexeme), start_line, start_col});
        };

        if (c == '#') {
            skip_line_comment();
            continue;
        }

        if (c == '/' && pos_ + 1 < source_.size()) {
            char next = source_[pos_ + 1];
            if (next == '/') {
                advance(); advance();
                skip_line_comment();
                continue;
            }
        }

        if (c == '{') {
            advance();
            add_token(TokenType::LBRACE, "{");
            continue;
        }

        if (c == '}') {
            advance();
            add_token(TokenType::RBRACE, "}");
            continue;
        }

        if (c == ';') {
            advance();
            add_token(TokenType::SEMICOLON, ";");
            continue;
        }

        if (c == '=') {
            advance();
            add_token(TokenType::EQUALS, "=");
            continue;
        }

        if (c == '"') {
            tokens.push_back(read_string());
            continue;
        }

        if (is_ident_start(c)) {
            tokens.push_back(read_identifier_or_keyword());
            continue;
        }

        advance();
        add_token(TokenType::ERROR, std::string(1, c));
    }

    tokens.push_back({TokenType::END_OF_FILE, "", line_, column_});
    return tokens;
}

Token Lexer::read_identifier_or_keyword() {
    std::size_t start_line = line_;
    std::size_t start_col  = column_;
    std::string lexeme;

    while (!is_at_end() && is_ident_continue(peek())) {
        lexeme += advance();
    }

    auto it = keywords.find(lexeme);
    TokenType type = (it != keywords.end()) ? it->second : TokenType::IDENTIFIER;

    return {type, std::move(lexeme), start_line, start_col};
}

Token Lexer::read_string() {
    std::size_t start_line = line_;
    std::size_t start_col  = column_;

    advance(); // consume opening "

    std::string lexeme;
    while (!is_at_end()) {
        char c = peek();
        if (c == '"') {
            advance(); // consume closing "
            return {TokenType::STRING, std::move(lexeme), start_line, start_col};
        }
        if (c == '\n') {
            return {TokenType::ERROR, std::move(lexeme), start_line, start_col};
        }
        lexeme += advance();
    }

    return {TokenType::ERROR, std::move(lexeme), start_line, start_col};
}

Token Lexer::read_error(std::string message) {
    return {TokenType::ERROR, std::move(message), line_, column_};
}

std::string token_type_name(TokenType type) {
    switch (type) {
        case TokenType::GROUP:       return "GROUP";
        case TokenType::CONTROLLER:  return "CONTROLLER";
        case TokenType::ITEM:        return "ITEM";
        case TokenType::MODE:        return "MODE";
        case TokenType::MATCH:       return "MATCH";
        case TokenType::TYPE:        return "TYPE";
        case TokenType::IDENTIFIER:  return "IDENTIFIER";
        case TokenType::STRING:      return "STRING";
        case TokenType::LBRACE:      return "LBRACE";
        case TokenType::RBRACE:      return "RBRACE";
        case TokenType::SEMICOLON:   return "SEMICOLON";
        case TokenType::EQUALS:      return "EQUALS";
        case TokenType::ERROR:       return "ERROR";
        case TokenType::END_OF_FILE: return "END_OF_FILE";
    }
    return "UNKNOWN";
}

} // namespace resource_manager
