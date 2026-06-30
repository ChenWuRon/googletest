#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>

class UserRepository {
 public:
  void AddUser(const std::string& name) { users_.push_back(name); }
  bool HasUser(const std::string& name) const {
    return std::find(users_.begin(), users_.end(), name) != users_.end();
  }
  void RemoveUser(const std::string& name) {
    users_.erase(std::remove(users_.begin(), users_.end(), name), users_.end());
  }
  int Count() const { return users_.size(); }
 private:
  std::vector<std::string> users_;
};

class UserRepositoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    repo_ = std::make_unique<UserRepository>();
    repo_->AddUser("Alice");
    repo_->AddUser("Bob");
    repo_->AddUser("Charlie");
  }
  void TearDown() override {
    // 资源清理，比如关闭文件、断开数据库连接
  }
  
  std::unique_ptr<UserRepository> repo_;
};
TEST_F(UserRepositoryTest, HasUserReturnsTrueForExistingUser) {
  EXPECT_TRUE(repo_->HasUser("Alice"));
  EXPECT_TRUE(repo_->HasUser("Bob"));
  EXPECT_TRUE(repo_->HasUser("Charlie"));
}
TEST_F(UserRepositoryTest, HasUserReturnsFalseForNonExistingUser) {
  EXPECT_FALSE(repo_->HasUser("David"));
}
TEST_F(UserRepositoryTest, RemoveUserDecreasesCount) {
  int initialCount = repo_->Count();
  repo_->RemoveUser("Alice");
  EXPECT_EQ(repo_->Count(), initialCount - 1);
  EXPECT_FALSE(repo_->HasUser("Alice"));
}
TEST_F(UserRepositoryTest, CountReturnsCorrectNumberOfUsers) {
  EXPECT_EQ(repo_->Count(), 3);
  repo_->RemoveUser("Bob");
  EXPECT_EQ(repo_->Count(), 2);
  repo_->RemoveUser("Charlie");
  EXPECT_EQ(repo_->Count(), 1);
}   
