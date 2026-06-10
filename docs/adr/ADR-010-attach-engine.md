# ADR-010 Attach Engine

Status: Accepted

Date: 2026-06-10

Authors: Project Team

---

# Context

进程发现后需要将进程（PID/TID）绑定到 cgroup。

Attach 逻辑不能放在 Driver 中：

* Driver 的职责是底层操作
* Attach 涉及流程编排和状态管理
* Attach 需要重试和恢复能力

Attach 逻辑也不能放在 Monitor 中：

* Monitor 的职责是检测变化
* Attach 是独立的操作单元

因此需要定义独立的 Attach Engine。

---

# Decision

定义 `AttachEngine`，负责 Attach 流程的编排和执行。

## 职责

* 接收 Discovery 结果
* 调用 Driver 执行 Attach
* 更新 RuntimeState
* 处理 Attach 失败重试

## 执行流程

```text
Discovery
    │
    ▼
AttachEngine
    │
    ├── 调用 Driver.createGroup()
    ├── 调用 Driver.setValue() (设置所有控制文件)
    ├── 调用 Driver.attachTask() (绑定 PID)
    └── 调用 Driver.attachTask() (绑定 TID 列表)
    │
    ▼
Driver
    │
    ▼
/sys/fs/cgroup
```

## Attach 步骤

| 步骤 | 操作 | 失败处理 |
|------|------|----------|
| 1 | 创建 cgroup 目录 | 重试，标记 Failed |
| 2 | 设置资源限制 | 回滚删除 cgroup |
| 3 | Attach PID | 重试 |
| 4 | Attach TIDs | 记录失败 TID，不影响整体 |

## AttachEngine 与 Driver 的关系

```text
AttachEngine              Driver
    │                        │
    │──── createGroup ──────▶│
    │──── setValue ─────────▶│
    │──── attachTask ───────▶│
    │                        │
```

AttachEngine 编排流程，Driver 执行具体操作。

---

# Consequences

采用本决策后：

优点：

* Attach 逻辑独立，职责清晰
* 支持分步执行和回滚
* 重试逻辑集中管理
* 不影响 Monitor 和 Driver 的单一职责

代价：

* 引入额外组件
* Attach 流程需要仔细设计回滚逻辑

---

# Related ADR

ADR-006 Cgroup Driver

ADR-007 Process Discovery

ADR-008 Recovery
