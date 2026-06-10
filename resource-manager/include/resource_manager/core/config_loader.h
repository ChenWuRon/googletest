#pragma once

#include <string>
#include <optional>

namespace resource_manager {

class ConfigLoader {
public:
    ConfigLoader() = default;

    std::optional<std::string> loadFromFile(const std::string& filepath);

    static std::string loadFromString(const std::string& content);
};

} // namespace resource_manager
