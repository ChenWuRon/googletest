# ADR-005 Runtime State

Status: Accepted

Date: 2026-06-10

Authors: Project Team

---

# Context

Resource Manager 管理两类状态：

* **期望状态** — 配置文件定义的资源规则（"应该给 nginx 分配多少 CPU"）
* **实际状态** — 进程正在使用哪些资源、绑定是否生效、进程是否在运行

如果两者不分离：

* 配置变更无法追踪是否已生效
* 进程重启后绑定丢失无法检测
* Attach 成功/失败无法判断
* Recovery 没有判断依据（不知道是否需要恢复）
* 无法回答"当前运行时和配置是否一致"

因此必须将**配置状态**与**运行状态**分离。

---

# Decision

定义两个独立状态：`ConfigState` 和 `RuntimeState`。

---

## ConfigState — Configuration State

### 定义

ConfigState 是配置文件解析后的期望状态。

```
ConfigState = ConfigDomain Tree（只读，不可变）
```

ConfigState 是声明式的：它描述系统*应该*达到的状态。

### 特性

| 特性 | 说明 |
|------|------|
| 来源 | ConfigParser 解析配置文件 |
| 可变性 | 只读（immutable）。变更通过替换整个 ConfigState 完成 |
| 生命周期 | 从配置加载到下一次配置重新加载 |
| 线程安全 | 是（只读，可安全并发访问） |
| 序列化 | 可以导出为配置文件和 JSON |

### 包含

ConfigState 包含 ADR-003 定义的完整 ConfigDomain Tree：

```
ROOT
 ├── GROUP("web")
 │    ├── CONTROLLER("cpu")     ── ITEM("cpu.max") = "100000 100000"
 │    └── CONTROLLER("memory")  ── ITEM("memory.max") = "1G"
 └── GROUP("db")
      └── CONTROLLER("cpu")     ── ITEM("cpu.max") = "200000 100000"
```

以及必要的元数据：

| 字段 | 类型 | 说明 |
|------|------|------|
| source | string | 配置文件路径 |
| loaded_at | timestamp | 加载时间 |
| version | string | 文件版本或哈希 |

---

## RuntimeState — 运行时状态

### 定义

RuntimeState 是系统运行时实际状态的快照。

它是命令式的：它记录系统*当前处于*什么状态。

### 组成

每个 `RuntimeState` 实例对应一个 **group-进程绑定对**。

```
RuntimeState  ───  per (Group, discovered_process)
```

#### Process Identity（进程身份）

进程重启后不变的身份信息：

| 字段 | 类型 | 说明 |
|------|------|------|
| mode | Mode | 来源：Group.mode。进程重启后不变 |
| match_pattern | string | 来源：Group.match.pattern。进程重启后不变 |
| service_name | string | 运行时检测到的服务名（如 systemd unit 名） |
| cgroup_path | string | 绑定的 cgroup 路径 |

Process Identity 用于 Recovery：进程重启后，Discovery 根据 identity 重新定位新 PID。

#### PID / TID

| 字段 | 类型 | 说明 |
|------|------|------|
| pid | int | 进程 PID。进程重启后变化 |
| tids | vector<int> | 线程 TID 列表。进程重启后变化 |

#### Status（状态机）

RuntimeState 维护三个独立的状态维度：

**AttachStatus**

```
enum AttachStatus {
    Pending,     // 等待 Attach（已发现，未绑定）
    Attached,    // 已成功绑定到 cgroup
    Detached,    // 已主动分离
    Failed,      // Attach 操作失败
}
```

**DiscoveryStatus**

```
enum DiscoveryStatus {
    Unknown,      // 尚未扫描
    Discovering,  // 正在扫描 /proc
    Discovered,   // 已在 /proc 中找到目标
    Missing,      // 曾经发现，现在 /proc 中不存在
    Excluded,     // 被显式排除（配置变更后不再匹配）
}
```

**RecoveryStatus**

```
enum RecoveryStatus {
    None,        // 无需恢复
    Detecting,   // 检测到 PID 变化，正在确认
    Recovering,  // 正在重新 Attach
    Recovered,   // 已成功恢复
    Failed,      // 恢复失败（达到重试上限）
}
```

#### Last Seen

| 字段 | 类型 | 说明 |
|------|------|------|
| last_discovery_time | timestamp | 最近一次成功发现的时间戳 |
| last_attach_time | timestamp | 最近一次成功 Attach 的时间戳 |
| last_seen_pid | int | 上一个 PID（用于检测变化） |

### 完整结构

```cpp
struct RuntimeState {
    // Identity（进程重启后不变）
    Mode mode;
    std::string match_pattern;
    std::string service_name;
    std::string cgroup_path;

    // Livelihood（进程重启后变化）
    int pid;
    std::vector<int> tids;

    // Status（三个独立维度）
    AttachStatus attach_status;
    DiscoveryStatus discovery_status;
    RecoveryStatus recovery_status;

    // Timestamps
    std::chrono::system_clock::time_point last_discovery_time;
    std::chrono::system_clock::time_point last_attach_time;
    int last_seen_pid;
};
```

---

## ConfigState vs RuntimeState — 差异

| 维度 | ConfigState | RuntimeState |
|------|-------------|--------------|
| 本质 | 期望状态（should be） | 实际状态（as-is） |
| 来源 | 配置文件 | 运行时观测（/proc, cgroupfs, events） |
| 可变性 | 只读（immutable） | 可变（continuous updates） |
| 粒度 | 全量配置（树） | per (group, process) 实例 |
| 生命周期 | 加载 → 下次加载 | 进程启动 → 进程退出 |
| 线程安全 | 天然安全（只读） | 需要互斥 |
| 序列化 | 可序列化为文件 | 不可序列化（运行时瞬态） |
| 一致性 | 总是自洽 | 可能 transiently 不一致（正在 Attach） |
| 所有者 | ConfigParser 生成 | RuntimeStateManager / Monitor |

---

## 1. What Happens After Process Restart

进程重启时 PID 变化，RuntimeState 经历以下转换：

```
初始状态（进程运行中）
├── pid: 1234
├── discovery_status: Discovered
├── attach_status: Attached
└── recovery_status: None

进程崩溃 / 重启
└── Monitor 检测：pid 1234 不再存在
    ├── discovery_status: Discovered → Missing
    ├── attach_status: Attached → Pending
    └── recovery_status: None → Detecting

Rediscovery
└── Discovery 根据 identity（mode + pattern）找到新 PID 5678
    ├── pid: 5678
    ├── discovery_status: Missing → Discovered
    └── recovery_status: Detecting → Recovering

Reattach
└── AttachEngine 将 PID 5678 绑定到 cgroup
    ├── attach_status: Pending → Attached
    └── recovery_status: Recovering → Recovered
```

关键不变：**Process Identity 不变**（mode, match_pattern, service_name, cgroup_path）。

---

## 2. What Survives Reload

**Config Reload** = 重新解析配置文件，生成新的 ConfigState。

### Survives（保留）

| RuntimeState 字段 | 保留条件 |
|-------------------|----------|
| mode | Group 未删除、mode 未变更 |
| match_pattern | Group 未删除、match 未变更 |
| cgroup_path | Group 未删除 |
| pid, tids | 进程仍在运行 |
| attach_status | 进程仍在运行 |
| discovery_status | 进程仍在运行 |
| recovery_status | 进程仍在运行 |

### Does Not Survive（需重建）

| RuntimeState 字段 | 原因 |
|-------------------|------|
| mode | Group 的 mode 被修改 |
| match_pattern | Group 的 match 被修改 |
| cgroup_path | Group 被删除或 controller 被修改 |

### Reload 决策逻辑

```
新 ConfigDomain
    │
    ▼
Diff (vs 旧 ConfigDomain)
    │
    ├── Group unchanged → RuntimeState 全部保留 ✓
    ├── Group mode/match modified → 对应 RuntimeState 重建
    ├── Group deleted → 对应 RuntimeState 销毁（Detach + cleanup）
    └── Group added → 创建新的 RuntimeState
```

---

## 3. What Survives Restart

**Daemon Restart** = Resource Manager 进程本身重启。

RuntimeState 存在内存中，进程重启后全部丢失。

### Lost on Restart

| 字段 | 原因 |
|------|------|
| pid, tids | 内存状态，重启后丢失。需要重新发现 |
| attach_status | 内存状态，重启后丢失 |
| discovery_status | 内存状态，重启后丢失 |
| recovery_status | 内存状态，重启后丢失 |
| last_discovery_time | 内存状态，重启后丢失 |
| last_seen_pid | 内存状态，重启后丢失 |

### Survives on Restart

唯一存活的是 ConfigState——因为配置文件在磁盘上。

```
Daemon Restart
    │
    ▼
Load Config → ConfigState（从文件重建）
    │
    ▼
RuntimeState（全部初始化为 Unknown/Pending/None）
    │
    ▼
Startup Lifecycle:
  1. Create cgroups（从 ConfigState 读取规则）
  2. Discover processes（扫描 /proc）
  3. Attach processes
  4. Start monitoring
```

**结论：RuntimeState 不存活于 Daemon Restart。** 每次重启后重新执行完整的 Startup Lifecycle。

### 例外：持久化 RuntimeState（未来扩展）

可以将来支持 RuntimeState 快照到磁盘文件，从而实现：

```
Daemon Restart → Load Config → Load RuntimeState snapshot
    → Compare → Resume monitoring（无需重新 Attach）
```

当前不实现。快照的复杂性（一致性问题、旧 PID 失效、竞态条件）超过收益。

---

## 4. Who Owns RuntimeState

### 所有者

`RuntimeStateManager` 是 RuntimeState 的唯一所有者。

```
RuntimeStateManager
├── owns: vector<RuntimeState>       # 所有活跃的运行时状态
├── owns: map<pid_t, RuntimeState*>  # PID → RuntimeState 索引
├── owns: map<string, RuntimeState*> # Group name → RuntimeState 索引
└── owns: mutex                      # 内部互斥
```

### 谁可以读写

| 组件 | 读 | 写 | 说明 |
|------|----|----|------|
| Monitor | ✓ | — | 读取 PID 变化，触发 Rediscovery |
| Discovery | — | ✓ | 写入 discovery_status, pid, tids |
| AttachEngine | — | ✓ | 写入 attach_status |
| Recovery | ✓ | ✓ | 读取状态，写入 recovery_status |
| Hot Reload | ✓ | — | 读取当前状态，与 ConfigState 对比 |
| CLI / API | ✓ | — | 读取状态用于观测 |

### 访问模式

```
Monitor ──reads──▶ RuntimeStateManager
     │
     ▼
  发现 PID 变化
     │
     ▼
Discovery ──writes──▶ RuntimeStateManager (pid, tids, discovery_status)
     │
     ▼
AttachEngine ──writes──▶ RuntimeStateManager (attach_status)
     │
     ▼
Recovery ──reads/writes──▶ RuntimeStateManager (recovery_status)
```

所有写操作通过 RuntimeStateManager 提供的方法完成，不直接修改 RuntimeState 字段。

```
class RuntimeStateManager {
    std::optional<RuntimeState> getByGroup(const std::string& group_name);
    std::optional<RuntimeState> getByPid(int pid);

    void updateDiscovery(int pid, DiscoveryStatus status);
    void updateAttach(int pid, AttachStatus status);
    void updateRecovery(int pid, RecoveryStatus status);

    void addState(const Group& group, const ProcessInfo& proc);
    void removeState(const std::string& group_name);
};
```

---

## 5. Relationship Between ConfigDomain and RuntimeState

### 核心关系

```
ConfigDomain（期望）                          RuntimeState（实际）
     │                                              │
     │  描述"应该"                                  │  记录"实际"
     │                                              │
     ├── GROUP("web") ── mode, match ──────►  identity (不变)
     │                                              │
     ├── GROUP("web")/CONTROLLER("cpu")             │
     │   └── ITEM("cpu.max") = "100000 100000"      │
     │                                              │
     │  Apply ──────────────────────────────────►  cgroup 写入
     │                                              │
     │  Diff ───────────────────────────────────►  compare
     │                                              │
```

### 对比表

| 维度 | ConfigDomain | RuntimeState |
|------|-------------|--------------|
| 结构 | 树（ROOT → GROUP → CONTROLLER → ITEM） | 扁平列表（per group-process binding） |
| 数量 | 1 棵树 | 0 到 N 个 RuntimeState |
| 生命周期 | 配置加载 → 下次配置加载 | 进程发现 → 进程消失 |
| 存储 | 堆上（智能指针树） | 堆上（vector + map 索引） |
| 一致性约束 | 类型校验 + 唯一性 | PID 唯一性、group 一对一 |
| 观测用途 | "配置是否符合预期" | "运行时是否和配置一致" |

### 一致性检查

一个常见的运维操作是对比 ConfigDomain 和 RuntimeState：

```
检查清单：
  [1] 每个 GROUP 是否有对应的 RuntimeState？      → 缺 = 进程未发现
  [2] 每个 RuntimeState 的 cgroup 值是否 = ConfigDomain 的期望值？ → 不等 = 绑定偏差
  [3] 每个 RuntimeState 的 attach_status = Attached？ → 否 = 绑定未生效
  [4] 每个 RuntimeState 的 recovery_status ≠ Failed？ → Failed = 需要人工介入
```

### 数据流

```
配置文件
    │
    ▼
ConfigParser ──────────► ConfigDomain（只读）
    │                          │
    │                     ConfigState
    │                          │
    │                    Apply Rules
    │                          │
    │                          ▼
    │                    ICgroupDriver
    │                          │
    │                          ▼
    │                    /sys/fs/cgroup
    │
    ▼
ProcessDiscovery ──────► RuntimeState（可变）
                              │
                         RuntimeStateManager
                              │
                    ┌─────────┼─────────┐
                    │         │         │
                    ▼         ▼         ▼
               Monitor  AttachEngine  Recovery
```

---

# Consequences

采用本决策后：

优点：

* ConfigState 和 RuntimeState 职责清晰，各自独立演化
* Process Identity 的引入使 Recovery 能够基于不变属性重新发现进程
* 三个独立状态维度（Attach / Discovery / Recovery）提供细粒度观测
* RuntimeStateManager 集中管理所有写操作，避免数据竞争
* 一致性检查可以精确回答"配置是否生效"
* Reload 决策逻辑清晰：Group 不变 → 状态保留；Group 变化 → 状态重建

代价：

* 需要维护两份状态，增加内存开销
* 需要 RuntimeStateManager 的线程安全实现
* Daemon Restart 后 RuntimeState 完全丢失，需要完整重启生命周期
* 状态同步逻辑（Monitor → Discovery → AttachEngine → Recovery）需要精心编排
* 三个独立状态维度增加了状态组合数，测试复杂度上升

---

# Related ADR

ADR-003 Config Tree — ConfigState 使用 ConfigDomain Tree
ADR-004 Mode System — Process Identity 中的 mode
ADR-008 Recovery — 依赖 RuntimeState 的 recovery_status
ADR-010 Attach Engine — 写入 RuntimeState 的 attach_status
ADR-011 Hot Reload — 对比 ConfigDomain 和 RuntimeState
