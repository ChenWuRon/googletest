#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <mutex>

namespace resource_manager {

enum class AttachStatus {
    Attached,
    Detached,
    Failed,
};

struct AttachRecord {
    int pid;
    int tid;
    std::string groupPath;
    std::chrono::system_clock::time_point timestamp;
    AttachStatus status;
};

class AttachTracker {
public:
    void registerAttach(int pid, int tid, const std::string& groupPath);
    void removeAttach(int pid, int tid);
    void markFailed(int pid, int tid);

    std::optional<AttachRecord> queryAttach(int pid, int tid) const;
    std::vector<AttachRecord> queryByPid(int pid) const;
    std::vector<AttachRecord> queryByGroup(const std::string& groupPath) const;

    bool isAttached(int pid) const;
    bool isThreadAttached(int pid, int tid) const;
    std::size_t size() const;

private:
    mutable std::mutex mutex_;
    std::vector<AttachRecord> records_;
};

} // namespace resource_manager
