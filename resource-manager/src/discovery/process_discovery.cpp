#include "resource_manager/discovery/iprocess_discovery.h"

namespace resource_manager {

class ProcessDiscovery : public IProcessDiscovery {
public:
    std::optional<std::vector<ProcessInfo>> find_process(const MatchRule& rule) override {
        // Phase 3 implementation.
        return std::nullopt;
    }

    std::optional<std::vector<ThreadInfo>> find_threads(int pid) override {
        // Phase 3 implementation.
        return std::nullopt;
    }
};

} // namespace resource_manager
