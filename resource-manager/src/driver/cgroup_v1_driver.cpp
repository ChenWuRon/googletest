#include "resource_manager/driver/icgroup_driver.h"

namespace resource_manager {

class CgroupV1Driver : public ICgroupDriver {
public:
    std::optional<Error> createGroup(const std::string& path) override {
        return std::nullopt;
    }

    std::optional<Error> removeGroup(const std::string& path) override {
        return std::nullopt;
    }

    bool exists(const std::string& path) override {
        return false;
    }

    std::optional<Error> enableController(const std::string& path, const std::string& controller) override {
        return std::nullopt;
    }

    std::optional<Error> setValue(const std::string& path, const std::string& file, const std::string& value) override {
        return std::nullopt;
    }

    std::optional<std::string> getValue(const std::string& path, const std::string& file) override {
        return std::nullopt;
    }

    std::optional<Error> attachProcess(const std::string& path, int pid) override {
        return std::nullopt;
    }

    std::optional<Error> attachThread(const std::string& path, int tid) override {
        return std::nullopt;
    }

    std::vector<std::string> listGroups(const std::string& parent) override {
        return {};
    }
};

} // namespace resource_manager
