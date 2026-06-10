#pragma once

#include <string>
#include <chrono>

namespace resource_manager {

enum class EventType {
    ProcessDiscovered,
    ProcessLost,
    ProcessRestarted,
    ProcessAttached,
    ProcessDetached,
    RecoveryStarted,
    RecoverySucceeded,
    RecoveryFailed,
};

std::string eventTypeToString(EventType type);

class RuntimeEvent {
public:
    RuntimeEvent(EventType type, int pid, std::string source);

    EventType type() const { return type_; }
    int pid() const { return pid_; }
    const std::string& source() const { return source_; }
    const std::chrono::system_clock::time_point& timestamp() const { return timestamp_; }

    std::string serialize() const;

private:
    EventType type_;
    int pid_;
    std::string source_;
    std::chrono::system_clock::time_point timestamp_;
};

} // namespace resource_manager
