#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>

#include "resource_manager/discovery/iprocess_discovery.h"
#include "resource_manager/discovery/discovery_rules.h"
#include "resource_manager/state/runtime_state_manager.h"
#include "resource_manager/state/runtime_event.h"

namespace resource_manager {

class DiscoveryService {
public:
    DiscoveryService(
        std::unique_ptr<IProcessDiscovery> discovery,
        RuntimeStateManager& stateManager
    );

    std::optional<ProcessInfo> discoverSingle(
        const MatchRule& rule,
        MatchType matchType,
        const std::string& source
    );

    std::vector<ProcessInfo> discoverAll(
        const MatchRule& rule,
        MatchType matchType,
        const std::string& source
    );

    bool exists(int pid);
    std::optional<ProcessInfo> findProcessByName(const std::string& name);

    const std::vector<RuntimeEvent>& events() const;

private:
    void publishEvent(EventType type, int pid, const std::string& source);

    std::unique_ptr<IProcessDiscovery> discovery_;
    RuntimeStateManager& stateManager_;
    std::vector<RuntimeEvent> events_;
};

} // namespace resource_manager
