#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <optional>

#include "resource_manager/state/runtime_state.h"

namespace resource_manager {

class RuntimeRepository {
public:
    RuntimeRepository() = default;

    bool registerProcess(const std::string& name, int pid);
    bool removeProcess(int pid);
    std::optional<RuntimeState> findByPid(int pid) const;
    std::optional<RuntimeState> findByName(const std::string& name) const;
    std::vector<RuntimeState> getAll() const;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<int, RuntimeState> byPid_;
    std::unordered_map<std::string, int> nameToPid_;
};

} // namespace resource_manager
