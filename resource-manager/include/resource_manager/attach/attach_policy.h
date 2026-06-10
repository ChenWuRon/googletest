#pragma once

#include <vector>

#include "resource_manager/state/runtime_state.h"

namespace resource_manager {

enum class AttachPolicyType {
    AttachAll,
    AttachMainOnly,
    AttachThreadsOnly,
    CustomSelection,
};

class AttachPolicy {
public:
    explicit AttachPolicy(AttachPolicyType type);

    bool shouldAttachProcess() const;
    bool shouldAttachThread(const ThreadState& thread) const;
    std::vector<ThreadState> filterThreads(const std::vector<ThreadState>& threads) const;
    AttachPolicyType type() const;

    void setCustomFilter(bool (*filter)(const ThreadState&));

private:
    AttachPolicyType type_;
    bool (*customFilter_)(const ThreadState&);
};

} // namespace resource_manager
