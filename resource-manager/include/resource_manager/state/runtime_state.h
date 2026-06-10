#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <optional>

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

struct ThreadState {
    int tid;
    std::string threadName;
};

struct ProcessState {
    int pid;
    std::string processName;
    bool attached;
    std::chrono::system_clock::time_point attachTimestamp;
    std::chrono::system_clock::time_point lastSeen;
    DiscoveryStatus discoveryStatus;
    RecoveryState recoveryStatus;
    int retryCount;
};

class RuntimeState {
public:
    RuntimeState();

    void addThread(const ThreadState& thread);
    void removeThread(int tid);
    std::optional<ThreadState> findThread(int tid) const;

    void updatePid(int pid, const std::string& processName);
    void markAttached();
    void markDetached();

    ProcessState& processState();
    const ProcessState& processState() const;
    std::vector<ThreadState>& threads();
    const std::vector<ThreadState>& threads() const;

private:
    ProcessState processState_;
    std::vector<ThreadState> threads_;
};

struct ConfigState {
    // Placeholder: will hold the ConfigDomain Tree reference
};

} // namespace resource_manager
