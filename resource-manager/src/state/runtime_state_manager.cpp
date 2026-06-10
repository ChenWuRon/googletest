#include <mutex>

#include "resource_manager/state/runtime_state_manager.h"

namespace resource_manager {

bool RuntimeStateManager::registerProcess(const std::string& name, int pid) {
    std::unique_lock lock(mutex_);

    if (pid <= 0 || name.empty()) {
        return false;
    }

    if (byPid_.find(pid) != byPid_.end()) {
        return false;
    }

    RuntimeState state;
    state.updatePid(pid, name);
    state.processState().discoveryStatus = DiscoveryStatus::Discovered;

    byPid_[pid] = std::move(state);
    nameToPid_[name] = pid;
    return true;
}

bool RuntimeStateManager::removeProcess(int pid) {
    std::unique_lock lock(mutex_);

    auto it = byPid_.find(pid);
    if (it == byPid_.end()) {
        return false;
    }

    const auto& name = it->second.processState().processName;
    nameToPid_.erase(name);
    byPid_.erase(it);
    return true;
}

bool RuntimeStateManager::updateProcessPid(const std::string& name, int newPid) {
    std::unique_lock lock(mutex_);

    auto nameIt = nameToPid_.find(name);
    if (nameIt == nameToPid_.end()) {
        return false;
    }

    int oldPid = nameIt->second;
    auto stateIt = byPid_.find(oldPid);
    if (stateIt == byPid_.end()) {
        return false;
    }

    RuntimeState state = std::move(stateIt->second);
    byPid_.erase(stateIt);
    state.updatePid(newPid, name);

    nameToPid_[name] = newPid;
    byPid_[newPid] = std::move(state);
    return true;
}

bool RuntimeStateManager::setProcessAttachStatus(const std::string& name, AttachStatus status) {
    std::unique_lock lock(mutex_);

    auto nameIt = nameToPid_.find(name);
    if (nameIt == nameToPid_.end()) {
        return false;
    }

    auto stateIt = byPid_.find(nameIt->second);
    if (stateIt == byPid_.end()) {
        return false;
    }

    stateIt->second.processState().attachStatus = status;
    return true;
}

bool RuntimeStateManager::setProcessRecoveryStatus(const std::string& name, RecoveryState status) {
    std::unique_lock lock(mutex_);

    auto nameIt = nameToPid_.find(name);
    if (nameIt == nameToPid_.end()) {
        return false;
    }

    auto stateIt = byPid_.find(nameIt->second);
    if (stateIt == byPid_.end()) {
        return false;
    }

    stateIt->second.processState().recoveryStatus = status;
    return true;
}

bool RuntimeStateManager::setProcessDiscoveryStatus(const std::string& name, DiscoveryStatus status) {
    std::unique_lock lock(mutex_);

    auto nameIt = nameToPid_.find(name);
    if (nameIt == nameToPid_.end()) {
        return false;
    }

    auto stateIt = byPid_.find(nameIt->second);
    if (stateIt == byPid_.end()) {
        return false;
    }

    stateIt->second.processState().discoveryStatus = status;
    return true;
}

std::optional<RuntimeState> RuntimeStateManager::findByPid(int pid) const {
    std::shared_lock lock(mutex_);

    auto it = byPid_.find(pid);
    if (it == byPid_.end()) {
        return std::nullopt;
    }

    return it->second;
}

std::optional<RuntimeState> RuntimeStateManager::findByName(const std::string& name) const {
    std::shared_lock lock(mutex_);

    auto nameIt = nameToPid_.find(name);
    if (nameIt == nameToPid_.end()) {
        return std::nullopt;
    }

    auto stateIt = byPid_.find(nameIt->second);
    if (stateIt == byPid_.end()) {
        return std::nullopt;
    }

    return stateIt->second;
}

std::vector<RuntimeState> RuntimeStateManager::getAll() const {
    std::shared_lock lock(mutex_);

    std::vector<RuntimeState> result;
    result.reserve(byPid_.size());
    for (const auto& [pid, state] : byPid_) {
        result.push_back(state);
    }
    return result;
}

bool RuntimeStateManager::markProcessLost(const std::string& name, int pid) {
    std::unique_lock lock(mutex_);

    auto nameIt = nameToPid_.find(name);
    if (nameIt == nameToPid_.end()) {
        return false;
    }

    auto stateIt = byPid_.find(pid);
    if (stateIt == byPid_.end()) {
        return false;
    }

    stateIt->second.processState().discoveryStatus = DiscoveryStatus::Missing;
    stateIt->second.processState().attachStatus = AttachStatus::Pending;
    stateIt->second.processState().recoveryStatus = RecoveryState::Detecting;
    return true;
}

bool RuntimeStateManager::markProcessRecovered(const std::string& name, int pid) {
    std::unique_lock lock(mutex_);

    auto nameIt = nameToPid_.find(name);
    if (nameIt == nameToPid_.end()) {
        return false;
    }

    auto stateIt = byPid_.find(pid);
    if (stateIt == byPid_.end()) {
        return false;
    }

    stateIt->second.processState().discoveryStatus = DiscoveryStatus::Discovered;
    stateIt->second.processState().recoveryStatus = RecoveryState::Recovered;
    return true;
}

bool RuntimeStateManager::setProcessGroupPath(const std::string& name, const std::string& groupPath) {
    std::unique_lock lock(mutex_);

    auto nameIt = nameToPid_.find(name);
    if (nameIt == nameToPid_.end()) {
        return false;
    }

    auto stateIt = byPid_.find(nameIt->second);
    if (stateIt == byPid_.end()) {
        return false;
    }

    stateIt->second.processState().attachedGroupPath = groupPath;
    return true;
}

bool RuntimeStateManager::markProcessFailed(const std::string& name) {
    std::unique_lock lock(mutex_);

    auto nameIt = nameToPid_.find(name);
    if (nameIt == nameToPid_.end()) {
        return false;
    }

    auto stateIt = byPid_.find(nameIt->second);
    if (stateIt == byPid_.end()) {
        return false;
    }

    stateIt->second.processState().recoveryStatus = RecoveryState::Failed;
    return true;
}

} // namespace resource_manager
