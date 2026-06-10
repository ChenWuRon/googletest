#include "resource_manager/recovery/recovery_manager.h"

#include <thread>
#include <algorithm>

namespace resource_manager {

RecoveryManager::RecoveryManager(
    RuntimeRepository& repo,
    DiscoveryService& discovery)
    : repo_(repo)
    , discovery_(discovery)
{
}

std::optional<Error> RecoveryManager::recoverProcess(const std::string& processName) {
    auto stateOpt = repo_.findByName(processName);
    if (!stateOpt) {
        publishEvent(EventType::RecoveryFailed, 0, "RecoveryManager");
        return Error(ErrorCode::ProcessNotFound,
                     "Process not found: " + processName,
                     "RecoveryManager");
    }

    int oldPid = stateOpt->processState().pid;

    auto info = discovery_.discovery().findProcess(
        MatchRule{processName, "exact"});
    if (!info) {
        repo_.setProcessRecoveryStatus(processName, RecoveryState::Failed);
        publishEvent(EventType::RecoveryFailed, oldPid, "RecoveryManager");
        return Error(ErrorCode::ProcessNotFound,
                     "Process not found during recovery: " + processName,
                     "RecoveryManager");
    }

    if (info->pid != oldPid) {
        repo_.updateProcessPid(processName, info->pid);
    }
    repo_.setProcessDiscoveryStatus(processName, DiscoveryStatus::Discovered);
    repo_.setProcessRecoveryStatus(processName, RecoveryState::Recovered);

    publishEvent(EventType::RecoverySucceeded, info->pid, "RecoveryManager");
    return std::nullopt;
}

std::optional<Error> RecoveryManager::recoverAll() {
    auto allStates = repo_.getAll();

    for (const auto& st : allStates) {
        if (st.processState().recoveryStatus == RecoveryState::Failed ||
            st.processState().discoveryStatus == DiscoveryStatus::Missing)
        {
            auto err = recoverProcess(st.processState().processName);
            if (err) {
                repo_.setProcessDiscoveryStatus(
                    st.processState().processName, DiscoveryStatus::Missing);
            }
        }
    }

    return std::nullopt;
}

std::optional<Error> RecoveryManager::recoverProcessWithRetry(
    const std::string& processName, int maxRetries)
{
    for (int attempt = 0; attempt < maxRetries; attempt++) {
        auto err = recoverProcess(processName);
        if (!err) {
            return std::nullopt;
        }

        if (attempt < maxRetries - 1) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    repo_.setProcessRecoveryStatus(processName, RecoveryState::Failed);

    return Error(ErrorCode::RecoveryFailed,
                 "Recovery failed after " + std::to_string(maxRetries) + " retries: " + processName,
                 "RecoveryManager");
}

std::vector<RuntimeEvent> RecoveryManager::events() const {
    std::lock_guard lock(eventsMutex_);
    return events_;
}

void RecoveryManager::publishEvent(EventType type, int pid, const std::string& source) {
    std::lock_guard lock(eventsMutex_);
    events_.emplace_back(type, pid, source);
}

} // namespace resource_manager
