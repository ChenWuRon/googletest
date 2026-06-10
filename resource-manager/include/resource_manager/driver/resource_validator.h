#pragma once

#include <string>
#include <optional>
#include <regex>

#include "resource_manager/driver/control_file_type.h"
#include "resource_manager/core/config_domain.h"
#include "resource_manager/error/error.h"

namespace resource_manager {

class ResourceValidator {
public:
    explicit ResourceValidator(const ControllerRegistry& registry);

    std::optional<Error> validate(const ConfigDomain& domain) const;
    std::optional<Error> validate(const ConfigNode& groupNode) const;
    std::optional<Error> validate(
        const std::string& controller,
        const std::string& item,
        const std::string& value) const;

private:
    bool matchPattern(const std::string& value, const ValidationRule& rule) const;
    bool checkEnum(const std::string& value, const ValidationRule& rule) const;
    bool checkRange(const std::string& value, const ValidationRule& rule) const;

    const ControllerRegistry& registry_;
};

} // namespace resource_manager
