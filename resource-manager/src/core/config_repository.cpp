#include "resource_manager/core/config_repository.h"

#include "resource_manager/core/config_loader.h"
#include "resource_manager/lexer/lexer.h"
#include "resource_manager/core/parser.h"
#include "resource_manager/core/validator.h"

#include <functional>

namespace resource_manager {

bool ConfigRepository::load(const std::string& filepath) {
    auto content = ConfigLoader().loadFromFile(filepath);
    if (!content.has_value()) {
        clear_errors();
        add_error(0, 0, "", "failed to open file: " + filepath);
        return false;
    }
    return apply_source(content.value(), filepath);
}

bool ConfigRepository::loadFromString(const std::string& content) {
    return apply_source(content, "memory");
}

bool ConfigRepository::apply_source(const std::string& content, const std::string& sourceName) {
    clear_errors();

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
    configState_.source = sourceName;
    configState_.loaded_at = std::chrono::system_clock::now();
    configState_.version = std::hash<std::string>{}(content);
    return true;
}

void ConfigRepository::replace(ConfigDomain domain) {
    clear_errors();
    domain_ = std::move(domain);
    configState_.source = "replaced";
    configState_.loaded_at = std::chrono::system_clock::now();
    configState_.version = 0;
}

const ConfigDomain& ConfigRepository::getRoot() const {
    return domain_;
}

const ConfigState& ConfigRepository::getConfigState() const {
    return configState_;
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
