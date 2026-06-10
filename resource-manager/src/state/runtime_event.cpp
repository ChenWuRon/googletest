#include "resource_manager/state/runtime_event.h"

#include <sstream>
#include <iomanip>
#include <ctime>

namespace resource_manager {

std::string eventTypeToString(EventType type) {
    switch (type) {
        case EventType::ProcessDiscovered: return "ProcessDiscovered";
        case EventType::ProcessLost: return "ProcessLost";
        case EventType::ProcessRestarted: return "ProcessRestarted";
        case EventType::ProcessAttached: return "ProcessAttached";
        case EventType::ProcessDetached: return "ProcessDetached";
        case EventType::RecoveryStarted: return "RecoveryStarted";
        case EventType::RecoverySucceeded: return "RecoverySucceeded";
        case EventType::RecoveryFailed: return "RecoveryFailed";
    }
    return "Unknown";
}

RuntimeEvent::RuntimeEvent(EventType type, int pid, std::string source)
    : type_(type)
    , pid_(pid)
    , source_(std::move(source))
    , timestamp_(std::chrono::system_clock::now())
{
}

static std::string formatTimePoint(const std::chrono::system_clock::time_point& tp) {
    auto tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
    localtime_r(&tt, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string RuntimeEvent::serialize() const {
    std::ostringstream oss;
    oss << "[" << formatTimePoint(timestamp_) << "] "
        << source_ << ": " << eventTypeToString(type_)
        << " (pid=" << pid_ << ")";
    return oss.str();
}

} // namespace resource_manager
