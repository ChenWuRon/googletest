#include <gtest/gtest.h>
#include "resource_manager/error/error.h"

using namespace resource_manager;

TEST(ErrorTest, ErrorCodeExists) {
    Error err(ErrorCode::ConfigParseError, "parse failed", "config");
    EXPECT_EQ(err.code, ErrorCode::ConfigParseError);
    EXPECT_EQ(err.message, "parse failed");
    EXPECT_EQ(err.source, "config");
}
