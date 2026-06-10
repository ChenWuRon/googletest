#pragma once

#include <string>

#include "resource_manager/core/config_domain.h"

namespace resource_manager {

class ConfigRenderer {
public:
    static std::string render(const ConfigDomain& domain);

private:
    static void render_node(const ConfigNode& node, std::string& out, int depth);
};

} // namespace resource_manager
