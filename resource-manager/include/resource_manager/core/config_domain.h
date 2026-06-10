#pragma once

// ADR-003 Config Tree
// ConfigDomain Tree: ROOT -> GROUP -> CONTROLLER -> ITEM
// Path format: /<group-name>/<controller-name>/<item-name>

#include <string>
#include <map>
#include <memory>
#include <vector>
#include <optional>
#include <stdexcept>

#include "resource_manager/mode/mode.h"

namespace resource_manager {

// ── ValueType ──────────────────────────────────────────────────────────

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

// ── MatchRule ────────────────────────────────────────────────────────────

struct MatchRule {
    std::string pattern;
    std::string type; // exact | prefix | wildcard
};

// ── ConfigNodeType ───────────────────────────────────────────────────────

enum class ConfigNodeType {
    ROOT,
    GROUP,
    CONTROLLER,
    ITEM,
};

std::string node_type_name(ConfigNodeType type);
bool is_valid_parent_child(ConfigNodeType parent, ConfigNodeType child);

// ── ConfigPath ───────────────────────────────────────────────────────────

class ConfigPath {
public:
    ConfigPath() = default;

    static ConfigPath root();
    static std::optional<ConfigPath> parse(const std::string& path);

    const std::vector<std::string>& segments() const;
    std::string to_string() const;
    ConfigPath parent() const;
    ConfigPath child(const std::string& segment) const;

    bool is_root() const;
    bool empty() const;
    size_t depth() const;

    bool operator==(const ConfigPath& other) const;
    bool operator!=(const ConfigPath& other) const;

private:
    friend class ConfigNode;
    explicit ConfigPath(std::vector<std::string> segs);
    std::vector<std::string> segments_;
};

// ── ConfigNode ───────────────────────────────────────────────────────────

class ConfigNode {
public:
    ConfigNode(ConfigNodeType type, std::string name);
    ~ConfigNode() = default;

    // Tree structure
    ConfigNode* addChild(std::unique_ptr<ConfigNode> child);
    std::unique_ptr<ConfigNode> removeChild(const std::string& name);
    ConfigNode* findChild(const std::string& name);
    const ConfigNode* findChild(const std::string& name) const;
    ConfigNode* findByPath(const ConfigPath& path);
    const ConfigNode* findByPath(const ConfigPath& path) const;

    // Navigation
    ConfigPath getPath() const;
    ConfigNode* parent();
    const ConfigNode* parent() const;
    const std::map<std::string, std::unique_ptr<ConfigNode>>& children() const;

    // Type and identity
    ConfigNodeType type() const;
    const std::string& name() const;
    bool is_root() const;

    // GROUP data
    Mode& mode();
    const Mode& mode() const;
    MatchRule& match_rule();
    const MatchRule& match_rule() const;

    // ITEM data
    std::string& value();
    const std::string& value() const;
    ValueType& value_type();
    const ValueType& value_type() const;

private:
    ConfigNodeType type_;
    std::string name_;
    ConfigNode* parent_ = nullptr;
    std::map<std::string, std::unique_ptr<ConfigNode>> children_;
    Mode mode_;
    MatchRule match_rule_;
    std::string value_;
    ValueType value_type_ = ValueType::String;
};

// ── ConfigDomain ─────────────────────────────────────────────────────────

class ConfigDomain {
public:
    ConfigDomain();
    ~ConfigDomain() = default;

    ConfigNode& root();
    const ConfigNode& root() const;

    ConfigNode* findByPath(const std::string& path);
    const ConfigNode* findByPath(const std::string& path) const;

    // Convenience builders
    ConfigNode* addGroup(const std::string& name);
    ConfigNode* addController(const std::string& group_name, const std::string& controller_name);
    ConfigNode* addItem(const std::string& group_name, const std::string& controller_name, const std::string& item_name);

    // Inspection
    size_t groupCount() const;
    size_t nodeCount() const;

private:
    std::unique_ptr<ConfigNode> root_;
    size_t node_count_ = 1; // root
};

} // namespace resource_manager
