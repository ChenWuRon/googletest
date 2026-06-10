#include "resource_manager/discovery/procfs_discovery.h"
#include "resource_manager/discovery/discovery_rules.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <signal.h>
#include <algorithm>
#include <cstring>

namespace resource_manager {

namespace fs = std::filesystem;

ProcfsDiscovery::ProcfsDiscovery(std::string procPath)
    : procPath_(std::move(procPath))
{
}

std::optional<ProcessInfo> ProcfsDiscovery::findProcess(const MatchRule& rule) {
    auto processes = findProcesses(rule);
    if (!processes || processes->empty()) {
        return std::nullopt;
    }
    return processes->front();
}

std::optional<std::vector<ProcessInfo>> ProcfsDiscovery::findProcesses(const MatchRule& rule) {
    auto all = scanAll();
    if (all.empty()) {
        return std::nullopt;
    }

    MatchType type = DiscoveryRules::parseType(rule.type);
    std::vector<ProcessInfo> matched;
    for (const auto& proc : all) {
        if (DiscoveryRules::match(rule.pattern, type, proc.comm)) {
            matched.push_back(proc);
        }
    }

    if (matched.empty()) {
        return std::nullopt;
    }
    return matched;
}

std::optional<std::vector<ThreadInfo>> ProcfsDiscovery::findThreads(int pid) {
    std::string taskPath = procPath_ + "/" + std::to_string(pid) + "/task";
    if (!fs::is_directory(taskPath)) {
        return std::nullopt;
    }

    std::vector<ThreadInfo> threads;
    for (const auto& entry : fs::directory_iterator(taskPath)) {
        std::string tidStr = entry.path().filename().string();

        char* end = nullptr;
        int tid = static_cast<int>(std::strtol(tidStr.c_str(), &end, 10));
        if (*end != '\0') continue;

        std::string comm = readFile(entry.path().string() + "/comm");
        if (!comm.empty() && comm.back() == '\n') {
            comm.pop_back();
        }

        threads.push_back({tid, comm});
    }

    if (threads.empty()) {
        return std::nullopt;
    }
    return threads;
}

bool ProcfsDiscovery::exists(int pid) {
    return ::kill(pid, 0) == 0;
}

std::vector<ProcessInfo> ProcfsDiscovery::scanAll() const {
    std::vector<ProcessInfo> result;

    if (!fs::is_directory(procPath_)) {
        return result;
    }

    for (const auto& entry : fs::directory_iterator(procPath_)) {
        std::string name = entry.path().filename().string();

        char* end = nullptr;
        int pid = static_cast<int>(std::strtol(name.c_str(), &end, 10));
        if (*end != '\0') continue;

        auto proc = readProcess(pid);
        if (proc) {
            result.push_back(std::move(*proc));
        }
    }

    return result;
}

std::optional<ProcessInfo> ProcfsDiscovery::readProcess(int pid) const {
    std::string basePath = procPath_ + "/" + std::to_string(pid);
    if (!fs::is_directory(basePath)) {
        return std::nullopt;
    }

    ProcessInfo info;
    info.pid = pid;
    info.comm = readFile(basePath + "/comm");
    if (!info.comm.empty() && info.comm.back() == '\n') {
        info.comm.pop_back();
    }

    info.cmdline = readFile(basePath + "/cmdline");

    info.exe = readLink(basePath + "/exe");

    std::string cgroupContent = readFile(basePath + "/cgroup");
    std::istringstream stream(cgroupContent);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty()) {
            size_t colon1 = line.find(':');
            size_t colon2 = line.find(':', colon1 + 1);
            if (colon1 != std::string::npos && colon2 != std::string::npos) {
                std::string controller = line.substr(colon1 + 1, colon2 - colon1 - 1);
                std::string path = line.substr(colon2 + 1);
                if (controller.empty()) {
                    controller = "default";
                }
                info.cgroup_paths[controller] = path;
            }
        }
    }

    return info;
}

std::string ProcfsDiscovery::readFile(const std::string& path) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string ProcfsDiscovery::readLink(const std::string& path) const {
    char buffer[4096];
    ssize_t len = ::readlink(path.c_str(), buffer, sizeof(buffer) - 1);
    if (len == -1) {
        return {};
    }
    buffer[len] = '\0';
    return std::string(buffer);
}

} // namespace resource_manager
