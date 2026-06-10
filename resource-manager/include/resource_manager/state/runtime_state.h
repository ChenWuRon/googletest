#pragma once

// ADR-005 Runtime State
// ConfigState (desired) and RuntimeState (actual) are separated.
// RuntimeState tracks: PID, TID, attach_state, recovery_state.

#include <string>
#include <vector>
#include <chrono>
#include <map>

#include "resource_manager/error/error.h"

namespace resource_manager {

enum class AttachState {
    Pending,
    Attached,
    Detached,
    Failed,
};

enum class RecoveryState {
    None,
    Detecting,
    Recovering,
    Recovered,
    Failed,
};

struct RuntimeState {
    int pid;
    std::vector<int> tids;
    AttachState attach_state;
    RecoveryState recovery_state;
    std::chrono::system_clock::time_point last_discovery_time;
    std::string cgroup_path;
};

struct ConfigState {
    // Placeholder: will hold the ConfigDomain Tree reference
};

} // namespace resource_manager
