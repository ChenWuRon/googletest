#include "resource_manager/core/config_domain.h"

namespace resource_manager {

ConfigNode::ConfigNode(ConfigNodeType type, std::string name)
    : type_(type), name_(std::move(name)) {}

ConfigNode* ConfigNode::addChild(std::unique_ptr<ConfigNode> child) {
    if (!child) return nullptr;
    if (children_.find(child->name()) != children_.end()) return nullptr;

    child->parent_ = this;
    auto* raw = child.get();
    children_[child->name()] = std::move(child);
    return raw;
}

std::unique_ptr<ConfigNode> ConfigNode::removeChild(const std::string& name) {
    auto it = children_.find(name);
    if (it == children_.end()) return nullptr;

    auto child = std::move(it->second);
    children_.erase(it);
    child->parent_ = nullptr;
    return child;
}

ConfigNode* ConfigNode::findChild(const std::string& name) {
    auto it = children_.find(name);
    return it != children_.end() ? it->second.get() : nullptr;
}

const ConfigNode* ConfigNode::findChild(const std::string& name) const {
    auto it = children_.find(name);
    return it != children_.end() ? it->second.get() : nullptr;
}

} // namespace resource_manager
