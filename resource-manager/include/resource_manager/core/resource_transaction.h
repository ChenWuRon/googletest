#pragma once

#include <string>
#include <vector>
#include <optional>

#include "resource_manager/driver/icgroup_driver.h"
#include "resource_manager/error/error.h"

namespace resource_manager {

class ResourceTransaction {
public:
    explicit ResourceTransaction(ICgroupDriver& driver);

    std::optional<Error> begin();
    std::optional<Error> apply(const std::string& cgroupPath, const std::string& file, const std::string& value);
    std::optional<Error> commit();
    std::optional<Error> rollback();

    bool isActive() const;

private:
    struct PendingOp {
        std::string cgroupPath;
        std::string file;
        std::string newValue;
        std::string oldValue;
    };

    ICgroupDriver& driver_;
    std::vector<PendingOp> pendingOps_;
    bool active_;
};

} // namespace resource_manager
