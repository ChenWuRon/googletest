#pragma once

#include <string>
#include <vector>
#include <optional>

#include "resource_manager/state/runtime_repository.h"
#include "resource_manager/monitor/process_watcher.h"
#include "resource_manager/error/error.h"

namespace resource_manager {

enum class ReconciliationAction {
    NoChange,
    UpdatePid,
    MarkLost,
    MarkRecovered,
};

struct ReconciliationResult {
    ReconciliationAction action;
    std::string processName;
    int pid;
    int oldPid;
};

class RuntimeReconciler {
public:
    std::vector<ReconciliationResult> reconcile(
        RuntimeRepository& repo,
        const ProcessChangeSet& changes) const;

    ReconciliationResult reconcileProcess(
        RuntimeState& state,
        const ProcessChange& change) const;
};

} // namespace resource_manager
