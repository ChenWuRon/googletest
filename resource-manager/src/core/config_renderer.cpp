#include "resource_manager/core/config_renderer.h"

namespace resource_manager {

std::string ConfigRenderer::render(const ConfigDomain& domain) {
    std::string out;
    render_node(domain.root(), out, 0);
    return out;
}

void ConfigRenderer::render_node(const ConfigNode& node, std::string& out, int depth) {
    std::string indent(static_cast<std::size_t>(depth) * 4, ' ');

    switch (node.type()) {
        case ConfigNodeType::ROOT:
            for (const auto& [_, child] : node.children()) {
                render_node(*child, out, depth);
                out += '\n';
            }
            break;

        case ConfigNodeType::GROUP:
            out += indent + "group " + node.name() + " {\n";
            for (const auto& [_, child] : node.children()) {
                render_node(*child, out, depth + 1);
            }
            out += indent + "}\n";
            break;

        case ConfigNodeType::CONTROLLER:
            out += indent + "controller " + node.name() + " {\n";
            for (const auto& [_, child] : node.children()) {
                render_node(*child, out, depth + 1);
            }
            out += indent + "}\n";
            break;

        case ConfigNodeType::ITEM:
            out += indent + "item " + node.name()
                 + " = \"" + node.value() + "\";\n";
            break;
    }
}

} // namespace resource_manager
