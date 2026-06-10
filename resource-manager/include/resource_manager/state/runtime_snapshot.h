#pragma once

#include <string>
#include <vector>
#include <chrono>

#include "resource_manager/state/runtime_state.h"

namespace resource_manager {

struct SnapshotDiff {
    bool pidChanged{false};
    bool processNameChanged{false};
    bool attachStatusChanged{false};
    bool discoveryStatusChanged{false};
    bool recoveryStatusChanged{false};
    bool retryCountChanged{false};
    bool threadCountChanged{false};
    bool lastSeenPidChanged{false};

    bool anyChanged() const;
};

class RuntimeSnapshot {
public:
    static RuntimeSnapshot capture(const RuntimeState& state);

    SnapshotDiff compare(const RuntimeSnapshot& other) const;
    std::string serialize() const;

    int pid() const { return processState_.pid; }
    const std::string& processName() const { return processState_.processName; }
    AttachStatus attachStatus() const { return processState_.attachStatus; }
    const std::chrono::system_clock::time_point& attachTimestamp() const { return processState_.attachTimestamp; }
    const std::chrono::system_clock::time_point& lastSeen() const { return processState_.lastSeen; }
    int lastSeenPid() const { return processState_.lastSeenPid; }
    DiscoveryStatus discoveryStatus() const { return processState_.discoveryStatus; }
    RecoveryState recoveryStatus() const { return processState_.recoveryStatus; }
    int retryCount() const { return processState_.retryCount; }
    const Mode& mode() const { return processState_.mode; }
    const std::string& matchPattern() const { return processState_.matchPattern; }
    const std::string& serviceName() const { return processState_.serviceName; }
    const std::vector<ThreadState>& threads() const { return threads_; }
    const std::chrono::system_clock::time_point& capturedAt() const { return capturedAt_; }

private:
    explicit RuntimeSnapshot(const RuntimeState& state);

    ProcessState processState_;
    std::vector<ThreadState> threads_;
    std::chrono::system_clock::time_point capturedAt_;
};

} // namespace resource_manager
