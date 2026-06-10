#pragma once

#include <string>
#include <vector>
#include <optional>

#include "resource_manager/state/runtime_state_manager.h"
#include "resource_manager/discovery/discovery_service.h"
#include "resource_manager/attach/attach_engine.h"
#include "resource_manager/core/config_repository.h"
#include "resource_manager/state/runtime_event.h"
#include "resource_manager/error/error.h"

namespace resource_manager {

class RecoveryManager {
public:
    RecoveryManager(
        RuntimeStateManager& stateManager,
        DiscoveryService& discovery,
        AttachEngine& attachEngine,
        ConfigRepository& configRepo);

    std::optional<Error> recoverProcess(const std::string& processName);
    std::optional<Error> recoverAll();
    std::optional<Error> recoverProcessWithRetry(const std::string& processName, int maxRetries = 3);

    std::vector<RuntimeEvent> events() const;

private:
    void publishEvent(EventType type, int pid, const std::string& source);

    const ConfigNode* findGroupInConfig(const std::string& name) const;

    RuntimeStateManager& stateManager_;
    DiscoveryService& discovery_;
    AttachEngine& attachEngine_;
    ConfigRepository& configRepo_;
    std::vector<RuntimeEvent> events_;
    mutable std::mutex eventsMutex_;
};

} // namespace resource_manager
