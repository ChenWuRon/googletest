#pragma once

#include <string>
#include <vector>
#include <chrono>

#include "resource_manager/driver/icgroup_driver.h"
#include "resource_manager/core/config_domain.h"

namespace resource_manager {

struct ResourceEntry {
    std::string cgroupPath;
    std::string controller;
    std::string item;
    std::string value;
};

struct ResourceChange {
    std::string cgroupPath;
    std::string controller;
    std::string item;
    std::string oldValue;
    std::string newValue;
};

struct ResourceDiff {
    std::vector<ResourceChange> changes;
    std::chrono::system_clock::time_point oldTimestamp;
    std::chrono::system_clock::time_point newTimestamp;

    bool empty() const;
    std::size_t size() const;
};

class ResourceSnapshot {
public:
    static ResourceSnapshot capture(ICgroupDriver& driver, const ConfigDomain& domain);

    ResourceDiff compare(const ResourceSnapshot& other) const;
    std::string serialize() const;

    const std::vector<ResourceEntry>& entries() const;
    const std::chrono::system_clock::time_point& capturedAt() const;

private:
    ResourceSnapshot() = default;

    std::vector<ResourceEntry> entries_;
    std::chrono::system_clock::time_point capturedAt_;
};

} // namespace resource_manager
