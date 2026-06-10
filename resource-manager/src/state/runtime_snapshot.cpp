#include "resource_manager/state/runtime_snapshot.h"

#include <sstream>
#include <iomanip>
#include <ctime>

namespace resource_manager {

bool SnapshotDiff::anyChanged() const {
    return pidChanged || processNameChanged || attachedChanged ||
           discoveryStatusChanged || recoveryStatusChanged ||
           retryCountChanged || threadCountChanged;
}

RuntimeSnapshot RuntimeSnapshot::capture(const RuntimeState& state) {
    return RuntimeSnapshot(state);
}

RuntimeSnapshot::RuntimeSnapshot(const RuntimeState& state)
    : processState_(state.processState())
    , threads_(state.threads())
    , capturedAt_(std::chrono::system_clock::now())
{
}

SnapshotDiff RuntimeSnapshot::compare(const RuntimeSnapshot& other) const {
    SnapshotDiff diff;
    diff.pidChanged = (pid() != other.pid());
    diff.processNameChanged = (processName() != other.processName());
    diff.attachedChanged = (attached() != other.attached());
    diff.discoveryStatusChanged = (discoveryStatus() != other.discoveryStatus());
    diff.recoveryStatusChanged = (recoveryStatus() != other.recoveryStatus());
    diff.retryCountChanged = (retryCount() != other.retryCount());
    diff.threadCountChanged = (threads().size() != other.threads().size());
    return diff;
}

static std::string formatTimePoint(const std::chrono::system_clock::time_point& tp) {
    auto tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
    localtime_r(&tt, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string RuntimeSnapshot::serialize() const {
    std::ostringstream oss;
    oss << "RuntimeSnapshot(capturedAt=" << formatTimePoint(capturedAt_)
        << ", pid=" << processState_.pid
        << ", processName=" << processState_.processName
        << ", attached=" << (processState_.attached ? "true" : "false")
        << ", attachTimestamp=" << formatTimePoint(processState_.attachTimestamp)
        << ", lastSeen=" << formatTimePoint(processState_.lastSeen)
        << ", discoveryStatus=" << static_cast<int>(processState_.discoveryStatus)
        << ", recoveryStatus=" << static_cast<int>(processState_.recoveryStatus)
        << ", retryCount=" << processState_.retryCount
        << ", threads=[";
    bool first = true;
    for (const auto& t : threads_) {
        if (!first) oss << ", ";
        first = false;
        oss << "{tid=" << t.tid << ", name=" << t.threadName << "}";
    }
    oss << "])";
    return oss.str();
}

} // namespace resource_manager
