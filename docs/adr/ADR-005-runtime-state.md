# ADR-005 Runtime State

Status: Accepted

Date: 2026-06-10

Authors: Project Team

---

# Context

配置状态描述了*应该是什么*，运行状态描述了*实际是什么*。

如果两者不分离：

* 配置变更无法追踪运行时影响
* 进程重启后配置丢失无法检测
* Attach 状态无法独立管理
* Recovery 流程无法准确判断

因此必须将配置状态与运行状态分离。

---

# Decision

定义两个独立状态：

* **ConfigState** —— 配置解析后的期望状态（ConfigDomain Tree）
* **RuntimeState** —— 运行时实际状态（进程映射、绑定关系）

## ConfigState

ConfigState 由配置解析生成，只读不可变。

包含：

```text
ConfigDomain Tree
```

## RuntimeState

RuntimeState 保存运行时动态信息。

包含：

| 字段 | 类型 | 说明 |
|------|------|------|
| PID | int | 进程 PID |
| TID | int[] | 线程 TID 列表 |
| attach_state | enum | Attach 状态 |
| recovery_state | enum | Recovery 状态 |
| last_discovery_time | time | 最近发现时间 |
| cgroup_path | string | 绑定的 cgroup 路径 |

## Attach 状态

```text
enum AttachState {
    Pending,     // 等待 Attach
    Attached,    // 已 Attach
    Detached,    // 已分离
    Failed       // Attach 失败
}
```

## Recovery 状态

```text
enum RecoveryState {
    None,        // 无需恢复
    Detecting,   // 检测中
    Recovering,  // 恢复中
    Recovered,   // 已恢复
    Failed       // 恢复失败
}
```

## 状态关系

```text
ConfigState  ──── 期望状态（只读）
                       │
                       ▼
                Diff / Apply
                       │
                       ▼
RuntimeState  ──── 实际状态（动态更新）
```

---

# Consequences

采用本决策后：

优点：

* 配置和运行状态解耦
* 支持增量 Apply
* Recovery 有明确的判断依据
* Attach 状态可观测

代价：

* 需要维护两份状态
* 需要实现状态同步机制

---

# Related ADR

ADR-003 Config Tree

ADR-008 Recovery

ADR-011 Hot Reload
