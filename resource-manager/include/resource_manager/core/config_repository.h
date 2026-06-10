#pragma once

#include <string>
#include <vector>

#include "resource_manager/core/config_domain.h"
#include "resource_manager/core/validator.h"

namespace resource_manager {

struct RepositoryError {
    std::size_t line;
    std::size_t column;
    std::string path;
    std::string message;
};

class ConfigRepository {
public:
    ConfigRepository() = default;

    bool load(const std::string& filepath);

    bool loadFromString(const std::string& content);

    void replace(ConfigDomain domain);

    const ConfigDomain& getRoot() const;

    const std::vector<RepositoryError>& errors() const;

private:
    ConfigDomain domain_;
    std::vector<RepositoryError> errors_;

    bool apply_source(const std::string& source);

    void clear_errors();
    void add_error(std::size_t line, std::size_t column,
                   std::string path, std::string message);
};

} // namespace resource_manager
