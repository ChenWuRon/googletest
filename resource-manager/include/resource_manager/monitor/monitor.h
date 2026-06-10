#pragma once

#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <functional>

#include "resource_manager/state/runtime_state_manager.h"
#include "resource_manager/discovery/discovery_service.h"
#include "resource_manager/state/runtime_event.h"
#include "resource_manager/monitor/process_watcher.h"
#include "resource_manager/monitor/runtime_reconciler.h"

namespace resource_manager {

class Monitor {
public:
    Monitor(
        RuntimeStateManager& stateManager,
        DiscoveryService& discovery,
        std::chrono::milliseconds interval = std::chrono::seconds(5));

    ~Monitor();

    Monitor(const Monitor&) = delete;
    Monitor& operator=(const Monitor&) = delete;

    void start();
    void stop();
    bool isRunning() const;

    std::vector<RuntimeEvent> poll();

    std::vector<RuntimeEvent> events() const;

private:
    void loop();

    RuntimeStateManager& stateManager_;
    DiscoveryService& discovery_;
    std::chrono::milliseconds interval_;
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> thread_;
    RuntimeReconciler reconciler_;
    std::vector<RuntimeEvent> events_;
    mutable std::mutex eventsMutex_;
};

} // namespace resource_manager
