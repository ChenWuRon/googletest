#include "resource_manager/core/config_differ.h"

namespace resource_manager {

DiffResult ConfigDiffer::diff(const ConfigDomain& old_domain,
                              const ConfigDomain& new_domain) {
    DiffResult result;
    diff_node(old_domain.root(), new_domain.root(), result);
    return result;
}

void ConfigDiffer::diff_node(const ConfigNode& old_node,
                             const ConfigNode& new_node,
                             DiffResult& result) {
    const auto& old_children = old_node.children();
    const auto& new_children = new_node.children();

    // Find added and compare common children
    for (const auto& [name, new_child] : new_children) {
        auto it = old_children.find(name);
        if (it == old_children.end()) {
            DiffEntry entry;
            entry.type = DiffType::ADDED;
            entry.path = new_child->path();
            entry.node_type = new_child->type();
            result.added.push_back(std::move(entry));
        } else if (new_child->type() == ConfigNodeType::ITEM &&
                   it->second->type() == ConfigNodeType::ITEM) {
            if (new_child->value() != it->second->value()) {
                DiffEntry entry;
                entry.type = DiffType::MODIFIED;
                entry.path = new_child->path();
                entry.node_type = ConfigNodeType::ITEM;
                entry.old_value = it->second->value();
                entry.new_value = new_child->value();
                result.modified.push_back(std::move(entry));
            }
        } else {
            diff_node(*it->second, *new_child, result);
        }
    }

    // Find removed
    for (const auto& [name, old_child] : old_children) {
        if (new_children.find(name) == new_children.end()) {
            DiffEntry entry;
            entry.type = DiffType::REMOVED;
            entry.path = old_child->path();
            entry.node_type = old_child->type();
            result.removed.push_back(std::move(entry));
        }
    }
}

} // namespace resource_manager
