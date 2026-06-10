#include "resource_manager/driver/mock_cgroup_driver.h"

namespace resource_manager {

std::optional<Error> MockCgroupDriver::createGroup(const std::string& path) {
    if (nextError_) {
        auto err = std::move(*nextError_);
        nextError_.reset();
        return err;
    }
    groups_[path] = std::map<std::string, std::string>();
    ops_.push_back({"createGroup", path, ""});
    return std::nullopt;
}

std::optional<Error> MockCgroupDriver::removeGroup(const std::string& path) {
    if (nextError_) {
        auto err = std::move(*nextError_);
        nextError_.reset();
        return err;
    }
    groups_.erase(path);
    ops_.push_back({"removeGroup", path, ""});
    return std::nullopt;
}

bool MockCgroupDriver::exists(const std::string& path) {
    return groups_.find(path) != groups_.end();
}

std::optional<Error> MockCgroupDriver::enableController(
    const std::string& path,
    const std::string& controller)
{
    if (nextError_) {
        auto err = std::move(*nextError_);
        nextError_.reset();
        return err;
    }
    enabledControllers_[path].insert(controller);
    ops_.push_back({"enableController", path, controller});
    return std::nullopt;
}

std::optional<Error> MockCgroupDriver::setValue(
    const std::string& path,
    const std::string& file,
    const std::string& value)
{
    if (nextError_) {
        auto err = std::move(*nextError_);
        nextError_.reset();
        return err;
    }
    if (groups_.find(path) == groups_.end()) {
        return Error(ErrorCode::CgroupWriteFailed,
                     "Group not found: " + path, "MockCgroupDriver");
    }
    groups_[path][file] = value;
    ops_.push_back({"setValue", path + "/" + file, value});
    return std::nullopt;
}

std::optional<std::string> MockCgroupDriver::getValue(
    const std::string& path,
    const std::string& file)
{
    auto git = groups_.find(path);
    if (git == groups_.end()) {
        return std::nullopt;
    }
    auto fit = git->second.find(file);
    if (fit == git->second.end()) {
        return std::nullopt;
    }
    return fit->second;
}

std::optional<Error> MockCgroupDriver::attachProcess(const std::string& path, int pid) {
    if (nextError_) {
        auto err = std::move(*nextError_);
        nextError_.reset();
        return err;
    }
    ops_.push_back({"attachProcess", path, std::to_string(pid)});
    return setValue(path, "cgroup.procs", std::to_string(pid));
}

std::optional<Error> MockCgroupDriver::attachThread(const std::string& path, int tid) {
    if (nextError_) {
        auto err = std::move(*nextError_);
        nextError_.reset();
        return err;
    }
    ops_.push_back({"attachThread", path, std::to_string(tid)});
    return setValue(path, "cgroup.threads", std::to_string(tid));
}

std::vector<std::string> MockCgroupDriver::listGroups(const std::string& parent) {
    std::vector<std::string> result;
    for (const auto& [name, _] : groups_) {
        if (parent.empty() || name.find(parent + "/") == 0) {
            result.push_back(name);
        }
    }
    return result;
}

void MockCgroupDriver::setNextError(Error err) {
    nextError_ = std::make_unique<Error>(std::move(err));
}

std::vector<MockOperation> MockCgroupDriver::operations() const {
    return ops_;
}

void MockCgroupDriver::clearOperations() {
    ops_.clear();
}

} // namespace resource_manager
