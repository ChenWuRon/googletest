#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <vector>

// ======== 依赖接口 ========

struct User {
  int id;
  std::string name;
  std::string email;
};

class Database {
 public:
  virtual ~Database() = default;
  virtual bool InsertUser(const User& user) = 0;
  virtual std::optional<User> FindUserById(int id) = 0;
  virtual bool DeleteUser(int id) = 0;
};

class Cache {
 public:
  virtual ~Cache() = default;
  virtual void Set(const std::string& key, const User& value) = 0;
  virtual std::optional<User> Get(const std::string& key) = 0;
  virtual void Remove(const std::string& key) = 0;
};

class Logger {
 public:
  virtual ~Logger() = default;
  virtual void Info(const std::string& message) = 0;
  virtual void Error(const std::string& message) = 0;
};

// ======== 业务类 ========

class UserService {
 public:
  UserService(Database* db, Cache* cache, Logger* logger)
      : db_(db), cache_(cache), logger_(logger) {}

  std::optional<User> GetUser(int id) {
    logger_->Info("GetUser: " + std::to_string(id));

    // 1. 查缓存
    auto cached = cache_->Get("user_" + std::to_string(id));
    if (cached.has_value()) {
      logger_->Info("GetUser cache hit: " + std::to_string(id));
      return cached;
    }

    // 2. 缓存未命中，查数据库
    auto user = db_->FindUserById(id);
    if (user.has_value()) {
      cache_->Set("user_" + std::to_string(id), user.value());
      logger_->Info("GetUser database hit: " + std::to_string(id));
    } else {
      logger_->Error("GetUser not found: " + std::to_string(id));
    }
    return user;
  }

  int CreateUser(const std::string& name, const std::string& email) {
    logger_->Info("CreateUser: " + name);

    static int next_id = 1;
    User user{next_id++, name, email};
    db_->InsertUser(user);
    return user.id;
  }  
  bool DeleteUser(int id) {
  logger_->Info("DeleteUser: " + std::to_string(id));
  cache_->Remove("user_" + std::to_string(id));
  bool ok = db_->DeleteUser(id);
  if (ok) {
    logger_->Info("DeleteUser success: " + std::to_string(id));
  } else {
    logger_->Error("DeleteUser failed: " + std::to_string(id));
  }
  return ok;
}

 private:
  Database* db_;
  Cache* cache_;
  Logger* logger_;
};

// ======== Mock 类 ========

class MockDatabase : public Database {
 public:
  MOCK_METHOD(bool, InsertUser, (const User&), (override));
  MOCK_METHOD(std::optional<User>, FindUserById, (int), (override));
  MOCK_METHOD(bool, DeleteUser, (int), (override));
};

class MockCache : public Cache {
 public:
  MOCK_METHOD(void, Set, (const std::string&, const User&), (override));
  MOCK_METHOD(std::optional<User>, Get, (const std::string&), (override));
  MOCK_METHOD(void, Remove, (const std::string&), (override));
};

class MockLogger : public Logger {
 public:
  MOCK_METHOD(void, Info, (const std::string&), (override));
  MOCK_METHOD(void, Error, (const std::string&), (override));
};

// ======== Fixture ========

class UserServiceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    db_ = std::make_unique<MockDatabase>();
    cache_ = std::make_unique<MockCache>();
    logger_ = std::make_unique<MockLogger>();
    service_ = std::make_unique<UserService>(db_.get(), cache_.get(), logger_.get());
  }

  std::unique_ptr<MockDatabase> db_;
  std::unique_ptr<MockCache> cache_;
  std::unique_ptr<MockLogger> logger_;
  std::unique_ptr<UserService> service_;
};

// ======== 测试 ========

// 1. 缓存命中 — 不查数据库
TEST_F(UserServiceTest, GetUserReturnsUserFromCache) {
  User cached_user{1, "Alice", "alice@example.com"};
  EXPECT_CALL(*cache_, Get("user_1"))
      .WillOnce(testing::Return(std::optional<User>(cached_user)));
  EXPECT_CALL(*db_, FindUserById).Times(0);  // 不应该查数据库
  EXPECT_CALL(*logger_, Info("GetUser: 1"));
  EXPECT_CALL(*logger_, Info("GetUser cache hit: 1"));

  auto result = service_->GetUser(1);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->name, "Alice");
}

// 2. 缓存未命中，数据库命中 — 回填缓存
TEST_F(UserServiceTest, GetUserReturnsUserFromDatabase) {
  EXPECT_CALL(*cache_, Get("user_2"))
      .WillOnce(testing::Return(std::nullopt));
  User db_user{2, "Bob", "bob@example.com"};
  EXPECT_CALL(*db_, FindUserById(2))
      .WillOnce(testing::Return(std::optional<User>(db_user)));
  EXPECT_CALL(*cache_, Set("user_2", testing::_));
  EXPECT_CALL(*logger_, Info("GetUser: 2"));
  EXPECT_CALL(*logger_, Info("GetUser database hit: 2"));

  auto result = service_->GetUser(2);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->name, "Bob");
}

// 3. 用户不存在
TEST_F(UserServiceTest, GetUserReturnsNullWhenUserNotFound) {
  EXPECT_CALL(*cache_, Get("user_99"))
      .WillOnce(testing::Return(std::nullopt));
  EXPECT_CALL(*db_, FindUserById(99))
      .WillOnce(testing::Return(std::nullopt));
  EXPECT_CALL(*logger_, Info("GetUser: 99"));
  EXPECT_CALL(*logger_, Error("GetUser not found: 99"));

  auto result = service_->GetUser(99);

  EXPECT_FALSE(result.has_value());
}

// 4. CreateUser — 验证插入和日志
TEST_F(UserServiceTest, CreateUserInsertsIntoDatabase) {
  EXPECT_CALL(*logger_, Info("CreateUser: Charlie"));
  EXPECT_CALL(*db_, InsertUser(testing::_));

  int id = service_->CreateUser("Charlie", "charlie@example.com");

  EXPECT_GT(id, 0);
}
// ======== 参数化测试：DeleteUser 成功/失败 ========

struct DeleteUserParam {
  bool db_result;
  bool expected_result;
  std::string expected_log;
};

std::ostream& operator<<(std::ostream& os, const DeleteUserParam& p) {
  return os << "db_result: " << p.db_result;
}

class UserServiceDeleteTest
    : public UserServiceTest,
      public ::testing::WithParamInterface<DeleteUserParam> {};

TEST_P(UserServiceDeleteTest, ClearsCacheAndLogs) {
  auto param = GetParam();

  EXPECT_CALL(*cache_, Remove("user_1"));
  EXPECT_CALL(*db_, DeleteUser(1)).WillOnce(testing::Return(param.db_result));
  EXPECT_CALL(*logger_, Info("DeleteUser: 1"));
  if (param.db_result) {
    EXPECT_CALL(*logger_, Info("DeleteUser success: 1"));
  } else {
    EXPECT_CALL(*logger_, Error("DeleteUser failed: 1"));
  }

  bool result = service_->DeleteUser(1);

  EXPECT_EQ(result, param.expected_result);
}

INSTANTIATE_TEST_SUITE_P(
    DeleteUserScenarios,
    UserServiceDeleteTest,
    ::testing::Values(
        DeleteUserParam{true, true, "DeleteUser success: 1"},
        DeleteUserParam{false, false, "DeleteUser failed: 1"}
    )
);