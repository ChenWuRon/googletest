# ADR-011 Hot Reload

Status: Accepted

Date: 2026-06-10

Authors: Project Team

---

# Context

配置文件可能在运行时发生变化。

Resource Manager 必须支持在不重启守护进程的情况下更新配置。

如果每次变更都重启：

* cgroup 绑定瞬断
* Recovery 流程重新执行（PID 不变但 cgroup 被重建）
* 无法平滑变更资源配额

但如果所有变更都热更新：

* 某些变更可能导致运行时状态不一致（如删除正在使用的 group）
* 某些变更需要重建 cgroup 层级（如修改 cpuset 的 partition 类型）

因此需要明确：**哪些变更允许热更新，哪些必须重启**。

---

# Decision

Hot Reload 遵循 **Load → Parse → Build Tree → Diff → Apply** 生命周期。

---

## Hot Reload Lifecycle

```
┌────────────────────────────────────────────────────────────┐
│                   Hot Reload Lifecycle                      │
├────────────────────────────────────────────────────────────┤
│                                                            │
│  ┌──────────┐                                               │
│  │   Load   │  检测配置文件变化（inotify / 定时轮询）      │
│  └────┬─────┘                                               │
│       │                                                     │
│       ▼                                                     │
│  ┌──────────┐                                               │
│  │  Parse   │  重新解析配置文件（ConfigParser）              │
│  └────┬─────┘                                               │
│       │                                                     │
│       ▼                                                     │
│  ┌────────────┐                                             │
│  │ Build Tree │  构建新 ConfigDomain Tree                    │
│  └────┬───────┘                                             │
│       │                                                     │
│       ▼                                                     │
│  ┌────────┐                                                 │
│  │  Diff  │  对比新旧两棵 ConfigDomain Tree                  │
│  └────┬───┘                                                 │
│       │                                                     │
│       ▼                                                     │
│  ┌─────────┐                                                │
│  │  Apply  │  增量应用 Diff 结果到 RuntimeState 和 cgroup   │
│  └─────────┘                                                │
│                                                            │
│  完成后：新 ConfigDomain 生效，旧 ConfigDomain 被替换        │
└────────────────────────────────────────────────────────────┘
```

### 触发方式

| 方式 | 说明 |
|------|------|
| inotify | 监听配置文件路径，文件写入后触发（默认） |
| SIGHUP | 收到 SIGHUP 信号后触发 |
| 定时轮询 | 兜底方案，每 30s 检查文件 mtime |

---

## Workflow Detail

### Load

```
触发条件:
  - inotify IN_CLOSE_WRITE 事件
  - SIGHUP 信号
  - 定时器到期且 mtime 变化

动作:
  - 读取配置文件到字符串
  - 比较文件内容 hash vs 当前生效的 hash
  - hash 相同 → 跳过（文件未实际变化）
  - hash 不同 → 继续 Parse
```

### Parse

```
动作:
  - ConfigParser::parse_string(file_content)
  - 成功 → 返回新 ConfigDomain
  - 失败 → 记录 Error，保留旧 ConfigDomain（不中断运行）
```

### Build Tree

```
动作:
  - ConfigDomain root 节点已经由 ConfigParser 构建
  - 无额外步骤
```

### Diff

```
输入:
  - 旧 ConfigDomain（当前生效）
  - 新 ConfigDomain（最新解析）

算法:
  - 递归对比两棵 ConfigDomain Tree
  - 以 ConfigPath 为节点标识（ADR-003 §8.2）
  - 找出：NodeAdded / NodeRemoved / NodeModified / NodeUnchanged

输出:
  - DiffResult: vector<DiffNode>
```

### Apply

```
根据 Diff 类型执行增量操作:

NodeAdded:
  ├── GROUP 新增  → createGroup + 首次 Attach
  ├── CONTROLLER  → 设置 subtree_control + setValue
  └── ITEM 新增   → setValue

NodeModified:
  ├── GROUP mode/match 变更 → 重新 Discovery + Reattach
  ├── CONTROLLER (类型不变) → 重新 setValue
  └── ITEM value 变更       → setValue(new_value)

NodeRemoved:
  ├── ITEM 删除     → setValue(default) 回滚默认值
  ├── CONTROLLER 删除 → 清理控制文件
  └── GROUP 删除    → Detach + removeGroup（必须重启？见第 2 节）
```

---

## 1. What Can Be Reloaded

| 变更 | 热更新 | 说明 |
|------|--------|------|
| ITEM value 修改 | ✓ | `driver->setValue()` 更新值 |
| ITEM 新增 | ✓ | `driver->setValue()` 写入新文件 |
| ITEM 删除 | ✓ | 恢复内核默认值 |
| CONTROLLER 新增 | ✓ | 启用 subtree_control + setValue |
| CONTROLLER 内 item 列表变更 | ✓ | 增删对应 item |
| GROUP match 规则扩展 | ✓ | 扩大匹配范围后重新发现（新增进程 Attach） |
| GROUP match 规则缩小 | ✓ | 缩小匹配范围后分离不再匹配的进程 |
| GROUP mode 不变时的任意修改 | ✓ | 不影响绑定的身份 |

---

## 2. What Cannot Be Reloaded

### 必须重启的场景

| 变更 | 原因 | 替代方案 |
|------|------|----------|
| GROUP 删除 | 删除正在使用的 cgroup 可能导致运行时状态不一致 | 先 detach 进程，确认无引用后再删除 |
| GROUP 改名 | 本质上是删除旧 group + 创建新 group | 配置中避免重命名，用唯一标识 |
| cgroup 版本切换 | v1/v2 的路径和控制文件完全不同 | 修改启动参数，重启 daemon |
| cgroup 挂载点变更 | 所有路径需要重新拼接 | 修改启动参数，重启 daemon |

### GROUP 删除的处理

```cpp
// Hot Reload 检测到 GROUP 被删除
Diff: NodeRemoved(path="/my_service")

// 步骤 1: 从 RuntimeStateManager 获取对应状态
auto state = runtime_state_manager->getByGroup("my_service");

// 步骤 2: Detach
auto err = attach_engine->detach(state);
// driver->attachProcess("/", state.pid);   // 移出 cgroup
// driver->removeGroup(state.cgroup_path);  // 删除目录

// 步骤 3: 销毁 RuntimeState
runtime_state_manager->removeState("my_service");
```

GROUP 删除的难点不在技术实现，而在**运维安全**——删除 group 前需要确认该 group 的资源不再被需要。因此标记为"需人工确认"，不自动执行。

---

## 3. Incremental Apply Strategy

### 排序规则

```
Apply 顺序：

1. 先 Apply NodeAdded（自根向下）
   ├── 先 create GROUP 目录
   ├── 再 create CONTROLLER 目录（同时启用 subtree_control）
   └── 再 setValue 写入 ITEM 值

2. 再 Apply NodeModified（任意顺序）
   └── ITEM value 变更: setValue(new)

3. 最后 Apply NodeRemoved（自叶向上）
   ├── 先清除 ITEM 值（恢复默认）
   ├── 再删除 CONTROLLER（清理目录）
   └── 最后删除 GROUP（detach + rmdir）
```

### 为什么 Added 先于 Removed

```
如果同时修改了 ITEM name:

旧树: /svc/cpu/cpu.max = "100000"
新树: /svc/cpu/cpu.peak = "200000"

正确顺序:
  1. NodeAdded("/svc/cpu/cpu.peak")  → setValue("cpu.peak", "200000")
  2. NodeRemoved("/svc/cpu/cpu.max")  → setValue("cpu.max", "max")

如果先 Removed 再 Added:
  1. setValue("cpu.max", "max")  → 限制被取消
  2. setValue("cpu.peak", "200000")  → 新限制生效
  → 中间有窗口期无限制 ✗
```

### 幂等性

所有 Driver 操作（createGroup、setValue、attachProcess）都是幂等的：

| 操作 | 重复执行的效果 |
|------|---------------|
| createGroup(path) | 目录已存在 → 成功（不覆盖已有文件） |
| setValue(path, file, val) | 写入相同值 → 无副作用 |
| attachProcess(path, pid) | PID 已在该 cgroup → 无副作用 |
| removeGroup(path) | 目录不存在 → 返回错误（Detach 场景下错误可忽略） |

---

## 4. RuntimeState Interaction

### Config Reload 对 RuntimeState 的影响

| 场景 | RuntimeState 变化 |
|------|-------------------|
| ITEM value 修改 | RuntimeState 不变（cgroup 值通过 setValue 原地更新） |
| CONTROLLER 新增 | RuntimeState 不变（新控制器的值通过 setValue 写入） |
| ITEM 删除 | RuntimeState 不变（值回滚默认，但 RuntimeState 记录的是进程状态） |
| GROUP match 变更 | 相关的 RuntimeState 可能需要重建（重新 Discovery） |
| GROUP 删除 | 相关的 RuntimeState 被销毁 |

### RuntimeState 一致性检查

Apply 完成后，执行一致性检查：

```cpp
for (auto& state : runtime_state_manager->states()) {
    // 检查 RuntimeState 是否还有对应的 GROUP
    auto* group = new_config->findByPath("/" + state.group_name);
    if (!group) {
        // GROUP 被删除，RuntimeState 应该被销毁
        log->warn("orphan runtime state: group={}", state.group_name);
    }
}
```

### ConfigState 切换

```
Apply 成功后:
  old_config_ = std::move(new_config_);
  // 旧 ConfigDomain 被释放

Apply 失败后:
  new_config_ 被丢弃
  old_config_ 继续生效
  // 运行时不受影响
```

---

## 5. Failure Rollback Strategy

### Apply 失败的处理

```
Apply 过程中任意步骤失败:

原则: 失败不中断，记录错误，继续 Apply 剩余项。

原因:
  - 部分成功比完全不成功更接近期望状态
  - 热更新不应该因为一个 ITEM 写入失败而丢弃整个更新
  - 失败项会在下一次 Diff + Apply 中重试
```

### 回滚的具体规则

| 阶段 | 失败处理 |
|------|----------|
| Load 失败 | 保留旧配置，不中断运行 |
| Parse 失败 | 保留旧 ConfigDomain，日志记录错误 |
| Diff 完成 | Diff 结果已计算，不受影响 |
| Apply: NodeAdded | 记录错误，继续 Apply 其他节点 |
| Apply: NodeModified | 记录错误，继续 Apply 其他节点 |
| Apply: NodeRemoved | 记录错误，继续 Apply 其他节点 |

### 手动回滚

手动回滚由运维人员触发——将配置文件恢复到旧版本，发送 SIGHUP 触发另一次 Hot Reload。

系统不提供自动回滚（自动回滚可能导致多次变更的反复震荡）。

### 原子性

Hot Reload 不保证 Apply 的原子性。

```
不提供:
  - 全量或全不（all-or-nothing）
  - 事务性提交

提供:
  - 每个节点操作的幂等性
  - 部分失败不影响其他节点
  - 失败后重试
```

---

# Consequences

采用本决策后：

优点：

* 大部分配置变更支持热更新，无需重启 daemon
* Diff 驱动增量更新，只处理变化的节点
* 幂等操作保证重复 Apply 安全
* 部分失败不中断整体 Apply
* 新旧 ConfigDomain 切换原子（指针切换）

代价：

* Diff 算法需要递归对比整棵树，配置大时可能有性能开销
* GROUP 删除需要人工确认安全
* 不提供事务性 Apply，部分成功场景需要运维关注失败项
* 需要 inotify / SIGHUP / 定时轮询三种触发机制

---

# Related ADR

ADR-003 Config Tree — Diff 模型和 NodeIdentity 定义
ADR-005 Runtime State — Hot Reload 读取 RuntimeState 做一致性检查
