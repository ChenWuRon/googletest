#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>

#include "resource_manager/error/error.h"
#include "resource_manager/core/config_domain.h"

namespace resource_manager {

struct ProcessInfo {
    int pid;
    std::string comm;
    std::string cmdline;
    std::string exe;
    std::map<std::string, std::string> cgroup_paths;
};

struct ThreadInfo {
    int tid;
    std::string comm;
};

class IProcessDiscovery {
public:
    virtual ~IProcessDiscovery() = default;

    virtual std::optional<ProcessInfo> findProcess(const MatchRule& rule) = 0;
    virtual std::optional<std::vector<ProcessInfo>> findProcesses(const MatchRule& rule) = 0;
    virtual std::optional<std::vector<ThreadInfo>> findThreads(int pid) = 0;
    virtual bool exists(int pid) = 0;
};

} // namespace resource_manager
