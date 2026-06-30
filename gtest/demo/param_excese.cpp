#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
int ParseLevel(const std::string& level) {
  if (level == "debug") return 0;
  if (level == "info") return 1;
  if (level == "warn") return 2;
  if (level == "error") return 3;
  throw std::invalid_argument("unknown level: " + level);
}
struct ParseLevelTestCase {
  std::string input;
  int expected;
};
std::ostream& operator<<(std::ostream& os, const ParseLevelTestCase& tc) {
  return os << "input: " << tc.input << ", expected: " << tc.expected;
}

class ParseLevelTest : public ::testing::TestWithParam<ParseLevelTestCase> {};

TEST_P(ParseLevelTest, ReturnsCorrectLevel) {
  auto param = GetParam();
  EXPECT_EQ(ParseLevel(param.input), param.expected);
}

TEST(ParseLevelTest, ThrowsForUnknownLevel) {
  EXPECT_THROW(ParseLevel("unknown"), std::invalid_argument);
}

TEST(ParseLevelTest, InvalidLevel) {
  EXPECT_THROW(ParseLevel("fatal"), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(
    ParseLevelValues,
    ParseLevelTest,
    ::testing::Values(
        ParseLevelTestCase{"debug", 0},
        ParseLevelTestCase{"info", 1},
        ParseLevelTestCase{"warn", 2},
        ParseLevelTestCase{"error", 3}
    )
);