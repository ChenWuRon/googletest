#include "resource_manager/core/config_loader.h"

#include <fstream>
#include <sstream>

namespace resource_manager {

std::optional<std::string> ConfigLoader::loadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return std::nullopt;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string ConfigLoader::loadFromString(const std::string& content) {
    return content;
}

} // namespace resource_manager
