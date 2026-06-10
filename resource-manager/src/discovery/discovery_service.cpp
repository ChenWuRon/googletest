#include "resource_manager/discovery/discovery_service.h"

namespace resource_manager {

DiscoveryService::DiscoveryService(
    std::unique_ptr<IProcessDiscovery> discovery,
    RuntimeRepository& repository
)
    : discovery_(std::move(discovery))
    , repository_(repository)
{
}

std::optional<ProcessInfo> DiscoveryService::discoverSingle(
    const MatchRule& rule,
    MatchType matchType,
    const std::string& source
) {
    auto result = discovery_->findProcess(rule);
    if (!result) {
        return std::nullopt;
    }

    if (!DiscoveryRules::match(rule.pattern, matchType, result->comm)) {
        return std::nullopt;
    }

    repository_.registerProcess(result->comm, result->pid);
    publishEvent(EventType::ProcessDiscovered, result->pid, source);

    return result;
}

std::vector<ProcessInfo> DiscoveryService::discoverAll(
    const MatchRule& rule,
    MatchType matchType,
    const std::string& source
) {
    auto result = discovery_->findProcesses(rule);
    if (!result) {
        return {};
    }

    std::vector<ProcessInfo> matched;
    for (const auto& proc : *result) {
        if (DiscoveryRules::match(rule.pattern, matchType, proc.comm)) {
            repository_.registerProcess(proc.comm, proc.pid);
            publishEvent(EventType::ProcessDiscovered, proc.pid, source);
            matched.push_back(proc);
        }
    }

    return matched;
}

const std::vector<RuntimeEvent>& DiscoveryService::events() const {
    return events_;
}

void DiscoveryService::publishEvent(EventType type, int pid, const std::string& source) {
    events_.emplace_back(type, pid, source);
}

} // namespace resource_manager
