#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <stdexcept>
int CalcDiscount(double price) {
  if (price <= 0) return 0;
  if (price < 100) return static_cast<int>(price * 0.1);
  if (price < 500) return static_cast<int>(price * 0.15);
  return static_cast<int>(price * 0.2);
}

// 1. 定义参数类型
struct DiscountTestCase {
  double input;
  int expected;
};

// 2. 参数化的打印支持（可选，让失败信息更好看）
std::ostream& operator<<(std::ostream& os, const DiscountTestCase& tc) {
  return os << "input: " << tc.input << ", expected: " << tc.expected;
}

// 3. 继承 ::testing::TestWithParam<T>
class DiscountTest : public ::testing::TestWithParam<DiscountTestCase> {};

// 4. 用 TEST_P 代替 TEST
TEST_P(DiscountTest, CalculatesCorrectDiscount) {
  auto param = GetParam();
  EXPECT_EQ(CalcDiscount(param.input), param.expected);
}

// 5. 枚举测试数据
INSTANTIATE_TEST_SUITE_P(
    DiscountValues,
    DiscountTest,
    ::testing::Values(
        DiscountTestCase{0, 0},
        DiscountTestCase{50, 5},
        DiscountTestCase{100, 15},
        DiscountTestCase{499, 74},
        DiscountTestCase{500, 100},
        DiscountTestCase{1000, 200}
    )
);
