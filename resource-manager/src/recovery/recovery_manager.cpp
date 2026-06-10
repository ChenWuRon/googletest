#include "resource_manager/recovery/recovery_manager.h"
#include "resource_manager/discovery/discovery_rules.h"

#include <thread>
#include <algorithm>

namespace resource_manager {

RecoveryManager::RecoveryManager(
    RuntimeStateManager& stateManager,
    DiscoveryService& discovery,
    AttachEngine& attachEngine,
    ConfigRepository& configRepo)
    : stateManager_(stateManager)
    , discovery_(discovery)
    , attachEngine_(attachEngine)
    , configRepo_(configRepo)
{
}

std::optional<Error> RecoveryManager::recoverProcess(const std::string& processName) {
    auto stateOpt = stateManager_.findByName(processName);
    if (!stateOpt) {
        publishEvent(EventType::RecoveryFailed, 0, "RecoveryManager");
        return Error(ErrorCode::ProcessNotFound,
                     "Process not found: " + processName,
                     "RecoveryManager");
    }

    int oldPid = stateOpt->processState().pid;

    auto* groupNode = findGroupInConfig(processName);
    if (!groupNode) {
        stateManager_.setProcessRecoveryStatus(processName, RecoveryState::Failed);
        publishEvent(EventType::RecoveryFailed, oldPid, "RecoveryManager");
        return Error(ErrorCode::ConfigNotFound,
                     "Group not found in config: " + processName,
                     "RecoveryManager");
    }

    std::optional<ProcessInfo> info;
    auto matchOpt = groupNode->getMatchRule();
    if (matchOpt) {
        MatchType matchType = DiscoveryRules::parseType(matchOpt->type);
        info = discovery_.discoverSingle(*matchOpt, matchType, "RecoveryManager");
    } else {
        info = discovery_.findProcessByName(processName);
    }

    if (!info) {
        stateManager_.setProcessRecoveryStatus(processName, RecoveryState::Failed);
        publishEvent(EventType::RecoveryFailed, oldPid, "RecoveryManager");
        return Error(ErrorCode::ProcessNotFound,
                     "Process not found during recovery: " + processName,
                     "RecoveryManager");
    }

    if (info->pid != oldPid) {
        stateManager_.updateProcessPid(processName, info->pid);
    }
    stateManager_.setProcessDiscoveryStatus(processName, DiscoveryStatus::Discovered);

    auto freshState = stateManager_.findByName(processName);
    if (!freshState) {
        stateManager_.setProcessRecoveryStatus(processName, RecoveryState::Failed);
        publishEvent(EventType::RecoveryFailed, info->pid, "RecoveryManager");
        return Error(ErrorCode::ProcessNotFound,
                     "Process state not found for reattach: " + processName,
                     "RecoveryManager");
    }

    auto err = attachEngine_.reattach(*groupNode, *freshState);
    if (err) {
        stateManager_.setProcessRecoveryStatus(processName, RecoveryState::Failed);
        publishEvent(EventType::RecoveryFailed, info->pid, "RecoveryManager");
        return err;
    }

    stateManager_.setProcessGroupPath(processName, groupNode->name());
    stateManager_.setProcessRecoveryStatus(processName, RecoveryState::Recovered);

    publishEvent(EventType::RecoverySucceeded, info->pid, "RecoveryManager");
    return std::nullopt;
}

const ConfigNode* RecoveryManager::findGroupInConfig(const std::string& name) const {
    return configRepo_.getRoot().root().findChild(name);
}

std::optional<Error> RecoveryManager::recoverAll() {
    auto allStates = stateManager_.getAll();

    for (const auto& st : allStates) {
        if (st.processState().recoveryStatus == RecoveryState::Failed ||
            st.processState().discoveryStatus == DiscoveryStatus::Missing)
        {
            auto err = recoverProcess(st.processState().processName);
            if (err) {
                stateManager_.setProcessDiscoveryStatus(
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

    stateManager_.setProcessRecoveryStatus(processName, RecoveryState::Failed);

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
