#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>

#include "resource_manager/discovery/iprocess_discovery.h"
#include "resource_manager/discovery/discovery_rules.h"
#include "resource_manager/state/runtime_repository.h"
#include "resource_manager/state/runtime_event.h"

namespace resource_manager {

class DiscoveryService {
public:
    DiscoveryService(
        std::unique_ptr<IProcessDiscovery> discovery,
        RuntimeRepository& repository
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

    const std::vector<RuntimeEvent>& events() const;

    IProcessDiscovery& discovery();

private:
    void publishEvent(EventType type, int pid, const std::string& source);

    std::unique_ptr<IProcessDiscovery> discovery_;
    RuntimeRepository& repository_;
    std::vector<RuntimeEvent> events_;
};

} // namespace resource_manager
