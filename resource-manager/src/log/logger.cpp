#include "resource_manager/log/logger.h"

#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace resource_manager {

static const char* level_string(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

Logger::Logger(LoggerConfig config)
    : config_(std::move(config)) {
    if (config_.file_output && !config_.file_path.empty()) {
        file_stream_.open(config_.file_path, std::ios::app);
        if (!file_stream_.is_open()) {
            std::cerr << "[Logger] Failed to open log file: "
                      << config_.file_path << std::endl;
        }
    }
}

Logger::~Logger() {
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < config_.min_level) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    auto formatted = format(level, message);

    if (config_.console_output) {
        write_console(formatted);
    }

    if (config_.file_output && file_stream_.is_open()) {
        auto err = write_file(formatted);
        if (err) {
            std::cerr << "[Logger] Write failed: " << err->message << std::endl;
        }
    }
}

std::optional<Error> Logger::rotate() {
    // Rotation placeholder.
    // No-op when max_file_size == 0.
    // Will use max_file_size_ and max_backup_files_ when implemented.
    if (config_.max_file_size == 0) {
        return std::nullopt;
    }
    return Error(ErrorCode::NotImplemented, "log rotation not yet implemented", "Logger");
}

std::string Logger::format(LogLevel level, const std::string& message) const {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << "[" << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count() << "] "
        << "[" << level_string(level) << "] "
        << message;
    return oss.str();
}

void Logger::write_console(const std::string& formatted) {
    std::cout << formatted << std::endl;
}

std::optional<Error> Logger::write_file(const std::string& formatted) {
    if (!file_stream_.is_open()) {
        return Error(ErrorCode::InternalError, "log file not open", "Logger");
    }

    file_stream_ << formatted << std::endl;
    if (file_stream_.fail()) {
        return Error(ErrorCode::InternalError, "failed to write log file", "Logger");
    }

    return std::nullopt;
}

} // namespace resource_manager
