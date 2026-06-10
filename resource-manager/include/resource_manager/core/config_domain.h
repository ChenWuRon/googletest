#pragma once

// ADR-003 Config Tree
// ConfigDomain Tree: ROOT -> GROUP -> CONTROLLER -> ITEM
// Path access model: /groups/<name>/controllers/<type>/items/<name>

#include <string>
#include <map>
#include <memory>
#include <vector>

#include "resource_manager/mode/mode.h"

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

struct Item {
    std::string name;
    std::string value;
    ValueType value_type;
};

struct Controller {
    std::string type;
    std::map<std::string, Item> items;
};

struct MatchRule {
    std::string pattern;
    std::string type; // exact | prefix | wildcard
};

struct Group {
    std::string name;
    Mode mode;
    MatchRule match;
    std::map<std::string, Controller> controllers;
};

struct ConfigDomain {
    std::map<std::string, Group> groups;
};

// Diff model
enum class DiffType {
    NodeAdded,
    NodeRemoved,
    NodeModified,
    NodeUnchanged,
};

struct DiffNode {
    std::string path;
    DiffType type;
    std::shared_ptr<void> old_value;
    std::shared_ptr<void> new_value;
};

using DiffResult = std::vector<DiffNode>;

} // namespace resource_manager
