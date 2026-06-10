#include "resource_manager/attach/attach_tracker.h"

#include <algorithm>

namespace resource_manager {

void AttachTracker::registerAttach(int pid, int tid, const std::string& groupPath) {
    std::lock_guard lock(mutex_);

    auto it = std::find_if(records_.begin(), records_.end(),
        [pid, tid](const AttachRecord& r) {
            return r.pid == pid && r.tid == tid;
        });

    if (it != records_.end()) {
        it->groupPath = groupPath;
        it->timestamp = std::chrono::system_clock::now();
        it->status = AttachStatus::Attached;
        return;
    }

    records_.push_back({
        pid, tid, groupPath,
        std::chrono::system_clock::now(),
        AttachStatus::Attached
    });
}

void AttachTracker::removeAttach(int pid, int tid) {
    std::lock_guard lock(mutex_);

    auto it = std::find_if(records_.begin(), records_.end(),
        [pid, tid](const AttachRecord& r) {
            return r.pid == pid && r.tid == tid;
        });

    if (it != records_.end()) {
        records_.erase(it);
    }
}

void AttachTracker::markFailed(int pid, int tid) {
    std::lock_guard lock(mutex_);

    auto it = std::find_if(records_.begin(), records_.end(),
        [pid, tid](const AttachRecord& r) {
            return r.pid == pid && r.tid == tid;
        });

    if (it != records_.end()) {
        it->status = AttachStatus::Failed;
    }
}

std::optional<AttachRecord> AttachTracker::queryAttach(int pid, int tid) const {
    std::lock_guard lock(mutex_);

    auto it = std::find_if(records_.begin(), records_.end(),
        [pid, tid](const AttachRecord& r) {
            return r.pid == pid && r.tid == tid;
        });

    if (it == records_.end()) {
        return std::nullopt;
    }
    return *it;
}

std::vector<AttachRecord> AttachTracker::queryByPid(int pid) const {
    std::lock_guard lock(mutex_);

    std::vector<AttachRecord> result;
    for (const auto& r : records_) {
        if (r.pid == pid) {
            result.push_back(r);
        }
    }
    return result;
}

std::vector<AttachRecord> AttachTracker::queryByGroup(const std::string& groupPath) const {
    std::lock_guard lock(mutex_);

    std::vector<AttachRecord> result;
    for (const auto& r : records_) {
        if (r.groupPath == groupPath) {
            result.push_back(r);
        }
    }
    return result;
}

bool AttachTracker::isAttached(int pid) const {
    std::lock_guard lock(mutex_);

    for (const auto& r : records_) {
        if (r.pid == pid && r.status == AttachStatus::Attached) {
            return true;
        }
    }
    return false;
}

bool AttachTracker::isThreadAttached(int pid, int tid) const {
    std::lock_guard lock(mutex_);

    for (const auto& r : records_) {
        if (r.pid == pid && r.tid == tid && r.status == AttachStatus::Attached) {
            return true;
        }
    }
    return false;
}

std::size_t AttachTracker::size() const {
    std::lock_guard lock(mutex_);
    return records_.size();
}

} // namespace resource_manager
