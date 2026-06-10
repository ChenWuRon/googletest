#pragma once

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

#include "resource_manager/driver/icgroup_driver.h"

namespace resource_manager {

class CgroupV2Driver : public ICgroupDriver {
public:
    explicit CgroupV2Driver(std::string mountPoint = "/sys/fs/cgroup");

    std::optional<Error> createGroup(const std::string& path) override;
    std::optional<Error> removeGroup(const std::string& path) override;
    bool exists(const std::string& path) override;
    std::optional<Error> enableController(const std::string& path, const std::string& controller) override;
    std::optional<Error> setValue(const std::string& path, const std::string& file, const std::string& value) override;
    std::optional<std::string> getValue(const std::string& path, const std::string& file) override;
    std::optional<Error> attachProcess(const std::string& path, int pid) override;
    std::optional<Error> attachThread(const std::string& path, int tid) override;
    std::vector<std::string> listGroups(const std::string& parent) override;

private:
    std::string absPath(const std::string& path) const;

    std::string mountPoint_;
};

} // namespace resource_manager
