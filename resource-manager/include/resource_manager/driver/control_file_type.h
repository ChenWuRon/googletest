#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <shared_mutex>

#include "resource_manager/core/config_domain.h"

namespace resource_manager {

struct ValidationRule {
    std::string pattern;
    std::string min;
    std::string max;
    std::vector<std::string> enums;
    bool allow_max;
};

struct ControlFileType {
    std::string name;
    ValueType value_type;
    int version;
    std::string default_value;
    ValidationRule validation;
};

struct ControllerDefinition {
    std::string name;
    std::map<std::string, ControlFileType> items;
    int min_version;
    int max_version;
};

class ControllerRegistry {
public:
    static ControllerRegistry& instance();

    void registerController(const ControllerDefinition& ctrl);
    void registerControlFile(const std::string& controller, const ControlFileType& type);

    const ControllerDefinition* findController(const std::string& name) const;
    const ControlFileType* findControlFile(const std::string& controller, const std::string& item) const;
    std::vector<std::string> listControllers() const;

    void registerBuiltins();

private:
    ControllerRegistry() = default;

    mutable std::shared_mutex mutex_;
    std::map<std::string, ControllerDefinition> registry_;
};

} // namespace resource_manager
