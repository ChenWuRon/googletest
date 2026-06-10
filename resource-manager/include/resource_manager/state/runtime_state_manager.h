#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <optional>

#include "resource_manager/state/runtime_state.h"

namespace resource_manager {

class RuntimeStateManager {
public:
    RuntimeStateManager() = default;

    bool registerProcess(const std::string& name, int pid);
    bool removeProcess(int pid);
    bool updateProcessPid(const std::string& name, int newPid);
    bool setProcessRecoveryStatus(const std::string& name, RecoveryState status);
    bool setProcessDiscoveryStatus(const std::string& name, DiscoveryStatus status);
    bool setProcessGroupPath(const std::string& name, const std::string& groupPath);

    std::optional<RuntimeState> findByPid(int pid) const;
    std::optional<RuntimeState> findByName(const std::string& name) const;
    std::vector<RuntimeState> getAll() const;

    bool markProcessLost(const std::string& name, int pid);
    bool markProcessRecovered(const std::string& name, int pid);
    bool markProcessFailed(const std::string& name);

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<int, RuntimeState> byPid_;
    std::unordered_map<std::string, int> nameToPid_;
};

} // namespace resource_manager
