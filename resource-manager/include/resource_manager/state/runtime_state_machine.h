#pragma once

#include <string>

namespace resource_manager {

enum class RuntimeStateName {
    Unknown,
    Discovered,
    Attached,
    Running,
    Lost,
    Recovering,
    Failed,
};

std::string runtimeStateNameToString(RuntimeStateName state);

class RuntimeStateMachine {
public:
    RuntimeStateMachine();

    bool transition(RuntimeStateName newState);
    RuntimeStateName current() const { return current_; }
    bool canTransitionTo(RuntimeStateName newState) const;
    void reset();

private:
    RuntimeStateName current_;

    static bool isValidTransition(RuntimeStateName from, RuntimeStateName to);
};

} // namespace resource_manager
