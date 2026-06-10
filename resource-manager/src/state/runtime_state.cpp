#include "resource_manager/state/runtime_state.h"

#include <algorithm>

namespace resource_manager {

RuntimeState::RuntimeState()
    : processState_{0, "", false, "", {}, {}, DiscoveryStatus::Unknown, RecoveryState::None, 0}
{
}

void RuntimeState::addThread(const ThreadState& thread) {
    threads_.push_back(thread);
}

void RuntimeState::removeThread(int tid) {
    threads_.erase(
        std::remove_if(threads_.begin(), threads_.end(),
            [tid](const ThreadState& t) { return t.tid == tid; }),
        threads_.end());
}

std::optional<ThreadState> RuntimeState::findThread(int tid) const {
    for (const auto& t : threads_) {
        if (t.tid == tid) {
            return t;
        }
    }
    return std::nullopt;
}

void RuntimeState::updatePid(int pid, const std::string& processName) {
    processState_.pid = pid;
    processState_.processName = processName;
}

void RuntimeState::markAttached(const std::string& groupPath) {
    processState_.attached = true;
    processState_.attachedGroupPath = groupPath;
    processState_.attachTimestamp = std::chrono::system_clock::now();
}

void RuntimeState::markDetached() {
    processState_.attached = false;
}

ProcessState& RuntimeState::processState() {
    return processState_;
}

const ProcessState& RuntimeState::processState() const {
    return processState_;
}

std::vector<ThreadState>& RuntimeState::threads() {
    return threads_;
}

const std::vector<ThreadState>& RuntimeState::threads() const {
    return threads_;
}

} // namespace resource_manager
