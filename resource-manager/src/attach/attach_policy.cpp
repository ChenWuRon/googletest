#include "resource_manager/attach/attach_policy.h"

namespace resource_manager {

AttachPolicy::AttachPolicy(AttachPolicyType type)
    : type_(type)
    , customFilter_(nullptr)
{
}

bool AttachPolicy::shouldAttachProcess() const {
    return type_ == AttachPolicyType::AttachAll ||
           type_ == AttachPolicyType::AttachMainOnly;
}

bool AttachPolicy::shouldAttachThread(const ThreadState& thread) const {
    switch (type_) {
        case AttachPolicyType::AttachAll:
            return true;
        case AttachPolicyType::AttachMainOnly:
            return false;
        case AttachPolicyType::AttachThreadsOnly:
            return thread.tid != 0;
        case AttachPolicyType::CustomSelection:
            return customFilter_ ? customFilter_(thread) : true;
    }
    return true;
}

std::vector<ThreadState> AttachPolicy::filterThreads(const std::vector<ThreadState>& threads) const {
    if (type_ == AttachPolicyType::AttachAll) {
        return threads;
    }
    if (type_ == AttachPolicyType::AttachMainOnly) {
        return {};
    }
    std::vector<ThreadState> filtered;
    for (const auto& t : threads) {
        if (shouldAttachThread(t)) {
            filtered.push_back(t);
        }
    }
    return filtered;
}

AttachPolicyType AttachPolicy::type() const {
    return type_;
}

void AttachPolicy::setCustomFilter(bool (*filter)(const ThreadState&)) {
    customFilter_ = filter;
}

} // namespace resource_manager
