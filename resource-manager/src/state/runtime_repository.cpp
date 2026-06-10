#include "resource_manager/state/runtime_repository.h"
#include <mutex>

namespace resource_manager {

bool RuntimeRepository::registerProcess(const std::string& name, int pid) {
    std::unique_lock lock(mutex_);

    if (byPid_.find(pid) != byPid_.end()) {
        return false;
    }
    if (nameToPid_.find(name) != nameToPid_.end()) {
        return false;
    }

    RuntimeState state;
    state.processState().pid = pid;
    state.processState().processName = name;

    byPid_[pid] = std::move(state);
    nameToPid_[name] = pid;
    return true;
}

bool RuntimeRepository::updateProcessPid(const std::string& name, int newPid) {
    std::unique_lock lock(mutex_);

    auto nit = nameToPid_.find(name);
    if (nit == nameToPid_.end()) {
        return false;
    }
    int oldPid = nit->second;
    if (oldPid == newPid) {
        return true;
    }
    auto oldIt = byPid_.find(oldPid);
    if (oldIt == byPid_.end()) {
        return false;
    }
    if (byPid_.find(newPid) != byPid_.end()) {
        return false;
    }
    auto state = std::move(oldIt->second);
    state.processState().pid = newPid;
    byPid_.erase(oldIt);
    byPid_[newPid] = std::move(state);
    nit->second = newPid;
    return true;
}

bool RuntimeRepository::setProcessRecoveryStatus(const std::string& name, RecoveryState status) {
    std::unique_lock lock(mutex_);

    auto nit = nameToPid_.find(name);
    if (nit == nameToPid_.end()) {
        return false;
    }
    auto it = byPid_.find(nit->second);
    if (it == byPid_.end()) {
        return false;
    }
    it->second.processState().recoveryStatus = status;
    return true;
}

bool RuntimeRepository::setProcessDiscoveryStatus(const std::string& name, DiscoveryStatus status) {
    std::unique_lock lock(mutex_);

    auto nit = nameToPid_.find(name);
    if (nit == nameToPid_.end()) {
        return false;
    }
    auto it = byPid_.find(nit->second);
    if (it == byPid_.end()) {
        return false;
    }
    it->second.processState().discoveryStatus = status;
    return true;
}

bool RuntimeRepository::removeProcess(int pid) {
    std::unique_lock lock(mutex_);

    auto it = byPid_.find(pid);
    if (it == byPid_.end()) {
        return false;
    }

    nameToPid_.erase(it->second.processState().processName);
    byPid_.erase(it);
    return true;
}

std::optional<RuntimeState> RuntimeRepository::findByPid(int pid) const {
    std::shared_lock lock(mutex_);

    auto it = byPid_.find(pid);
    if (it == byPid_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<RuntimeState> RuntimeRepository::findByName(const std::string& name) const {
    std::shared_lock lock(mutex_);

    auto it = nameToPid_.find(name);
    if (it == nameToPid_.end()) {
        return std::nullopt;
    }
    return findByPid(it->second);
}

std::vector<RuntimeState> RuntimeRepository::getAll() const {
    std::shared_lock lock(mutex_);

    std::vector<RuntimeState> result;
    result.reserve(byPid_.size());
    for (const auto& [pid, state] : byPid_) {
        result.push_back(state);
    }
    return result;
}

} // namespace resource_manager
