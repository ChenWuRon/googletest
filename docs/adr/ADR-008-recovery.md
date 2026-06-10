# ADR-008 Recovery

Status: Accepted

Date: 2026-06-10

Authors: Project Team

---

# Context

进程重启后 PID 发生变化，之前的 cgroup 资源绑定会丢失。

如果没有恢复机制：

* 服务重启后资源限制失效
* 必须手动重新绑定
* 线上事故风险高

因此需要设计自动恢复流程。

---

# Decision

恢复流程由三个组件协作完成：**Monitor**、**Discovery**、**AttachEngine**。

## 职责边界

| 组件 | 职责 |
|------|------|
| Monitor | 检测 PID 变化，触发恢复流程 |
| Discovery | 重新扫描 `/proc`，发现新 PID |
| AttachEngine | 执行 Attach 操作，将新 PID 绑定到 cgroup |

## 恢复流程

```text
进程重启
    │
    ▼
PID 变化
    │
    ▼
Monitor 检测到变化
    │
    ▼
触发 Rediscovery
    │
    ▼
Discovery 扫描 /proc
    │
    ▼
找到新 PID
    │
    ▼
更新 RuntimeState
    │
    ▼
AttachEngine 执行 Reattach
    │
    ▼
恢复完成
```

## RuntimeState 更新

恢复过程中 RuntimeState 的变化：

1. `recovery_state`: None → Detecting
2. 发现新 PID 后: Detecting → Recovering
3. AttachEngine 执行成功: Recovering → Recovered
4. AttachEngine 执行失败: Recovering → Failed

## 重试策略

| 参数 | 默认值 |
|------|--------|
| 最大重试次数 | 3 |
| 重试间隔 | 1s |
| 总超时 | 10s |

---

# Consequences

采用本决策后：

优点：

* 进程重启后自动恢复资源绑定
* 职责清晰，Monitor/Discovery/AttachEngine 各司其职
* RuntimeState 反映完整的恢复状态

代价：

* 需要额外的监控线程
* 存在恢复延迟（毫秒级）
* 需要处理重复恢复的场景

---

# Related ADR

ADR-005 Runtime State

ADR-007 Process Discovery

ADR-010 Attach Engine
