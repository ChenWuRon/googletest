#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>

// 被测试的函数
int Divide(int a, int b) {
  if (b == 0) {
    throw std::invalid_argument("division by zero");
  }
  return a / b;
}

struct User {
  std::string name;
  int age;
};

// ----------  EXPECT_*  失败后继续执行 ----------

TEST(AssertionTest, ExpectEq) {
  EXPECT_EQ(Divide(10, 2), 5);
  EXPECT_EQ(Divide(9, 3), 3);
}

TEST(AssertionTest, ExpectNe) {
  EXPECT_NE(Divide(10, 2), 4);   // !=
  EXPECT_NE(Divide(10, 3), 4);   // 10/3 = 3, 3 ≠ 4
}

TEST(AssertionTest, ExpectTrueFalse) {
  EXPECT_TRUE(Divide(10, 2) == 5);
  EXPECT_FALSE(Divide(10, 2) == 4);
}

TEST(AssertionTest, ExpectLtGt) {
  EXPECT_LT(Divide(10, 3), 4);    // <
  EXPECT_GT(Divide(10, 2), 4);    // >
}

TEST(AssertionTest, ExpectThrow) {
  EXPECT_THROW(Divide(1, 0), std::invalid_argument);
  EXPECT_NO_THROW(Divide(1, 1));
}

TEST(AssertionTest, ExpectStrEq) {
  std::string actual = "hello";
  EXPECT_STREQ(actual.c_str(), "hello");       // C 风格字符串
  EXPECT_STRNE(actual.c_str(), "world");
}

TEST(AssertionTest, ExpectFloatEq) {
  float a = 0.1f + 0.2f;
  EXPECT_FLOAT_EQ(a, 0.3f);       // 4 ULP 误差
  double b = 0.1 + 0.2;
  EXPECT_DOUBLE_EQ(b, 0.3);
}

// ----------  ASSERT_*  失败后立即返回 ----------

TEST(AssertionTest, AssertEq) {
  ASSERT_EQ(Divide(10, 2), 5);    // 如果失败，下面代码不执行
  int result = Divide(10, 2);
  EXPECT_GT(result, 0);
}

TEST(AssertionTest, AssertTrue) {
  ASSERT_TRUE(Divide(10, 2) == 5);
}

TEST(AssertionTest, AssertThrow) {
  ASSERT_THROW(Divide(1, 0), std::invalid_argument);
}
class BankAccount {
 public:
  explicit BankAccount(double balance) : balance_(balance) {}
  void Deposit(double amount) {
    if (amount <= 0) throw std::invalid_argument("amount must be positive");
    balance_ += amount;
  }
  void Withdraw(double amount) {
    if (amount <= 0) throw std::invalid_argument("amount must be positive");
    if (amount > balance_) throw std::runtime_error("insufficient funds");
    balance_ -= amount;
  }
  double GetBalance() const { return balance_; }
 private:
  double balance_;
};  

TEST(BankAccountTest, DepositIncreasesBalance) {
  BankAccount account(100.0);
  account.Deposit(50.0);
  EXPECT_DOUBLE_EQ(account.GetBalance(), 150.0);
}
TEST(BankAccountTest, DepositNegativeAmount) {
  BankAccount account(100.0);
  EXPECT_THROW(account.Deposit(-50.0), std::invalid_argument);
}

TEST(BankAccountTest, WithdrawSubtractsBalance) {
  BankAccount account(100.0);
  account.Withdraw(30.0);
  EXPECT_DOUBLE_EQ(account.GetBalance(), 70.0);
}
TEST(BankAccountTest, WithdrawInsufficientFunds) {
  BankAccount account(100.0);
  EXPECT_THROW(account.Withdraw(150.0), std::runtime_error);
} 
TEST(BankAccountTest, GetBalanceInitialBalance) {
  BankAccount account(100.0);
  EXPECT_DOUBLE_EQ(account.GetBalance(), 100.0);
}