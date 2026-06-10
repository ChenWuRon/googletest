#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>

#include "resource_manager/driver/icgroup_driver.h"

namespace resource_manager {

struct MockOperation {
    std::string method;
    std::string path;
    std::string value;
};

class MockCgroupDriver : public ICgroupDriver {
public:
    std::optional<Error> createGroup(const std::string& path) override;
    std::optional<Error> removeGroup(const std::string& path) override;
    bool exists(const std::string& path) override;
    std::optional<Error> enableController(const std::string& path, const std::string& controller) override;
    std::optional<Error> setValue(const std::string& path, const std::string& file, const std::string& value) override;
    std::optional<std::string> getValue(const std::string& path, const std::string& file) override;
    std::optional<Error> attachProcess(const std::string& path, int pid) override;
    std::optional<Error> attachThread(const std::string& path, int tid) override;
    std::vector<std::string> listGroups(const std::string& parent) override;

    void setNextError(Error err);
    std::vector<MockOperation> operations() const;
    void clearOperations();

private:
    std::map<std::string, std::map<std::string, std::string>> groups_;
    std::map<std::string, std::set<std::string>> enabledControllers_;
    std::vector<MockOperation> ops_;
    std::unique_ptr<Error> nextError_;
};

} // namespace resource_manager
