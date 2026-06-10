#include "resource_manager/driver/cgroup_v2_driver.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace resource_manager {

namespace fs = std::filesystem;

CgroupV2Driver::CgroupV2Driver(std::string mountPoint)
    : mountPoint_(std::move(mountPoint))
{
}

std::optional<Error> CgroupV2Driver::createGroup(const std::string& path) {
    auto full = absPath(path);
    if (fs::exists(full)) {
        return std::nullopt;
    }
    if (!fs::create_directories(full)) {
        return Error(ErrorCode::CgroupCreateFailed,
                     "Failed to create cgroup directory: " + full,
                     "CgroupV2Driver");
    }
    return std::nullopt;
}

std::optional<Error> CgroupV2Driver::removeGroup(const std::string& path) {
    auto full = absPath(path);
    if (!fs::exists(full)) {
        return std::nullopt;
    }
    std::error_code ec;
    fs::remove_all(full, ec);
    if (ec) {
        return Error(ErrorCode::CgroupRemoveFailed,
                     "Failed to remove cgroup directory: " + full + ": " + ec.message(),
                     "CgroupV2Driver");
    }
    return std::nullopt;
}

bool CgroupV2Driver::exists(const std::string& path) {
    return fs::is_directory(absPath(path));
}

std::optional<Error> CgroupV2Driver::enableController(
    const std::string& path,
    const std::string& controller)
{
    auto full = absPath(path);
    auto subtreeFile = full + "/cgroup.subtree_control";
    if (!fs::exists(subtreeFile)) {
        return Error(ErrorCode::CgroupWriteFailed,
                     "cgroup.subtree_control not found: " + subtreeFile,
                     "CgroupV2Driver");
    }

    std::string current;
    {
        std::ifstream in(subtreeFile);
        if (in.is_open()) {
            std::getline(in, current);
        }
    }

    std::ostringstream value;
    value << "+" << controller;
    if (!current.empty() && current.find("+" + controller) == std::string::npos) {
        value << " " << current;
    }

    std::ofstream out(subtreeFile);
    if (!out.is_open()) {
        return Error(ErrorCode::CgroupWriteFailed,
                     "Cannot write " + subtreeFile,
                     "CgroupV2Driver");
    }
    out << value.str() << std::endl;
    if (out.fail()) {
        return Error(ErrorCode::CgroupWriteFailed,
                     "Write failed for " + subtreeFile,
                     "CgroupV2Driver");
    }
    return std::nullopt;
}

std::optional<Error> CgroupV2Driver::setValue(
    const std::string& path,
    const std::string& file,
    const std::string& value)
{
    auto fullPath = absPath(path) + "/" + file;
    std::ofstream out(fullPath);
    if (!out.is_open()) {
        return Error(ErrorCode::CgroupWriteFailed,
                     "Cannot open " + fullPath,
                     "CgroupV2Driver");
    }
    out << value << std::endl;
    if (out.fail()) {
        return Error(ErrorCode::CgroupWriteFailed,
                     "Write failed for " + fullPath,
                     "CgroupV2Driver");
    }
    return std::nullopt;
}

std::optional<std::string> CgroupV2Driver::getValue(
    const std::string& path,
    const std::string& file)
{
    auto fullPath = absPath(path) + "/" + file;
    std::ifstream in(fullPath);
    if (!in.is_open()) {
        return std::nullopt;
    }
    std::string value;
    std::getline(in, value);
    return value;
}

std::optional<Error> CgroupV2Driver::attachProcess(const std::string& path, int pid) {
    return setValue(path, "cgroup.procs", std::to_string(pid));
}

std::optional<Error> CgroupV2Driver::attachThread(const std::string& path, int tid) {
    return setValue(path, "cgroup.threads", std::to_string(tid));
}

std::vector<std::string> CgroupV2Driver::listGroups(const std::string& parent) {
    auto full = absPath(parent);
    if (!fs::is_directory(full)) {
        return {};
    }

    std::vector<std::string> groups;
    for (const auto& entry : fs::directory_iterator(full)) {
        if (entry.is_directory()) {
            std::string name = entry.path().filename().string();
            if (!name.empty() && name[0] != '.') {
                if (parent.empty()) {
                    groups.push_back(name);
                } else {
                    groups.push_back(parent + "/" + name);
                }
            }
        }
    }
    return groups;
}

std::string CgroupV2Driver::absPath(const std::string& path) const {
    if (path.empty()) {
        return mountPoint_;
    }
    return mountPoint_ + "/" + path;
}

} // namespace resource_manager
