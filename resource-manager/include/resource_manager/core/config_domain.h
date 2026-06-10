#pragma once

#include <string>
#include <map>
#include <memory>

namespace resource_manager {

enum class ValueType {
    String,
    Quota,
    Size,
    Weight,
    Bitmap,
    List,
    Int,
    Enum,
};

struct MatchRule {
    std::string pattern;
    std::string type;
};

enum class ConfigNodeType {
    ROOT,
    GROUP,
    CONTROLLER,
    ITEM,
};

class ConfigNode {
public:
    ConfigNode(ConfigNodeType type, std::string name);
    ~ConfigNode() = default;

    ConfigNode* addChild(std::unique_ptr<ConfigNode> child);
    std::unique_ptr<ConfigNode> removeChild(const std::string& name);
    ConfigNode* findChild(const std::string& name);
    const ConfigNode* findChild(const std::string& name) const;

    ConfigNodeType type() const { return type_; }
    const std::string& name() const { return name_; }
    ConfigNode* parent() { return parent_; }
    const ConfigNode* parent() const { return parent_; }
    const std::map<std::string, std::unique_ptr<ConfigNode>>& children() const { return children_; }

private:
    ConfigNodeType type_;
    std::string name_;
    ConfigNode* parent_ = nullptr;
    std::map<std::string, std::unique_ptr<ConfigNode>> children_;
};

class ConfigDomain {
public:
    ConfigDomain()
        : root_(std::make_unique<ConfigNode>(ConfigNodeType::ROOT, "")) {}
    ~ConfigDomain() = default;

    ConfigNode& root() { return *root_; }
    const ConfigNode& root() const { return *root_; }

private:
    std::unique_ptr<ConfigNode> root_;
};

} // namespace resource_manager
