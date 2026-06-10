#pragma once

#include <vector>
#include <string>

#include "resource_manager/state/runtime_snapshot.h"
#include "resource_manager/state/runtime_state_manager.h"
#include "resource_manager/discovery/iprocess_discovery.h"

namespace resource_manager {

enum class ProcessChangeType {
    ProcessCreated,
    ProcessLost,
    PIDChanged,
    ThreadChanged,
};

struct ProcessChange {
    ProcessChangeType type;
    int pid;
    int oldPid;
    std::string processName;
    std::vector<ThreadState> oldThreads;
    std::vector<ThreadState> newThreads;
};

struct ProcessChangeSet {
    std::vector<ProcessChange> changes;

    bool empty() const;
    std::size_t size() const;
    void clear();
};

class ProcessWatcher {
public:
    ProcessChangeSet detectDiscoveryChanges(
        const std::vector<ProcessInfo>& discovered,
        const RuntimeStateManager& stateManager) const;

    ProcessChangeSet detectSnapshotChanges(
        const RuntimeSnapshot& oldSnap,
        const RuntimeSnapshot& newSnap) const;
};

} // namespace resource_manager
