#pragma once

// ADR-012 Error Model
// Unified error code and error structure for all components.

#include <string>
#include <map>
#include <memory>
#include <optional>

namespace resource_manager {

enum class ErrorCode {
    // Config
    ConfigParseError,
    ConfigValidationError,
    ConfigFileNotFound,

    // Mode
    InvalidMode,
    ModeMismatch,

    // Controller
    ControllerNotFound,
    ControllerNotSupported,
    InvalidControlValue,

    // Process
    ProcessNotFound,
    ProcessDiscoveryFailed,
    ProcessExited,

    // Attach
    AttachFailed,
    AttachTimeout,
    DetachFailed,

    // Cgroup
    CgroupWriteFailed,
    CgroupReadFailed,
    CgroupCreateFailed,
    CgroupRemoveFailed,
    CgroupVersionMismatch,

    // Recovery
    RecoveryFailed,
    RecoveryTimeout,

    // Internal
    InternalError,
    NotImplemented,
};

struct Error {
    ErrorCode code;
    std::string message;
    std::string source;
    std::map<std::string, std::string> context;
    std::unique_ptr<Error> cause;

    Error(ErrorCode c, std::string msg, std::string src)
        : code(c), message(std::move(msg)), source(std::move(src)) {}
};

} // namespace resource_manager
