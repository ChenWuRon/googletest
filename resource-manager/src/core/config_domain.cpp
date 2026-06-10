#include "resource_manager/core/config_domain.h"

#include <sstream>
#include <algorithm>

namespace resource_manager {

// ── ConfigNodeType utilities ─────────────────────────────────────────────

std::string node_type_name(ConfigNodeType type) {
    switch (type) {
        case ConfigNodeType::ROOT:       return "ROOT";
        case ConfigNodeType::GROUP:      return "GROUP";
        case ConfigNodeType::CONTROLLER: return "CONTROLLER";
        case ConfigNodeType::ITEM:       return "ITEM";
    }
    return "UNKNOWN";
}

bool is_valid_parent_child(ConfigNodeType parent, ConfigNodeType child) {
    switch (parent) {
        case ConfigNodeType::ROOT:
            return child == ConfigNodeType::GROUP;
        case ConfigNodeType::GROUP:
            return child == ConfigNodeType::CONTROLLER;
        case ConfigNodeType::CONTROLLER:
            return child == ConfigNodeType::ITEM;
        case ConfigNodeType::ITEM:
            return false;
    }
    return false;
}

// ── ConfigPath ───────────────────────────────────────────────────────────

ConfigPath ConfigPath::root() {
    return ConfigPath();
}

std::optional<ConfigPath> ConfigPath::parse(const std::string& path) {
    if (path.empty()) {
        return std::nullopt;
    }
    if (path[0] != '/') {
        return std::nullopt;
    }

    std::vector<std::string> segs;
    if (path.size() == 1) {
        // just "/" -> root
        return ConfigPath(segs);
    }

    std::istringstream stream(path);
    std::string segment;
    // consume leading '/'
    std::getline(stream, segment, '/');
    while (std::getline(stream, segment, '/')) {
        if (segment.empty()) {
            return std::nullopt; // double slash
        }
        segs.push_back(segment);
    }

    return ConfigPath(std::move(segs));
}

ConfigPath::ConfigPath(std::vector<std::string> segs)
    : segments_(std::move(segs)) {}

const std::vector<std::string>& ConfigPath::segments() const {
    return segments_;
}

std::string ConfigPath::to_string() const {
    if (segments_.empty()) {
        return "/";
    }
    std::string result;
    for (const auto& seg : segments_) {
        result += '/' + seg;
    }
    return result;
}

ConfigPath ConfigPath::parent() const {
    if (segments_.empty()) {
        return ConfigPath();
    }
    auto parent_segs = segments_;
    parent_segs.pop_back();
    return ConfigPath(std::move(parent_segs));
}

ConfigPath ConfigPath::child(const std::string& segment) const {
    auto child_segs = segments_;
    child_segs.push_back(segment);
    return ConfigPath(std::move(child_segs));
}

bool ConfigPath::is_root() const {
    return segments_.empty();
}

bool ConfigPath::empty() const {
    return segments_.empty();
}

size_t ConfigPath::depth() const {
    return segments_.size();
}

bool ConfigPath::operator==(const ConfigPath& other) const {
    return segments_ == other.segments_;
}

bool ConfigPath::operator!=(const ConfigPath& other) const {
    return segments_ != other.segments_;
}

// ── ConfigNode ───────────────────────────────────────────────────────────

ConfigNode::ConfigNode(ConfigNodeType type, std::string name)
    : type_(type), name_(std::move(name)) {}

ConfigNode* ConfigNode::addChild(std::unique_ptr<ConfigNode> child) {
    if (!child) return nullptr;
    if (!is_valid_parent_child(type_, child->type())) return nullptr;

    const auto& child_name = child->name();
    if (children_.find(child_name) != children_.end()) return nullptr;

    child->parent_ = this;
    auto* raw = child.get();
    children_[child_name] = std::move(child);
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

ConfigNode* ConfigNode::findByPath(const ConfigPath& path) {
    return const_cast<ConfigNode*>(
        static_cast<const ConfigNode*>(this)->findByPath(path));
}

const ConfigNode* ConfigNode::findByPath(const ConfigPath& path) const {
    const ConfigNode* current = this;
    for (const auto& seg : path.segments()) {
        current = current->findChild(seg);
        if (!current) return nullptr;
    }
    return current;
}

ConfigPath ConfigNode::getPath() const {
    std::vector<std::string> segs;
    const ConfigNode* current = this;
    while (current->parent_) {
        segs.push_back(current->name_);
        current = current->parent_;
    }
    std::reverse(segs.begin(), segs.end());
    return ConfigPath(std::move(segs));
}

ConfigNode* ConfigNode::parent() {
    return parent_;
}

const ConfigNode* ConfigNode::parent() const {
    return parent_;
}

const std::map<std::string, std::unique_ptr<ConfigNode>>& ConfigNode::children() const {
    return children_;
}

ConfigNodeType ConfigNode::type() const {
    return type_;
}

const std::string& ConfigNode::name() const {
    return name_;
}

bool ConfigNode::is_root() const {
    return type_ == ConfigNodeType::ROOT;
}

Mode& ConfigNode::mode() {
    return mode_;
}

const Mode& ConfigNode::mode() const {
    return mode_;
}

MatchRule& ConfigNode::match_rule() {
    return match_rule_;
}

const MatchRule& ConfigNode::match_rule() const {
    return match_rule_;
}

std::string& ConfigNode::value() {
    return value_;
}

const std::string& ConfigNode::value() const {
    return value_;
}

ValueType& ConfigNode::value_type() {
    return value_type_;
}

const ValueType& ConfigNode::value_type() const {
    return value_type_;
}

// ── ConfigDomain ─────────────────────────────────────────────────────────

ConfigDomain::ConfigDomain()
    : root_(std::make_unique<ConfigNode>(ConfigNodeType::ROOT, "")) {}

ConfigNode& ConfigDomain::root() {
    return *root_;
}

const ConfigNode& ConfigDomain::root() const {
    return *root_;
}

ConfigNode* ConfigDomain::findByPath(const std::string& path) {
    return const_cast<ConfigNode*>(
        static_cast<const ConfigDomain*>(this)->findByPath(path));
}

const ConfigNode* ConfigDomain::findByPath(const std::string& path) const {
    auto parsed = ConfigPath::parse(path);
    if (!parsed) return nullptr;
    return root_->findByPath(*parsed);
}

ConfigNode* ConfigDomain::addGroup(const std::string& name) {
    auto group = std::make_unique<ConfigNode>(ConfigNodeType::GROUP, name);
    auto* raw = root_->addChild(std::move(group));
    if (raw) ++node_count_;
    return raw;
}

ConfigNode* ConfigDomain::addController(const std::string& group_name, const std::string& controller_name) {
    auto* group = root_->findChild(group_name);
    if (!group) return nullptr;

    auto ctrl = std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, controller_name);
    auto* raw = group->addChild(std::move(ctrl));
    if (raw) ++node_count_;
    return raw;
}

ConfigNode* ConfigDomain::addItem(const std::string& group_name, const std::string& controller_name, const std::string& item_name) {
    auto* group = root_->findChild(group_name);
    if (!group) return nullptr;

    auto* ctrl = group->findChild(controller_name);
    if (!ctrl) return nullptr;

    auto item = std::make_unique<ConfigNode>(ConfigNodeType::ITEM, item_name);
    auto* raw = ctrl->addChild(std::move(item));
    if (raw) ++node_count_;
    return raw;
}

size_t ConfigDomain::groupCount() const {
    return root_->children().size();
}

size_t ConfigDomain::nodeCount() const {
    return node_count_;
}

} // namespace resource_manager
