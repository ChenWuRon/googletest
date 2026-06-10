#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>

#include "resource_manager/error/error.h"

namespace resource_manager {

class ICgroupDriver {
public:
    virtual ~ICgroupDriver() = default;

    virtual std::optional<Error> createGroup(const std::string& path) = 0;
    virtual std::optional<Error> removeGroup(const std::string& path) = 0;
    virtual bool exists(const std::string& path) = 0;
    virtual std::optional<Error> enableController(const std::string& path, const std::string& controller) = 0;
    virtual std::optional<Error> setValue(const std::string& path, const std::string& file, const std::string& value) = 0;
    virtual std::optional<std::string> getValue(const std::string& path, const std::string& file) = 0;
    virtual std::optional<Error> attachProcess(const std::string& path, int pid) = 0;
    virtual std::optional<Error> attachThread(const std::string& path, int tid) = 0;
    virtual std::vector<std::string> listGroups(const std::string& parent) = 0;

    static std::unique_ptr<ICgroupDriver> create(int cgroup_version);
};

} // namespace resource_manager
