#pragma once

#include <string>
#include <vector>
#include <cstddef>

#include "resource_manager/core/config_domain.h"

namespace resource_manager {

struct ValidationError {
    std::size_t line;
    std::size_t column;
    std::string path;
    std::string message;
};

struct ValidationResult {
    bool valid() const { return errors.empty(); }
    std::vector<ValidationError> errors;
};

class Validator {
public:
    ValidationResult validate(const ConfigDomain& domain);

private:
    std::vector<ValidationError> errors_;

    void validate_node(const ConfigNode& node);
    void check_hierarchy(const ConfigNode& node);
    void check_duplicates(const ConfigNode& node);
    void check_missing_value(const ConfigNode& node);

    void add_error(const ConfigNode& node, std::string message);
};

} // namespace resource_manager
