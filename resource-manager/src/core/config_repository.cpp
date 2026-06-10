#include "resource_manager/core/config_repository.h"

#include <fstream>
#include <sstream>

#include "resource_manager/lexer/lexer.h"
#include "resource_manager/core/parser.h"
#include "resource_manager/core/validator.h"

namespace resource_manager {

bool ConfigRepository::load(const std::string& filepath) {
    clear_errors();

    std::ifstream file(filepath);
    if (!file.is_open()) {
        add_error(0, 0, "", "failed to open file: " + filepath);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // ── Lex ─────────────────────────────────────────────────────────────
    Lexer lexer(content);
    std::vector<Token> tokens = lexer.tokenize();

    for (const auto& tok : tokens) {
        if (tok.type == TokenType::ERROR) {
            add_error(tok.line, tok.column, "",
                      "unexpected character '" + tok.lexeme + "'");
        }
    }
    if (!errors_.empty()) return false;

    // ── Parse ───────────────────────────────────────────────────────────
    Parser parser(tokens);
    ParseResult parse_result = parser.parse();

    for (const auto& err : parse_result.errors) {
        add_error(err.line, err.column, "", err.message);
    }
    if (!errors_.empty()) return false;

    // ── Validate ────────────────────────────────────────────────────────
    Validator validator;
    ValidationResult validation_result = validator.validate(parse_result.domain);

    for (const auto& err : validation_result.errors) {
        add_error(err.line, err.column, err.path, err.message);
    }
    if (!errors_.empty()) return false;

    domain_ = std::move(parse_result.domain);
    return true;
}

void ConfigRepository::replace(ConfigDomain domain) {
    clear_errors();
    domain_ = std::move(domain);
}

const ConfigDomain& ConfigRepository::getRoot() const {
    return domain_;
}

const std::vector<RepositoryError>& ConfigRepository::errors() const {
    return errors_;
}

void ConfigRepository::clear_errors() {
    errors_.clear();
}

void ConfigRepository::add_error(std::size_t line, std::size_t column,
                                  std::string path, std::string message) {
    errors_.push_back({line, column, std::move(path), std::move(message)});
}

} // namespace resource_manager
