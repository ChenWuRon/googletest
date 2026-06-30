#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

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

// 1. 继承 ::testing::Test
class BankAccountTest : public ::testing::Test {
 protected:
  // 2. SetUp() — 每个 TEST_F 执行前运行
  void SetUp() override {
    account_ = std::make_unique<BankAccount>(100.0);
  }

  // 3. TearDown() — 每个 TEST_F 执行后运行 (这里用默认即可)
  void TearDown() override {
    // 资源清理，比如关闭文件、断开数据库连接
  }

  std::unique_ptr<BankAccount> account_;
};

// 4. 用 TEST_F 代替 TEST
TEST_F(BankAccountTest, DepositIncreasesBalance) {
  account_->Deposit(50.0);
  EXPECT_DOUBLE_EQ(account_->GetBalance(), 150.0);
}

TEST_F(BankAccountTest, WithdrawSubtractsBalance) {
  account_->Withdraw(30.0);
  EXPECT_DOUBLE_EQ(account_->GetBalance(), 70.0);
}

TEST_F(BankAccountTest, InitialBalance) {
  EXPECT_DOUBLE_EQ(account_->GetBalance(), 100.0);
}
