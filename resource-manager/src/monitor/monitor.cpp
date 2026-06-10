#include "resource_manager/monitor/monitor.h"

#include <algorithm>
#include <thread>

namespace resource_manager {

Monitor::Monitor(
    RuntimeRepository& repo,
    DiscoveryService& discovery,
    std::chrono::milliseconds interval)
    : repo_(repo)
    , discovery_(discovery)
    , interval_(interval)
    , running_(false)
{
}

Monitor::~Monitor() {
    stop();
}

void Monitor::start() {
    if (running_.exchange(true)) {
        return;
    }
    thread_ = std::make_unique<std::thread>(&Monitor::loop, this);
}

void Monitor::stop() {
    running_ = false;
    if (thread_ && thread_->joinable()) {
        thread_->join();
        thread_.reset();
    }
}

bool Monitor::isRunning() const {
    return running_.load();
}

std::vector<RuntimeEvent> Monitor::poll() {
    std::vector<RuntimeEvent> newEvents;
    auto allStates = repo_.getAll();

    for (const auto& state : allStates) {
        int pid = state.processState().pid;
        if (pid <= 0) continue;

        auto info = discovery_.discovery().findProcess(
            MatchRule{state.processState().processName, "exact"});

        if (info && info->pid != pid) {
            ProcessChange change;
            change.type = ProcessChangeType::PIDChanged;
            change.pid = info->pid;
            change.oldPid = pid;
            change.processName = state.processState().processName;

            ProcessChangeSet changes;
            changes.changes.push_back(std::move(change));

            reconciler_.reconcile(repo_, changes);

            newEvents.emplace_back(EventType::ProcessRestarted, info->pid, "Monitor");
        } else if (!info) {
            bool exists = discovery_.discovery().exists(pid);
            if (!exists) {
                ProcessChange change;
                change.type = ProcessChangeType::ProcessLost;
                change.pid = pid;
                change.processName = state.processState().processName;

                ProcessChangeSet changes;
                changes.changes.push_back(std::move(change));

                reconciler_.reconcile(repo_, changes);

                newEvents.emplace_back(EventType::ProcessLost, pid, "Monitor");
            }
        }
    }

    {
        std::lock_guard lock(eventsMutex_);
        for (const auto& ev : newEvents) {
            events_.push_back(ev);
        }
    }

    return newEvents;
}

std::vector<RuntimeEvent> Monitor::events() const {
    std::lock_guard lock(eventsMutex_);
    return events_;
}

void Monitor::loop() {
    while (running_) {
        poll();
        std::this_thread::sleep_for(interval_);
    }
}

} // namespace resource_manager
