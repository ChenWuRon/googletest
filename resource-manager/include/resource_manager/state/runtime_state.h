#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <optional>

#include "resource_manager/mode/mode.h"

namespace resource_manager {

enum class RecoveryState {
    None,
    Detecting,
    Recovering,
    Recovered,
    Failed,
};

enum class DiscoveryStatus {
    Unknown,
    Discovering,
    Discovered,
    Missing,
    Excluded,
};

enum class AttachStatus {
    None,
    Pending,
    Attached,
    Detached,
    Failed,
};

struct ThreadState {
    int tid;
    std::string threadName;
};

struct ProcessState {
    // Process Identity (survives restart conceptually)
    Mode mode;
    std::string processName;
    std::string matchPattern;
    std::string serviceName;

    // Livelihood (changes on restart)
    int pid = 0;

    // Status (three orthogonal dimensions)
    AttachStatus attachStatus = AttachStatus::None;
    DiscoveryStatus discoveryStatus = DiscoveryStatus::Unknown;
    RecoveryState recoveryStatus = RecoveryState::None;

    // Attachment context
    std::string attachedGroupPath;
    std::chrono::system_clock::time_point attachTimestamp;

    // Last seen tracking
    std::chrono::system_clock::time_point lastSeen;
    int lastSeenPid = 0;

    // Retry
    int retryCount = 0;
};

class RuntimeState {
public:
    RuntimeState();

    void addThread(const ThreadState& thread);
    void removeThread(int tid);
    std::optional<ThreadState> findThread(int tid) const;

    void updatePid(int pid, const std::string& processName);
    void markAttached(const std::string& groupPath = "");
    void markDetached();
    void setAttachStatus(AttachStatus status);
    void updateLastSeen(int pid);

    ProcessState& processState();
    const ProcessState& processState() const;
    std::vector<ThreadState>& threads();
    const std::vector<ThreadState>& threads() const;

private:
    ProcessState processState_;
    std::vector<ThreadState> threads_;
};

struct ConfigState {
    std::string source;
    std::chrono::system_clock::time_point loaded_at;
    std::size_t version = 0;
};

} // namespace resource_manager
