#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <cstdio>

#include "resource_manager/log/logger.h"

using namespace resource_manager;

TEST(LoggerTest, DefaultConfig) {
    LoggerConfig config;
    Logger logger(config);

    EXPECT_NO_THROW(logger.info("hello"));
    EXPECT_NO_THROW(logger.trace("hidden"));  // below default INFO, filtered
}

TEST(LoggerTest, LogLevelFiltering) {
    LoggerConfig config;
    config.min_level = LogLevel::WARN;
    config.console_output = false;
    Logger logger(config);

    // Should not crash or produce output
    EXPECT_NO_THROW(logger.trace("trace"));
    EXPECT_NO_THROW(logger.debug("debug"));
    EXPECT_NO_THROW(logger.info("info"));
    EXPECT_NO_THROW(logger.warn("warn"));
    EXPECT_NO_THROW(logger.error("error"));
}

TEST(LoggerTest, FileOutput) {
    const char* test_file = "/tmp/test_logger_file_output.log";

    // Clean up before
    std::remove(test_file);

    LoggerConfig config;
    config.console_output = false;
    config.file_output = true;
    config.file_path = test_file;
    Logger logger(config);

    logger.info("line one");
    logger.warn("line two");
    logger.error("line three");

    // Verify file was written
    std::ifstream file(test_file);
    ASSERT_TRUE(file.is_open());

    std::string line;
    int count = 0;
    while (std::getline(file, line)) {
        EXPECT_FALSE(line.empty());
        count++;
    }
    EXPECT_EQ(count, 3);

    file.close();
    std::remove(test_file);
}

TEST(LoggerTest, FileOutputFailure) {
    LoggerConfig config;
    config.file_output = true;
    config.file_path = "/nonexistent_dir_xyz/log.txt";
    // Should not crash, logs error to stderr
    Logger logger(config);
    EXPECT_NO_THROW(logger.info("should not crash on bad path"));
}

TEST(LoggerTest, AllLevelsProduceOutput) {
    LoggerConfig config;
    config.min_level = LogLevel::TRACE;
    config.console_output = false;
    config.file_output = true;
    config.file_path = "/tmp/test_logger_all_levels.log";

    std::remove(config.file_path.c_str());

    Logger logger(config);

    logger.trace("trace msg");
    logger.debug("debug msg");
    logger.info("info msg");
    logger.warn("warn msg");
    logger.error("error msg");

    std::ifstream file(config.file_path);
    ASSERT_TRUE(file.is_open());

    std::string line;
    int count = 0;
    while (std::getline(file, line)) {
        EXPECT_FALSE(line.empty());
        count++;
    }
    EXPECT_EQ(count, 5);

    file.close();
    std::remove(config.file_path.c_str());
}

TEST(LoggerTest, RotatePlaceholder) {
    LoggerConfig config;
    config.console_output = false;
    Logger logger(config);

    // max_file_size == 0 -> no-op, returns nullopt
    auto result = logger.rotate();
    EXPECT_FALSE(result.has_value());

    LoggerConfig config2;
    config2.max_file_size = 1024;
    Logger logger2(config2);

    // max_file_size > 0 -> returns NotImplemented
    auto result2 = logger2.rotate();
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2->code, ErrorCode::NotImplemented);
}

TEST(LoggerTest, ConsoleOutput) {
    LoggerConfig config;
    config.console_output = true;
    config.file_output = false;
    Logger logger(config);

    // Just verify no crash
    EXPECT_NO_THROW(logger.info("console output test"));
    EXPECT_NO_THROW(logger.warn("console warning"));
    EXPECT_NO_THROW(logger.error("console error"));
}

TEST(LoggerTest, ILoggerInterface) {
    LoggerConfig config;
    config.console_output = false;
    Logger logger(config);

    // Test via interface pointer
    ILogger* ilogger = &logger;
    EXPECT_NO_THROW(ilogger->info("via interface"));
    EXPECT_NO_THROW(ilogger->error("via interface"));
}

TEST(LoggerTest, LogLevelEnum) {
    EXPECT_LT(static_cast<int>(LogLevel::TRACE), static_cast<int>(LogLevel::DEBUG));
    EXPECT_LT(static_cast<int>(LogLevel::DEBUG), static_cast<int>(LogLevel::INFO));
    EXPECT_LT(static_cast<int>(LogLevel::INFO), static_cast<int>(LogLevel::WARN));
    EXPECT_LT(static_cast<int>(LogLevel::WARN), static_cast<int>(LogLevel::ERROR));
}
