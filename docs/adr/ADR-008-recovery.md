# ADR-008 Recovery Mechanism

Status: Accepted

Date: 2026-06-10

Authors: Project Team

---

# Context

进程重启后 PID 变化，之前的 cgroup 资源绑定丢失。

如果没有恢复机制：

* 服务重启后资源限制失效
* 必须手动重新绑定
* 线上事故风险高
* 运维人员需要时刻关注 PID 变化

恢复流程需要检测 PID 变化 → 重新发现进程 → 重新绑定资源。这个过程涉及多个组件，职责必须清晰划分。

---

# Decision

恢复流程由三个组件协作完成：**Monitor** → **Discovery** → **AttachEngine**。

---

## Recovery Workflow

```
┌─────────────────────────────────────────────────────────┐
│                    Recovery Workflow                     │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  正常运行时                                              │
│  ┌──────────────────────┐                               │
│  │ PID: 1234            │  Monitor 定期校验 PID 存活     │
│  │ attach: Attached     │                               │
│  │ recovery: None       │                               │
│  └──────────────────────┘                               │
│                                                         │
│  Monitor 检测到 PID 变化                                 │
│  ┌──────────────────────┐                               │
│  │ PID 1234 不存在      │  kill(1234, 0) 返回 ESRCH    │
│  │ attach: Attached → Pending                           │
│  │ recovery: None → Detecting                           │
│  └──────────────────────┘                               │
│                                                         │
│  Discovery 重新扫描 /proc                                │
│  ┌──────────────────────┐                               │
│  │ 按 identity 重新查找  │  comm/exe/cgroup 不变        │
│  │ 找到新 PID 5678      │                               │
│  │ recovery: Detecting → Recovering                     │
│  └──────────────────────┘                               │
│                                                         │
│  AttachEngine 执行 Reattach                             │
│  ┌──────────────────────┐                               │
│  │ 1. createGroup       │  路径不变，driver 幂等        │
│  │ 2. setValue × N      │  重新写入所有控制文件         │
│  │ 3. attachProcess     │  将 PID 5678 写入 cgroup.procs│
│  │ 4. attachThreads × N │  将 TID 写入 cgroup.threads  │
│  │ attach: Pending → Attached                           │
│  │ recovery: Recovering → Recovered                     │
│  └──────────────────────┘                               │
│                                                         │
│  恢复完成                                                │
│  ┌──────────────────────┐                               │
│  │ PID: 5678            │  Monitor 继续检测             │
│  │ attach: Attached     │                               │
│  │ recovery: Recovered  │                               │
│  └──────────────────────┘                               │
└─────────────────────────────────────────────────────────┘
```

---

## Process Restart Lifecycle

一个进程从启动到最终消亡，RuntimeState 经历如下状态序列：

```
Unknown
  │  (首次发现)
  ▼
Discovered → Attached
  │  (进程重启)           ──── 事件 A
  ▼
Missing → Pending → Detecting
  │  (Rediscovery)        ──── 事件 B
  ▼
Discovered → Recovering
  │  (Reattach)           ──── 事件 C
  ▼
Attached → Recovered
  │  (正常运行)
  ▼
... (可能再次重启，回到 Missing)
  │
  │  (配置变更，group 删除)  ──── 事件 D
  ▼
Detached → Removed (RuntimeState 销毁)
```

| 事件 | 触发 | 动作 |
|------|------|------|
| A: 进程重启 | PID 消失 | Monitor 检测到 PID 不存在 |
| B: Rediscovery | Monitor 触发 | Discovery 扫描 `/proc` |
| C: Reattach | Discovery 返回新 PID | AttachEngine 执行 |
| D: Group 删除 | Hot Reload | Detach + 清理 |

---

## 1. What Is Considered Process Restart

### 定义为

PID 变化且满足以下任一条件：

| 条件 | 判断方法 | 说明 |
|------|----------|------|
| 原 PID 不存在 | `kill(pid, 0)` → ESRCH | 进程已退出 |
| 原 PID 的 comm 变化 | 比较 `/proc/pid/comm` | PID 被回收，新进程复用 |
| 原 PID 的 cgroup 路径变化 | 比较 `/proc/pid/cgroup` | 进程移动到不同 cgroup |

### 什么不是进程重启

| 场景 | 是否重启 | 原因 |
|------|----------|------|
| PID 存活，comm 未变 | 否 | 正常运行 |
| PID 存活，cgroup 未变 | 否 | 正常运行 |
| PID 存活但线程 TID 增减 | 否 | 线程创建/销毁，不涉及重启 |
| PID 不存在且无新匹配 PID | 否 | 进程已退出，不再恢复 |

---

## 2. How PID Change Is Detected

### 检测方法

Monitor 在每个检测周期执行：

```cpp
void Monitor::check() {
    for (auto& state : runtime_states_) {
        if (state.pid == 0) continue;

        // Step 1: 检查 PID 是否存活
        if (kill(state.pid, 0) == 0) {
            continue;  // PID 存活，跳过
        }

        // Step 2: PID 不存在，检查是否被复用
        auto current_comm = read_comm(state.pid);
        if (current_comm == state.comm) {
            // PID 被回收但恰好 comm 相同（罕见）
            // 需要通过更多属性确认
            continue;
        }

        // Step 3: 确认 PID 变化，开始恢复
        state.attach_status = AttachStatus::Pending;
        state.recovery_status = RecoveryStatus::Detecting;
        trigger_recovery(state);
    }
}
```

### 检测开销

| 操作 | 耗时 | 频率 |
|------|------|------|
| `kill(pid, 0)` | 微秒级 | 每秒 |
| `read /proc/pid/comm` | 微秒级 | 仅 PID 不存在时 |
| `read /proc/pid/cgroup` | 微秒级 | 仅验证时 |

### PID 复用处理

PID 是有限资源，可能被回收后分配给新进程。

```
旧进程: PID=100, comm="nginx"
旧进程退出，PID 100 被回收

新进程: PID=100, comm="sshd"  ← PID 被复用
  → Monitor 检测到 comm 从 "nginx" 变为 "sshd"
  → 确认进程重启
```

---

## 3. How Rediscovery Works

### 触发条件

```
PID 不存活 → Monitor 标记 Detecting
    → Monitor 调用 Discovery.findProcesses(group.match_rule)
    → Discovery 扫描 /proc
```

### Rediscovery 策略

| 策略 | 说明 |
|------|------|
| 使用原始 MatchRule | 从 Group 中获取 match 规则，与首次发现一致 |
| 全量扫描 | 每次重新扫描整个 `/proc`（ADR-007 无缓存策略） |
| 匹配范围 | 由 Mode 决定（service / namespace / entity） |

### Rediscovery 结果

| 结果 | 处理 |
|------|------|
| 找到新 PID | 更新 RuntimeState.pid，进入 Recovering |
| 未找到 | 维持 Detecting，等待下一周期 |
| 连续 N 次未找到 | 标记 Failed，上报告警 |

---

## 4. How Reattach Works

Reattach 由 AttachEngine 执行，与首次 Attach 逻辑相同：

```
Reattach 步骤:
  1. createGroup(path)          ── 幂等，路径已存在时成功
  2. setValue(path, file, val)  ── 重新写入所有控制文件
  3. attachProcess(path, pid)   ── 将新 PID 绑定到 cgroup
  4. attachThread(path, tid)    ── 逐个绑定线程
```

每个步骤失败时：

| 步骤 | 失败处理 |
|------|----------|
| createGroup | 目录已存在 → 幂等成功；其他错误 → 重试 |
| setValue | 控制文件不存在 → 可能是控制器未启用 → 启用后重试 |
| attachProcess | PID 在 Attach 前退出 → 重新 Discovery |
| attachThread | 单个线程失败 → 记录日志，不影响其他线程 |

---

## 5. Recovery Timeout Policy

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `recovery_timeout` | 10s | 从 Detecting 到 Recovered 的最长时间 |
| `discovery_interval` | 1s | Rediscovery 的最小间隔 |
| `stale_pid_ttl` | 30s | PID 不存活后等待多久才标记 Failed |

### 超时后的行为

```
recovery_timeout (10s) 到期
    │
    ├── 仍在 Detecting（未找到新 PID）
    │   └── → RecoveryFailed, 上报告警
    │
    ├── 仍在 Recovering（Attach 失败）
    │   └── → RecoveryFailed, 上报告警
    │
    └── Recovery 进行中
        └── → 允许额外重试（见第 6 节）
```

---

## 6. Recovery Retry Policy

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `max_retries` | 3 | 单次 Recovery 的最大重试次数 |
| `retry_interval` | 1s | 重试间隔（线性） |

### 重试序列

```
Recovery 触发
  → 第 1 次尝试 → 失败 → 等待 retry_interval (1s)
  → 第 2 次尝试 → 失败 → 等待 retry_interval (1s)
  → 第 3 次尝试 → 失败 → 标记 RecoveryFailed
```

### 什么触发重试

| 失败原因 | 是否重试 |
|----------|----------|
| ProcessNotFound（未找到新 PID） | 是（等待进程启动） |
| AttachFailed（写入 cgroup 失败） | 是（内核暂时繁忙） |
| CgroupWriteFailed | 是（文件系统暂时错误） |
| ProcessExited（Attach 前进程退出） | 是（重新 Discovery） |
| CgroupVersionMismatch | 否（配置错误，重试无意义） |

---

## 7. Recovery Ownership

### 组件职责

| 组件 | 拥有 | 职责 |
|------|------|------|
| **Monitor** | 检测循环 | 定期检查 PID 存活；检测到变化后触发 Recovery |
| **Discovery** | 扫描 `/proc` | 按 MatchRule 重新发现进程；返回 ProcessInfo |
| **AttachEngine** | Driver + 编排 | 执行 createGroup → setValue → attachProcess/Thread |
| **Driver** | 内核接口 | 底层 cgroup 文件系统操作 |

### Monitor 不拥有什么

Monitor 不拥有 Discovery 的执行权——它只发起请求，Discovery 独立完成扫描。

Monitor 不拥有 Attach——它只传递新的 ProcessInfo，AttachEngine 独立执行。

### 所有权链

```
RuntimeStateManager（状态所有者）
    │
    ├── reads:  Monitor（检测 PID 存活）
    │
    ├── writes: Discovery（更新 pid/tids/discovery_status）
    │
    └── writes: AttachEngine（更新 attach_status）

Monitor（流程驱动者）
    ├── owns:   IProcessDiscovery（注入）
    └── owns:   AttachEngine（注入）
```

### 代码关系

```cpp
class Monitor {
    void check() {
        for (auto& state : states_) {
            if (pid_alive(state.pid)) continue;

            // 1. 触发 Rediscovery
            auto result = discovery_->findProcesses(group.match);
            if (!result || result->empty()) continue;  // 重试

            // 2. 更新状态
            auto& proc = result->at(0);
            state.pid = proc.pid;
            state.tids = discovery_->findThreads(proc.pid);
            state.discovery_status = DiscoveryStatus::Discovered;

            // 3. 触发 Reattach
            auto err = attach_engine_->reattach(group, state);
            if (!err) {
                state.recovery_status = RecoveryStatus::Recovered;
            }
        }
    }
};
```

---

# Consequences

采用本决策后：

优点：

* 恢复流程清晰：Monitor 检测 → Discovery 重新发现 → AttachEngine 重新绑定
* 三个组件职责正交，独立测试
* PID 检测通过 `kill(pid, 0)` 实现，开销极低
* 超时和重试策略覆盖常见失败场景
* PID 复用通过 comm 比较检测，避免误判

代价：

* Recovery 流程涉及三个组件串行调用，端到端延迟取决于 Discovery 扫描时间
* 重试期间资源绑定缺失，极端情况（持续重启）可能达到重试上限
* PID 复用检测在 PID 回收后立即被新进程复用的极端场景下有竞态条件

---

# Related ADR

ADR-005 Runtime State — Recovery 读写 RuntimeState 的 recovery_status
ADR-007 Process Discovery — Monitor 调用 Discovery 重新扫描
ADR-010 Attach Engine — Monitor 调用 AttachEngine 执行 Reattach
