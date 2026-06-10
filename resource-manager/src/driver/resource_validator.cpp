#include "resource_manager/driver/resource_validator.h"

namespace resource_manager {

ResourceValidator::ResourceValidator(const ControllerRegistry& registry)
    : registry_(registry)
{
}

std::optional<Error> ResourceValidator::validate(const ConfigDomain& domain) const {
    for (const auto& [name, child] : domain.root().children()) {
        if (child->type() == ConfigNodeType::GROUP) {
            auto err = validate(*child);
            if (err) return err;
        }
    }
    return std::nullopt;
}

std::optional<Error> ResourceValidator::validate(const ConfigNode& groupNode) const {
    for (const auto& [ctrlName, ctrlChild] : groupNode.children()) {
        if (ctrlChild->type() != ConfigNodeType::CONTROLLER) continue;

        const ControllerDefinition* ctrlDef = registry_.findController(ctrlName);
        if (!ctrlDef) {
            return Error(ErrorCode::ControllerNotSupported,
                         "Unknown controller: " + ctrlName,
                         "ResourceValidator");
        }

        if (ctrlDef->max_version < 2) {
            return Error(ErrorCode::CgroupVersionMismatch,
                         "Controller " + ctrlName + " requires cgroup v1, but current driver is v2",
                         "ResourceValidator");
        }

        for (const auto& [itemName, itemChild] : ctrlChild->children()) {
            if (itemChild->type() != ConfigNodeType::ITEM) continue;

            const ControlFileType* itemDef = registry_.findControlFile(ctrlName, itemName);
            if (!itemDef) {
                return Error(ErrorCode::InvalidControlValue,
                             "Unknown item: " + itemName + " for controller " + ctrlName,
                             "ResourceValidator");
            }

            auto err = validate(ctrlName, itemName, itemChild->value());
            if (err) return err;
        }
    }
    return std::nullopt;
}

std::optional<Error> ResourceValidator::validate(
    const std::string& controller,
    const std::string& item,
    const std::string& value) const
{
    const ControlFileType* itemDef = registry_.findControlFile(controller, item);
    if (!itemDef) {
        return Error(ErrorCode::InvalidControlValue,
                     "Unknown item: " + item + " for controller " + controller,
                     "ResourceValidator");
    }

    const ValidationRule& rule = itemDef->validation;

    if (!rule.enums.empty()) {
        if (!checkEnum(value, rule)) {
            return Error(ErrorCode::InvalidControlValue,
                         "Value '" + value + "' is not valid for " + controller + "." + item +
                         ". Allowed: " + [&]() -> std::string {
                             std::string s;
                             for (size_t i = 0; i < rule.enums.size(); i++) {
                                 if (i > 0) s += ", ";
                                 s += rule.enums[i];
                             }
                             return s;
                         }(),
                         "ResourceValidator");
        }
        return std::nullopt;
    }

    if (!matchPattern(value, rule)) {
        return Error(ErrorCode::InvalidControlValue,
                     "Value '" + value + "' format mismatch for " + controller + "." + item,
                     "ResourceValidator");
    }

    if (!checkRange(value, rule)) {
        return Error(ErrorCode::InvalidControlValue,
                     "Value '" + value + "' out of range for " + controller + "." + item,
                     "ResourceValidator");
    }

    return std::nullopt;
}

bool ResourceValidator::matchPattern(const std::string& value, const ValidationRule& rule) const {
    if (rule.pattern.empty()) {
        return true;
    }

    if (rule.allow_max && value == "max") {
        return true;
    }

    try {
        std::regex re(rule.pattern);
        return std::regex_match(value, re);
    } catch (const std::regex_error&) {
        return true;
    }
}

bool ResourceValidator::checkEnum(const std::string& value, const ValidationRule& rule) const {
    if (rule.enums.empty()) {
        return true;
    }
    for (const auto& e : rule.enums) {
        if (value == e) {
            return true;
        }
    }
    return false;
}

bool ResourceValidator::checkRange(const std::string& value, const ValidationRule& rule) const {
    if (rule.min.empty() && rule.max.empty()) {
        return true;
    }
    if (rule.allow_max && value == "max") {
        return true;
    }

    try {
        long long val = std::stoll(value);
        if (!rule.min.empty()) {
            long long minVal = std::stoll(rule.min);
            if (val < minVal) return false;
        }
        if (!rule.max.empty()) {
            long long maxVal = std::stoll(rule.max);
            if (val > maxVal) return false;
        }
    } catch (const std::exception&) {
        return true;
    }
    return true;
}

} // namespace resource_manager
