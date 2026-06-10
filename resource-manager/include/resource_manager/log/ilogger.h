#pragma once

// Logging Framework — ILogger interface
// Provides TRACE / DEBUG / INFO / WARN / ERROR levels.
// All components log through this interface.

#include <string>

#include "resource_manager/error/error.h"

namespace resource_manager {

enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO  = 2,
    WARN  = 3,
    ERROR = 4,
};

class ILogger {
public:
    virtual ~ILogger() = default;

    virtual void log(LogLevel level, const std::string& message) = 0;

    virtual void trace(const std::string& message) { log(LogLevel::TRACE, message); }
    virtual void debug(const std::string& message) { log(LogLevel::DEBUG, message); }
    virtual void info(const std::string& message)  { log(LogLevel::INFO,  message); }
    virtual void warn(const std::string& message)  { log(LogLevel::WARN,  message); }
    virtual void error(const std::string& message) { log(LogLevel::ERROR, message); }
};

} // namespace resource_manager
