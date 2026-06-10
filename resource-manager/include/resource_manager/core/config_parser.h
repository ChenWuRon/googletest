#pragma once

// ADR-002 Config Grammar
// Parses configuration into ConfigDomain Tree.
// Grammar: group -> controller -> item.

#include <string>

#include "resource_manager/core/config_domain.h"
#include "resource_manager/error/error.h"

namespace resource_manager {

class ConfigParser {
public:
    ConfigParser() = default;
    ~ConfigParser() = default;

    std::optional<ConfigDomain> parse(const std::string& filepath);
    std::optional<ConfigDomain> parse_string(const std::string& content);

private:
    bool validate(const ConfigDomain& domain);
};

} // namespace resource_manager
