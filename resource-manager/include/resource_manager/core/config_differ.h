#pragma once

#include <string>
#include <vector>

#include "resource_manager/core/config_domain.h"

namespace resource_manager {

enum class DiffType {
    ADDED,
    REMOVED,
    MODIFIED,
};

struct DiffEntry {
    DiffType type;
    std::string path;
    ConfigNodeType node_type;
    std::string old_value;
    std::string new_value;
};

struct DiffResult {
    std::vector<DiffEntry> added;
    std::vector<DiffEntry> removed;
    std::vector<DiffEntry> modified;

    bool has_changes() const {
        return !added.empty() || !removed.empty() || !modified.empty();
    }
};

class ConfigDiffer {
public:
    static DiffResult diff(const ConfigDomain& old_domain,
                           const ConfigDomain& new_domain);

private:
    static void diff_node(const ConfigNode& old_node,
                          const ConfigNode& new_node,
                          DiffResult& result);
};

} // namespace resource_manager
