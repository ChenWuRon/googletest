#pragma once

// ADR-006 Cgroup Driver
// ICgroupDriver interface. Business layer MUST NOT access /sys/fs/cgroup directly.
// Supported: v1 (CgroupV1Driver), v2 (CgroupV2Driver), mock (MockCgroupDriver).

#include <string>
#include <memory>

#include "resource_manager/error/error.h"

namespace resource_manager {

class ICgroupDriver {
public:
    virtual ~ICgroupDriver() = default;

    virtual std::optional<Error> create_group(const std::string& path) = 0;
    virtual std::optional<Error> remove_group(const std::string& path) = 0;
    virtual std::optional<Error> set_value(const std::string& path, const std::string& file, const std::string& value) = 0;
    virtual std::optional<std::string> get_value(const std::string& path, const std::string& file) = 0;
    virtual std::optional<Error> attach_task(const std::string& path, int tid) = 0;

    static std::unique_ptr<ICgroupDriver> create(int cgroup_version);
};

} // namespace resource_manager
