#include "resource_manager/monitor/runtime_reconciler.h"

namespace resource_manager {

std::vector<ReconciliationResult> RuntimeReconciler::reconcile(
    RuntimeStateManager& stateManager,
    const ProcessChangeSet& changes) const
{
    std::vector<ReconciliationResult> results;

    for (const auto& change : changes.changes) {
        auto stateOpt = stateManager.findByName(change.processName);
        if (!stateOpt) {
            if (change.type == ProcessChangeType::ProcessCreated) {
                stateManager.registerProcess(change.processName, change.pid);
                results.push_back({ReconciliationAction::UpdatePid,
                                  change.processName, change.pid, 0});
            }
            continue;
        }

        auto result = reconcileProcess(*stateOpt, change);
        results.push_back(result);

        if (result.action == ReconciliationAction::MarkLost) {
            stateManager.markProcessLost(change.processName, change.pid);
            stateManager.removeProcess(change.pid);
        } else if (result.action == ReconciliationAction::UpdatePid) {
            stateManager.updateProcessPid(change.processName, change.pid);
            stateManager.setProcessDiscoveryStatus(change.processName, DiscoveryStatus::Discovered);
            stateManager.setProcessRecoveryStatus(change.processName, RecoveryState::Recovered);
        }
    }

    return results;
}

ReconciliationResult RuntimeReconciler::reconcileProcess(
    RuntimeState& state,
    const ProcessChange& change) const
{
    switch (change.type) {
        case ProcessChangeType::ProcessCreated:
            state.updatePid(change.pid, change.processName);
            state.processState().discoveryStatus = DiscoveryStatus::Discovered;
            return {ReconciliationAction::UpdatePid,
                    change.processName, change.pid, 0};

        case ProcessChangeType::ProcessLost:
            state.processState().discoveryStatus = DiscoveryStatus::Missing;
            state.processState().attachStatus = AttachStatus::Pending;
            state.processState().recoveryStatus = RecoveryState::Detecting;
            return {ReconciliationAction::MarkLost,
                    change.processName, change.pid, change.oldPid};

        case ProcessChangeType::PIDChanged:
            state.updatePid(change.pid, change.processName);
            state.processState().discoveryStatus = DiscoveryStatus::Discovered;
            state.processState().recoveryStatus = RecoveryState::Recovered;
            return {ReconciliationAction::UpdatePid,
                    change.processName, change.pid, change.oldPid};

        case ProcessChangeType::ThreadChanged:
            for (const auto& t : change.newThreads) {
                auto existing = state.findThread(t.tid);
                if (!existing) {
                    state.addThread(t);
                }
            }
            for (const auto& t : change.oldThreads) {
                bool stillExists = false;
                for (const auto& nt : change.newThreads) {
                    if (nt.tid == t.tid) {
                        stillExists = true;
                        break;
                    }
                }
                if (!stillExists) {
                    state.removeThread(t.tid);
                }
            }
            return {ReconciliationAction::NoChange,
                    change.processName, change.pid, change.oldPid};
    }

    return {ReconciliationAction::NoChange, "", 0, 0};
}

} // namespace resource_manager
