#pragma once

// ADR-007 Process Discovery
// IProcessDiscovery interface. Scans /proc to find target processes/threads.
// Supports: exact, prefix, wildcard matching.

#include <string>
#include <vector>
#include <map>

#include "resource_manager/error/error.h"
#include "resource_manager/core/config_domain.h"

namespace resource_manager {

struct ProcessInfo {
    int pid;
    std::string comm;
    std::string cmdline;
    std::map<std::string, std::string> cgroup_paths;
};

struct ThreadInfo {
    int tid;
    std::string comm;
};

class IProcessDiscovery {
public:
    virtual ~IProcessDiscovery() = default;

    virtual std::optional<std::vector<ProcessInfo>> find_process(const MatchRule& rule) = 0;
    virtual std::optional<std::vector<ThreadInfo>> find_threads(int pid) = 0;
};

} // namespace resource_manager
