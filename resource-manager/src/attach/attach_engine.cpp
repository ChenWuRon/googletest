#include "resource_manager/attach/attach_engine.h"

namespace resource_manager {

AttachEngine::AttachEngine(
    std::unique_ptr<ICgroupDriver> driver,
    std::unique_ptr<AttachPolicy> policy)
    : driver_(std::move(driver))
    , policy_(std::move(policy))
{
}

std::optional<Error> AttachEngine::attach(const ConfigNode& group, RuntimeState& state) {
    std::string cgroupPath = group.name();
    return executeAttach(cgroupPath, group, state);
}

std::optional<Error> AttachEngine::detach(RuntimeState& state) {
    std::string cgroupPath = state.processState().processName;
    if (cgroupPath.empty()) {
        return Error(ErrorCode::DetachFailed, "No cgroup path in state", "AttachEngine");
    }

    if (state.processState().pid > 0) {
        driver_->attachProcess("/", state.processState().pid);
    }

    auto err = driver_->removeGroup(cgroupPath);
    if (err) return err;

    state.markDetached();
    return std::nullopt;
}

std::optional<Error> AttachEngine::reattach(const ConfigNode& group, RuntimeState& state) {
    std::string cgroupPath = group.name();

    const int maxRetries = 3;
    for (int attempt = 0; attempt < maxRetries; attempt++) {
        auto err = executeAttach(cgroupPath, group, state);
        if (!err) return std::nullopt;

        if (err->code == ErrorCode::ControllerNotSupported ||
            err->code == ErrorCode::InvalidControlValue ||
            err->code == ErrorCode::CgroupVersionMismatch) {
            return err;
        }
    }

    driver_->removeGroup(cgroupPath);
    state.processState().recoveryStatus = RecoveryState::Failed;
    return Error(ErrorCode::AttachFailed, "Reattach failed after retries", "AttachEngine");
}

std::optional<Error> AttachEngine::executeAttach(
    const std::string& cgroupPath,
    const ConfigNode& group,
    RuntimeState& state)
{
    auto err = driver_->createGroup(cgroupPath);
    if (err) return err;

    for (const auto& [ctrlName, ctrlChild] : group.children()) {
        if (ctrlChild->type() != ConfigNodeType::CONTROLLER) continue;

        err = driver_->enableController(cgroupPath, ctrlName);
        if (err) return err;

        for (const auto& [itemName, itemChild] : ctrlChild->children()) {
            if (itemChild->type() != ConfigNodeType::ITEM) continue;
            err = driver_->setValue(cgroupPath, itemName, itemChild->value());
            if (err) return err;
        }
    }

    if (policy_->shouldAttachProcess() && state.processState().pid > 0) {
        err = driver_->attachProcess(cgroupPath, state.processState().pid);
        if (err) return err;
    }

    if (policy_->shouldAttachProcess()) {
        auto threads = policy_->filterThreads(state.threads());
        for (const auto& t : threads) {
            err = driver_->attachThread(cgroupPath, t.tid);
            if (err) {
                continue;
            }
        }
    }

    state.markAttached(cgroupPath);
    return std::nullopt;
}

} // namespace resource_manager
