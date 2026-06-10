# ADR-006 Cgroup Driver

Status: Accepted

Date: 2026-06-10

Authors: Project Team

---

# Context

Resource Manager 的核心操作是操控 cgroup：创建层级、设置资源限制、将进程绑定到 cgroup。

这些操作最终读写 `/sys/fs/cgroup` 文件系统。

直接操作 sysfs 会导致：

* **业务代码与内核接口耦合** — 业务层需要理解 cgroup v1 和 v2 的路径差异、控制文件差异、写入语义差异
* **v1/v2 差异渗透到上层** — AttachEngine、Monitor、Recovery 等组件都需要判断当前 cgroup 版本
* **无法单元测试** — 测试代码需要 root 权限，需要真实的 cgroup 文件系统
* **难以模拟错误场景** — 磁盘满、权限拒绝、文件不存在等场景在真实文件系统上难以构造
* **违反 Driver Abstraction 原则** — architecture.md 明确要求业务层禁止直接访问 `/sys/fs/cgroup`

因此需要定义一个统一的 Cgroup Driver 接口，所有 cgroup 操作通过该接口完成。

---

# Decision

定义 `ICgroupDriver` 接口和三种实现，隔离所有内核接口操作。

---

## Interface — ICgroupDriver

```cpp
class ICgroupDriver {
public:
    virtual ~ICgroupDriver() = default;

    // 创建 cgroup 目录
    virtual std::optional<Error> createGroup(const std::string& path) = 0;

    // 递归删除 cgroup 目录
    virtual std::optional<Error> removeGroup(const std::string& path) = 0;

    // 写入控制文件（如 cpu.max）
    virtual std::optional<Error> setValue(
        const std::string& path,
        const std::string& file,
        const std::string& value) = 0;

    // 读取控制文件
    virtual std::optional<std::string> getValue(
        const std::string& path,
        const std::string& file) = 0;

    // 将进程 PID 写入 cgroup.procs
    virtual std::optional<Error> attachProcess(
        const std::string& path,
        int pid) = 0;

    // 将线程 TID 写入 tasks（v1）或 cgroup.threads（v2）
    virtual std::optional<Error> attachThread(
        const std::string& path,
        int tid) = 0;
};
```

### 方法说明

| 方法 | 操作目标 | 说明 |
|------|----------|------|
| `createGroup` | cgroup 目录 | `mkdir` 对应路径，v2 自动继承父级控制器 |
| `removeGroup` | cgroup 目录 | `rmdir` 对应路径，允许递归（先清理子 cgroup） |
| `setValue` | 控制文件 | 写入字符串到控制文件（如 `echo 100000 > cpu.max`） |
| `getValue` | 控制文件 | 读取控制文件全部内容，去除尾部换行 |
| `attachProcess` | cgroup.procs | 写入 PID，将该进程所有线程移入 cgroup |
| `attachThread` | cgroup.threads | 写入 TID，仅移动单个线程 |

### 参数约定

| 参数 | 类型 | 说明 | 示例 |
|------|------|------|------|
| `path` | string | 相对于 cgroup 挂载点的路径 | `"my_service/cpu"` |
| `file` | string | 控制文件名，不含路径 | `"cpu.max"` |
| `value` | string | 写入值，与内核期望格式一致 | `"100000 100000"` |
| `pid`/`tid` | int | 进程/线程 ID | `1234` |

path 永远是相对路径。Driver 内部拼接到探测到的 cgroup 根路径。

---

## Implementations

| 实现类 | cgroup 版本 | 用途 |
|--------|-------------|------|
| `CgroupV2Driver` | v2（默认） | 生产环境，原生 cgroup v2 |
| `CgroupV1Driver` | v1 | 兼容遗留系统 |
| `MockCgroupDriver` | — | 单元测试，记录所有操作到内存 |

### CgroupV2Driver

v2 实现的核心特性：

| 特性 | 说明 |
|------|------|
| 挂载点探测 | 读取 `/proc/mounts` 找到 `cgroup2` 类型挂载点（通常为 `/sys/fs/cgroup`） |
| 路径拼接 | `{mount_point}/{relative_path}` |
| 目录创建 | `mkdirat()` 系统调用 |
| 控制文件写入 | `write()` 到对应 fd |
| 进程 Attach | `write(pid)` 到 `cgroup.procs` |
| 线程 Attach | `write(tid)` 到 `cgroup.threads` |
| 控制器启用 | 通过 `cgroup.subtree_control` 写入 |

### CgroupV1Driver

v1 实现需要处理多个控制器挂载点：

| 特性 | 说明 |
|------|------|
| 多挂载点 | 每个控制器独立挂载（`/sys/fs/cgroup/cpu/`, `/sys/fs/cgroup/memory/`） |
| 路径映射 | `{controller_mount_point}/{relative_path}` |
| 进程 Attach | `write(pid)` 到 `cgroup.procs`（写入每个控制器目录） |
| 线程 Attach | `write(tid)` 到 `tasks`（v1 没有 `cgroup.threads`） |

### MockCgroupDriver

| 特性 | 说明 |
|------|------|
| 内存模拟 | 所有 cgroup 路径和文件保存在 `map<string, map<string, string>>` |
| 操作记录 | 每次调用追加到操作日志 |
| 错误注入 | `setNextError()` 可以控制下一次调用返回指定的 Error |
| 无副作用 | 不读写真实的文件系统 |

---

## Factory

驱动实例化通过静态工厂方法完成：

```cpp
auto driver = ICgroupDriver::create(cgroup_version);
```

| version | 返回值 |
|---------|--------|
| 0 | `MockCgroupDriver` |
| 1 | `CgroupV1Driver` |
| 2 | `CgroupV2Driver`（默认） |
| 其他 | 返回 `nullptr`（非法版本） |

驱动选择在**启动时**完成，运行时不可切换。

---

## 1. Why Driver Abstraction Exists

### 1.1 职责分离

```
业务层（AttachEngine, Monitor, Recovery）
    │
    ▼
ICgroupDriver（抽象接口）
    │
    ├── CgroupV2Driver  ──▶ /sys/fs/cgroup (v2)
    ├── CgroupV1Driver  ──▶ /sys/fs/cgroup (v1)
    └── MockCgroupDriver ──▶ memory (test)
```

业务层不感知 v1 还是 v2，不感知路径拼接规则，不感知控制文件命名差异。

### 1.2 可测试性

没有 Driver 抽象时，测试 cgroup 操作需要：

* root 权限
* 操作真实 cgroup 文件系统
* 清理测试产生的 cgroup 目录
* 无法测试磁盘满、权限拒绝等错误路径

有了 MockCgroupDriver：

* 单元测试无需 root 权限
* 操作在内存中完成，无副作用
* 错误注入可以覆盖所有错误路径

### 1.3 未来兼容性

如果 Linux 内核未来引入 cgroup v3，只需要新增 `CgroupV3Driver` 实现 `ICgroupDriver`。

```
CgroupV3Driver : ICgroupDriver  // 新增，不改任何业务代码
```

---

## 2. Why Business Code Cannot Access /sys/fs/cgroup Directly

架构原则（architecture.md §7.2）：

> **Driver Abstraction**
> 业务层禁止直接访问：`/sys/fs/cgroup`
> **必须通过 Driver。**

直接访问违反该原则，导致以下问题：

### 2.1 版本耦合

```cpp
// 直接访问（禁止）
std::string path = "/sys/fs/cgroup/my_service/cpu.max";
write_value(path, "100000 100000");

// 这段代码在 v2 上工作，在 v1 上路径完全不同
// 如果换成 v3，每处直接访问都需要修改
```

### 2.2 测试不可模拟

```cpp
// 直接访问（禁止）
std::ofstream file("/sys/fs/cgroup/my_service/cgroup.procs");
file << pid;
file.close();

// 不可能在单元测试中隔离这段代码
// 没有 mock 点，没有依赖注入入口
```

### 2.3 错误处理分散

```cpp
// 直接访问（禁止）
auto fd = open(path.c_str(), O_WRONLY);
if (fd < 0) {
    // 每处调用者都要处理 errno
    // 错误处理方式不一致
}
```

通过 Driver，所有错误集中到一处处理（见第 6 节 Error Handling Strategy）。

### 2.4 审计困难

直接访问意味着 cgroup 操作分散在代码库各处。通过 Driver：

* 所有 cgroup 操作在 `src/driver/` 目录下
* 审计只需要检查一个目录
* 可以统一添加日志、指标、trace

---

## 3. How v1/v2 Compatibility Works

### 3.1 版本探测

启动时自动探测当前内核的 cgroup 版本：

```
1. 检查 /sys/fs/cgroup 是否挂载了 cgroup2
   → /proc/mounts: "cgroup2 /sys/fs/cgroup cgroup2 ..."
   → 是 → CgroupV2Driver

2. 检查 /sys/fs/cgroup 是否挂载了 cgroup v1 控制器
   → /proc/mounts: "cgroup /sys/fs/cgroup/cpu cgroup cpu ..."
   → 是 → CgroupV1Driver

3. 两者都不是 → 错误：cgroup 不可用
```

用户也可以显式指定版本：

```
resource-manager --cgroup-version=1
resource-manager --cgroup-version=2
```

### 3.2 路径差异

| 层面 | v1 | v2 |
|------|----|----|
| 挂载点 | 每个控制器独立挂载 | 统一挂载点 |
| 路径 | `{controller}/group` | `group` |
| 进程文件 | `cgroup.procs`（每个控制器目录下） | `cgroup.procs`（统一） |
| 线程文件 | `tasks` | `cgroup.threads` |

CgroupV1Driver 内部维护控制器到挂载点的映射：

```
/sys/fs/cgroup/cpu/      → cpu controller
/sys/fs/cgroup/memory/   → memory controller
/sys/fs/cgroup/cpuset/   → cpuset controller
```

`setValue("my_group", "cpu.max", "100000")` 在 v1 下自动映射为写入 `/sys/fs/cgroup/cpu/my_group/cpu.max`。

### 3.3 控制文件差异

| 功能 | v1 文件 | v2 文件 |
|------|---------|---------|
| CPU 配额 | `cpu.cfs_quota_us`, `cpu.cfs_period_us` | `cpu.max` |
| CPU 权重 | `cpu.shares` | `cpu.weight` |
| 内存限制 | `memory.limit_in_bytes` | `memory.max` |
| 内存软限制 | `memory.soft_limit_in_bytes` | `memory.high` |
| Cpuset | `cpuset.cpus` | `cpuset.cpus` |

CgroupV1Driver 根据 ADR-009 Controller Model 的 `ControlFileType` 映射表做名称转换。

### 3.4 线程 Attach 差异

| 版本 | v1 | v2 |
|------|----|----|
| 进程 Attach | `write(pid) → cgroup.procs` | `write(pid) → cgroup.procs` |
| 线程 Attach | `write(tid) → tasks` | `write(tid) → cgroup.threads` |

v1 中 `tasks` 文件允许任意 TID 独立移动。v2 中 `cgroup.threads` 同样支持线程级移动，但需要线程粒度管理（Threaded Controller）。

---

## 4. Driver Ownership Model

### 4.1 Ownership

Driver 实例由 `AttachEngine` 唯一持有。

```
ConfigDomain
    │
    ▼
AttachEngine
    │
    └── owns: unique_ptr<ICgroupDriver>  driver_
```

### 4.2 为什么不共享

Driver 不是全局单例。原因：

| 原因 | 说明 |
|------|------|
| 测试隔离 | 每个 AttachEngine 实例可以使用不同的 Driver（生产 / mock） |
| 无全局状态 | 避免隐藏依赖，构造时显式注入 |
| 线程安全 | 每个 Driver 实例有自己的文件描述符和状态，无需全局锁 |

### 4.3 依赖注入

```cpp
// 生产代码
auto driver = ICgroupDriver::create(2);  // CgroupV2Driver
auto engine = std::make_unique<AttachEngine>(std::move(driver));

// 测试代码
auto mock = ICgroupDriver::create(0);   // MockCgroupDriver
auto engine = std::make_unique<AttachEngine>(std::move(mock));
```

Driver 的所有权从 `ICgroupDriver::create` 转移到 `AttachEngine`。

---

## 5. Controller Enablement Strategy

### 5.1 cgroup v2 — subtree_control

在 cgroup v2 中，控制器默认在根 cgroup 启用，子 cgroup 继承。

但控制器不会自动在子 cgroup 生效——父 cgroup 必须在 `cgroup.subtree_control` 中写入控制器名。

```
// 启用 cpu 和 memory 控制器
echo "+cpu +memory" > /sys/fs/cgroup/cgroup.subtree_control
echo "+cpu +memory" > /sys/fs/cgroup/my_service/cgroup.subtree_control
```

### 5.2 Strategy

Driver 在 `createGroup` 时自动启用必要的控制器：

```
createGroup("my_service")
    │
    ├── 1. mkdir /sys/fs/cgroup/my_service
    ├── 2. 读取 ConfigDomain 中该 group 使用的控制器列表
    ├── 3. 对每个控制器检查是否已在 subtree_control 中启用
    └── 4. 未启用 → 写入 "+controller" 到父级 subtree_control
```

### 5.3 控制器依赖

某些控制器依赖其他控制器：

| 控制器 | 依赖 |
|--------|------|
| cpu | 无 |
| memory | 无 |
| cpuset | 无 |
| io | 依赖 cpu (weight 映射) |
| pids | 无 |

Driver 在启用时自动处理依赖链。

### 5.4 v1 控制器

v1 不需要显式启用。控制器通过挂载时决定，只要控制器被挂载即可使用。

CgroupV1Driver 探测 `ls /sys/fs/cgroup/` 中的子目录来确定可用控制器。

---

## 6. Error Handling Strategy

### 6.1 Error Type

所有 Driver 方法使用 ADR-012 统一的 `Error` 结构。

```cpp
std::optional<Error>          // 成功 = nullopt, 失败 = Error
std::optional<std::string>    // getValue 的返回类型
```

### 6.2 Error Codes

| 场景 | ErrorCode | 条件 |
|------|-----------|------|
| 目录创建失败 | `CgroupCreateFailed` | `mkdir()` 返回错误 |
| 目录删除失败 | `CgroupRemoveFailed` | `rmdir()` 返回错误 |
| 控制文件写入失败 | `CgroupWriteFailed` | `write()` 返回错误 |
| 控制文件读取失败 | `CgroupReadFailed` | `read()` / `open()` 返回错误 |
| Attach 失败 | `AttachFailed` | `write()` 到 cgroup.procs 返回错误 |
| 版本不匹配 | `CgroupVersionMismatch` | v1 Driver 操作 v2 路径，反之亦然 |
| 控制器不存在 | `ControllerNotSupported` | 尝试写入未启用的控制器文件 |

### 6.3 Context

每个 Error 携带上下文：

```cpp
Error(CgroupWriteFailed, "write cpu.max failed", "CgroupV2Driver")
    .with("path", "my_service/cpu")
    .with("file", "cpu.max")
    .with("value", "100000 100000")
    .with("errno", std::to_string(errno));
```

### 6.4 Mock Error Injection

MockCgroupDriver 支持错误注入：

```cpp
auto mock = dynamic_cast<MockCgroupDriver*>(driver.get());
mock->setNextError("createGroup",
    Error(CgroupCreateFailed, "simulated failure", "Mock"));

auto err = driver->createGroup("test"); // returns Error
```

### 6.5 调用方职责

Driver 方法的调用方（AttachEngine）负责：

1. 检查返回值（nullopt = 成功，有值 = 失败）
2. 记录 Error 到日志
3. 根据 ErrorCode 决定重试（如 `AttachFailed` 可重试）
4. 更新 RuntimeState 的 attach_status

```
auto err = driver->attachProcess(path, pid);
if (err) {
    logger->error("attach failed: " + err->message);
    state.attach_status = AttachStatus::Failed;
    return err;
}
```

---

## 7. Testing Strategy

### 7.1 三层测试

| 层 | 测试对象 | 方法 |
|----|----------|------|
| 单元测试 | 业务组件（AttachEngine, Monitor） | 注入 MockCgroupDriver |
| 集成测试 | CgroupV2Driver 真实调用 | 在容器中操作真实 cgroup |
| 兼容性测试 | CgroupV1Driver | 在 v1 环境运行同样的测试套件 |

### 7.2 单元测试覆盖

| 测试场景 | MockCgroupDriver 特性 |
|----------|----------------------|
| createGroup 成功 | 验证 mock 记录了创建路径 |
| createGroup 失败 | `setNextError` 注入创建失败 |
| setValue 值验证 | 读取 mock 中的值确认写入 |
| attachProcess PID 记录 | 验证 mock 记录了 PID |
| 重复创建 | mock 返回已有的路径 |
| 错误路径覆盖 | 每种 ErrorCode 至少覆盖一次 |

### 7.3 集成测试覆盖

集成测试在 Docker 容器中运行，要求：

- `--privileged` 或 `--cgroupns=host`
- cgroup v2 文件系统挂载

| 测试场景 | 验证 |
|----------|------|
| 创建 cgroup | `ls /sys/fs/cgroup/test_xxx` 目录存在 |
| 设置 cpu.max | `cat /sys/fs/cgroup/test_xxx/cpu.max` 值匹配 |
| 设置 memory.max | `cat /sys/fs/cgroup/test_xxx/memory.max` 值匹配 |
| Attach 进程 | `cat /sys/fs/cgroup/test_xxx/cgroup.procs` 包含 PID |
| 删除 cgroup | 目录不再存在 |

### 7.4 Mock 行为验证

MockCgroupDriver 提供 `operations()` 返回操作日志：

```cpp
auto ops = mock->operations();
// ops[0] = { "createGroup", "my_group", "" }
// ops[1] = { "setValue", "my_group/cpu.max", "100000" }
// ops[2] = { "attachProcess", "my_group", "1234" }
```

测试可以验证：

1. 操作顺序正确
2. 每次操作的参数正确
3. 操作次数符合预期

---

# Rejected Alternatives

## Direct Filesystem Access

直接在业务代码中 `open()` / `write()` `/sys/fs/cgroup/...`。

**被拒绝**。

违反 Driver Abstraction 架构原则。不满足可测试性要求。v1/v2 差异会渗透到整个代码库。审计困难。

## Singleton Global Driver

```cpp
class CgroupDriver {
    static ICgroupDriver& instance();
};
```

**被拒绝**。

全局单例导致：

| 问题 | 说明 |
|------|------|
| 测试困难 | 无法在测试中替换为 Mock，需要全局状态管理 |
| 隐式依赖 | 组件依赖 Driver 但构造签名不体现 |
| 并发风险 | 全局状态需要额外同步 |
| 无法多实例 | 同一进程无法使用不同的 Driver 配置 |

替代方案：构造注入（Constructor Injection）。AttachEngine 构造时接收 `unique_ptr<ICgroupDriver>`，所有权显式。

---

# Consequences

采用本决策后：

优点：

* 所有 cgroup 操作通过 6 个方法收敛，审计路径清晰
* v1/v2 差异隔离在 `src/driver/` 目录下，业务层零感知
* Mock 驱动支持完整单元测试，无需 root 权限
* 错误处理统一，Error 携带上下文
* 错误注入覆盖所有错误路径
* 构造注入模式使依赖关系显式
* 未来 v3 兼容只需新增实现类

代价：

* 需要实现三套 Driver（v1 / v2 / mock），开发成本增加
* v1 和 v2 的路径映射和控制文件映射需要维护映射表
* v2 的 subtree_control 启用逻辑增加了 createGroup 的复杂度
* 需要容器化集成测试环境

---

# Related ADR

ADR-009 Controller Model — ControlFileType 提供控制文件的名称和类型映射
ADR-010 Attach Engine — 唯一持有 Driver 实例，编排 cgroup 操作
ADR-012 Error Model — Driver 使用 ErrorCode 统一错误体系
