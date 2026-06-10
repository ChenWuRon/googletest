#pragma once

// Logging Framework — Logger implementation
// Supports: console output, file output, rotation placeholder.

#include <string>
#include <fstream>
#include <mutex>

#include "resource_manager/log/ilogger.h"
#include "resource_manager/error/error.h"

namespace resource_manager {

struct LoggerConfig {
    std::string file_path;        // empty = no file output
    LogLevel min_level = LogLevel::INFO;
    bool console_output = true;
    bool file_output = false;
    size_t max_file_size = 0;     // rotation placeholder: 0 = no rotation
    size_t max_backup_files = 0;  // rotation placeholder
};

class Logger : public ILogger {
public:
    explicit Logger(LoggerConfig config);
    ~Logger() override;

    void log(LogLevel level, const std::string& message) override;

    // Rotation placeholder — returns NotImplemented when max_file_size > 0.
    // Will be replaced with real log rotation in Phase 4.
    std::optional<Error> rotate();

private:
    std::string format(LogLevel level, const std::string& message) const;
    void write_console(const std::string& formatted);
    std::optional<Error> write_file(const std::string& formatted);

    LoggerConfig config_;
    std::ofstream file_stream_;
    std::mutex mutex_;
};

} // namespace resource_manager
