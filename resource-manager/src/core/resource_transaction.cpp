#include "resource_manager/core/resource_transaction.h"

namespace resource_manager {

ResourceTransaction::ResourceTransaction(ICgroupDriver& driver)
    : driver_(driver)
    , active_(false)
{
}

std::optional<Error> ResourceTransaction::begin() {
    pendingOps_.clear();
    active_ = true;
    return std::nullopt;
}

std::optional<Error> ResourceTransaction::apply(
    const std::string& cgroupPath,
    const std::string& file,
    const std::string& value)
{
    if (!active_) {
        return Error(ErrorCode::InternalError, "Transaction not started", "ResourceTransaction");
    }

    std::string oldValue;
    auto existing = driver_.getValue(cgroupPath, file);
    if (existing) {
        oldValue = std::move(*existing);
    }

    pendingOps_.push_back({cgroupPath, file, value, oldValue});

    auto err = driver_.setValue(cgroupPath, file, value);
    if (err) {
        return err;
    }

    return std::nullopt;
}

std::optional<Error> ResourceTransaction::commit() {
    if (!active_) {
        return Error(ErrorCode::InternalError, "Transaction not started", "ResourceTransaction");
    }

    pendingOps_.clear();
    active_ = false;
    return std::nullopt;
}

std::optional<Error> ResourceTransaction::rollback() {
    if (!active_) {
        return Error(ErrorCode::InternalError, "Transaction not started", "ResourceTransaction");
    }

    std::optional<Error> lastError;

    for (auto it = pendingOps_.rbegin(); it != pendingOps_.rend(); ++it) {
        auto err = driver_.setValue(it->cgroupPath, it->file, it->oldValue);
        if (err) {
            lastError = std::move(err);
        }
    }

    pendingOps_.clear();
    active_ = false;
    return lastError;
}

bool ResourceTransaction::isActive() const {
    return active_;
}

} // namespace resource_manager
