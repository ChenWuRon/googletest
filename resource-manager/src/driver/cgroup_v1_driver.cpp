#include "resource_manager/driver/icgroup_driver.h"

namespace resource_manager {

class CgroupV1Driver : public ICgroupDriver {
public:
    std::optional<Error> create_group(const std::string& path) override {
        // Phase 4 implementation.
        return std::nullopt;
    }

    std::optional<Error> remove_group(const std::string& path) override {
        // Phase 4 implementation.
        return std::nullopt;
    }

    std::optional<Error> set_value(const std::string& path, const std::string& file, const std::string& value) override {
        // Phase 4 implementation.
        return std::nullopt;
    }

    std::optional<std::string> get_value(const std::string& path, const std::string& file) override {
        // Phase 4 implementation.
        return std::nullopt;
    }

    std::optional<Error> attach_task(const std::string& path, int tid) override {
        // Phase 4 implementation.
        return std::nullopt;
    }
};

} // namespace resource_manager
