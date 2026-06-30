#include <gtest/gtest.h>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

int Add(int a, int b) {
  return a + b;
}

auto Trim(const std::string& str)->std::string {
  size_t first = str.find_first_not_of(' ');
  if (first == std::string::npos) {
    return "";
  }
  size_t last = str.find_last_not_of(' ');
  return str.substr(first, (last - first + 1));
}
TEST(StringUtilTest, Trim) {
  EXPECT_EQ(Trim("  hello  "), "hello");
  EXPECT_EQ(Trim(""), "");
}
TEST(StringUtilTest, TrimEmptyString) {
  EXPECT_EQ(Trim(""), "");
}
auto Split(const std::string& str, char delimiter) -> std::vector<std::string> {
  std::vector<std::string> tokens;
  std::string token;
  std::istringstream tokenStream(str);
  while (std::getline(tokenStream, token, delimiter)) {
    tokens.push_back(token);
  }
  return tokens;
}
TEST(StringUtilTest, Split) {
  std::vector<std::string> expected = {"hello", "world"};
  EXPECT_EQ(Split("hello,world", ','), expected);
} 


auto ToUpper(const std::string& str) -> std::string {
  std::string result = str;
  std::transform(result.begin(), result.end(), result.begin(), ::toupper);
  return result;
}
TEST(StringUtilTest, ToUpper) {
  EXPECT_EQ(ToUpper("hello"), "HELLO");
  EXPECT_EQ(ToUpper("Hello World"), "HELLO WORLD");
}
TEST(AddTest, PositiveNumbers) {
  EXPECT_EQ(Add(1, 2), 3);
  EXPECT_EQ(Add(10, 20), 30);
}

TEST(AddTest, NegativeNumbers) {
  EXPECT_EQ(Add(-1, -2), -3);
  EXPECT_EQ(Add(-5, 5), 0);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
