#pragma once

#include <string>
#include <vector>
#include <optional>

#include "resource_manager/discovery/iprocess_discovery.h"

namespace resource_manager {

class ProcfsDiscovery : public IProcessDiscovery {
public:
    explicit ProcfsDiscovery(std::string procPath = "/proc");

    std::optional<ProcessInfo> findProcess(const MatchRule& rule) override;
    std::optional<std::vector<ProcessInfo>> findProcesses(const MatchRule& rule) override;
    std::optional<std::vector<ThreadInfo>> findThreads(int pid) override;
    bool exists(int pid) override;

private:
    std::vector<ProcessInfo> scanAll() const;
    std::optional<ProcessInfo> readProcess(int pid) const;
    std::string readFile(const std::string& path) const;
    std::string readLink(const std::string& path) const;

    std::string procPath_;
};

} // namespace resource_manager
