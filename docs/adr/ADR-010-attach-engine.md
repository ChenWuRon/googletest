# ADR-010 Attach Engine

Status: Accepted

Date: 2026-06-10

Authors: Project Team

---

# Context

发现目标进程后，需要执行一系列操作将其绑定到 cgroup：

1. 创建 cgroup 目录
2. 设置所有控制文件的值
3. 写入 PID 到 cgroup.procs
4. 写入 TID 到 cgroup.threads

这个流程涉及**流程编排**、**状态管理**、**重试逻辑**和**回滚**。

把 Attach 放在 Driver 中会让 Driver 承担编排职责（违反单一职责）；
把 Attach 放在 Monitor 中会让 Monitor 承担状态管理职责（违反职责分离）。

因此需要独立的 AttachEngine。

---

# Decision

定义 `AttachEngine`，负责 Attach / Detach / Reattach 流程的编排和执行。

---

## Responsibilities

| 职责 | 说明 |
|------|------|
| **resolve target** | 根据 ConfigDomain 的 GROUP 和 CONTROLLER 节点确定 cgroup 路径和控制文件列表 |
| **validate target** | 校验 Controller 和 Item 存在性及值格式（委托 ControllerRegistry） |
| **invoke driver** | 编排 createGroup → setValue × N → attachProcess → attachThread × N |
| **track attach status** | 写入 RuntimeState 的 attach_status |

---

## Interface

```cpp
class AttachEngine {
public:
    explicit AttachEngine(std::unique_ptr<ICgroupDriver> driver);

    // 首次 Attach：创建 cgroup + 设置限制 + 绑定进程
    std::optional<Error> attach(
        const ConfigNode& group,
        RuntimeState& state);

    // 完全分离：从 cgroup 移除进程 + 删除 cgroup
    std::optional<Error> detach(RuntimeState& state);

    // 恢复 Attach：进程重启后重新绑定
    std::optional<Error> reattach(
        const ConfigNode& group,
        RuntimeState& state);

private:
    std::unique_ptr<ICgroupDriver> driver_;
};
```

### Attach Flow

```
attach(group, state)
    │
    ├── Step 1: resolve
    │    ├── 从 group 读取 cgroup_path（或根据 group name 构造）
    │    └── 遍历 group 的 CONTROLLER 子节点，收集 Item 列表
    │
    ├── Step 2: validate
    │    └── 对每个 Item，调用 ControllerRegistry::validate()
    │
    ├── Step 3: createGroup
    │    └── driver->createGroup(cgroup_path)
    │
    ├── Step 4: setValue × N
    │    └── for each controller:
    │         └── for each item:
    │              └── driver->setValue(cgroup_path, item.name, item.value)
    │
    ├── Step 5: attachProcess
    │    └── driver->attachProcess(cgroup_path, state.pid)
    │
    ├── Step 6: attachThread × N
    │    └── for each tid in state.tids:
    │         └── driver->attachThread(cgroup_path, tid)
    │
    ├── Step 7: track status
    │    └── state.attach_status = Attached
    │
    └── return nullopt (success)
```

### Detach Flow

```
detach(state)
    │
    ├── Step 1: 移出进程
    │    └── driver->attachProcess("/", state.pid)
    │        (将 PID 写回根 cgroup，解除绑定)
    │
    ├── Step 2: 删除目录
    │    └── driver->removeGroup(state.cgroup_path)
    │
    └── state.attach_status = Detached
```

### Reattach Flow

reattach = attach（流程完全一致）。

唯一区别：`createGroup` 路径已存在时幂等成功。

---

## 1. Why Attach Is Not Part of Driver

### 职责边界

| 维度 | Driver | AttachEngine |
|------|--------|-------------|
| 职责 | 底层文件操作 | 流程编排 |
| 操作 | `mkdir`, `write`, `read` | resolve → validate → invoke → track |
| 状态 | 无状态 | 管理 attach_status |
| 重试 | 单次操作重试 | 整个流程重试 |
| 回滚 | 无 | 失败时清理已创建的目录 |
| 关心 | 文件描述符、errno | Item 列表、Controller 是否存在 |

### 如果 Attach 放在 Driver 中

```cpp
// 反例：Driver 同时负责编排和文件操作
class ICgroupDriver {
    // 文件操作
    virtual Result createGroup(path) = 0;
    virtual Result setValue(path, file, val) = 0;

    // 编排（污染接口）
    virtual Result attachGroup(const ConfigNode& group) = 0;
    virtual Result detachGroup(const std::string& path) = 0;
};
```

问题：

| 问题 | 说明 |
|------|------|
| 接口膨胀 | Driver 需要理解 ConfigNode、ControllerRegistry 等上层概念 |
| 违反单一职责 | Driver 既要管"怎么写文件"又要管"写哪些文件" |
| Mock 复杂度 | Mock 需要模拟编排逻辑，而不仅仅是文件操作 |
| 可测试性 | 编排逻辑的测试需要同时 Mock Driver 和 ControllerRegistry |

---

## 2. Why Attach Is Not Part of Monitor

### 职责边界

| 维度 | Monitor | AttachEngine |
|------|---------|-------------|
| 职责 | 检测变化 | 执行绑定 |
| 运行频率 | 每秒（持续检测） | 仅启动时 + 恢复时 |
| 状态 | 只读 RuntimeState (pid) | 写入 attach_status |
| 关心 | PID 是否存活 | cgroup 文件是否写入成功 |

### 如果 Attach 放在 Monitor 中

```cpp
// 反例：Monitor 同时负责检测和执行
class Monitor {
    void check() {
        // 检测逻辑
        if (!pid_alive(state.pid)) {
            // 执行逻辑（不属于 Monitor）
            driver->createGroup(path);
            driver->setValue(...);
            driver->attachProcess(path, new_pid);
        }
    }
};
```

问题：

| 问题 | 说明 |
|------|------|
| 检测延迟 | Attach 的 I/O 操作阻塞 Monitor 的检测循环 |
| 测试困难 | Monitor 测试需要模拟 Driver、ControllerRegistry、RuntimeState |
| 内聚性 | 检测逻辑和执行逻辑的变化原因不同，应该分开 |

---

## 3. Attach Ordering

### 顺序规则

```
1. createGroup           ← 必须先创建目录
2. setValue (all items)  ← 目录存在后才能写入
3. attachProcess         ← 进程必须在限制就位后加入
4. attachThread          ← 进程级别绑定后，再处理线程
```

### 为什么先 setValue 再 attachProcess

```
先 write cgroup.procs，再 setValue
  → 进程在写入期间不受限制
  → 可能短时间内资源使用超标

先 setValue，再 write cgroup.procs
  → 进程加入时限制已就位
  → 无超标窗口
```

### Item 写入顺序

Item 写入无依赖关系，顺序不重要。

但如果存在控制器间依赖，Driver 在 `createGroup` 时已通过 `subtree_control` 启用依赖控制器。

---

## 4. Thread Attach Strategy

### 策略

```
attachProcess(path, pid)
    → 内核将 PID 对应的所有线程移入 cgroup
    → 后续新创建的线程默认也在该 cgroup 中
    → 不需要显式 attach 每个 tid（大多数场景）

attachThread(path, tid)
    → 仅在线程级资源管理时使用
    → 将单个线程移动到指定 cgroup
```

### 默认策略：不显式 Attach 线程

大多数场景下，`attachProcess` 就已经足够——内核会自动移动所有线程。

显式 Attach 线程仅在以下场景需要：

| 场景 | 说明 |
|------|------|
| 线程级隔离 | 不同线程分配到不同 cgroup |
| Threaded Controller | v2 的 threaded controller 需要线程级管理 |
| 观测 | 验证某个特定线程是否在正确 cgroup 中 |

### 失败处理

```
for each tid in state.tids:
    err = driver->attachThread(path, tid)
    if err:
        log->warn("thread attach failed: tid={}", tid)
        continue  // 不影响其他线程
```

---

## 5. Retry Policy

### 重试范围

AttachEngine 的重试是**全流程重试**，不是单步重试。

```
失败场景:
  setValue("cpu.max") 失败（CgroupWriteFailed）
    → 重试整个 attach 流程
    → createGroup（幂等，路径已存在）
    → setValue × N（重新写入所有 Item）
    → attachProcess

不是:
  setValue("cpu.max") 失败
    → 只重试 setValue("cpu.max")
    → 因为其他 Item 可能也受到同一个内核错误的影响
```

### 重试参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| max_retries | 3 | 最大重试次数 |
| retry_interval | 500ms | 重试间隔 |

### 什么情况不重试

| 错误 | 不重试原因 |
|------|------------|
| `ControllerNotSupported` | 内核不支持该控制器，重试无意义 |
| `InvalidControlValue` | 值格式错误，重试结果相同 |
| `CgroupVersionMismatch` | Driver 和内核版本不匹配 |

### 回滚

重试耗尽后，如果 `createGroup` 已成功但 `setValue` 失败：

```cpp
if (retries_exhausted) {
    // 回滚：删除已创建的 cgroup 目录
    driver->removeGroup(path);
    state.attach_status = AttachStatus::Failed;
    return last_error;
}
```

---

# Consequences

采用本决策后：

优点：

* Attach 流程编排独立，不污染 Driver 和 Monitor 的职责
* 完整的 resolve → validate → invoke → track 链路
* 全流程重试，回滚策略完整
* 线程 Attach 可跳过，降低默认路径开销
* setValue 在 attachProcess 之前执行，消除超标窗口

代价：

* 额外组件，增加依赖注入复杂度
* 全流程重试比单步重试成本高
* 回滚逻辑增加了 AttachEngine 的测试路径

---

# Related ADR

ADR-003 Config Tree — AttachEngine 遍历 ConfigNode 获取 Item 列表
ADR-005 Runtime State — AttachEngine 写入 attach_status
ADR-006 Cgroup Driver — AttachEngine 唯一持有 Driver 实例
ADR-008 Recovery — Recovery 流程调用 AttachEngine::reattach
ADR-009 Controller Model — AttachEngine 使用 ControllerRegistry 校验
