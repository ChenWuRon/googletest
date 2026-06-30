# Google Test 完整学习笔记

## 第一课：基础

### 核心概念

| 概念 | 说明 |
|---|---|
| `TEST(SuiteName, TestName)` | 定义一条测试用例。SuiteName 分组，TestName 标识。同一 SuiteName 的测试归为一组 |
| `EXPECT_EQ(a, b)` | 断言 a == b。**失败后继续执行** |
| `ASSERT_EQ(a, b)` | 断言 a == b。**失败后立即返回** |
| `::testing::InitGoogleTest(&argc, argv)` | 初始化 gtest，解析命令行参数（如 `--gtest_filter`） |
| `RUN_ALL_TESTS()` | 运行所有测试，返回 0 表示全部通过 |

### EXPECT vs ASSERT 选择原则

```cpp
// 必须用 ASSERT：后续代码依赖这个条件
TEST(UserServiceTest, GetUser) {
  User* u = db.FindUser(42);
  ASSERT_NE(u, nullptr);       // 不满足就没必要继续
  EXPECT_EQ(u->name, "Alice"); // 这里失败了可以继续检查其他字段
}
```

### 真实项目用法

```cpp
TEST(PaymentServiceTest, DeductBalance) {
  PaymentService svc;
  EXPECT_TRUE(svc.Deduct("user_123", 100.0));
  EXPECT_EQ(svc.GetBalance("user_123"), 900.0);
}
```

### 常见错误

1. **依赖间接 include** — `<vector>`、`<algorithm>` 等必须显式 include，不要依赖 gtest 间接包含
2. **忘记 main 函数** — 用 `gtest_main` 库就不用自己写 main；如果用 `gtest` 库必须自己写

### 面试问题与答案

**Q: TEST() 的两个参数分别是什么？**
A: 第一个是测试套件名（Test Suite），用于分组。第二个是测试名（Test Name）。输出格式为 `SuiteName.TestName`。

**Q: EXPECT_EQ 和 ASSERT_EQ 什么区别？什么时候用哪个？**
A: `EXPECT_EQ` 失败后当前测试继续执行，可以收集多个失败。`ASSERT_EQ` 失败后当前测试立即终止。用 `ASSERT` 的场景：后续代码依赖这个条件（如指针判空后解引用）。

**Q: RUN_ALL_TESTS() 返回值代表什么？**
A: 返回 0 表示所有测试通过，非 0 表示有测试失败。

---

## 第二课：Assertions

### 完整断言表

| 断言 | 用途 | 失败行为 |
|---|---|---|
| `EXPECT_EQ(a, b)` | a == b | 继续 |
| `EXPECT_NE(a, b)` | a != b | 继续 |
| `EXPECT_TRUE(expr)` | expr 为 true | 继续 |
| `EXPECT_FALSE(expr)` | expr 为 false | 继续 |
| `EXPECT_LT(a, b)` | a < b | 继续 |
| `EXPECT_GT(a, b)` | a > b | 继续 |
| `EXPECT_LE(a, b)` | a <= b | 继续 |
| `EXPECT_GE(a, b)` | a >= b | 继续 |
| `EXPECT_THROW(stmt, exc)` | 抛指定异常 | 继续 |
| `EXPECT_NO_THROW(stmt)` | 不抛异常 | 继续 |
| `EXPECT_ANY_THROW(stmt)` | 抛任何异常 | 继续 |
| `EXPECT_STREQ(a, b)` | C 字符串 (char*) 相等 | 继续 |
| `EXPECT_STRNE(a, b)` | C 字符串不等 | 继续 |
| `EXPECT_FLOAT_EQ(a, b)` | float 近似相等 (~4 ULP) | 继续 |
| `EXPECT_DOUBLE_EQ(a, b)` | double 近似相等 | 继续 |
| `ASSERT_EQ(a, b)` | a == b | **终止** |
| `ASSERT_NE(a, b)` | a != b | **终止** |
| `ASSERT_TRUE(expr)` | expr 为 true | **终止** |
| `ASSERT_FALSE(expr)` | expr 为 false | **终止** |
| `ASSERT_THROW(stmt, exc)` | 抛指定异常 | **终止** |
| `ASSERT_NO_THROW(stmt)` | 不抛异常 | **终止** |

### 选择原则

- 后续代码依赖 ⇒ `ASSERT_*`
- 后续代码不依赖 ⇒ `EXPECT_*`
- 浮点数用 `EXPECT_FLOAT_EQ` / `EXPECT_DOUBLE_EQ`，**不要用 `EXPECT_EQ`**
- C 字符串（`const char*`）用 `EXPECT_STREQ`，不用 `EXPECT_EQ`（`EXPECT_EQ` 比较的是指针地址）

### 真实项目案例

```cpp
// 解析 JSON 配置
TEST(ConfigParserTest, ParsePort) {
  auto cfg = ConfigParser::ParseFile("test.json");
  ASSERT_TRUE(cfg.has_value());              // 解析失败就不用测了
  EXPECT_GT(cfg->port, 0);
  EXPECT_LT(cfg->port, 65536);
}

// 测试网络请求
TEST(HttpClientTest, PostRequest) {
  HttpClient client;
  EXPECT_NO_THROW(client.Post("/api/order"));
  EXPECT_EQ(client.Status(), 200);
}

// 测试浮点数计算
TEST(PriceCalcTest, Discount) {
  double price = CalcDiscount(100.0, 0.15);
  EXPECT_DOUBLE_EQ(price, 85.0);
}
```

### 常见错误

1. `EXPECT_EQ` 比较 `const char*` 会**比较指针地址**，要用 `EXPECT_STREQ`
2. 整型除法 `10/3` 结果是 3，不是 3.33。需要浮点时用 `EXPECT_FLOAT_EQ`
3. `EXPECT_TRUE(result == true)` → 直接 `EXPECT_TRUE(result)` 即可

### 面试问题与答案

**Q: 为什么 `EXPECT_STREQ` 存在？`EXPECT_EQ(s1, s2)` 对 const char\* 会怎样？**
A: `EXPECT_EQ` 对指针比较的是地址值。两个内容相同但地址不同的字符串会被判为不等。`EXPECT_STREQ` 用 `strcmp` 比较内容。

**Q: 浮点数为什么不能用 EXPECT_EQ？**
A: 浮点数有精度误差（0.1 + 0.2 ≠ 0.3 在二进制浮点数中是真命题）。`EXPECT_FLOAT_EQ` 使用 4 ULP（Units in the Last Place）误差容忍。

---

## 第三课：Test Fixture

### 概念

| 概念 | 说明 |
|---|---|
| `class FooTest : public ::testing::Test` | Fixture 类，必须继承 `::testing::Test` |
| `void SetUp() override` | 每个 `TEST_F` 执行前运行 |
| `void TearDown() override` | 每个 `TEST_F` 执行后运行 |
| `TEST_F(FooTest, Name)` | 使用 Fixture 的测试，第一个参数必须等于 Fixture 类名 |
| `protected:` | Fixture 成员通常放 protected，让 TEST_F 可以访问 |

### 执行顺序

```
BankAccountTest 套件开始
  ├─ SetUp()    ← 创建 account(100.0)
  ├─ TEST_F: DepositIncreasesBalance
  ├─ TearDown() ← 资源清理
  ├─ SetUp()    ← 再创建 account(100.0)
  ├─ TEST_F: WithdrawSubtractsBalance
  ├─ TearDown()
  ├─ SetUp()    ← 再创建 account(100.0)
  ├─ TEST_F: InitialBalance
  └─ TearDown()
```

每个 `TEST_F` 都是**独立**的 — 一个测试修改了对象不会影响另一个。

### 什么时候必须用 Fixture？

- 多个测试需要相同的初始化（如创建相同配置的对象）
- 初始化成本高（如打开数据库连接、加载配置文件）
- 需要资源清理（如删除临时文件、断开数据库连接）

### 什么时候不要用 Fixture？

- 测试之间没有公共代码 — 用普通 `TEST()` 更清晰
- 初始化逻辑完全不同 — 强行塞进一个 Fixture 会让代码更难懂
- Fixture 过于庞大 — 如果一个 Fixture 里有 20 个测试但用了 5 种不同初始化路径，考虑拆成多个 Fixture

### 真实项目案例

```cpp
// Database Fixture — 每个测试有独立的事务
class OrderRepoTest : public ::testing::Test {
 protected:
  void SetUp() override {
    db_ = Database::Connect("test_config.json");
    db_->BeginTransaction();
  }
  void TearDown() override {
    db_->Rollback();  // 回滚，不影响下次运行
    db_->Disconnect();
  }
  std::unique_ptr<Database> db_;
};

TEST_F(OrderRepoTest, CreateOrder) { ... }
TEST_F(OrderRepoTest, CancelOrder) { ... }
```

### 常见错误

1. **Fixture 类名和 TEST_F 第一个参数不匹配** — 编译报错
2. **忘记 `override`** — C++17 建议加上，编译器会帮你检查参数类型
3. **把 SetUp/TearDown 写成 Setup/Teardown**（大小写错误）— 不会调用，也不会报错
4. **在 TEST_F 里共享状态** — 认为一个测试改了成员变量，另一个测试能看到。错：每个 TEST_F 独立 SetUp

### 面试问题与答案

**Q: TEST_F 和 TEST 的区别？**
A: `TEST` 不依赖 Fixture。`TEST_F` 需要一个继承 `::testing::Test` 的 Fixture 类，第一个参数必须是类名。`TEST_F` 内部可以访问 Fixture 的 protected 成员。

**Q: SetUp / TearDown 什么时候调用？每个 TEST_F 调用几次？**
A: 每个 TEST_F 执行前调一次 SetUp，执行后调一次 TearDown。如果有 5 个 TEST_F，SetUp 和 TearDown 各被调 5 次。

**Q: 为什么 Fixture 的成员通常放在 protected？**
A: TEST_F 宏展开后生成的匿名类继承自 Fixture 类，只能访问 public 和 protected 成员。

---

## 第四课：Parameterized Test

### 为什么需要？

```cpp
// 不好的写法：4 个值合在 1 个 TEST 里，挂了不知道哪个值
TEST(PriceCalcTest, Discount) {
  EXPECT_EQ(CalcDiscount(0), 0);
  EXPECT_EQ(CalcDiscount(50), 5);
  EXPECT_EQ(CalcDiscount(100), 15);
  EXPECT_EQ(CalcDiscount(500), 100);
}

// 参数化写法：每条数据变独立测试
// CalcDiscount(50) 挂了能看到 DiscountValues/CalculatesCorrectDiscount/1
```

### 核心语法

| 概念 | 说明 |
|---|---|
| `TestWithParam<T>` | 参数化测试的基类，T 是参数类型 |
| `TEST_P(Fixture, Name)` | 定义参数化测试，用 `GetParam()` 获取当前参数 |
| `INSTANTIATE_TEST_SUITE_P(Prefix, Fixture, Generators...)` | 绑定参数到测试 |
| `GetParam()` | 获取当前这组参数 |

### 参数生成器

| 生成器 | 用途 |
|---|---|
| `Values(a, b, c...)` | 枚举具体值 |
| `Range(begin, end)` | 生成范围 [begin, end) |
| `ValuesIn(vec)` | 从容器/数组取值 |
| `Combine(g1, g2)` | 笛卡尔积 |

### 完整示例

```cpp
struct DiscountTestCase { double input; int expected; };
std::ostream& operator<<(std::ostream& os, const DiscountTestCase& tc) {
  return os << "input: " << tc.input << ", expected: " << tc.expected;
}

class DiscountTest : public ::testing::TestWithParam<DiscountTestCase> {};

TEST_P(DiscountTest, CalculatesCorrectDiscount) {
  auto param = GetParam();
  EXPECT_EQ(CalcDiscount(param.input), param.expected);
}

INSTANTIATE_TEST_SUITE_P(
    DiscountValues, DiscountTest,
    ::testing::Values(
        DiscountTestCase{0, 0},
        DiscountTestCase{50, 5},
        DiscountTestCase{100, 15},
        DiscountTestCase{1000, 200}
    )
);
```

### 什么时候用？

- 同一个函数，多种输入/输出的组合
- 边界值测试：0、负数、最大值、空字符串
- 不同配置：不同协议、不同超时时间
- 回归测试：把线上报过的 bug 的入参写成参数化

### 什么时候不用？

- 每个参数需要不同的断言逻辑 — 用普通 TEST
- 参数只有 1~2 组 — 没必要引入参数化

### 常见错误

1. **TEST_P 写了但没 INSTANTIATE_TEST_SUITE_P** — 测试不会运行，gtest 会报 warning
2. **struct 参数没有 operator<<** — 断言失败时打印不了参数值（仍能运行，只是信息不友好）

### 面试问题与答案

**Q: TEST_P 和 TEST 区别？**
A: `TEST_P` 定义参数化测试，需要结合 `INSTANTIATE_TEST_SUITE_P` 提供参数数据，每条测试数据变成一条独立测试。`TEST` 是普通测试，只能写固定值。

**Q: 参数化测试解决了什么痛点？**
A: 避免在一条 TEST 里写多个 EXPECT（挂了不知道哪个出问题）。把重复的测试逻辑提取出来，数据集中管理，新增测试数据只需加一行。

**Q: INSTANTIATE_TEST_SUITE_P 的三个参数各代表什么？**
A: 前缀名（输出标识）、TEST_P 的 Fixture 类、参数生成器。

**Q: 为什么参数化测试比在 TEST 里 for 循环好？**
A: for 循环在第一次 EXPECT 失败时就停（或跳过后续），看不出哪些后续数据也会失败。参数化的每条数据是独立测试，全部失败都能看到。

---

## 第五课：Mock

### 为什么需要 Mock？

真实项目很少有孤立的类。常见依赖链：
```
OrderService
  ├─ Database::SaveOrder()    → 真的写库会留下测试数据
  ├─ Logger::Log()            → 刷屏，污染日志文件
  └─ PaymentGateway::Charge() → 真的扣钱！！！
```

Mock 让你用替身替换真实依赖，定点验证行为。

### Fake vs Stub vs Mock

| 技术 | 行为 | 验证 |
|---|---|---|
| **Fake** | 轻量实现（如内存数据库、内存文件系统） | 业务结果 |
| **Stub** | 返回固定值（`return true`） | 业务结果 |
| **Mock** | 返回固定值 + 记录调用信息 | **业务结果 + 交互行为** |

```cpp
// Fake 例子
class FakeDatabase : public Database {
  std::vector<Order> orders_;
  void SaveOrder(Order o) override { orders_.push_back(o); }
  Order GetOrder(int id) override { return orders_[id]; }
};

// Mock 例子
class MockDatabase : public Database {
  MOCK_METHOD(void, SaveOrder, (Order), (override));
  MOCK_METHOD(Order, GetOrder, (int), (override));
};
```

### 什么时候必须用 Mock？

1. **有副作用**：支付、发邮件、写文件、改数据库
2. **依赖不可控**：第三方 API、分布式服务、硬件
3. **难造场景**：网络超时、权限不足、余额不足
4. **太慢**：数据库查询、HTTP 请求（ms 级 → μs 级）

### 什么时候不要 Mock？

- **值对象**（struct、DTO）— 直接传真实对象
- **STL 容器** — 不要 Mock vector、map 等
- **简单依赖** — 可以用 Fake 或真实对象
- **Mock 过多**（一个测试 Mock 了 5+ 个依赖）— 说明设计有问题，考虑拆分

### 核心语法

```cpp
// 声明 Mock 方法
MOCK_METHOD(return_type, method_name, (args...), (override));

// 设置期望
EXPECT_CALL(mock_object, method(matchers...))
    .WillOnce(action)             // 第 1 次调用的行为
    .WillOnce(action)             // 第 2 次
    .WillRepeatedly(action);      // 剩余调用

// 验证次数
.Times(n)       // 精确 n 次
.Times(0)       // 不应该被调

// 常用匹配器
testing::_              // 匹配任意值
testing::Eq(x)          // 等于 x（默认行为）
testing::Ne(x)          // 不等于
testing::Lt(x) / Gt(x)  // 小于 / 大于
testing::StartsWith(s)  // 字符串以 s 开头

// 常用动作
testing::Return(v)      // 返回 v
testing::Throw(e)       // 抛异常
```

### WillOnce / Return 详解

```cpp
EXPECT_CALL(db, SaveOrder("order_001", 99.9))
    .WillOnce(testing::Return(false));
```

当被测函数调用 `db->SaveOrder("order_001", 99.9)` 时，gmock 依次执行：
1. 检查参数 → "order_001" 和 99.9 ✅
2. 检查次数 → 这是第 1 次 ✅
3. 执行动作 → 执行 `Return(false)`（等价于 mock 函数体内 `return false`）
4. 返回 `false` 给调用方

多组动作：
```cpp
EXPECT_CALL(db, SaveOrder(_, _))
    .WillOnce(Return(false))   // 第 1 次返回 false
    .WillOnce(Return(true))    // 第 2 次返回 true
    .WillRepeatedly(Return(true));  // 第 3 次起返回 true
```

### 完整示例

```cpp
// ---- 依赖接口 ----
class Database {
 public:
  virtual ~Database() = default;
  virtual bool SaveOrder(const std::string& id, double amount) = 0;
};

// ---- Mock ----
class MockDatabase : public Database {
 public:
  MOCK_METHOD(bool, SaveOrder, (const std::string&, double), (override));
};

// ---- 测试 ----
TEST(OrderServiceTest, PlaceOrderSuccess) {
  MockDatabase db;
  MockLogger logger;
  OrderService service(&db, &logger);

  EXPECT_CALL(logger, Log("Placing order: order_001"));
  EXPECT_CALL(db, SaveOrder("order_001", 99.9))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(logger, Log("Order placed: order_001"));

  bool result = service.PlaceOrder("order_001", 99.9);

  EXPECT_TRUE(result);
}
```

### 常见错误

1. **忘记 `testing::` 前缀** — `Return(true)` 应该是 `testing::Return(true)`
2. **虚函数忘加 `virtual`** — `MOCK_METHOD` 要求基类函数是 virtual
3. **MOCK_METHOD 参数含逗号** — 用括号包裹 `(const std::string&, double)` 解决宏的逗号冲突
4. **EXPECT_CALL 设置但函数没被调用** — 测试结束时失败，显示 "uninteresting mock function call"

### 面试问题与答案

**Q: EXPECT_CALL 和 ON_CALL 区别？**
A: `EXPECT_CALL` 设置期望并验证调用（次数、参数）。`ON_CALL` 只设置默认返回值，不验证是否被调用。工作中 90% 用 `EXPECT_CALL`。

**Q: Mock 和 Stub 的区别？**
A: Stub 只返回固定值，不记录调用信息。Mock 同时记录和验证调用（次数、参数、顺序）。

**Q: 如果 MOCK_METHOD 的参数含逗号怎么办？**
A: 参数列表用括号包起来，如 `MOCK_METHOD(void, Foo, (std::pair<int, int>), (override))`。

---

## 第六课：真实项目 — UserService

### 项目架构

```
UserService
  ├─ GetUser(id)         → optional<User>
  ├─ CreateUser(name, email) → int
  └─ DeleteUser(id)      → bool
      依赖:
       ├─ Database  → 持久化（Mock）
       ├─ Cache     → 缓存（Mock）
       └─ Logger    → 日志（Mock）
```

### 为什么用依赖注入？

```cpp
// UserService 不自己创建依赖，而是从外面传进来
UserService(Database* db, Cache* cache, Logger* logger)
```

好处：不改 `UserService` 一行代码，测试时替换成 Mock。

### 测试策略

| 场景 | Cache | DB | 验证重点 |
|---|---|---|---|
| 缓存命中 | 有值 | 不调 (`Times(0)`) | 返回缓存数据，不走 DB |
| 缓存未命中，DB 命中 | null | 有值 | 回填缓存 (cache.Set 被调) |
| 都不存在 | null | null | Error 日志 |
| 创建用户 | — | InsertUser 被调 | id > 0 |
| 删除用户（参数化） | Remove 被调 | true/false | 无论结果都清缓存 |

### Fixture + Parameterized 组合

```cpp
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

// 同时继承 Fixture + WithParamInterface
class UserServiceDeleteTest
    : public UserServiceTest,
      public ::testing::WithParamInterface<DeleteUserParam> {};

TEST_P(UserServiceDeleteTest, ClearsCacheAndLogs) {
  auto param = GetParam();
  // 验证：无论 DB 成功还是失败，cache.Remove 都会被调
  EXPECT_CALL(*cache_, Remove("user_1"));
  EXPECT_CALL(*db_, DeleteUser(1)).WillOnce(testing::Return(param.db_result));
  ...
}
```

### 面试问题与答案

**Q: 为什么 GetUser 测试里要写 `.Times(0)`？**
A: 验证"缓存命中时不查数据库"这一业务规则。如果没有 `.Times(0)`，即使代码偷偷改了顺序（先查 DB 再查缓存），测试也不会红。

**Q: CreateUser 测试里为什么用 `testing::_` 而不是具体 User？**
A: CreateUser 内部自动生成 id（`static int next_id++`），我们无法精确预知 User 对象的值。用 `testing::_` 表示"只要 InsertUser 被调了就行"。

**Q: 参数化 DeleteUser 验证了什么关键业务逻辑？**
A: 验证"删除用户时，不管数据库操作成功还是失败，都要先清缓存"。这是缓存一致性的关键保障。

**Q: 如果 GetUser 没有先查缓存直接查 DB，哪个测试会挂？**
A: `GetUserReturnsUserFromCache`。该测试中 cache.Get 有返回值，正常情况下不应查 DB。如果代码直接查 DB，db.FindUserById 会被调，但它的 EXPECT_CALL 设了 `.Times(0)`，测试就会失败。

---

## 第七课：工程实践

### 项目目录组织

```
project/
  src/
    user_service.cpp
    payment_service.cpp
  tests/
    user_service_test.cpp      // 和源文件一一对应
    payment_service_test.cpp
    main.cpp                   // 共用的 main（或用 gtest_main 库跳过）
  CMakeLists.txt
```

**规则**：
- 测试文件和源文件一一对应
- 测试放在单独的 `tests/` 目录
- 测试文件用 `_test` 后缀

### AAA Pattern

每个测试按三部分组织，空行分隔：

```cpp
TEST_F(UserServiceTest, GetUserReturnsUserFromCache) {
  // Arrange — 准备数据、设置 Mock 期望
  EXPECT_CALL(*cache_, Get("user_1"))
      .WillOnce(testing::Return(std::optional<User>(cached_user)));
  EXPECT_CALL(*db_, FindUserById).Times(0);

  // Act — 执行被测函数（通常一行）
  auto result = service_->GetUser(1);

  // Assert — 验证结果
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->name, "Alice");
}
```

| 阶段 | 做什么 | 常见错误 |
|---|---|---|
| Arrange | 创建对象、设置 Mock、准备输入 | 和 Act 混在一起 |
| Act | 执行被测函数 | 一个测试里调了多个方法 |
| Assert | 验证结果 | 忘了验证边界条件 |

### 一个测试测一个行为

```cpp
// ❌ 不要：一个测试测了多个独立行为
TEST(UserTest, All) {
  EXPECT_EQ(user.GetName(), "Alice");
  EXPECT_EQ(user.GetAge(), 25);
}

// ✅ 应该：拆成两个测试
TEST(UserTest, GetNameReturnsCorrectName) { ... }
TEST(UserTest, GetAgeReturnsCorrectAge) { ... }
```

例外：同一行为的多个角度可放一起（如"创建用户"同时验证 id 和日志）。

### 什么时候拆测试？

- 分支不同（if/else 各一条）
- 边界值不同（空、最小值、最大值各一条）
- 异常路径（每种异常各一条）
- 意图不同（"创建后能查到"和"创建后 id 自增"是两种意图）

### 不要测试 private 函数

```cpp
class OrderService {
 public:
  bool PlaceOrder(Order o) { ... }         // ✅ 测这个
 private:
  double CalculateTax(double amount) {      // ❌ 不要直接测
    return amount * 0.15;
  }
};
```

原因：private 是实现细节，重构时可以改。如果 private 函数太复杂，提取成独立类：
```cpp
class TaxCalculator {
 public:
  double Calculate(double amount);  // ✅ 现在可以测了
};
```

### 测试命名规范

```
{ClassName}_{MethodName}_{Scenario}
```

```cpp
TEST(UserServiceTest, GetUser_WhenCacheHit_ReturnsUser)
TEST(OrderServiceTest, PlaceOrder_WhenDbFails_ReturnsFalse)
```

或精简版（驼峰）：
```cpp
TEST(UserServiceTest, GetUserReturnsUserFromCache)
TEST(OrderServiceTest, PlaceOrderReturnsFalseWhenDbFails)
```

团队统一风格即可。

### Code Review 关注什么

| 关注点 | 好 | 坏 |
|---|---|---|
| 可读性 | AAA 结构清晰 | Arrange 和 Act 混在一起 |
| 独立性 | 每个测试独立 SetUp | 测试之间共享状态 |
| 命名 | `DepositWithNegativeAmount_Throws` | `test1` |
| 覆盖 | 成功 + 失败 + 边界 | 只有 happy path |
| Mock 粒度 | 只 Mock 外部依赖 | Mock 了 struct/value object |
| 断言 | 用 `STREQ` / `FLOAT_EQ` | 全部用 `EXPECT_TRUE` |

### 面试问题与答案

**Q: AAA 是什么？为什么重要？**
A: Arrange-Act-Assert。把测试分为准备、执行、验证。让测试可读，一眼看出测什么、输入是什么、期望什么。

**Q: 为什么不应该测 private 函数？**
A: private 是实现细节，重构时可以改。测了 private 会导致重构时被迫改测试。如果太复杂，提取成独立类。

**Q: 一个测试里可以放多少个 EXPECT？**
A: 无数量限制，但应只验证一个行为的多个角度。如"创建用户"同时验证 id > 0、日志被调 — 同一行为的多个验证点。

---

## 第八课：最佳实践

### 什么值得测试 / 不值得测试

| 必须测 | 可选测 | 不测 |
|---|---|---|
| 核心业务逻辑 | Getter（无逻辑） | 第三方库的行为 |
| 所有 if/else 分支 | 简单委托方法 | 编译器能检查的 |
| 边界条件（0、空、null） | UI 渲染 | 配置文件默认值 |
| 异常路径 | 性能（用 benchmark） | 短期实验代码 |
| 修过的 bug（回归） | | 已被覆盖的 private 辅助函数 |

### 单元测试 vs 集成测试

| | 单元测试 | 集成测试 |
|---|---|---|
| 范围 | 一个类/函数 | 多个模块协作 |
| 依赖 | Mock/Fake | 真实数据库、真实 API |
| 速度 | 毫秒级 | 秒级 |
| 失败定位 | 精确到类 | 模块间交互 |
| 运行频率 | 每次提交 | CI 中 |
| 数量 | 90% | 10% |

```
     ⬆  集成测试（少量）
    ⬆⬆
   ⬆⬆⬆
  ⬆⬆⬆⬆
 ⬆⬆⬆⬆⬆  单元测试（大量）
```

### 写可维护测试的 3 条规则

**规则 1：测行为，不测实现**
```cpp
// ❌ 脆弱：和内部分页算法耦合
EXPECT_CALL(*db_, Select("LIMIT 20 OFFSET 0"));

// ✅ 健壮：只验证结果
auto users = service_->GetUsers({.page = 1, .size = 20});
EXPECT_EQ(users.size(), 20);
```

**规则 2：测试数据接近真实**
```cpp
// ❌ 不真实
EXPECT_EQ(CalcDiscount(100), 15);

// ✅ 真实（100 元打 15%，实付 85）
EXPECT_DOUBLE_EQ(CalcDiscount(100.0), 85.0);
```

**规则 3：Fixture 做公共初始化**
```cpp
class OrderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    db_ = std::make_unique<MockDatabase>();
    service_ = std::make_unique<OrderService>(db_.get());
  }
  void ExpectOrderSaved() {
    EXPECT_CALL(*db_, Save(testing::_));
  }
  std::unique_ptr<MockDatabase> db_;
  std::unique_ptr<OrderService> service_;
};
```

### 常见反模式

1. **巨无霸测试** — 500 行测了所有功能 → 拆成小测试
2. **条件化断言** — `if (flag) { EXPECT_EQ(a, 1); } else { EXPECT_EQ(a, 2); }` → 参数化或拆测试
3. **依赖未定义行为** — 依赖 `unordered_map` 内部顺序 → 用 `UnorderedElementsAre`
4. **共享可变状态** — 测试间依赖 static 变量顺序 → 每个测试独立
5. **Mock 一切** — 连值对象也 Mock → 值对象直接 new

### 新人最容易犯的 10 个错误

| # | 错误 | 正确做法 |
|---|---|---|
| 1 | 只测 happy path | 至少成功 + 失败两条 |
| 2 | 用 `EXPECT_EQ` 比浮点数 | `EXPECT_FLOAT_EQ` |
| 3 | 用 `EXPECT_EQ` 比 `const char*` | `EXPECT_STREQ` |
| 4 | 一个 TEST 测了所有 | 一个测试一个行为 |
| 5 | 测试依赖执行顺序 | 每个测试独立 SetUp |
| 6 | Mock 了不该 Mock 的 | 只 Mock 外部依赖 |
| 7 | 测试和源文件分离太远 | 一一对应目录 |
| 8 | 测试数据写真实生产数据 | 用 Fake 数据 |
| 9 | 不用 Fixture 到处复制代码 | 公共初始化放 Fixture |
| 10 | 测试太长（> 50 行） | 保持 AAA 结构清晰 |

### 面试问题与答案

**Q: 什么值得测，什么不值得测？**
A: 值得测：核心业务逻辑、所有 if/else 分支、边界条件、异常路径、修过的 bug。不值得测：无逻辑的 Getter、STL 操作、第三方库行为。

**Q: 单元测试和集成测试的区别？**
A: 单元测试测单个类，Mock 外部依赖，毫秒级，每次提交跑。集成测试测模块间协作，用真实依赖，秒级，CI 中跑。

**Q: 如何判断一个测试写得好？**
A: 独立（不依赖其他测试）、快速（毫秒级）、可读（AAA）、可靠（不偶发失败）、定位精准（失败了知道哪里问题）。

**Q: 重构时测试需要改吗？**
A: 如果测的是行为（public 接口），重构不改测试。如果测的是实现细节，重构必须改测试 — 这是坏味道。
