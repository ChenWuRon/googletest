#include "resource_manager/monitor/process_watcher.h"

#include <algorithm>

namespace resource_manager {

bool ProcessChangeSet::empty() const {
    return changes.empty();
}

std::size_t ProcessChangeSet::size() const {
    return changes.size();
}

void ProcessChangeSet::clear() {
    changes.clear();
}

ProcessChangeSet ProcessWatcher::detectDiscoveryChanges(
    const std::vector<ProcessInfo>& discovered,
    const RuntimeStateManager& stateManager) const
{
    ProcessChangeSet result;

    for (const auto& info : discovered) {
        auto existing = stateManager.findByName(info.comm);
        if (!existing) {
            ProcessChange change;
            change.type = ProcessChangeType::ProcessCreated;
            change.pid = info.pid;
            change.processName = info.comm;
            result.changes.push_back(std::move(change));
        } else if (existing->processState().pid != info.pid) {
            ProcessChange change;
            change.type = ProcessChangeType::PIDChanged;
            change.pid = info.pid;
            change.oldPid = existing->processState().pid;
            change.processName = info.comm;
            result.changes.push_back(std::move(change));
        }
    }

    auto allStates = stateManager.getAll();
    for (const auto& state : allStates) {
        int pid = state.processState().pid;
        auto it = std::find_if(discovered.begin(), discovered.end(),
            [pid](const ProcessInfo& info) { return info.pid == pid; });

        if (it == discovered.end()) {
            auto nameIt = std::find_if(discovered.begin(), discovered.end(),
                [&state](const ProcessInfo& info) {
                    return info.comm == state.processState().processName;
                });

            if (nameIt == discovered.end()) {
                ProcessChange change;
                change.type = ProcessChangeType::ProcessLost;
                change.pid = pid;
                change.processName = state.processState().processName;
                result.changes.push_back(std::move(change));
            }
        }
    }

    return result;
}

ProcessChangeSet ProcessWatcher::detectSnapshotChanges(
    const RuntimeSnapshot& oldSnap,
    const RuntimeSnapshot& newSnap) const
{
    ProcessChangeSet result;

    if (oldSnap.pid() != newSnap.pid()) {
        if (oldSnap.pid() == 0) {
            ProcessChange change;
            change.type = ProcessChangeType::ProcessCreated;
            change.pid = newSnap.pid();
            change.processName = newSnap.processName();
            result.changes.push_back(std::move(change));
        } else {
            ProcessChange change;
            change.type = ProcessChangeType::PIDChanged;
            change.pid = newSnap.pid();
            change.oldPid = oldSnap.pid();
            change.processName = newSnap.processName();
            result.changes.push_back(std::move(change));
        }
    }

    if (oldSnap.processName() != newSnap.processName()) {
        if (oldSnap.processName().empty() && oldSnap.pid() == 0) {
            ProcessChange change;
            change.type = ProcessChangeType::ProcessCreated;
            change.pid = newSnap.pid();
            change.processName = newSnap.processName();
            result.changes.push_back(std::move(change));
        }
    }

    const auto& oldThreads = oldSnap.threads();
    const auto& newThreads = newSnap.threads();

    if (oldThreads.size() != newThreads.size()) {
        ProcessChange change;
        change.type = ProcessChangeType::ThreadChanged;
        change.pid = newSnap.pid();
        change.processName = newSnap.processName();
        change.oldThreads = oldThreads;
        change.newThreads = newThreads;
        result.changes.push_back(std::move(change));
    } else {
        bool threadDiff = false;
        for (size_t i = 0; i < oldThreads.size(); i++) {
            if (oldThreads[i].tid != newThreads[i].tid) {
                threadDiff = true;
                break;
            }
        }
        if (threadDiff) {
            ProcessChange change;
            change.type = ProcessChangeType::ThreadChanged;
            change.pid = newSnap.pid();
            change.processName = newSnap.processName();
            change.oldThreads = oldThreads;
            change.newThreads = newThreads;
            result.changes.push_back(std::move(change));
        }
    }

    return result;
}

} // namespace resource_manager
