#include "resource_manager/driver/icgroup_driver.h"
#include <iostream>

namespace resource_manager {

class MockCgroupDriver : public ICgroupDriver {
public:
    std::optional<Error> create_group(const std::string& path) override {
        std::cout << "[Mock] create_group: " << path << std::endl;
        return std::nullopt;
    }

    std::optional<Error> remove_group(const std::string& path) override {
        std::cout << "[Mock] remove_group: " << path << std::endl;
        return std::nullopt;
    }

    std::optional<Error> set_value(const std::string& path, const std::string& file, const std::string& value) override {
        std::cout << "[Mock] set_value: " << path << "/" << file << " = " << value << std::endl;
        return std::nullopt;
    }

    std::optional<std::string> get_value(const std::string& path, const std::string& file) override {
        std::cout << "[Mock] get_value: " << path << "/" << file << std::endl;
        return std::nullopt;
    }

    std::optional<Error> attach_task(const std::string& path, int tid) override {
        std::cout << "[Mock] attach_task: " << path << " tid=" << tid << std::endl;
        return std::nullopt;
    }
};

} // namespace resource_manager
