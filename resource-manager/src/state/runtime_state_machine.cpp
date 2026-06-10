#include "resource_manager/state/runtime_state_machine.h"

namespace resource_manager {

std::string runtimeStateNameToString(RuntimeStateName state) {
    switch (state) {
        case RuntimeStateName::Unknown: return "Unknown";
        case RuntimeStateName::Discovered: return "Discovered";
        case RuntimeStateName::Attached: return "Attached";
        case RuntimeStateName::Running: return "Running";
        case RuntimeStateName::Lost: return "Lost";
        case RuntimeStateName::Recovering: return "Recovering";
        case RuntimeStateName::Failed: return "Failed";
    }
    return "Unknown";
}

RuntimeStateMachine::RuntimeStateMachine()
    : current_(RuntimeStateName::Unknown)
{
}

bool RuntimeStateMachine::isValidTransition(RuntimeStateName from, RuntimeStateName to) {
    switch (from) {
        case RuntimeStateName::Unknown:
            return to == RuntimeStateName::Discovered;
        case RuntimeStateName::Discovered:
            return to == RuntimeStateName::Attached;
        case RuntimeStateName::Attached:
            return to == RuntimeStateName::Running;
        case RuntimeStateName::Running:
            return to == RuntimeStateName::Lost;
        case RuntimeStateName::Lost:
            return to == RuntimeStateName::Recovering;
        case RuntimeStateName::Recovering:
            return to == RuntimeStateName::Running || to == RuntimeStateName::Failed;
        case RuntimeStateName::Failed:
            return false;
    }
    return false;
}

bool RuntimeStateMachine::transition(RuntimeStateName newState) {
    if (!isValidTransition(current_, newState)) {
        return false;
    }
    current_ = newState;
    return true;
}

bool RuntimeStateMachine::canTransitionTo(RuntimeStateName newState) const {
    return isValidTransition(current_, newState);
}

void RuntimeStateMachine::reset() {
    current_ = RuntimeStateName::Unknown;
}

} // namespace resource_manager
