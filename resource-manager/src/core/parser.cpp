#include "resource_manager/core/parser.h"

#include <sstream>

namespace resource_manager {

Parser::Parser(const std::vector<Token>& tokens)
    : pos_(0), tokens_(tokens) {}

ParseResult Parser::parse() {
    errors_.clear();

    ConfigDomain domain;
    ConfigNode& root = domain.root();

    while (!is_at_end()) {
        if (match(TokenType::GROUP)) {
            parse_group(root);
        } else if (match(TokenType::END_OF_FILE)) {
            break;
        } else {
            error(peek(), "expected 'group'");
            advance();
        }
    }

    return {std::move(domain), std::move(errors_)};
}

// ── helpers ─────────────────────────────────────────────────────────────

const Token& Parser::peek() const {
    return tokens_[pos_];
}

const Token& Parser::advance() {
    if (!is_at_end()) pos_++;
    return previous();
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::check(TokenType type) const {
    if (is_at_end()) return false;
    return peek().type == type;
}

bool Parser::is_at_end() const {
    return pos_ >= tokens_.size() || tokens_[pos_].type == TokenType::END_OF_FILE;
}

const Token& Parser::previous() const {
    return tokens_[pos_ - 1];
}

void Parser::error(const Token& token, std::string message) {
    errors_.push_back({token.line, token.column, std::move(message)});
}

// ── group ───────────────────────────────────────────────────────────────

ConfigNode* Parser::parse_group(ConfigNode& parent) {
    if (!check(TokenType::IDENTIFIER)) {
        error(peek(), "expected group name");
        synchronize_group_body();
        return nullptr;
    }
    const Token& name_tok = peek();
    std::string name = name_tok.lexeme;
    advance();

    if (!match(TokenType::LBRACE)) {
        error(peek(), "expected '{' after group name");
        synchronize_group_body();
        return nullptr;
    }

    auto group = std::make_unique<ConfigNode>(ConfigNodeType::GROUP, name, "",
                                              name_tok.line, name_tok.column);
    ConfigNode* raw = parent.addChild(std::move(group));
    if (!raw) {
        error(name_tok, "duplicate group '" + name + "'");
        synchronize_group_body();
        return nullptr;
    }

    parse_group_body(*raw);

    return raw;
}

void Parser::parse_group_body(ConfigNode& group) {
    while (!is_at_end() && !check(TokenType::RBRACE)) {
        if (match(TokenType::MODE)) {
            parse_mode_statement();
        } else if (match(TokenType::MATCH)) {
            parse_match_statement(group);
        } else if (match(TokenType::CONTROLLER)) {
            parse_controller(group);
        } else if (match(TokenType::ITEM)) {
            parse_item(group);
        } else if (match(TokenType::GROUP)) {
            error(previous(), "nested groups are not allowed");
            parse_group(group);
        } else {
            error(peek(), "expected 'mode', 'match', 'controller', or '}'");
            synchronize_group_body();
        }
    }

    if (!match(TokenType::RBRACE)) {
        error(peek(), "expected '}' at end of group");
    }
}

void Parser::parse_mode_statement() {
    if (!check(TokenType::IDENTIFIER)) {
        error(peek(), "expected mode value");
        synchronize_to_semicolon();
        return;
    }
    advance();

    if (!match(TokenType::SEMICOLON)) {
        error(peek(), "expected ';' after mode value");
    }
}

void Parser::parse_match_statement(ConfigNode& group) {
    if (!check(TokenType::STRING)) {
        error(peek(), "expected match pattern string");
        synchronize_group_body();
        return;
    }
    const Token& patternTok = peek();
    std::string pattern = patternTok.lexeme;
    advance();

    if (!match(TokenType::LBRACE)) {
        error(peek(), "expected '{' after match pattern");
        synchronize_group_body();
        return;
    }

    std::string matchType;
    if (!match(TokenType::TYPE)) {
        error(peek(), "expected 'type' in match block");
    } else {
        if (!check(TokenType::IDENTIFIER)) {
            error(peek(), "expected match type value");
        } else {
            matchType = peek().lexeme;
            advance();
        }

        if (!match(TokenType::SEMICOLON)) {
            error(peek(), "expected ';' after match type");
        }
    }

    if (!match(TokenType::RBRACE)) {
        error(peek(), "expected '}' at end of match block");
    }

    if (!matchType.empty()) {
        group.setMatchRule({std::move(pattern), std::move(matchType)});
    }
}

void Parser::parse_controller(ConfigNode& parent) {
    if (!check(TokenType::IDENTIFIER)) {
        error(peek(), "expected controller type");
        synchronize_controller_body();
        return;
    }
    const Token& name_tok = peek();
    std::string name = name_tok.lexeme;
    advance();

    if (!match(TokenType::LBRACE)) {
        error(peek(), "expected '{' after controller type");
        synchronize_controller_body();
        return;
    }

    auto controller = std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, name, "",
                                                   name_tok.line, name_tok.column);
    ConfigNode* raw = parent.addChild(std::move(controller));
    if (!raw) {
        error(name_tok, "duplicate controller '" + name + "' in group");
        synchronize_controller_body();
        return;
    }

    while (!is_at_end() && !check(TokenType::RBRACE)) {
        if (match(TokenType::ITEM)) {
            parse_item(*raw);
        } else if (match(TokenType::CONTROLLER)) {
            error(previous(), "nested controllers are not allowed");
            parse_controller(*raw);
        } else {
            error(peek(), "expected 'item' or '}'");
            synchronize_controller_body();
        }
    }

    if (!match(TokenType::RBRACE)) {
        error(peek(), "expected '}' at end of controller");
    }
}

void Parser::parse_item(ConfigNode& parent) {
    if (!check(TokenType::IDENTIFIER)) {
        error(peek(), "expected item name");
        synchronize_to_semicolon();
        return;
    }
    const Token& name_tok = peek();
    std::string name = name_tok.lexeme;
    advance();

    if (!match(TokenType::EQUALS)) {
        error(peek(), "expected '=' after item name");
        synchronize_to_semicolon();
        return;
    }

    if (!check(TokenType::STRING)) {
        error(peek(), "expected item value string");
        synchronize_to_semicolon();
        return;
    }
    std::string value = peek().lexeme;
    advance();

    auto item = std::make_unique<ConfigNode>(ConfigNodeType::ITEM, name, value,
                                              name_tok.line, name_tok.column);
    if (!parent.addChild(std::move(item))) {
        error(name_tok, "duplicate item '" + name + "' in controller");
    }

    if (!match(TokenType::SEMICOLON)) {
        error(peek(), "expected ';' after item value");
    }
}

// ── synchronization ─────────────────────────────────────────────────────

void Parser::synchronize_group_body() {
    while (!is_at_end()) {
        if (check(TokenType::RBRACE)) return;
        if (check(TokenType::GROUP)) return;
        if (check(TokenType::CONTROLLER)) return;
        advance();
    }
}

void Parser::synchronize_controller_body() {
    while (!is_at_end()) {
        if (check(TokenType::RBRACE)) return;
        if (check(TokenType::CONTROLLER)) return;
        if (check(TokenType::GROUP)) return;
        advance();
    }
}

void Parser::synchronize_to_semicolon() {
    while (!is_at_end() && !check(TokenType::SEMICOLON)) {
        advance();
    }
    if (check(TokenType::SEMICOLON)) advance();
}

} // namespace resource_manager
