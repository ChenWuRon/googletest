#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

// ---- 真实项目的依赖接口 ----

class Database {
 public:
  virtual ~Database() = default;
  virtual bool SaveOrder(const std::string& order_id, double amount) = 0;
};

class Logger {
 public:
  virtual ~Logger() = default;
  virtual void Log(const std::string& message) = 0;
};

// ---- 被测试的业务类 ----

class OrderService {
 public:
  explicit OrderService(Database* db, Logger* logger)
      : db_(db), logger_(logger) {}

  bool PlaceOrder(const std::string& order_id, double amount) {
    logger_->Log("Placing order: " + order_id);
    bool ok = db_->SaveOrder(order_id, amount);
    if (ok) {
      logger_->Log("Order placed: " + order_id);
    } else {
      logger_->Log("Failed to place order: " + order_id);
    }
    return ok;
  }

 private:
  Database* db_;
  Logger* logger_;
};

// ---- Mock 类 ----

class MockDatabase : public Database {
 public:
  MOCK_METHOD(bool, SaveOrder, (const std::string&, double), (override));
};

class MockLogger : public Logger {
 public:
  MOCK_METHOD(void, Log, (const std::string&), (override));
};

// ---- 测试 ----

TEST(OrderServiceTest, PlaceOrderSuccess) {
  // Arrange
  MockDatabase db;
  MockLogger logger;
  OrderService service(&db, &logger);

  EXPECT_CALL(logger, Log("Placing order: order_001"));
  EXPECT_CALL(db, SaveOrder("order_001", 99.9))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(logger, Log("Order placed: order_001"));

  // Act
  bool result = service.PlaceOrder("order_001", 99.9);

  // Assert
  EXPECT_TRUE(result);
}

TEST(OrderServiceTest, PlaceOrderFailure) {
  // Arrange
  MockDatabase db;
  MockLogger logger;
  OrderService service(&db, &logger);

  EXPECT_CALL(logger, Log("Placing order: order_002"));
  EXPECT_CALL(db, SaveOrder("order_002", 50.0))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(logger, Log("Failed to place order: order_002"));

  // Act
  bool result = service.PlaceOrder("order_002", 50.0);

  // Assert
  EXPECT_FALSE(result);
}

TEST(OrderServiceTest, PlaceOrderLogsErrorMessageOnDatabaseFailure) {
  // Arrange
  MockDatabase db;
  MockLogger logger;
  OrderService service(&db, &logger);

  EXPECT_CALL(logger, Log(testing::_)).Times(2);
  EXPECT_CALL(db, SaveOrder(testing::_, testing::_))
      .WillOnce(testing::Return(false));

  // Act
  bool result = service.PlaceOrder("order_003", 150.0);

  // Assert
  EXPECT_FALSE(result);
}
