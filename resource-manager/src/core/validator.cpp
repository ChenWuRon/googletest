#include "resource_manager/core/validator.h"

#include <set>

namespace resource_manager {

ValidationResult Validator::validate(const ConfigDomain& domain) {
    errors_.clear();
    validate_node(domain.root());
    return {std::move(errors_)};
}

void Validator::validate_node(const ConfigNode& node) {
    check_hierarchy(node);
    check_duplicates(node);
    check_missing_value(node);

    for (const auto& [_, child] : node.children()) {
        validate_node(*child);
    }
}

void Validator::check_hierarchy(const ConfigNode& node) {
    const ConfigNode* p = node.parent();

    switch (node.type()) {
        case ConfigNodeType::ROOT:
            if (p != nullptr) {
                add_error(node, "ROOT node must not have a parent");
            }
            break;

        case ConfigNodeType::GROUP:
            if (!p || p->type() != ConfigNodeType::ROOT) {
                add_error(node, "GROUP '" + node.name() +
                          "' must be placed directly under ROOT");
            }
            break;

        case ConfigNodeType::CONTROLLER:
            if (!p || p->type() != ConfigNodeType::GROUP) {
                add_error(node, "CONTROLLER '" + node.name() +
                          "' must be placed under a GROUP");
            }
            break;

        case ConfigNodeType::ITEM:
            if (!p || p->type() != ConfigNodeType::CONTROLLER) {
                add_error(node, "ITEM '" + node.name() +
                          "' must be placed under a CONTROLLER");
            }
            break;
    }
}

void Validator::check_duplicates(const ConfigNode& node) {
    std::set<std::string> seen;

    for (const auto& [name, _] : node.children()) {
        if (!seen.insert(name).second) {
            auto* child = node.findChild(name);
            if (child) {
                add_error(*child, "duplicate name '" + name + "' at " + node.path());
            }
        }
    }
}

void Validator::check_missing_value(const ConfigNode& node) {
    if (node.type() == ConfigNodeType::ITEM && node.value().empty()) {
        add_error(node, "ITEM '" + node.name() + "' is missing a value");
    }
}

void Validator::add_error(const ConfigNode& node, std::string message) {
    errors_.push_back({
        node.source_line(),
        node.source_column(),
        node.path(),
        std::move(message),
    });
}

} // namespace resource_manager
